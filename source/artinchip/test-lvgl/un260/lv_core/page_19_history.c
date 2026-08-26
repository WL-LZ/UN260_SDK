#include "page_19_history.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "un260/counting/counting_reject_reason.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/lv_page_event.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_components/lv_upgrade_popup.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/ui_history_export_data.h"
#include "un260/lv_system/ui_text.h"

LV_FONT_DECLARE(lv_font_instrument_sans_medium_14);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_16);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_18);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_20);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_24);

#define HISTORY_DETAIL_SECTION_ROWS 40
#define HISTORY_CARD_ROW_GAP 65

typedef struct {
    lv_obj_t *root;
    lv_obj_t *sidebar;
    lv_obj_t *history_nav_bg;
    lv_obj_t *user_nav_bg;
    lv_obj_t *history_nav_label;
    lv_obj_t *user_nav_label;
    lv_obj_t *breadcrumb_home;
    lv_obj_t *breadcrumb_sep;
    lv_obj_t *breadcrumb_curr;
    lv_obj_t *total_label;
    lv_obj_t *total_value;
    lv_obj_t *clean_btn;
    lv_obj_t *clean_btn_label;
    lv_obj_t *list_area;
    lv_obj_t *list_spacer;
    lv_obj_t *empty_label;
    lv_obj_t *detail_panel;
    lv_obj_t *detail_empty_label;
    lv_obj_t *detail_a_card;
    lv_obj_t *detail_b_card;
    lv_obj_t *detail_c_card;
    lv_obj_t *detail_a_title;
    lv_obj_t *detail_b_title;
    lv_obj_t *detail_c_title;
    lv_obj_t *detail_a_text;
    lv_obj_t *detail_b_text;
    lv_obj_t *detail_c_text;
    lv_obj_t *bottom_export;
    lv_obj_t *bottom_export_label;
    lv_obj_t *bottom_back;
    lv_obj_t *bottom_back_label;
    lv_obj_t *bottom_select;
    lv_obj_t *bottom_select_label;
    lv_obj_t *bottom_delete;
    lv_obj_t *bottom_delete_label;
    lv_obj_t *clean_overlay;
    lv_obj_t *clean_dialog;
    lv_obj_t *clean_dialog_title;
    lv_obj_t *clean_dialog_text;
    lv_obj_t *clean_dialog_cancel;
    lv_obj_t *clean_dialog_confirm;
    bool detail_mode;
    uint8_t detail_index;
    uint8_t visible_map[UI_HISTORY_MAX_RECORDS];
} history_page_ctx_t;

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *body;
    lv_obj_t *title;
    lv_obj_t *col_1_title;
    lv_obj_t *col_2_title;
    lv_obj_t *col_3_title;
    lv_obj_t *row_no[HISTORY_DETAIL_SECTION_ROWS];
    lv_obj_t *row_col_2[HISTORY_DETAIL_SECTION_ROWS];
    lv_obj_t *row_col_3[HISTORY_DETAIL_SECTION_ROWS];
} history_detail_section_ui_t;

typedef struct {
    lv_obj_t *card;
    lv_obj_t *bar;
    lv_obj_t *check_box;
    lv_obj_t *check_mark;
    lv_obj_t *no_label;
    lv_obj_t *currency_label;
    lv_obj_t *time_label;
} history_card_ui_t;

static history_page_ctx_t g_history_page;
static history_card_ui_t g_history_cards[UI_HISTORY_MAX_RECORDS];
static history_detail_section_ui_t g_history_detail_sections[3];

static void history_page_refresh(void);
static void history_page_show_clean_dialog(void);
static void history_page_hide_clean_dialog(void);
static void history_page_clear_total(void);
static void history_page_update_detail_panel(const ui_history_record_t *rec);
static void history_page_show_toast(const char *text, bool alarm);
static void history_page_show_list_mode(void);
static void history_page_show_detail_mode(uint8_t index);
static void history_page_update_footer_buttons(void);
static void history_page_select_all_toggle(void);
static void history_page_delete_selected(void);
static void history_detail_section_reset(history_detail_section_ui_t *section);
static void history_detail_section_apply_layout(history_detail_section_ui_t *section, int section_id);
static void history_detail_section_set_header(history_detail_section_ui_t *section, int section_id);
static void history_detail_section_set_row(history_detail_section_ui_t *section, int row, const char *c1, const char *c2, const char *c3, bool visible);
static int history_detail_split_lines(const char *src, char out[][96], int max_lines);
static int history_detail_parse_denom_rows(const ui_history_record_t *rec, char out[][3][96], int max_rows);
static int history_detail_parse_sn_rows(const ui_history_record_t *rec, char out[][3][96], int max_rows);
static int history_detail_parse_reject_rows(const ui_history_record_t *rec, char out[][3][96], int max_rows);
static bool history_hex_to_bytes(const char *text, uint8_t *buf, int buf_size, int *out_len);

static void history_format_u32_commas(char *buf, size_t size, uint32_t value)
{
    char tmp[32];
    size_t len;
    size_t i;
    size_t j = 0;

    if (buf == NULL || size == 0) {
        return;
    }

    lv_snprintf(tmp, sizeof(tmp), "%u", (unsigned)value);
    len = strlen(tmp);
    if (len <= 3) {
        lv_snprintf(buf, size, "%s", tmp);
        return;
    }

    for (i = 0; i < len && j + 1 < size; i++) {
        buf[j++] = tmp[i];
        if (i + 1 < len && ((len - i - 1) % 3) == 0 && j + 1 < size) {
            buf[j++] = ',';
        }
    }
    buf[j] = '\0';
}

static void history_set_text(lv_obj_t *obj, const char *text)
{
    if (obj != NULL && lv_obj_is_valid(obj)) {
        lv_label_set_text(obj, text ? text : "");
    }
}

static void history_label_set_single_line(lv_obj_t *obj, const char *text)
{
    if (obj == NULL || !lv_obj_is_valid(obj)) {
        return;
    }
    lv_label_set_text(obj, text ? text : "");
}

static void history_style_label(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    if (label == NULL || !lv_obj_is_valid(label)) {
        return;
    }
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
}

static lv_obj_t *history_create_text_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                           lv_coord_t w, lv_coord_t h,
                                           const char *text, const lv_font_t *font,
                                           lv_color_t color, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_align(label, align, 0);
    history_style_label(label, font, color);
    return label;
}

static void history_btn_feedback_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_opa_t opa = LV_OPA_COVER;

    if (btn == NULL || !lv_obj_is_valid(btn)) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        opa = LV_OPA_70;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        opa = LV_OPA_COVER;
    } else {
        return;
    }

    lv_obj_set_style_bg_opa(btn, opa, 0);
}

