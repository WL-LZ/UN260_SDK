/* ================= page_10_debug.h ================= */
#ifndef PAGE_10_DEBUG_H
#define PAGE_10_DEBUG_H

#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_resources/lv_image_declear.h" 
#include "un260/lv_resources/lv_img_init.h" 
#include "lv_page_event.h"
#include "un260/lv_system/user_cfg.h"
#include <string.h>
#include "lvgl/lvgl.h"
#include "../aic_ui/aic_ui.h"
#include "../../third-party/lvgl-8.3.2/src/widgets/lv_textarea.h"



#endif

/* ================= page_10_debug.c ================= */
#include "page_10_debug.h"
#include "un260/lv_components/lv_debug_overlay.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_system/ui_export_data.h"
#include "un260/lv_system/ui_text.h"
#include "un260/recording/screen_recording_service.h"
#include "aic_ui/perf_stats.h"
#include <stdio.h>
#include <string.h>

#define MAX_LOG_LABELS 200

static lv_obj_t* page_debug = NULL;

typedef struct {
    lv_obj_t *input;
    lv_obj_t *keyboard;
    lv_obj_t *log_area;
    lv_obj_t *log_labels[MAX_LOG_LABELS];
    lv_obj_t *tx_count_label;
    lv_obj_t *rx_count_label;
    int log_count;
    uint32_t log_sequence;
    uint32_t tx_count;
    uint32_t rx_count;
} debug_page_context_t;

static debug_page_context_t g_debug_page;

static void debug_page_context_reset(void)
{
    memset(&g_debug_page, 0, sizeof(g_debug_page));
}

/* ========= HEX 键盘布局 ========= */
static const char* kb_hex_map[] = {
    "1", "2", "3", "4", "5", "\n",
    "6", "7", "8", "9", "0", "\n",
    "A", "B", "C", "D", "E", "F", "\n",
    "SPACE", "CLR", ""
};

static const lv_btnmatrix_ctrl_t kb_hex_ctrl_map[] = {
    1, 1, 1, 1, 1,                    // 1-5
    1, 1, 1, 1, 1,                    // 6-0
    1, 1, 1, 1, 1, 1,                 // A-F
    2,  1                           // SPACE, <-, CLR
};

/* ---------- 自动添加空格逻辑 ---------- */
static void ta_auto_space_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t* ta = lv_event_get_target(e);
    const char* txt = lv_textarea_get_text(ta);
    int len = strlen(txt);

    // 如果长度小于前缀，直接恢复前缀
    if (len < 6) { // "FD DF " 长度为6
        lv_textarea_set_text(ta, "FD DF ");
        lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
        return;
    }

    // 每输入2个HEX字符自动加空格（从第6位开始）
    if (len > 6 && txt[len - 1] != ' ' && ((len - 6) % 3) == 2) {
        lv_textarea_add_char(ta, ' ');
    }
}

/* ---------- HEX键盘按键事件 ---------- */
static void kb_hex_event_cb(lv_event_t* e) {
    lv_obj_t* kb = lv_event_get_target(e);
    uint16_t btn_id = lv_btnmatrix_get_selected_btn(kb);
    const char* txt;

    if (btn_id == LV_BTNMATRIX_BTN_NONE) {
        return;
    }
    txt = lv_btnmatrix_get_btn_text(kb, btn_id);
    if (txt == NULL) {
        return;
    }
    if (strcmp(txt, "SPACE") == 0) {
        lv_textarea_add_char(g_debug_page.input, ' ');
    }
    else if (strcmp(txt, "CLR") == 0) {
        lv_textarea_set_text(g_debug_page.input, "");
    }
    else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_del_char_forward(g_debug_page.input);
    }
    else {
        lv_textarea_add_text(g_debug_page.input, txt);
    }
}

