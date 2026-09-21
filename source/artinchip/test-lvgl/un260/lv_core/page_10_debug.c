#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_10_debug.h"
#include "settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_system/user_cfg.h"
#include "lv_port_indev.h"
#include "un260/lv_components/lv_debug_overlay.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_system/ui_export_data.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/recording/screen_recording_service.h"
#include "aic_ui/perf_stats.h"
#include <stdio.h>
#include <string.h>

#define MAX_LOG_LABELS 200

static lv_obj_t *mode_retry_button;
static void mode_retry_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) work_mode_service_retry();
}
static void mode_retry_refresh(void)
{
    if (!mode_retry_button) return;
    work_mode_snapshot_t mode;
    work_mode_service_get_snapshot(&mode);
    if (mode.phase == WORK_MODE_FAILED) lv_obj_clear_flag(mode_retry_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mode_retry_button, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t* page_debug = NULL;
static lv_settings_frame_t debug_frame;
static lv_obj_t *communication_view, *tools_view, *send_button, *export_button;
static lv_obj_t *debug_tabs[2];
static lv_timer_t *debug_timer;

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

static const char *kb_hex_map[] = {
    "1","2","3","4","5","6","\n",
    "7","8","9","0","A","B","\n",
    "C","D","E","F","Delete","Clear",""
};

/* ---------- 自动添加空格逻辑 ---------- */
/* Format once per key, never from textarea VALUE_CHANGED. With accepted_chars,
 * LVGL set_text emits that event while rebuilding individual characters; a
 * prefix-restoring handler recursively calls set_text on the partial value. */

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
    if (strcmp(txt, "Clear") == 0) {
        lv_textarea_set_text(g_debug_page.input, "FD DF ");
    } else if (strcmp(txt, "Delete") == 0) {
        int length = (int)strlen(lv_textarea_get_text(g_debug_page.input));
        if (length > 6) {
            char input[192];
            snprintf(input, sizeof(input), "%s", lv_textarea_get_text(g_debug_page.input));
            while (length > 6 && input[length - 1] == ' ') length--;
            if (length > 6) input[--length] = '\0';
            lv_textarea_set_text(g_debug_page.input, input);
            lv_textarea_set_cursor_pos(g_debug_page.input, LV_TEXTAREA_CURSOR_LAST);
        }
    } else {
        char input[192];
        snprintf(input, sizeof(input), "%s", lv_textarea_get_text(g_debug_page.input));
        size_t length = strlen(input);
        if (length < 6 || length >= sizeof(input)-2 || strlen(txt)!=1) return;
        input[length++] = txt[0];
        if ((length-6)%3 == 2) input[length++] = ' ';
        input[length] = '\0';
        lv_textarea_set_text(g_debug_page.input,input);
    }
    lv_textarea_set_cursor_pos(g_debug_page.input,LV_TEXTAREA_CURSOR_LAST);
}

/* ---------- 追加日志（带颜色） ---------- */
static void append_log(const char* prefix, const char* data, const char* color_hex)
{
    if (!g_debug_page.log_area || !lv_obj_is_valid(g_debug_page.log_area)) return;

    bool follow_tail = lv_obj_get_scroll_bottom(g_debug_page.log_area) <= 24;
    lv_coord_t keep_scroll = lv_obj_get_scroll_y(g_debug_page.log_area);
    if (g_debug_page.log_count >= MAX_LOG_LABELS) {
        // 删除最老的一条
        if (g_debug_page.log_labels[0] &&
            lv_obj_is_valid(g_debug_page.log_labels[0])) {
            keep_scroll -= lv_obj_get_height(g_debug_page.log_labels[0]) +
                           lv_obj_get_style_pad_row(g_debug_page.log_area,0);
            if (keep_scroll < 0) keep_scroll = 0;
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

    lv_obj_set_width(lbl, lv_pct(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl, &lv_font_instrument_sans_medium_14, 0);
    /* Native flex compacts after oldest-row deletion; no animated log scroll. */
    lv_obj_update_layout(g_debug_page.log_area);
    if (follow_tail) lv_obj_scroll_to_view(lbl, LV_ANIM_OFF);
    else lv_obj_scroll_to_y(g_debug_page.log_area,keep_scroll,LV_ANIM_OFF);
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
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !work_mode_service_diagnostic_ready()) return;
    const char* cmd_str = lv_textarea_get_text(g_debug_page.input);
    if (!cmd_str || strlen(cmd_str) < 8) return;

    uint8_t frame[64];
    int len = hex_str_to_bytes(cmd_str, frame, sizeof(frame));
    if (len < PROTOCOL_FRAME_MIN_SIZE) {
        append_log("ERR", "Frame too short", "FFAAA3");
        return;
    }

    if (!protocol_frame_is_valid(frame, (size_t)len)) {
        append_log("ERR", "Invalid frame or length", "FFAAA3");
        return;
    }
    if (frame[len - 1] != PROTOCOL_FRAME_TRAILER) {
        append_log("ERR", "Invalid frame trailer", "FFAAA3");
        return;
    }

    uint8_t cmd_g = frame[3];                 // CMD-G
    uint8_t *cmd_s = &frame[4];               // CMD-Sx
    uint16_t cmd_s_len = len - PROTOCOL_FRAME_OVERHEAD;

    if (cmd_g == 0x38 || cmd_g == 0x52 || cmd_g == 0x53 || cmd_g == 0x54 ||
        cmd_g == 0x5B || cmd_g == 0x5F || cmd_g == 0x46 || cmd_g == 0xC0 ||
        cmd_g == 0x0A) {
        append_log("ERR", "Use the dedicated test controls for this command", "FFAAA3");
        return;
    }

    /* ===== 调用真实发送 ===== */
    if (protocol_send(cmd_g, cmd_s, cmd_s_len) < 0) {
        append_log("ERR", "Send failed", "FFAAA3");
        return;
    }

    /* ===== UI 显示 ===== */
    append_log("TX", cmd_str, "7FD0AF");
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

static void debug_switch_card(lv_obj_t *parent,int x,int y,const char *title,const char *hint,
                              bool enabled,lv_event_cb_t callback)
{
    lv_obj_t *card=lv_settings_box(parent,x,y,608,115,0xFFFFFF);
    lv_obj_set_style_radius(card,14,0);
    lv_settings_label(card,title,24,24,&lv_font_instrument_sans_medium_20,0x1D2B34);
    lv_obj_t *copy=lv_settings_label(card,hint,24,61,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_obj_set_width(copy,445);lv_label_set_long_mode(copy,LV_LABEL_LONG_WRAP);
    lv_obj_t *sw=lv_switch_create(card);
    lv_obj_remove_style_all(sw);
    lv_obj_set_pos(sw,516,39);lv_obj_set_size(sw,64,36);
    lv_obj_set_ext_click_area(sw,10);
    lv_obj_set_style_radius(sw,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(sw,lv_color_hex(0xD9E2E7),0);lv_obj_set_style_bg_opa(sw,255,0);
    lv_obj_set_style_radius(sw,LV_RADIUS_CIRCLE,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw,lv_color_hex(0x1462CC),LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(sw,255,LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw,lv_color_white(),LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sw,255,LV_PART_KNOB);
    lv_obj_set_style_radius(sw,LV_RADIUS_CIRCLE,LV_PART_KNOB);
    lv_obj_set_style_pad_all(sw,-3,LV_PART_KNOB);
    if(enabled)lv_obj_add_state(sw,LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw,callback,LV_EVENT_VALUE_CHANGED,NULL);
}
static void debug_tab(lv_event_t *e)
{
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
    bool tools=(uintptr_t)lv_event_get_user_data(e)==1;
    if(tools) {
        lv_obj_add_flag(communication_view,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(tools_view,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(export_button,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(communication_view,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tools_view,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(export_button,LV_OBJ_FLAG_HIDDEN);
    }
    for(unsigned i=0;i<2;i++)
        lv_obj_set_style_bg_color(debug_tabs[i],lv_color_hex((i==1)==tools?0xFFFFFF:LV_SETTINGS_CONTROL_SURFACE),0);
}
static void debug_tick(lv_timer_t *timer)
{
    (void)timer;
    mode_retry_refresh();
    bool ready=work_mode_service_diagnostic_ready();
    if(ready)lv_obj_clear_state(send_button,LV_STATE_DISABLED);
    else lv_obj_add_state(send_button,LV_STATE_DISABLED);
    const char *message=ready?"HEX commands are sent directly to the controller.":work_mode_service_status_text();
    if(strcmp(lv_label_get_text(debug_frame.message),message))lv_label_set_text(debug_frame.message,message);
}
void page_10_back_btn_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e)==LV_EVENT_CLICKED)ui_manager_pop_page();
}
void ui_page_10_debug_create(void)
{
    if(page_debug)return;
    debug_page_context_reset();lv_debug_overlay_init();
    lv_settings_header_t header={"Debug","About & security / Service tools",NULL,page_10_back_btn_event_cb,NULL};
    debug_frame=lv_settings_frame_create(lv_scr_act(),&header);page_debug=debug_frame.root;
    settings_detail_add_run(page_debug);
    lv_obj_set_style_bg_opa(debug_frame.body,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(debug_frame.body,0,0);
    lv_obj_t *tabs=lv_settings_box(page_debug,632,22,390,46,LV_SETTINGS_CONTROL_SURFACE);
    lv_obj_set_style_radius(tabs,12,0);
    debug_tabs[0]=lv_settings_button(tabs,3,3,226,40,"Communication",false,debug_tab,(void *)0);
    debug_tabs[1]=lv_settings_button(tabs,229,3,158,40,"Tools",false,debug_tab,(void *)1);
    lv_obj_set_style_bg_color(debug_tabs[0],lv_color_white(),0);
    lv_obj_set_style_bg_color(debug_tabs[1],lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),0);

    communication_view=lv_settings_box(debug_frame.body,0,0,1232,242,0);
    lv_obj_set_style_bg_opa(communication_view,LV_OPA_TRANSP,0);
    lv_obj_t *input_panel=lv_settings_box(communication_view,0,0,630,242,0xFFFFFF);
    lv_obj_set_style_radius(input_panel,14,0);
    g_debug_page.input=lv_textarea_create(input_panel);
    lv_obj_set_pos(g_debug_page.input,14,14);lv_obj_set_size(g_debug_page.input,454,48);
    lv_textarea_set_one_line(g_debug_page.input,true);
    lv_textarea_set_max_length(g_debug_page.input,191);
    lv_textarea_set_accepted_chars(g_debug_page.input,"0123456789ABCDEF ");
    lv_textarea_set_text(g_debug_page.input,"FD DF ");
    lv_obj_set_style_text_font(g_debug_page.input,&lv_font_instrument_sans_medium_18,0);
    lv_obj_set_style_text_color(g_debug_page.input,lv_color_hex(0x1D2B34),0);
    lv_obj_set_style_bg_color(g_debug_page.input,lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),0);
    lv_obj_set_style_border_width(g_debug_page.input,1,0);
    lv_obj_set_style_border_color(g_debug_page.input,lv_color_hex(0xC7D7E8),0);
    lv_obj_set_style_radius(g_debug_page.input,10,0);
    lv_obj_set_style_pad_all(g_debug_page.input,12,0);
    lv_obj_set_size(g_debug_page.input,454,48);
    send_button=lv_settings_button(input_panel,480,14,136,48,"Send",true,btn_send_event_cb,NULL);
    g_debug_page.keyboard=lv_btnmatrix_create(input_panel);
    lv_btnmatrix_set_map(g_debug_page.keyboard,kb_hex_map);
    lv_obj_remove_style_all(g_debug_page.keyboard);
    lv_obj_set_pos(g_debug_page.keyboard,8,73);lv_obj_set_size(g_debug_page.keyboard,614,163);
    lv_obj_set_style_pad_all(g_debug_page.keyboard,6,0);
    lv_obj_set_style_pad_row(g_debug_page.keyboard,8,0);
    lv_obj_set_style_pad_column(g_debug_page.keyboard,8,0);
    lv_obj_set_style_radius(g_debug_page.keyboard,9,LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_debug_page.keyboard,lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(g_debug_page.keyboard,255,LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_debug_page.keyboard,lv_color_hex(0xE2E9EE),LV_PART_ITEMS|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(g_debug_page.keyboard,lv_color_hex(0x1D2B34),LV_PART_ITEMS);
    lv_obj_set_style_text_font(g_debug_page.keyboard,&lv_font_instrument_sans_medium_18,LV_PART_ITEMS);
    lv_obj_add_event_cb(g_debug_page.keyboard,kb_hex_event_cb,LV_EVENT_VALUE_CHANGED,NULL);

    lv_obj_t *log=lv_settings_box(communication_view,646,0,586,242,0xFFFFFF);
    lv_obj_set_style_radius(log,14,0);
    lv_settings_label(log,"Communication log",16,19,&lv_font_instrument_sans_medium_18,0x1D2B34);
    g_debug_page.tx_count_label=lv_settings_label(log,"TX: 0",246,22,&lv_font_instrument_sans_medium_14,0x586B78);
    g_debug_page.rx_count_label=lv_settings_label(log,"RX: 0",340,22,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_settings_button(log,466,8,104,44,"Clear",false,btn_clear_log_event_cb,NULL);
    g_debug_page.log_area=lv_settings_box(log,16,62,554,164,0x1D2B34);
    lv_obj_set_style_radius(g_debug_page.log_area,10,0);
    lv_obj_add_flag(g_debug_page.log_area,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_port_indev_set_drag_obj(g_debug_page.log_area,true);
    lv_obj_set_scroll_dir(g_debug_page.log_area,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_debug_page.log_area,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(g_debug_page.log_area,12,0);
    lv_obj_set_flex_flow(g_debug_page.log_area,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_debug_page.log_area,7,0);

    tools_view=lv_settings_box(debug_frame.body,0,0,1232,242,0);
    lv_obj_set_style_bg_opa(tools_view,LV_OPA_TRANSP,0);
    debug_switch_card(tools_view,0,0,"Screenshots","Save a capture to USB using the screenshot shortcut.",user_cfg_screenshot_enabled(),screenshot_switch_event_cb);
    debug_switch_card(tools_view,624,0,"Screen recording","Enable recording controls. Turning off stops recording.",user_cfg_screen_recording_enabled(),screen_recording_switch_event_cb);
    debug_switch_card(tools_view,0,127,"Performance monitor","Show live rendering and system statistics.",user_cfg_performance_monitor_enabled(),performance_monitor_switch_event_cb);
    debug_switch_card(tools_view,624,127,"Performance profile","Enable detailed performance sampling for diagnosis.",user_cfg_performance_profile_enabled(),performance_profile_switch_event_cb);
    lv_obj_add_flag(tools_view,LV_OBJ_FLAG_HIDDEN);
    export_button=lv_settings_button(debug_frame.footer,1050,0,182,46,"Export log",false,btn_download_log_event_cb,NULL);
    mode_retry_button=lv_settings_button(debug_frame.footer,904,0,130,46,"Retry",false,mode_retry_clicked,NULL);
    lv_obj_add_flag(mode_retry_button,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(debug_frame.message,884);
    debug_timer=lv_timer_create(debug_tick,200,NULL);debug_tick(NULL);
}
void ui_page_10_debug_destroy(void)
{
    if(debug_timer)lv_timer_del(debug_timer);
    debug_timer=NULL;
    if(page_debug)lv_obj_del(page_debug);
    page_debug=NULL;debug_frame=(lv_settings_frame_t){0};
    mode_retry_button=NULL;
    communication_view=tools_view=send_button=export_button=NULL;
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
    append_log("RX", data, "8EBFFA");
    g_debug_page.rx_count++;
    if (g_debug_page.rx_count_label &&
        lv_obj_is_valid(g_debug_page.rx_count_label)) {
        lv_label_set_text_fmt(g_debug_page.rx_count_label, "RX: %u",
                              g_debug_page.rx_count);
    }
}