static void history_checkbox_update(history_card_ui_t *ui, bool selected)
{
    if (ui == NULL || ui->card == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(ui->bar, lv_color_hex(selected ? 0x00CFE0 : 0xE0E4EC), 0);
    lv_obj_set_style_border_color(ui->check_box, lv_color_hex(selected ? 0x00CFE0 : 0xE0E4EC), 0);
    lv_obj_set_style_bg_opa(ui->check_mark, selected ? LV_OPA_COVER : LV_OPA_0, 0);
    lv_obj_set_style_bg_color(ui->check_mark, lv_color_hex(0x00CFE0), 0);
    if (selected) {
        lv_obj_clear_flag(ui->check_mark, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui->check_mark, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_shadow_width(ui->card, selected ? 8 : 4, 0);
    lv_obj_set_style_shadow_opa(ui->card, selected ? LV_OPA_20 : LV_OPA_10, 0);
}

static int history_page_store_index_from_visible(uint8_t visible_index)
{
    if (visible_index >= UI_HISTORY_MAX_RECORDS) {
        return -1;
    }
    return (int)g_history_page.visible_map[visible_index];
}

static void history_card_toggle_selected(uint8_t index)
{
    int store_index = history_page_store_index_from_visible(index);

    if (store_index >= 0 && ui_history_record_toggle_selected((uint8_t)store_index)) {
        history_page_refresh();
    }
}

static void history_card_click_cb(lv_event_t *e)
{
    uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    int store_index;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    g_history_page.detail_mode = true;
    store_index = history_page_store_index_from_visible(index);
    g_history_page.detail_index = (uint8_t)((store_index >= 0) ? store_index : index);
    history_page_refresh();
}

static void history_check_click_cb(lv_event_t *e)
{
    uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    history_card_toggle_selected(index);
}

static lv_obj_t *history_create_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                       lv_coord_t w, lv_coord_t h, lv_color_t bg,
                                       const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 10, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_10, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn, history_btn_feedback_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn, history_btn_feedback_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn, history_btn_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

    if (text != NULL) {
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        history_style_label(label, &lv_font_instrument_sans_medium_16, lv_color_hex(0xE74D4D));
    }

    return btn;
}

static void history_nav_refresh(void)
{
    lv_obj_set_style_bg_opa(g_history_page.history_nav_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_history_page.history_nav_bg, lv_color_hex(0xE5FAFC), 0);
    lv_obj_set_style_bg_opa(g_history_page.user_nav_bg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(g_history_page.user_nav_bg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(g_history_page.user_nav_bg, LV_OBJ_FLAG_HIDDEN);
    history_style_label(g_history_page.history_nav_label, &lv_font_instrument_sans_medium_18, lv_color_hex(0x5A869A));
    history_style_label(g_history_page.user_nav_label, &lv_font_instrument_sans_medium_18, lv_color_hex(0x5A869A));
    lv_obj_add_flag(g_history_page.user_nav_label, LV_OBJ_FLAG_HIDDEN);
}

static void history_detail_section_create(history_detail_section_ui_t *section, lv_obj_t *parent, int section_id)
{
    int i;

    if (section == NULL || parent == NULL) {
        return;
    }

    history_detail_section_reset(section);
    section->panel = lv_obj_create(parent);
    lv_obj_remove_style_all(section->panel);
    lv_obj_clear_flag(section->panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(section->panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(section->panel, 0, 0);

    section->title = history_create_text_label(section->panel, 18, 10, 24, 20,
                                               "",
                                               &lv_font_instrument_sans_medium_16, lv_color_hex(0x00CFE0), LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(section->title, LV_OBJ_FLAG_HIDDEN);

    section->body = lv_obj_create(section->panel);
    lv_obj_remove_style_all(section->body);
    lv_obj_set_pos(section->body, 0, 43);
    lv_obj_set_size(section->body, 1, 1);
    lv_obj_set_scroll_dir(section->body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(section->body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(section->body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(section->body, 0, 0);

    section->col_1_title = history_create_text_label(section->panel, 0, 0, 40, 18, "",
                                                     &lv_font_instrument_sans_medium_14, lv_color_hex(0x5A869A), LV_TEXT_ALIGN_LEFT);
    section->col_2_title = history_create_text_label(section->panel, 0, 0, 60, 18, "",
                                                     &lv_font_instrument_sans_medium_14, lv_color_hex(0x5A869A), LV_TEXT_ALIGN_LEFT);
    section->col_3_title = history_create_text_label(section->panel, 0, 0, 80, 18, "",
                                                     &lv_font_instrument_sans_medium_14, lv_color_hex(0x5A869A), LV_TEXT_ALIGN_LEFT);

    for (i = 0; i < HISTORY_DETAIL_SECTION_ROWS; i++) {
        section->row_no[i] = history_create_text_label(section->body, 0, 0, 40, 18, "",
                                                       &lv_font_instrument_sans_medium_14,
                                                       section_id == 2 ? lv_color_hex(0x18495A) : lv_color_hex(0x00CFE0),
                                                       LV_TEXT_ALIGN_LEFT);
        section->row_col_2[i] = history_create_text_label(section->body, 0, 0, 160, 18, "",
                                                          &lv_font_instrument_sans_medium_14, lv_color_hex(0x000000), LV_TEXT_ALIGN_LEFT);
        section->row_col_3[i] = history_create_text_label(section->body, 0, 0, 170, 18, "",
                                                          &lv_font_instrument_sans_medium_14, lv_color_hex(0x18495A), LV_TEXT_ALIGN_LEFT);
    }
    history_detail_section_set_header(section, section_id);
}

static int history_detail_split_lines(const char *src, char out[][96], int max_lines)
{
    int count = 0;
    const char *p;

    if (src == NULL || out == NULL || max_lines <= 0) {
        return 0;
    }

    p = src;
    while (*p != '\0' && count < max_lines) {
        const char *line_start = p;
        size_t len = 0;

        while (p[len] != '\0' && p[len] != '\n' && p[len] != '\r') {
            len++;
        }

        if (len > 0) {
            size_t copy_len = len;
            if (copy_len >= sizeof(out[count])) {
                copy_len = sizeof(out[count]) - 1;
            }
            memcpy(out[count], line_start, copy_len);
            out[count][copy_len] = '\0';
            count++;
        }

        p += len;
        while (*p == '\n' || *p == '\r') {
            p++;
        }
    }

    return count;
}

static int history_detail_split_lines_large(const char *src, char out[][256], int max_lines)
{
    int count = 0;
    const char *p;

    if (src == NULL || out == NULL || max_lines <= 0) {
        return 0;
    }

    p = src;
    while (*p != '\0' && count < max_lines) {
        const char *line_start = p;
        size_t len = 0;

        while (p[len] != '\0' && p[len] != '\n' && p[len] != '\r') {
            len++;
        }

        if (len > 0) {
            size_t copy_len = len;
            if (copy_len >= sizeof(out[count])) {
                copy_len = sizeof(out[count]) - 1;
            }
            memcpy(out[count], line_start, copy_len);
            out[count][copy_len] = '\0';
            count++;
        }

        p += len;
        while (*p == '\n' || *p == '\r') {
            p++;
        }
    }

    return count;
}

static int history_hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    return -1;
}

static bool history_hex_to_bytes(const char *text, uint8_t *buf, int buf_size, int *out_len)
{
    int len = 0;
    int hi = -1;

    if (text == NULL || buf == NULL || buf_size <= 0) {
        return false;
    }

    for (const char *p = text; *p != '\0'; p++) {
        int v = history_hex_value(*p);
        if (v < 0) {
            continue;
        }
        if (hi < 0) {
            hi = v;
        } else {
            if (len >= buf_size) {
                break;
            }
            buf[len++] = (uint8_t)((hi << 4) | v);
            hi = -1;
        }
    }

    if (out_len) {
        *out_len = len;
    }
    return len > 0;
}

static void history_parse_0x0d_rows(const ui_history_record_t *rec, char out[][3][96], int max_rows)
{
    char lines[64][96];
    int line_count;
    int i;
    int parsed = 0;

    if (rec == NULL || out == NULL || max_rows <= 0) {
        return;
    }

    line_count = history_detail_split_lines(rec->session_log, lines, 64);
    for (i = 0; i < line_count && parsed < max_rows; i++) {
        uint8_t raw[256];
        int raw_len = 0;
        const char *space;
        char ascii_buf[256];
        int ascii_len;
        int j;
        char *p;
        int denom = 0;

        if (strncmp(lines[i], "0x0D", 4) != 0) {
            continue;
        }

        space = strchr(lines[i], ' ');
        if (space == NULL) {
            continue;
        }
        if (!history_hex_to_bytes(space + 1, raw, (int)sizeof(raw), &raw_len) || raw_len < 8) {
            continue;
        }
        if (raw[4] == 0x00 || raw[4] == 0xFF) {
            continue;
        }
        if (raw_len <= 5) {
            continue;
        }

        ascii_len = raw_len - 6;
        if (ascii_len <= 0) {
            continue;
        }
        if (ascii_len >= (int)sizeof(ascii_buf)) {
            ascii_len = (int)sizeof(ascii_buf) - 1;
        }

        for (j = 0; j < ascii_len; j++) {
            ascii_buf[j] = (char)raw[5 + j];
        }
        ascii_buf[ascii_len] = '\0';

        j = ascii_len - 1;
        while (j >= 0 && ascii_buf[j] == ' ') {
            ascii_buf[j--] = '\0';
        }
        p = ascii_buf;
        while (*p == ' ') p++;
        if (*p == '\0') {
            continue;
        }
        while (*p && isdigit((unsigned char)*p)) {
            denom = denom * 10 + (*p - '0');
            p++;
        }
        while (*p == ' ') p++;
        if (*p == '\0') {
            continue;
        }

        lv_snprintf(out[parsed][0], sizeof(out[parsed][0]), "%02d", parsed + 1);
        lv_snprintf(out[parsed][1], sizeof(out[parsed][1]), "%s", p);
        lv_snprintf(out[parsed][2], sizeof(out[parsed][2]), "%d", denom);
        parsed++;
    }
}

static void history_detail_section_reset(history_detail_section_ui_t *section)
{
    if (section == NULL) {
        return;
    }
    memset(section, 0, sizeof(*section));
}

static void history_detail_section_apply_layout(history_detail_section_ui_t *section, int section_id)
{
    static const lv_coord_t panel_x[3] = {0, 356, 786};
    static const lv_coord_t panel_w[3] = {354, 420, 281};
    static const lv_coord_t col_1_x[3] = {8, 8, 8};
    static const lv_coord_t col_2_x[3] = {106, 60, 60};
    static const lv_coord_t col_3_x[3] = {213, 215, 130};
    static const lv_coord_t col_1_w[3] = {77, 40, 40};
    static const lv_coord_t col_2_w[3] = {54, 150, 60};
    static const lv_coord_t col_3_w[3] = {80, 78, 170};
    lv_coord_t row_y;
    int i;

    if (section == NULL || section_id < 0 || section_id > 2) {
        return;
    }

    if (section->panel == NULL || !lv_obj_is_valid(section->panel)) {
        return;
    }

    lv_obj_set_pos(section->panel, panel_x[section_id], 0);
    lv_obj_set_size(section->panel, panel_w[section_id], 296);
    lv_obj_set_style_bg_color(section->panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(section->panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(section->panel, 0, 0);

    if (section->body && lv_obj_is_valid(section->body)) {
        lv_obj_set_pos(section->body, 0, 43);
        lv_obj_set_size(section->body, panel_w[section_id], 253);
        lv_obj_set_style_bg_opa(section->body, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(section->body, 0, 0);
        lv_obj_set_scroll_dir(section->body, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(section->body, LV_SCROLLBAR_MODE_OFF);
    }

    if (section->title) {
        lv_obj_set_pos(section->title, 18, 10);
        history_style_label(section->title, &lv_font_instrument_sans_semibold_16, lv_color_hex(0x00CFE0));
        lv_obj_add_flag(section->title, LV_OBJ_FLAG_HIDDEN);
    }

    if (section->col_1_title && section->col_2_title && section->col_3_title) {
        lv_obj_set_pos(section->col_1_title, col_1_x[section_id], 24);
        lv_obj_set_pos(section->col_2_title, col_2_x[section_id], 24);
        lv_obj_set_pos(section->col_3_title, col_3_x[section_id], 24);
        lv_obj_set_width(section->col_1_title, col_1_w[section_id]);
        lv_obj_set_width(section->col_2_title, col_2_w[section_id]);
        lv_obj_set_width(section->col_3_title, col_3_w[section_id]);
        history_style_label(section->col_1_title, &lv_font_instrument_sans_semibold_14, lv_color_hex(0x5A869A));
        history_style_label(section->col_2_title, &lv_font_instrument_sans_semibold_14, lv_color_hex(0x5A869A));
        history_style_label(section->col_3_title, &lv_font_instrument_sans_semibold_14, lv_color_hex(0x5A869A));
    }

    for (i = 0; i < HISTORY_DETAIL_SECTION_ROWS; i++) {
        row_y = (lv_coord_t)(4 + i * 35);
        if (section->row_no[i]) {
            const lv_font_t *row_no_font = section_id == 0 ? &lv_font_manrope_bold_14 : &lv_font_instrument_sans_medium_14;

            lv_obj_set_pos(section->row_no[i], col_1_x[section_id], row_y);
            lv_obj_set_width(section->row_no[i], col_1_w[section_id]);
            history_style_label(section->row_no[i], row_no_font,
                                section_id == 2 ? lv_color_hex(0x18495A) : lv_color_hex(0x00CFE0));
        }
        if (section->row_col_2[i]) {
            lv_obj_set_pos(section->row_col_2[i], col_2_x[section_id], row_y);
            lv_obj_set_width(section->row_col_2[i], col_2_w[section_id]);
            history_style_label(section->row_col_2[i],
                                (section_id == 0 || section_id == 2) ? &lv_font_manrope_bold_14 : &lv_font_instrument_sans_medium_14,
                                lv_color_hex(0x000000));
        }
        if (section->row_col_3[i]) {
            lv_obj_set_pos(section->row_col_3[i], col_3_x[section_id], row_y);
            lv_obj_set_width(section->row_col_3[i], col_3_w[section_id]);
            history_style_label(section->row_col_3[i],
                                section_id == 0 ? &lv_font_manrope_extrabold_14 : &lv_font_instrument_sans_medium_14,
                                lv_color_hex(0x18495A));
            if (section_id == 2) {
                lv_obj_set_height(section->row_col_3[i], 33);
                lv_label_set_long_mode(section->row_col_3[i], LV_LABEL_LONG_WRAP);
            } else {
                lv_obj_set_height(section->row_col_3[i], 18);
            }
        }
    }
}

static void history_detail_section_set_header(history_detail_section_ui_t *section, int section_id)
{
    if (section == NULL) {
        return;
    }

    switch (section_id) {
    case 1:
        history_label_set_single_line(section->col_1_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_NO));
        history_label_set_single_line(section->col_2_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_SN));
        history_label_set_single_line(section->col_3_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_DENOM));
        break;
    case 2:
        history_label_set_single_line(section->col_1_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_NO));
        history_label_set_single_line(section->col_2_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_PCS));
        history_label_set_single_line(section->col_3_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_REJECT));
        break;
    case 0:
    default:
        history_label_set_single_line(section->col_1_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_DENOM));
        history_label_set_single_line(section->col_2_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_PCS));
        history_label_set_single_line(section->col_3_title, ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_AMOUNT));
        break;
    }
    history_detail_section_apply_layout(section, section_id);
}

static void history_detail_section_set_row(history_detail_section_ui_t *section, int row,
                                           const char *c1, const char *c2, const char *c3, bool visible)
{
    if (section == NULL || row < 0 || row >= HISTORY_DETAIL_SECTION_ROWS) {
        return;
    }

    if (!visible) {
        if (section->row_no[row]) lv_obj_add_flag(section->row_no[row], LV_OBJ_FLAG_HIDDEN);
        if (section->row_col_2[row]) lv_obj_add_flag(section->row_col_2[row], LV_OBJ_FLAG_HIDDEN);
        if (section->row_col_3[row]) lv_obj_add_flag(section->row_col_3[row], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (section->row_no[row]) {
        history_label_set_single_line(section->row_no[row], c1);
        lv_obj_clear_flag(section->row_no[row], LV_OBJ_FLAG_HIDDEN);
    }
    if (section->row_col_2[row]) {
        history_label_set_single_line(section->row_col_2[row], c2);
        lv_obj_clear_flag(section->row_col_2[row], LV_OBJ_FLAG_HIDDEN);
    }
    if (section->row_col_3[row]) {
        history_label_set_single_line(section->row_col_3[row], c3);
        lv_obj_clear_flag(section->row_col_3[row], LV_OBJ_FLAG_HIDDEN);
    }
}

static int history_detail_parse_denom_rows(const ui_history_record_t *rec, char out[][3][96], int max_rows)
{
    char lines[HISTORY_DETAIL_SECTION_ROWS][96];
    int line_count;
    int i;

    if (rec == NULL || out == NULL || max_rows <= 0) {
        return 0;
    }

    line_count = history_detail_split_lines(rec->denom_text, lines, HISTORY_DETAIL_SECTION_ROWS);
    for (i = 0; i < line_count && i < max_rows; i++) {
        unsigned int denom = 0;
        unsigned int pcs = 0;
        unsigned int amount = 0;

        if (sscanf(lines[i], "%u x %u", &denom, &pcs) == 2 || sscanf(lines[i], "%uX%u", &denom, &pcs) == 2) {
            amount = denom * pcs;
            lv_snprintf(out[i][0], sizeof(out[i][0]), "%u", denom);
            lv_snprintf(out[i][1], sizeof(out[i][1]), "%u", pcs);
            history_format_u32_commas(out[i][2], sizeof(out[i][2]), amount);
        } else {
            lv_snprintf(out[i][0], sizeof(out[i][0]), "%s", lines[i]);
            lv_snprintf(out[i][1], sizeof(out[i][1]), "%s", "");
            lv_snprintf(out[i][2], sizeof(out[i][2]), "%s", "");
        }
    }

    return line_count < max_rows ? line_count : max_rows;
}

static int history_detail_parse_sn_rows(const ui_history_record_t *rec, char out[][3][96], int max_rows)
{
    int i;
    int count = 0;

    if (rec == NULL || out == NULL || max_rows <= 0) {
        return 0;
    }

    if (rec->sn_detail_text[0] != '\0') {
        char lines[HISTORY_DETAIL_SECTION_ROWS][256];
        int line_count = history_detail_split_lines_large(rec->sn_detail_text, lines, HISTORY_DETAIL_SECTION_ROWS);

        for (i = 0; i < line_count && i < max_rows; i++) {
            char *tab1 = strchr(lines[i], '\t');
            char *tab2 = NULL;
            char *no_text = "";
            char *sn_text = "";
            char *denom_text = "";

            if (tab1 != NULL) {
                *tab1++ = '\0';
                tab2 = strchr(tab1, '\t');
                if (tab2 != NULL) {
                    *tab2++ = '\0';
                }
            }

            no_text = lines[i];
            denom_text = tab1 ? tab1 : "--";
            sn_text = tab2 ? tab2 : "--";

            lv_snprintf(out[i][0], sizeof(out[i][0]), "%s", no_text);
            lv_snprintf(out[i][1], sizeof(out[i][1]), "%s", sn_text);
            lv_snprintf(out[i][2], sizeof(out[i][2]), "%s", denom_text);
        }
        return line_count < max_rows ? line_count : max_rows;
    }

    if (rec->session_log[0] != '\0') {
        history_parse_0x0d_rows(rec, out, max_rows);
        for (i = 0; i < max_rows; i++) {
            if (out[i][1][0] == '\0') {
                continue;
            }
            count++;
        }
        if (count > 0) {
            return count;
        }
    }

    {
        char lines[HISTORY_DETAIL_SECTION_ROWS][96];
        int line_count = history_detail_split_lines(rec->sn_text, lines, HISTORY_DETAIL_SECTION_ROWS);
        for (i = 0; i < line_count && i < max_rows; i++) {
            lv_snprintf(out[i][0], sizeof(out[i][0]), "%02d", i + 1);
            lv_snprintf(out[i][1], sizeof(out[i][1]), "%s", lines[i]);
            lv_snprintf(out[i][2], sizeof(out[i][2]), "%s", "--");
        }
        return line_count < max_rows ? line_count : max_rows;
    }
}

static int history_detail_parse_reject_rows(const ui_history_record_t *rec, char out[][3][96], int max_rows)
{
    char lines[HISTORY_DETAIL_SECTION_ROWS][96];
    int i;
    int line_count;
    int parsed = 0;

    if (rec == NULL || out == NULL || max_rows <= 0) {
        return 0;
    }

    line_count = history_detail_split_lines(rec->session_log, lines, HISTORY_DETAIL_SECTION_ROWS);
    for (i = 0; i < line_count && parsed < max_rows; i++) {
        uint8_t raw[160];
        int raw_len = 0;
        const char *space;
        unsigned err_code = 0;
        unsigned pcs = 0;
        const char *reason = "--";

        if (strncmp(lines[i], "0x0C", 4) != 0) {
            continue;
        }

        space = strchr(lines[i], ' ');
        if (space == NULL) {
            continue;
        }

        if (!history_hex_to_bytes(space + 1, raw, (int)sizeof(raw), &raw_len) || raw_len < 6) {
            continue;
        }

        err_code = raw[4];
        pcs = raw[5];
        if (err_code == 0x00 || err_code == 0xFF) {
            continue;
        }
        reason = counting_reject_reason_get((uint8_t)err_code);

        lv_snprintf(out[parsed][0], sizeof(out[parsed][0]), "%02d", parsed + 1);
        lv_snprintf(out[parsed][1], sizeof(out[parsed][1]), "%u", pcs);
        lv_snprintf(out[parsed][2], sizeof(out[parsed][2]), "%s", reason);
        parsed++;
    }

    return parsed;
}

static void history_page_update_detail_panel(const ui_history_record_t *rec)
{
    if (rec == NULL || !rec->valid) {
        history_set_text(g_history_page.detail_empty_label, "Select a record to view details");
        lv_obj_clear_flag(g_history_page.detail_empty_label, LV_OBJ_FLAG_HIDDEN);
        for (int s = 0; s < 3; s++) {
            lv_obj_add_flag(g_history_detail_sections[s].panel, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    history_set_text(g_history_page.detail_empty_label, "");
    lv_obj_add_flag(g_history_page.detail_empty_label, LV_OBJ_FLAG_HIDDEN);
    if (g_history_page.total_label) lv_obj_add_flag(g_history_page.total_label, LV_OBJ_FLAG_HIDDEN);
    if (g_history_page.total_value) lv_obj_add_flag(g_history_page.total_value, LV_OBJ_FLAG_HIDDEN);
        for (int s = 0; s < 3; s++) {
            lv_obj_clear_flag(g_history_detail_sections[s].panel, LV_OBJ_FLAG_HIDDEN);
        }

    {
        char row_a[HISTORY_DETAIL_SECTION_ROWS][3][96] = {{{0}}};
        char row_b[HISTORY_DETAIL_SECTION_ROWS][3][96] = {{{0}}};
        char row_c[HISTORY_DETAIL_SECTION_ROWS][3][96] = {{{0}}};
        int count_a = history_detail_parse_denom_rows(rec, row_a, HISTORY_DETAIL_SECTION_ROWS);
        int count_b = history_detail_parse_sn_rows(rec, row_b, HISTORY_DETAIL_SECTION_ROWS);
        int count_c = history_detail_parse_reject_rows(rec, row_c, HISTORY_DETAIL_SECTION_ROWS);
        int i;

        for (i = 0; i < HISTORY_DETAIL_SECTION_ROWS; i++) {
            history_detail_section_set_row(&g_history_detail_sections[0], i,
                                           row_a[i][0], row_a[i][1], row_a[i][2], i < count_a);
            history_detail_section_set_row(&g_history_detail_sections[1], i,
                                           row_b[i][0], row_b[i][1], row_b[i][2], i < count_b);
            history_detail_section_set_row(&g_history_detail_sections[2], i,
                                           row_c[i][0], row_c[i][1], row_c[i][2], i < count_c);
        }
        for (int s = 0; s < 3; s++) {
            if (g_history_detail_sections[s].body) {
                lv_obj_scroll_to_y(g_history_detail_sections[s].body, 0, LV_ANIM_OFF);
            }
        }
    }
}

static void history_page_show_toast(const char *text, bool alarm)
{
    lv_print_toast_config_t cfg = lv_print_toast_get_default_config();

    cfg.w = 320;
    cfg.h = 101;
    cfg.text = text ? text : "";
    cfg.show_loader = !alarm;
    cfg.align_center = true;
    cfg.use_text_area = false;
    cfg.loader_color = alarm ? lv_color_hex(0xC0392B) : LV_PRINT_TOAST_DEFAULT_LOADER_COLOR;
    cfg.auto_hide_ms = 2000;
    lv_print_toast_show_with_config(&cfg);
}

static void history_page_clean_confirm_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    history_page_clear_total();
    history_page_hide_clean_dialog();
}

static void history_page_clean_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    history_page_hide_clean_dialog();
}

static void history_page_export_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_history_export_data_request();
}

static void history_page_select_all_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    history_page_select_all_toggle();
}

static void history_page_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    history_page_delete_selected();
}

static void history_page_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (g_history_page.detail_mode) {
        g_history_page.detail_mode = false;
        history_page_refresh();
        return;
    }

    ui_manager_pop_page();
}

static void history_page_clean_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    history_page_show_clean_dialog();
}

static void history_page_update_footer_buttons(void)
{
    int selected_count = ui_history_record_selected_count_get();
    int record_count = ui_history_data_get() ? ui_history_data_get()->record_count : 0;
    lv_obj_t *select_label;
    bool detail_mode = g_history_page.detail_mode;

    if (g_history_page.clean_btn) {
        if (detail_mode) lv_obj_add_flag(g_history_page.clean_btn, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(g_history_page.clean_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_history_page.bottom_export) {
        if (detail_mode) lv_obj_add_flag(g_history_page.bottom_export, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(g_history_page.bottom_export, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_history_page.bottom_select) {
        if (detail_mode) lv_obj_add_flag(g_history_page.bottom_select, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(g_history_page.bottom_select, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_history_page.bottom_delete) {
        if (detail_mode) lv_obj_add_flag(g_history_page.bottom_delete, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(g_history_page.bottom_delete, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_history_page.bottom_select_label != NULL) {
        select_label = g_history_page.bottom_select_label;
        lv_label_set_text(select_label, (record_count > 0 && selected_count == record_count) ? "CLEAR" : "SELECT");
    }
}

static void history_page_select_all_toggle(void)
{
    const ui_history_store_t *store = ui_history_data_get();
    int selected_count = ui_history_record_selected_count_get();
    int record_count = store ? store->record_count : 0;

    if (record_count <= 0) {
        history_page_show_toast("Please Count First", true);
        return;
    }

    if (selected_count == record_count) {
        ui_history_record_clear_selected();
    } else {
        ui_history_record_set_all_selected(true);
    }
    history_page_refresh();
}

static void history_page_delete_selected(void)
{
    if (ui_history_record_selected_count_get() <= 0) {
        history_page_show_toast("Please select record first", true);
        return;
    }

    if (!ui_history_record_delete_selected()) {
        history_page_show_toast("Delete Failed", true);
        return;
    }

    g_history_page.detail_mode = false;
    history_page_refresh();
}

static void history_card_bind(history_card_ui_t *ui, uint8_t index, const ui_history_record_t *rec)
{
    char no_buf[8];
    char time_buf[32];

    if (ui == NULL || rec == NULL) {
        return;
    }

    lv_obj_clear_flag(ui->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->check_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->no_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->currency_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->time_label, LV_OBJ_FLAG_HIDDEN);

    lv_snprintf(no_buf, sizeof(no_buf), "%02u", (unsigned)(rec->slot_no ? rec->slot_no : (((rec->record_no - 1u) % UI_HISTORY_MAX_RECORDS) + 1u)));
    lv_snprintf(time_buf, sizeof(time_buf), "%04u-%02u-%02u %02u:%02u:%02u",
                (unsigned)rec->year, (unsigned)rec->month, (unsigned)rec->day,
                (unsigned)rec->hour, (unsigned)rec->minute, (unsigned)rec->second);

    lv_label_set_text(ui->no_label, no_buf);
    lv_label_set_text(ui->currency_label, rec->currency);
    lv_label_set_text(ui->time_label, time_buf);
    history_checkbox_update(ui, rec->selected);
}

static void history_card_hide(history_card_ui_t *ui)
{
    if (ui == NULL) {
        return;
    }
    if (ui->card) lv_obj_add_flag(ui->card, LV_OBJ_FLAG_HIDDEN);
    if (ui->bar) lv_obj_add_flag(ui->bar, LV_OBJ_FLAG_HIDDEN);
    if (ui->check_box) lv_obj_add_flag(ui->check_box, LV_OBJ_FLAG_HIDDEN);
    if (ui->check_mark) lv_obj_add_flag(ui->check_mark, LV_OBJ_FLAG_HIDDEN);
    if (ui->no_label) lv_obj_add_flag(ui->no_label, LV_OBJ_FLAG_HIDDEN);
    if (ui->currency_label) lv_obj_add_flag(ui->currency_label, LV_OBJ_FLAG_HIDDEN);
    if (ui->time_label) lv_obj_add_flag(ui->time_label, LV_OBJ_FLAG_HIDDEN);
}

static void history_page_build_cards(void)
{
    const ui_history_store_t *store = ui_history_data_get();
    uint16_t row_count = 0;
    uint32_t content_h;
    int i;
    int visible = 0;

    if (store->record_count == 0) {
        history_set_text(g_history_page.empty_label, "NO HISTORY");
        lv_obj_clear_flag(g_history_page.empty_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_history_page.empty_label, LV_OBJ_FLAG_HIDDEN);
    }

    memset(g_history_page.visible_map, 0xFF, sizeof(g_history_page.visible_map));

    row_count = (store->record_count + 1) / 2;
    if (row_count < 3) {
        row_count = 3;
    }
    content_h = (row_count - 1) * HISTORY_CARD_ROW_GAP + 41 + 24;
    lv_obj_set_height(g_history_page.list_spacer, (lv_coord_t)content_h);

    for (i = 0; i < UI_HISTORY_MAX_RECORDS; i++) {
        history_card_hide(&g_history_cards[i]);
    }

    for (i = 0; i < store->record_count && visible < UI_HISTORY_MAX_RECORDS; i++) {
        ui_history_record_t rec;

        if (!ui_history_record_get((uint8_t)i, &rec)) {
            continue;
        }
        if (!rec.valid) {
            continue;
        }
        g_history_page.visible_map[visible] = (uint8_t)i;
        history_card_bind(&g_history_cards[visible], (uint8_t)visible, &rec);
        visible++;
    }
}

static void history_page_refresh_total(void)
{
    char total_buf[32];

    history_format_u32_commas(total_buf, sizeof(total_buf), ui_history_total_notes_counted_get());
    lv_label_set_text(g_history_page.total_value, total_buf);
}

static void history_page_show_list_mode(void)
{
    lv_obj_clear_flag(g_history_page.list_area, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_history_page.detail_panel, LV_OBJ_FLAG_HIDDEN);
    if (g_history_page.total_label) lv_obj_clear_flag(g_history_page.total_label, LV_OBJ_FLAG_HIDDEN);
    if (g_history_page.total_value) lv_obj_clear_flag(g_history_page.total_value, LV_OBJ_FLAG_HIDDEN);
}

static void history_page_show_detail_mode(uint8_t index)
{
    ui_history_record_t rec;

    if (!ui_history_record_get(index, &rec)) {
        history_page_update_detail_panel(NULL);
        return;
    }

    lv_obj_add_flag(g_history_page.list_area, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_history_page.detail_panel, LV_OBJ_FLAG_HIDDEN);
    history_page_update_detail_panel(&rec);
}

static void history_page_refresh(void)
{
    history_page_refresh_total();
    history_nav_refresh();
    history_page_build_cards();
    history_page_update_footer_buttons();
    if (g_history_page.detail_mode) {
        history_page_show_detail_mode(g_history_page.detail_index);
    } else {
        history_page_show_list_mode();
    }
}

void ui_page_19_history_refresh(void)
{
    history_page_refresh();
}

static void history_page_clear_total(void)
{
    ui_history_total_notes_counted_clear();
    history_page_refresh_total();
}

static void history_page_show_clean_dialog(void)
{
    if (g_history_page.clean_overlay == NULL) {
        return;
    }
    lv_obj_clear_flag(g_history_page.clean_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_history_page.clean_overlay);
}

static void history_page_hide_clean_dialog(void)
{
    if (g_history_page.clean_overlay == NULL) {
        return;
    }
    lv_obj_add_flag(g_history_page.clean_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void history_page_create_clean_dialog(lv_obj_t *parent)
{
    lv_obj_t *overlay;
    lv_obj_t *card;
    lv_obj_t *title;
    lv_obj_t *text;
    lv_obj_t *cancel_btn;
    lv_obj_t *confirm_btn;

    overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, 1280, 400);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_40, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    card = lv_obj_create(overlay);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 420, 200);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 26, 0);
    lv_obj_set_style_shadow_width(card, 20, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(card);
    lv_label_set_text(title, "CLEAN HISTORY");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 20);
    history_style_label(title, &lv_font_instrument_sans_semibold_20, lv_color_hex(0x111111));

    text = lv_label_create(card);
    lv_label_set_text(text, "Clear total notes counted?");
    lv_obj_set_size(text, 360, 50);
    lv_obj_align(text, LV_ALIGN_TOP_LEFT, 24, 58);
    lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
    history_style_label(text, &lv_font_instrument_sans_medium_16, lv_color_hex(0x467085));

    cancel_btn = lv_btn_create(card);
    lv_obj_set_size(cancel_btn, 150, 42);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_LEFT, 24, -22);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xF3F4F6), 0);
    lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cancel_btn, 12, 0);
    lv_obj_add_event_cb(cancel_btn, history_page_clean_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(cancel_btn, history_btn_feedback_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(cancel_btn, history_btn_feedback_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(cancel_btn, history_btn_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_label_create(cancel_btn);
    lv_obj_t *cancel_label = lv_obj_get_child(cancel_btn, 0);
    lv_label_set_text(cancel_label, "CANCEL");
    lv_obj_center(cancel_label);
    history_style_label(cancel_label, &lv_font_instrument_sans_bold_16, lv_color_hex(0x275467));

    confirm_btn = lv_btn_create(card);
    lv_obj_set_size(confirm_btn, 150, 42);
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_RIGHT, -24, -22);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0xFDECEC), 0);
    lv_obj_set_style_bg_opa(confirm_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(confirm_btn, 12, 0);
    lv_obj_add_event_cb(confirm_btn, history_page_clean_confirm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(confirm_btn, history_btn_feedback_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(confirm_btn, history_btn_feedback_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(confirm_btn, history_btn_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_label_create(confirm_btn);
    lv_obj_t *confirm_label = lv_obj_get_child(confirm_btn, 0);
    lv_label_set_text(confirm_label, "CONFIRM");
    lv_obj_center(confirm_label);
    history_style_label(confirm_label, &lv_font_instrument_sans_bold_16, lv_color_hex(0xE74D4D));

    g_history_page.clean_overlay = overlay;
    g_history_page.clean_dialog = card;
    g_history_page.clean_dialog_title = title;
    g_history_page.clean_dialog_text = text;
    g_history_page.clean_dialog_cancel = cancel_btn;
    g_history_page.clean_dialog_confirm = confirm_btn;
    lv_obj_add_flag(g_history_page.clean_overlay, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *history_create_card(lv_obj_t *parent, uint8_t index)
{
    const lv_coord_t row = (index / 2);
    const lv_coord_t col = (index % 2);
    const lv_coord_t x = 30 + col * 552;
    const lv_coord_t y = row * HISTORY_CARD_ROW_GAP;
    history_card_ui_t *ui = &g_history_cards[index];
    lv_obj_t *card;
    lv_obj_t *hit_btn;
    lv_obj_t *bar;
    lv_obj_t *box;
    lv_obj_t *mark;
    lv_obj_t *no_label;
    lv_obj_t *currency_label;
    lv_obj_t *time_label;

    card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, 445, 41);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 8, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_shadow_ofs_y(card, 2, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xA8B3C5), 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, history_card_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)index);

    hit_btn = lv_btn_create(card);
    lv_obj_remove_style_all(hit_btn);
    lv_obj_set_pos(hit_btn, 0, 0);
    lv_obj_set_size(hit_btn, 64, 41);
    lv_obj_set_style_bg_opa(hit_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hit_btn, 0, 0);
    lv_obj_clear_flag(hit_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(hit_btn, history_check_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)index);

    bar = lv_obj_create(card);
    lv_obj_remove_style_all(bar);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, 5, 41);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE0E4EC), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    box = lv_btn_create(card);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, 14, 11);
    lv_obj_set_size(box, 19, 19);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0xE0E4EC), 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(box, history_check_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)index);

    mark = lv_obj_create(box);
    lv_obj_remove_style_all(mark);
    lv_obj_center(mark);
    lv_obj_set_size(mark, 9, 9);
    lv_obj_set_style_bg_color(mark, lv_color_hex(0x00CFE0), 0);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mark, 999, 0);
    lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);

    no_label = lv_label_create(card);
    lv_label_set_text(no_label, "01");
    lv_obj_set_pos(no_label, 54, 9);
    history_style_label(no_label, &lv_font_instrument_sans_medium_18, lv_color_hex(0x00CFE0));

    currency_label = lv_label_create(card);
    lv_label_set_text(currency_label, "CNY");
    lv_obj_set_pos(currency_label, 112, 9);
    history_style_label(currency_label, &lv_font_instrument_sans_medium_18, lv_color_hex(0x000000));

    time_label = lv_label_create(card);
    lv_label_set_text(time_label, "2026-04-23 17:36:00");
    lv_obj_set_pos(time_label, 213, 9);
    lv_obj_set_width(time_label, 210);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_RIGHT, 0);
    history_style_label(time_label, &lv_font_instrument_sans_medium_14, lv_color_hex(0x18495A));

    ui->card = card;
    (void)hit_btn;
    ui->bar = bar;
    ui->check_box = box;
    ui->check_mark = mark;
    ui->no_label = no_label;
    ui->currency_label = currency_label;
    ui->time_label = time_label;
    return card;
}

static void history_page_reset_cards(void)
{
    int i;

    for (i = 0; i < UI_HISTORY_MAX_RECORDS; i++) {
        memset(&g_history_cards[i], 0, sizeof(g_history_cards[i]));
    }
}

void ui_page_19_history_create(lv_obj_t *parent)
{
    int i;

    if (parent == NULL) {
        parent = lv_scr_act();
    }

    if (g_history_page.root != NULL && lv_obj_is_valid(g_history_page.root)) {
        history_page_refresh();
        return;
    }

    ui_history_data_init();
    history_page_reset_cards();

    memset(&g_history_page, 0, sizeof(g_history_page));
    g_history_page.root = lv_obj_create(parent);
    lv_obj_remove_style_all(g_history_page.root);
    lv_obj_set_pos(g_history_page.root, 0, 0);
    lv_obj_set_size(g_history_page.root, 1280, 400);
    lv_obj_clear_flag(g_history_page.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_history_page.root, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g_history_page.root, LV_OPA_COVER, 0);

    g_history_page.sidebar = lv_obj_create(g_history_page.root);
    lv_obj_remove_style_all(g_history_page.sidebar);
    lv_obj_set_pos(g_history_page.sidebar, 0, 0);
    lv_obj_set_size(g_history_page.sidebar, 213, 400);
    lv_obj_set_style_bg_color(g_history_page.sidebar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g_history_page.sidebar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_history_page.sidebar, LV_OBJ_FLAG_SCROLLABLE);

    {
        lv_obj_t *counter_title = history_create_text_label(g_history_page.sidebar, 26, 14, 180, 24,
                                                            "COUNTER SYSTEM", &lv_font_instrument_sans_medium_18,
                                                            lv_color_hex(0x5E697D), LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(counter_title, LV_LABEL_LONG_CLIP);
    }

    g_history_page.history_nav_bg = lv_obj_create(g_history_page.sidebar);
    lv_obj_remove_style_all(g_history_page.history_nav_bg);
    lv_obj_set_pos(g_history_page.history_nav_bg, 0, 52);
    lv_obj_set_size(g_history_page.history_nav_bg, 213, 49);
    lv_obj_set_style_bg_color(g_history_page.history_nav_bg, lv_color_hex(0xE5FAFC), 0);
    lv_obj_set_style_bg_opa(g_history_page.history_nav_bg, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_history_page.history_nav_bg, LV_OBJ_FLAG_SCROLLABLE);

    g_history_page.user_nav_bg = lv_obj_create(g_history_page.sidebar);
    lv_obj_remove_style_all(g_history_page.user_nav_bg);
    lv_obj_set_pos(g_history_page.user_nav_bg, 0, 101);
    lv_obj_set_size(g_history_page.user_nav_bg, 213, 49);
    lv_obj_set_style_bg_color(g_history_page.user_nav_bg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g_history_page.user_nav_bg, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_history_page.user_nav_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_history_page.user_nav_bg, LV_OBJ_FLAG_HIDDEN);

    g_history_page.history_nav_label = history_create_text_label(g_history_page.sidebar, 62, 68, 120, 20,
                                                                 "HISTORY", &lv_font_instrument_sans_bold_18,
                                                                 lv_color_hex(0x00CFE0), LV_TEXT_ALIGN_LEFT);
    g_history_page.user_nav_label = history_create_text_label(g_history_page.sidebar, 62, 117, 120, 20,
                                                              "USER", &lv_font_instrument_sans_medium_18,
                                                              lv_color_hex(0x8A94A8), LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(g_history_page.user_nav_label, LV_OBJ_FLAG_HIDDEN);

    history_create_text_label(g_history_page.root, 267, 31, 210, 20,
                              "SYSTEM", &lv_font_instrument_sans_medium_14,
                              lv_color_hex(0x8893A6), LV_TEXT_ALIGN_LEFT);
    history_create_text_label(g_history_page.root, 335, 31, 12, 20,
                              ">", &lv_font_instrument_sans_medium_14, lv_color_hex(0x85B2C2), LV_TEXT_ALIGN_LEFT);
    g_history_page.breadcrumb_curr = history_create_text_label(g_history_page.root, 353, 31, 120, 20,
                                                               "HISTORY", &lv_font_instrument_sans_bold_14,
                                                               lv_color_hex(0x00CFE0), LV_TEXT_ALIGN_LEFT);

    g_history_page.total_label = history_create_text_label(g_history_page.root, 267, 81, 240, 24,
                                                           "TOTAL NOTES COUNTED", &lv_font_instrument_sans_semibold_16,
                                                           lv_color_hex(0x4D5564), LV_TEXT_ALIGN_LEFT);
    g_history_page.total_value = history_create_text_label(g_history_page.root, 700, 73, 360, 38,
                                                           "0", &lv_font_manrope_bold_32,
                                                           lv_color_hex(0x00CFE0), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_style_text_font(g_history_page.total_value, &lv_font_manrope_bold_32, 0);

    g_history_page.clean_btn = history_create_button(g_history_page.root, 1083, 70, 92, 35,
                                                     lv_color_hex(0xFDF0F0), "CLEAN",
                                                     history_page_clean_btn_cb);
    history_style_label(lv_obj_get_child(g_history_page.clean_btn, 0), &lv_font_instrument_sans_bold_16, lv_color_hex(0xE74D4D));

    g_history_page.list_area = lv_obj_create(g_history_page.root);
    lv_obj_remove_style_all(g_history_page.list_area);
    lv_obj_set_pos(g_history_page.list_area, 213, 106);
    lv_obj_set_size(g_history_page.list_area, 1067, 242);
    lv_obj_set_style_bg_color(g_history_page.list_area, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g_history_page.list_area, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(g_history_page.list_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_history_page.list_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_move_foreground(g_history_page.clean_btn);

    g_history_page.empty_label = history_create_text_label(g_history_page.list_area, 0, 0, 200, 24,
                                                           "NO HISTORY", &lv_font_instrument_sans_medium_16,
                                                           lv_color_hex(0xA4AEBE), LV_TEXT_ALIGN_CENTER);
    lv_obj_center(g_history_page.empty_label);
    lv_obj_add_flag(g_history_page.empty_label, LV_OBJ_FLAG_HIDDEN);

    g_history_page.detail_panel = lv_obj_create(g_history_page.root);
    lv_obj_remove_style_all(g_history_page.detail_panel);
    lv_obj_set_pos(g_history_page.detail_panel, 213, 106);
    lv_obj_set_size(g_history_page.detail_panel, 1067, 242);
    lv_obj_set_style_bg_color(g_history_page.detail_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g_history_page.detail_panel, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_history_page.detail_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_history_page.detail_panel, LV_OBJ_FLAG_HIDDEN);

    g_history_page.detail_empty_label = history_create_text_label(g_history_page.detail_panel, 28, 24, 500, 24,
                                                                  "Select a record to view details",
                                                                  &lv_font_instrument_sans_medium_16, lv_color_hex(0x78A2B3),
                                                                  LV_TEXT_ALIGN_LEFT);
    history_detail_section_create(&g_history_detail_sections[0], g_history_page.detail_panel, 0);
    history_detail_section_create(&g_history_detail_sections[1], g_history_page.detail_panel, 1);
    history_detail_section_create(&g_history_detail_sections[2], g_history_page.detail_panel, 2);
    lv_obj_add_flag(g_history_detail_sections[0].panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_history_detail_sections[1].panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_history_detail_sections[2].panel, LV_OBJ_FLAG_HIDDEN);

    g_history_page.list_spacer = lv_obj_create(g_history_page.list_area);
    lv_obj_remove_style_all(g_history_page.list_spacer);
    lv_obj_set_pos(g_history_page.list_spacer, 0, 0);
    lv_obj_set_size(g_history_page.list_spacer, 1,
                    (((UI_HISTORY_MAX_RECORDS + 1) / 2) - 1) * HISTORY_CARD_ROW_GAP + 41 + 24);
    lv_obj_set_style_bg_opa(g_history_page.list_spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_history_page.list_spacer, 0, 0);

    for (i = 0; i < UI_HISTORY_MAX_RECORDS; i++) {
        history_create_card(g_history_page.list_area, (uint8_t)i);
    }

    g_history_page.bottom_export = history_create_button(g_history_page.root, 27, 356, 93, 44,
                                                         lv_color_hex(0x00CFE0), "EXPORT",
                                                         history_page_export_cb);
    history_style_label(lv_obj_get_child(g_history_page.bottom_export, 0), &lv_font_instrument_sans_medium_16, lv_color_hex(0xFFFFFF));

    g_history_page.bottom_select = history_create_button(g_history_page.root, 157, 356, 93, 44,
                                                         lv_color_hex(0xF4F7FB), "SELECT",
                                                         history_page_select_all_cb);
    g_history_page.bottom_select_label = lv_obj_get_child(g_history_page.bottom_select, 0);
    history_style_label(g_history_page.bottom_select_label, &lv_font_instrument_sans_bold_16, lv_color_hex(0x2A5568));

    g_history_page.bottom_delete = history_create_button(g_history_page.root, 286, 356, 93, 44,
                                                         lv_color_hex(0xFDECEC), "DELETE",
                                                         history_page_delete_cb);
    g_history_page.bottom_delete_label = lv_obj_get_child(g_history_page.bottom_delete, 0);
    history_style_label(g_history_page.bottom_delete_label, &lv_font_instrument_sans_bold_16, lv_color_hex(0xE74D4D));

    g_history_page.bottom_back = history_create_button(g_history_page.root, 415, 356, 93, 44,
                                                       lv_color_hex(0x818182), "BACK",
                                                       history_page_back_cb);
    g_history_page.bottom_back_label = lv_obj_get_child(g_history_page.bottom_back, 0);
    history_style_label(g_history_page.bottom_back_label, &lv_font_instrument_sans_bold_16, lv_color_hex(0xFFFFFF));

    history_page_create_clean_dialog(g_history_page.root);
    history_page_refresh();
}

void ui_page_19_history_destroy(void)
{
    if (g_history_page.root != NULL && lv_obj_is_valid(g_history_page.root)) {
        lv_obj_del(g_history_page.root);
    }

    memset(&g_history_page, 0, sizeof(g_history_page));
    memset(g_history_cards, 0, sizeof(g_history_cards));
}