/* ---------- 追加日志（带颜色） ---------- */
static void append_log(const char* prefix, const char* data, const char* color_hex)
{
    if (!g_debug_page.log_area || !lv_obj_is_valid(g_debug_page.log_area)) return;

    if (g_debug_page.log_count >= MAX_LOG_LABELS) {
        // 删除最老的一条
        if (g_debug_page.log_labels[0] &&
            lv_obj_is_valid(g_debug_page.log_labels[0])) {
            lv_obj_del(g_debug_page.log_labels[0]);
        }
        for (int i = 1; i < g_debug_page.log_count; i++) {
            g_debug_page.log_labels[i - 1] = g_debug_page.log_labels[i];
        }
        g_debug_page.log_labels[--g_debug_page.log_count] = NULL;
    }

    lv_obj_t* lbl = lv_label_create(g_debug_page.log_area);
    g_debug_page.log_labels[g_debug_page.log_count++] = lbl;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s %04u: %s", prefix,
             ++g_debug_page.log_sequence, data);
    lv_label_set_text(lbl, buf);

    // 解析颜色
    unsigned int r = 0, g = 0, b = 0;
    if (strlen(color_hex) == 6) sscanf(color_hex, "%02X%02X%02X", &r, &g, &b);
    lv_color_t c = lv_color_make(r, g, b);
    lv_obj_set_style_text_color(lbl, c, 0);

    // 排列纵向
    lv_coord_t y = 5;
    for (int i = 0; i < g_debug_page.log_count - 1; i++) {
        y += lv_obj_get_height(g_debug_page.log_labels[i]) + 2;
    }
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 5, y);

    lv_obj_scroll_to_y(g_debug_page.log_area, y, LV_ANIM_ON);
}
static int hex_str_to_bytes(const char *str, uint8_t *out, int max_len)
{
    int count = 0;
    while (*str && *(str + 1) && count < max_len) {
        if (*str == ' ') {
            str++;
            continue;
        }
        unsigned int val;
        if (sscanf(str, "%2x", &val) != 1)
            break;
        out[count++] = (uint8_t)val;
        str += 2;
    }
    return count;
}

static void btn_send_event_cb(lv_event_t* e)
{
    const char* cmd_str = lv_textarea_get_text(g_debug_page.input);
    if (!cmd_str || strlen(cmd_str) < 8) return;

    uint8_t frame[64];
    int len = hex_str_to_bytes(cmd_str, frame, sizeof(frame));
    if (len < PROTOCOL_FRAME_MIN_SIZE) {
        append_log("ERR", "Frame too short", "FF0000");
        return;
    }

    if (!protocol_frame_is_valid(frame, (size_t)len)) {
        append_log("ERR", "Invalid frame or length", "FF0000");
        return;
    }
    if (frame[len - 1] != PROTOCOL_FRAME_TRAILER) {
        append_log("ERR", "Invalid frame trailer", "FF0000");
        return;
    }

    uint8_t cmd_g = frame[3];                 // CMD-G
    uint8_t *cmd_s = &frame[4];               // CMD-Sx
    uint16_t cmd_s_len = len - PROTOCOL_FRAME_OVERHEAD;

    /* ===== 调用真实发送 ===== */
    if (protocol_send(cmd_g, cmd_s, cmd_s_len) < 0) {
        append_log("ERR", "Send failed", "FF0000");
        return;
    }

    /* ===== UI 显示 ===== */
    append_log("TX", cmd_str, "00FF00");
    g_debug_page.tx_count++;
    if (g_debug_page.tx_count_label &&
        lv_obj_is_valid(g_debug_page.tx_count_label)) {
        lv_label_set_text_fmt(g_debug_page.tx_count_label, "TX: %u",
                              g_debug_page.tx_count);
    }

    lv_textarea_set_text(g_debug_page.input, "FD DF ");
    lv_textarea_set_cursor_pos(g_debug_page.input, LV_TEXTAREA_CURSOR_LAST);
}



/* ---------- 清空日志按钮 ---------- */
static void btn_clear_log_event_cb(lv_event_t* e) {
    LV_UNUSED(e);
    for (int i = 0; i < g_debug_page.log_count; i++) {
        if (g_debug_page.log_labels[i] &&
            lv_obj_is_valid(g_debug_page.log_labels[i])) {
            lv_obj_del(g_debug_page.log_labels[i]);
        }
    }
    memset(g_debug_page.log_labels, 0, sizeof(g_debug_page.log_labels));
    g_debug_page.log_count = 0;
    g_debug_page.log_sequence = 0;
    g_debug_page.tx_count = 0;
    g_debug_page.rx_count = 0;
    if (g_debug_page.tx_count_label &&
        lv_obj_is_valid(g_debug_page.tx_count_label)) {
        lv_label_set_text(g_debug_page.tx_count_label, "TX: 0");
    }
    if (g_debug_page.rx_count_label &&
        lv_obj_is_valid(g_debug_page.rx_count_label)) {
        lv_label_set_text(g_debug_page.rx_count_label, "RX: 0");
    }
}

static void debug_log_show_toast(const char* text, bool alarm)
{
    lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

    toast_cfg.w = 380;
    toast_cfg.h = 101;
    toast_cfg.text = text;
    toast_cfg.show_loader = false;
    toast_cfg.align_center = true;
    toast_cfg.use_text_area = false;
    toast_cfg.loader_color = alarm ? lv_color_hex(0xC0392B) : lv_color_hex(0x18A66A);
    toast_cfg.auto_hide_ms = 1800;
    lv_print_toast_show_with_config(&toast_cfg);
}

static void btn_download_log_event_cb(lv_event_t* e)
{
    const char* lines[MAX_LOG_LABELS];
    ui_export_text_result_t result;
    size_t line_count = 0;

    LV_UNUSED(e);
    for (int i = 0; i < g_debug_page.log_count; i++) {
        if (g_debug_page.log_labels[i] != NULL &&
            lv_obj_is_valid(g_debug_page.log_labels[i])) {
            lines[line_count++] = lv_label_get_text(g_debug_page.log_labels[i]);
        }
    }

    result = ui_export_text_lines("comm_log", lines, line_count);
    if (result == UI_EXPORT_TEXT_OK) {
        debug_log_show_toast(ui_text_get(UI_TEXT_DEBUG_DOWNLOAD_SUCCESS), false);
    } else if (result == UI_EXPORT_TEXT_EMPTY) {
        debug_log_show_toast(ui_text_get(UI_TEXT_DEBUG_NO_LOG), true);
    } else if (result == UI_EXPORT_TEXT_USB_NOT_READY) {
        debug_log_show_toast(ui_text_get(UI_TEXT_WIDGET_SCREENSHOT_INSERT_USB), true);
    } else {
        debug_log_show_toast(ui_text_get(UI_TEXT_DEBUG_DOWNLOAD_FAILED), true);
    }
}


/* ---------- 清空输入框按钮 ---------- */
static void btn_clear_input_event_cb(lv_event_t* e) {
    LV_UNUSED(e);
    lv_textarea_set_text(g_debug_page.input, "");
}

/* ---------- 快捷命令按钮 ---------- */
static void btn_quick_cmd_event_cb(lv_event_t* e) {
    const char* cmd = (const char*)lv_event_get_user_data(e);
    lv_textarea_set_text(g_debug_page.input, cmd);
}

static void screenshot_switch_event_cb(lv_event_t* e)
{
    lv_obj_t* sw;
    bool enabled;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    sw = lv_event_get_target(e);
    enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (!user_cfg_screenshot_save(enabled)) {
        if (enabled) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
    }
}

static void screen_recording_switch_event_cb(lv_event_t* e)
{
    lv_obj_t* sw;
    bool enabled;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    sw = lv_event_get_target(e);
    enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (!user_cfg_screen_recording_save(enabled)) {
        if (enabled) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        return;
    }
    if (!enabled) {
        screen_recording_service_request_stop();
    }
}

static void performance_monitor_switch_event_cb(lv_event_t* e)
{
    lv_obj_t* sw;
    bool enabled;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    sw = lv_event_get_target(e);
    enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (!user_cfg_performance_monitor_save(enabled)) {
        if (enabled) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        return;
    }
    lv_debug_overlay_set_enabled(enabled);
}

static void performance_profile_switch_event_cb(lv_event_t* e)
{
    lv_obj_t* sw;
    bool enabled;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    sw = lv_event_get_target(e);
    enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (!user_cfg_performance_profile_save(enabled)) {
        if (enabled) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        return;
    }
    perf_profile_set_enabled(enabled);
}

static lv_obj_t* debug_create_setting_switch(lv_obj_t* parent,
                                             lv_coord_t group_x,
                                             const char* label_text,
                                             lv_color_t checked_color,
                                             bool enabled,
                                             lv_event_cb_t event_cb)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_t* sw;

    lv_label_set_text(label, label_text);
    lv_obj_set_size(label, 76, 28);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &lv_font_instrument_sans_medium_10, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xB8C1CC), 0);
    lv_obj_set_pos(label, group_x, 3);

    sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 46, 22);
    lv_obj_set_pos(sw, group_x + 15, 38);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x4A4F57), 0);
    lv_obj_set_style_bg_color(sw, checked_color,
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    if (enabled) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return sw;
}

void page_10_back_btn_event_cb(lv_event_t* e) {

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();

 }
/* ========== 主创建函数 ========== */
void ui_page_10_debug_create(void) {
    // page_debug 已存在，清理后重新创建
    if (page_debug && lv_obj_is_valid(page_debug)) {
        lv_obj_clean(page_debug);  // 清理所有子对象
        lv_obj_clear_flag(page_debug, LV_OBJ_FLAG_HIDDEN);  // 显示page_debug
    } else {
        page_debug = lv_obj_create(lv_scr_act());
    }
    debug_page_context_reset();
    lv_debug_overlay_init();

    lv_obj_set_size(page_debug, 1280, 400);
    lv_obj_set_pos(page_debug, 0, 0);
    lv_obj_set_style_bg_color(page_debug, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(page_debug, 0, 0);
    lv_obj_set_style_radius(page_debug, 0, 0);
    lv_obj_set_style_pad_all(page_debug, 0, 0);
    lv_obj_clear_flag(page_debug, LV_OBJ_FLAG_SCROLLABLE);

    /* ================= 左侧输入区（宽度550） ================= */
    lv_obj_t* left_panel = lv_obj_create(page_debug);
    lv_obj_set_size(left_panel, 550, 375);
    lv_obj_set_pos(left_panel, 10, 10);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(left_panel, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_border_width(left_panel, 1, 0);
    lv_obj_set_style_radius(left_panel, 8, 0);
    lv_obj_set_style_pad_all(left_panel, 0, 0);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    lv_obj_t* label_title = lv_label_create(left_panel);
    lv_label_set_text(label_title, "HEX Command");
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_obj_set_pos(label_title, 15, 10);

    // 输入框
    g_debug_page.input = lv_textarea_create(left_panel);
    lv_obj_set_size(g_debug_page.input, 380, 50);
    lv_obj_set_pos(g_debug_page.input, 15, 75);
    lv_textarea_set_placeholder_text(g_debug_page.input, "FD DF XX XX 0A");
    lv_textarea_set_text(g_debug_page.input, "FD DF ");        // 默认固定前缀
    lv_textarea_set_cursor_pos(g_debug_page.input, LV_TEXTAREA_CURSOR_LAST); // 光标移动到末尾
    lv_textarea_set_one_line(g_debug_page.input, true);
    lv_obj_set_style_bg_color(g_debug_page.input, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_text_color(g_debug_page.input, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_color(g_debug_page.input, lv_color_hex(0x4A9EFF), 0);
    lv_obj_add_event_cb(g_debug_page.input, ta_auto_space_event_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);


    // 发送按钮
    lv_obj_t* btn_send = lv_btn_create(left_panel);
    lv_obj_set_size(btn_send, 110, 90);
    lv_obj_set_pos(btn_send, 405, 75);
    lv_obj_set_style_bg_color(btn_send, lv_color_hex(0x00AA00), 0);
    lv_obj_add_event_cb(btn_send, btn_send_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_send_label = lv_label_create(btn_send);
    lv_label_set_text(btn_send_label, "Send");
    lv_obj_set_style_text_font(btn_send_label, &lv_font_instrument_sans_bold_20, 0);
    lv_obj_center(btn_send_label);
    // 发送按钮
    lv_obj_t* btn_sec = lv_btn_create(left_panel);
    lv_obj_set_pos(btn_sec, 120, 4);
    lv_obj_set_size(btn_sec, 78, 30);
    lv_obj_set_style_bg_color(btn_sec, lv_color_hex(0x00AA00), 0);
    lv_obj_add_event_cb(btn_sec, page_06_back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_esc_label = lv_label_create(btn_sec);
    lv_label_set_text(btn_esc_label, "ESC");
    lv_obj_set_style_text_font(btn_esc_label, &lv_font_instrument_sans_bold_20, 0);
    lv_obj_center(btn_esc_label);

    debug_create_setting_switch(
        left_panel, 207, ui_text_get(UI_TEXT_WIDGET_SCREENSHOT_LABEL),
        lv_color_hex(0x18A66A), user_cfg_screenshot_enabled(),
        screenshot_switch_event_cb);
    debug_create_setting_switch(
        left_panel, 289, ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_LABEL),
        lv_color_hex(0xE53935), user_cfg_screen_recording_enabled(),
        screen_recording_switch_event_cb);
    debug_create_setting_switch(
        left_panel, 371,
        ui_text_get(UI_TEXT_WIDGET_PERFORMANCE_MONITOR_LABEL),
        lv_color_hex(0x4A9EFF), user_cfg_performance_monitor_enabled(),
        performance_monitor_switch_event_cb);
    debug_create_setting_switch(
        left_panel, 453,
        ui_text_get(UI_TEXT_WIDGET_PERFORMANCE_PROFILE_LABEL),
        lv_color_hex(0x00A7C4), user_cfg_performance_profile_enabled(),
        performance_profile_switch_event_cb);
    
    // 清空输入按钮
    lv_obj_t* btn_clear_input = lv_btn_create(left_panel);
    lv_obj_set_size(btn_clear_input, 100, 35);
    lv_obj_set_pos(btn_clear_input, 15, 130);
    lv_obj_set_style_bg_color(btn_clear_input, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_clear_input, btn_clear_input_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_clr = lv_label_create(btn_clear_input);
    lv_label_set_text(lbl_clr, "Clear");
    lv_obj_center(lbl_clr);

    // 快捷命令按钮（示例）
    lv_obj_t* btn_quick1 = lv_btn_create(left_panel);
    lv_obj_set_size(btn_quick1, 100, 35);
    lv_obj_set_pos(btn_quick1, 125, 130);
    lv_obj_set_style_bg_color(btn_quick1, lv_color_hex(0x4A9EFF), 0);
    lv_obj_add_event_cb(btn_quick1, btn_quick_cmd_event_cb, LV_EVENT_CLICKED, "FD DF");

    lv_obj_t* lbl_q1 = lv_label_create(btn_quick1);
    lv_label_set_text(lbl_q1, "Query");
    lv_obj_center(lbl_q1);

    // HEX键盘
    g_debug_page.keyboard = lv_btnmatrix_create(left_panel);
    lv_btnmatrix_set_map(g_debug_page.keyboard, kb_hex_map);
    lv_btnmatrix_set_ctrl_map(g_debug_page.keyboard, kb_hex_ctrl_map);
    lv_obj_set_size(g_debug_page.keyboard, 500, 185);
    lv_obj_set_pos(g_debug_page.keyboard, 15, 175);
    lv_obj_set_style_bg_color(g_debug_page.keyboard, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(g_debug_page.keyboard, 0, 0);
    lv_obj_add_event_cb(g_debug_page.keyboard, kb_hex_event_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);

    /* ================= 右侧日志区（宽度680） ================= */
    lv_obj_t* right_panel = lv_obj_create(page_debug);
    lv_obj_set_size(right_panel, 700, 375);
    lv_obj_set_pos(right_panel, 570, 10);
    lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(right_panel, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_border_width(right_panel, 1, 0);
    lv_obj_set_style_radius(right_panel, 8, 0);
    lv_obj_set_style_pad_all(right_panel, 0, 0);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 顶部栏
    lv_obj_t* top_bar = lv_obj_create(right_panel);
    lv_obj_set_size(top_bar, 650, 40);
    lv_obj_set_pos(top_bar, 15, 10);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* label_log = lv_label_create(top_bar);
    lv_label_set_text(label_log, "Comm Log");
    lv_obj_set_style_text_color(label_log, lv_color_white(), 0);
    // 居中
    lv_coord_t a_parent_h = lv_obj_get_height(top_bar);
    lv_coord_t a_label_h = lv_obj_get_height(label_log);
    lv_obj_set_pos(label_log, 5, (a_parent_h - a_label_h) / 2);

    // 统计标签
    g_debug_page.tx_count_label = lv_label_create(top_bar);
    lv_label_set_text(g_debug_page.tx_count_label, "TX: 0");
    lv_obj_set_style_text_color(g_debug_page.tx_count_label,
                                lv_color_hex(0x00FF00), 0);
    lv_coord_t b_parent_h = lv_obj_get_height(top_bar);
    lv_coord_t b_label_h = lv_obj_get_height(label_log);
    lv_obj_set_pos(g_debug_page.tx_count_label, 175,
                   (b_parent_h - b_label_h) / 2);
    g_debug_page.rx_count_label = lv_label_create(top_bar);
    lv_label_set_text(g_debug_page.rx_count_label, "RX: 0");
    lv_obj_set_style_text_color(g_debug_page.rx_count_label,
                                lv_color_hex(0x4A9EFF), 0);
    lv_coord_t c_parent_h = lv_obj_get_height(top_bar);
    lv_coord_t c_label_h = lv_obj_get_height(label_log);
    lv_obj_set_pos(g_debug_page.rx_count_label, 240,
                   (c_parent_h - c_label_h) / 2);
    // 下载日志按钮
    lv_obj_t* btn_download_log = lv_btn_create(top_bar);
    lv_obj_set_size(btn_download_log, 100, 30);
    lv_obj_set_pos(btn_download_log, 310, 5);
    lv_obj_set_style_bg_color(btn_download_log, lv_color_hex(0x0066CC), 0);
    lv_obj_add_event_cb(btn_download_log, btn_download_log_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_download_log = lv_label_create(btn_download_log);
    lv_label_set_text(lbl_download_log, ui_text_get(UI_TEXT_DEBUG_DOWNLOAD));
    lv_obj_center(lbl_download_log);

    // 清空日志按钮
    lv_obj_t* btn_clear_log = lv_btn_create(top_bar);
    lv_obj_set_size(btn_clear_log, 80, 30);
    lv_obj_set_pos(btn_clear_log, 420, 5);
    lv_obj_set_style_bg_color(btn_clear_log, lv_color_hex(0xAA0000), 0);
    lv_obj_add_event_cb(btn_clear_log, btn_clear_log_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_clr_log = lv_label_create(btn_clear_log);
    lv_label_set_text(lbl_clr_log, "clear");
    lv_obj_center(lbl_clr_log);

    // 日志滚动区域
    g_debug_page.log_area = lv_obj_create(right_panel);
    lv_obj_set_size(g_debug_page.log_area, 650, 300);
    lv_obj_set_pos(g_debug_page.log_area, 15, 60);
    lv_obj_set_style_bg_color(g_debug_page.log_area, lv_color_black(), 0);
    lv_obj_set_style_border_color(g_debug_page.log_area,
                                  lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(g_debug_page.log_area, 4, 0);
    lv_obj_set_style_pad_all(g_debug_page.log_area, 5, 0);
    lv_obj_set_scroll_dir(g_debug_page.log_area, LV_DIR_VER);
}

void ui_page_10_debug_destroy(void) {
    if (page_debug && lv_obj_is_valid(page_debug)) {
        lv_obj_clean(page_debug);  // 清空子对象，但不删除page_debug本身
        lv_obj_add_flag(page_debug, LV_OBJ_FLAG_HIDDEN);  // 隐藏page_debug
    }
    debug_page_context_reset();
}

/* ========== 外部调用：追加接收日志 ========== */
bool debug_page_rx_log_is_active(void)
{
    return g_debug_page.log_area != NULL &&
           lv_obj_is_valid(g_debug_page.log_area);
}

void debug_append_rx_log(const char* data) {
    if (!debug_page_rx_log_is_active() || data == NULL) return;
    append_log("RX", data, "4A9EFF");
    g_debug_page.rx_count++;
    if (g_debug_page.rx_count_label &&
        lv_obj_is_valid(g_debug_page.rx_count_label)) {
        lv_label_set_text_fmt(g_debug_page.rx_count_label, "RX: %u",
                              g_debug_page.rx_count);
    }
}
