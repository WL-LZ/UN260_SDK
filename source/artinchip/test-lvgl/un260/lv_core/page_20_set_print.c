#include "page_20_set_print.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/ui_text.h"
#include "un260/print/print_config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PRINT_HEAD_MAX_LEN       PRINT_SETTING_HEAD_MAX_LEN
#define PRINT_SPACE_MAX_LINES    PRINT_SETTING_SPACE_MAX_LINES

typedef enum {
    PRINT_FIELD_SPACE_TOP = 0,
    PRINT_FIELD_HEAD1,
    PRINT_FIELD_HEAD2,
    PRINT_FIELD_SPACE_BOTTOM,
} print_field_t;

typedef enum {
    PRINT_CONTENT_LIST = PRINT_SETTING_CONTENT_LIST,
    PRINT_CONTENT_SN = PRINT_SETTING_CONTENT_SN,
    PRINT_CONTENT_LIST_SN = PRINT_SETTING_CONTENT_LIST_SN,
} print_content_t;

static lv_obj_t* print_page = NULL;
static lv_obj_t* value_space_top = NULL;
static lv_obj_t* value_head1 = NULL;
static lv_obj_t* value_head2 = NULL;
static lv_obj_t* value_space_bottom = NULL;
static lv_obj_t* field_boxes[4] = { NULL };
static lv_obj_t* status_label = NULL;
static lv_obj_t* content_boxes[3] = { NULL };
static lv_obj_t* preview_head1 = NULL;
static lv_obj_t* preview_head2 = NULL;
static lv_obj_t* preview_content = NULL;
static int active_content_box = -1;
static print_field_t active_field = PRINT_FIELD_SPACE_TOP;
static bool active_field_valid = false;

static void print_refresh_view(void);

static void print_set_status(const char* text, lv_color_t color)
{
    if (!status_label) return;
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_color(status_label, color, 0);
}

static int print_content_index(print_content_t content)
{
    switch (content) {
    case PRINT_CONTENT_LIST:
        return 0;
    case PRINT_CONTENT_SN:
        return 1;
    case PRINT_CONTENT_LIST_SN:
        return 2;
    default:
        return -1;
    }
}

static void print_set_active_field(bool active, print_field_t field)
{
    if (active_field_valid) {
        settings_detail_set_focus_box_active(field_boxes[active_field], false);
    }

    active_field = field;
    active_field_valid = active;

    if (active) {
        settings_detail_set_focus_box_active(field_boxes[field], true);
    }
}

static void print_keyboard_close_cb(void* user_data)
{
    (void)user_data;
    print_set_active_field(false, active_field);
}

static uint8_t print_parse_space(const char* value)
{
    long v;

    if (!value || value[0] == '\0') {
        return 0;
    }

    v = strtol(value, NULL, 10);
    if (v < 0) v = 0;
    if (v > PRINT_SPACE_MAX_LINES) v = PRINT_SPACE_MAX_LINES;
    return (uint8_t)v;
}

static bool print_send_content(print_content_t content,
                               const print_config_value_t* target)
{
    uint8_t payload[2] = { 0x01, (uint8_t)content };
    if (!print_config_request(payload[0], payload, sizeof(payload), target)) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_UART_NOT_READY), lv_color_hex(0xC03A2B));
        return false;
    }
    return true;
}

static bool print_send_head(uint8_t index, const char* text,
                            const print_config_value_t* target)
{
    uint8_t payload[2 + PRINT_HEAD_MAX_LEN];

    payload[0] = 0x02;
    payload[1] = index;
    memset(&payload[2], ' ', PRINT_HEAD_MAX_LEN);

    if (text) {
        size_t len = strlen(text);
        if (len > PRINT_HEAD_MAX_LEN) {
            len = PRINT_HEAD_MAX_LEN;
        }
        memcpy(&payload[2], text, len);
    }

    if (!print_config_request(payload[0], payload, sizeof(payload), target)) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_UART_NOT_READY), lv_color_hex(0xC03A2B));
        return false;
    }
    return true;
}

static bool print_send_space(uint8_t index, uint8_t lines,
                             const print_config_value_t* target)
{
    uint8_t payload[3] = { 0x03, index, lines };
    if (!print_config_request(payload[0], payload, sizeof(payload), target)) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_UART_NOT_READY), lv_color_hex(0xC03A2B));
        return false;
    }
    return true;
}

static void print_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_detail_keyboard_hide();
    ui_manager_pop_page();
}

static void print_field_keyboard_done(const char* value, void* user_data)
{
    print_field_t field = (print_field_t)(uintptr_t)user_data;
    print_config_value_t config;

    print_config_get(&config);

    switch (field) {
    case PRINT_FIELD_SPACE_TOP: {
        uint8_t lines = print_parse_space(value);
        config.space_top = lines;
        print_send_space(0x01, lines, &config);
        break;
    }

    case PRINT_FIELD_HEAD1: {
        char text[PRINT_HEAD_MAX_LEN + 1];
        lv_snprintf(text, sizeof(text), "%s", value ? value : "");
        lv_snprintf(config.head1, sizeof(config.head1), "%s", text);
        print_send_head(0x01, text, &config);
        break;
    }

    case PRINT_FIELD_HEAD2: {
        char text[PRINT_HEAD_MAX_LEN + 1];
        lv_snprintf(text, sizeof(text), "%s", value ? value : "");
        lv_snprintf(config.head2, sizeof(config.head2), "%s", text);
        print_send_head(0x02, text, &config);
        break;
    }

    case PRINT_FIELD_SPACE_BOTTOM: {
        uint8_t lines = print_parse_space(value);
        config.space_bottom = lines;
        print_send_space(0x02, lines, &config);
        break;
    }

    default:
        break;
    }

    print_refresh_view();
}

static void print_input_cb(lv_event_t* e)
{
    print_field_t field;
    char value[24];
    const char* title = "";
    settings_detail_keyboard_mode_t mode = SETTINGS_DETAIL_KEYBOARD_NUM;
    uint16_t max_len = 2;
    print_config_value_t config;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    field = (print_field_t)(uintptr_t)lv_event_get_user_data(e);
    value[0] = '\0';
    print_config_get(&config);

    switch (field) {
    case PRINT_FIELD_SPACE_TOP:
        title = ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_TOP);
        lv_snprintf(value, sizeof(value), "%u", (unsigned)config.space_top);
        break;

    case PRINT_FIELD_HEAD1:
        title = ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD1);
        lv_snprintf(value, sizeof(value), "%s", config.head1);
        mode = SETTINGS_DETAIL_KEYBOARD_TEXT;
        max_len = PRINT_HEAD_MAX_LEN;
        break;

    case PRINT_FIELD_HEAD2:
        title = ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD2);
        lv_snprintf(value, sizeof(value), "%s", config.head2);
        mode = SETTINGS_DETAIL_KEYBOARD_TEXT;
        max_len = PRINT_HEAD_MAX_LEN;
        break;

    case PRINT_FIELD_SPACE_BOTTOM:
        title = ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_BOTTOM);
        lv_snprintf(value, sizeof(value), "%u", (unsigned)config.space_bottom);
        break;

    default:
        return;
    }

    active_content_box = -1;
    print_refresh_view();
    print_set_active_field(true, field);

    if (!settings_detail_keyboard_show_ex(title, value, max_len, mode,
                                          print_field_keyboard_done,
                                          (void*)(uintptr_t)field,
                                          print_keyboard_close_cb, NULL)) {
        print_set_active_field(false, field);
    }
}

static void print_content_cb(lv_event_t* e)
{
    print_content_t content;
    int index;
    print_config_value_t config;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    content = (print_content_t)(uintptr_t)lv_event_get_user_data(e);
    index = print_content_index(content);
    print_set_active_field(false, active_field);
    active_content_box = index;
    print_refresh_view();

    print_config_get(&config);
    config.content = (uint8_t)content;
    print_send_content(content, &config);
}

static lv_obj_t* print_create_value_box(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w, const char* text,
                                        print_field_t field)
{
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, 42);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xF8F9FB), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(box, print_input_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)field);
    field_boxes[field] = box;

    lv_obj_t* label = settings_detail_create_label(box, text, &lv_font_instrument_sans_medium_18,
                                                   lv_color_hex(0x0D3440), 12, 12);
    lv_obj_set_width(label, w - 24);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static void print_create_field(lv_obj_t* parent, lv_coord_t y,
                               const char* title, lv_obj_t** out_value,
                               print_field_t field)
{
    settings_detail_create_label(parent, title, &lv_font_instrument_sans_semibold_16,
                                 lv_color_hex(0x0D3440), 24, y + 12);
    *out_value = print_create_value_box(parent, 250, y, 440, "", field);
}

static void print_create_content_row(lv_obj_t* parent, int index,
                                     lv_coord_t x, lv_coord_t y, lv_coord_t w,
                                     const char* text,
                                     print_content_t content)
{
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, x, y);
    lv_obj_set_size(row, w, 30);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    content_boxes[index] = settings_detail_create_select_box(row, 0, 3, 24,
                                                             print_content_cb,
                                                             (void*)(uintptr_t)content);

    lv_obj_t* label = settings_detail_create_label(row, text, &lv_font_instrument_sans_medium_16,
                                                   lv_color_hex(0x0D3440), 36, 6);
    lv_obj_set_width(label, w - 42);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
}

static void print_refresh_view(void)
{
    const char* content_text = ui_text_get(UI_TEXT_SETTINGS_PRINT_CONTENT_LIST);
    print_config_value_t config;
    print_content_t content;

    print_config_get(&config);
    switch (config.content) {
    case PRINT_CONTENT_LIST:
    case PRINT_CONTENT_SN:
    case PRINT_CONTENT_LIST_SN:
        content = (print_content_t)config.content;
        break;
    default:
        content = PRINT_CONTENT_LIST;
        break;
    }

    if (value_space_top) {
        lv_label_set_text_fmt(value_space_top, "%u", (unsigned)config.space_top);
    }
    if (value_head1) {
        lv_label_set_text(value_head1, config.head1[0] ? config.head1 : "---");
    }
    if (value_head2) {
        lv_label_set_text(value_head2, config.head2[0] ? config.head2 : "---");
    }
    if (value_space_bottom) {
        lv_label_set_text_fmt(value_space_bottom, "%u", (unsigned)config.space_bottom);
    }

    settings_detail_set_select_box_checked(content_boxes[0], content == PRINT_CONTENT_LIST);
    settings_detail_set_select_box_checked(content_boxes[1], content == PRINT_CONTENT_SN);
    settings_detail_set_select_box_checked(content_boxes[2], content == PRINT_CONTENT_LIST_SN);
    for (int i = 0; i < 3; i++) {
        settings_detail_set_select_box_active(content_boxes[i], i == active_content_box);
    }

    if (preview_head1) {
        lv_label_set_text(preview_head1, config.head1[0] ? config.head1 : "----------------");
    }
    if (preview_head2) {
        lv_label_set_text(preview_head2, config.head2[0] ? config.head2 : "----------------");
    }

    if (content == PRINT_CONTENT_SN) {
        content_text = ui_text_get(UI_TEXT_SETTINGS_PRINT_CONTENT_SN);
    } else if (content == PRINT_CONTENT_LIST_SN) {
        content_text = ui_text_get(UI_TEXT_SETTINGS_PRINT_CONTENT_LIST_SN);
    }

    if (preview_content) {
        lv_label_set_text_fmt(preview_content,
                              "%s\n\nCOIN      QTY      VALUE\n----      ---      -----\n100       24       2400\n50        25       1250\n20        25       520\n\nTotal:          4571 ALL\nCount:           151 PCS",
                              content_text);
    }
}

static void print_create_preview(lv_obj_t* parent)
{
    lv_obj_t* card = settings_detail_create_card(parent, 820, 18, 370, 306);
    lv_obj_set_style_shadow_width(card, 8, 0);

    lv_obj_t* header = lv_obj_create(card);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 370, 42);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    settings_detail_create_label(header, ui_text_get(UI_TEXT_SETTINGS_PRINT_PREVIEW),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0xFFFFFF), 128, 12);

    preview_head1 = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_14,
                                                 lv_color_hex(0x0D3440), 24, 58);
    preview_head2 = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_14,
                                                 lv_color_hex(0x0D3440), 24, 80);
    preview_content = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_14,
                                                   lv_color_hex(0x0D3440), 24, 112);
    lv_obj_set_width(preview_content, 320);
    lv_label_set_long_mode(preview_content, LV_LABEL_LONG_WRAP);
}

void ui_page_20_set_print_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (print_page) return;
    active_content_box = -1;
    active_field_valid = false;

    print_page = settings_detail_create_page(parent,
                                             ui_text_get(UI_TEXT_SETTINGS_PRINT_TITLE),
                                             print_esc_cb, &content);

    lv_obj_t* card = settings_detail_create_card(content, 38, 18, 730, 306);

    print_create_field(card, 18, ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_TOP),
                       &value_space_top, PRINT_FIELD_SPACE_TOP);

    print_create_field(card, 70, ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD1),
                       &value_head1, PRINT_FIELD_HEAD1);
    print_create_field(card, 122, ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD2),
                       &value_head2, PRINT_FIELD_HEAD2);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_PRINT_CONTENT),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x5686A5), 24, 168);
    print_create_content_row(card, 0, 250, 171, 190,
                             ui_text_get(UI_TEXT_SETTINGS_PRINT_CONTENT_LIST),
                             PRINT_CONTENT_LIST);
    print_create_content_row(card, 1, 470, 171, 220,
                             ui_text_get(UI_TEXT_SETTINGS_PRINT_CONTENT_SN),
                             PRINT_CONTENT_SN);
    print_create_content_row(card, 2, 250, 213, 260,
                             ui_text_get(UI_TEXT_SETTINGS_PRINT_CONTENT_LIST_SN),
                             PRINT_CONTENT_LIST_SN);

    print_create_field(card, 260, ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_BOTTOM),
                       &value_space_bottom, PRINT_FIELD_SPACE_BOTTOM);

    status_label = settings_detail_create_label(content, ui_text_get(UI_TEXT_SETTINGS_PRINT_READY),
                                                &lv_font_instrument_sans_medium_14,
                                                lv_color_hex(0x24D6A1), 982, 332);

    print_create_preview(content);
    print_refresh_view();
}

void ui_page_20_set_print_destroy(void)
{
    settings_detail_keyboard_hide();

    if (print_page && lv_obj_is_valid(print_page)) {
        lv_obj_del(print_page);
    }

    print_page = NULL;
    value_space_top = NULL;
    value_head1 = NULL;
    value_head2 = NULL;
    value_space_bottom = NULL;
    status_label = NULL;
    preview_head1 = NULL;
    preview_head2 = NULL;
    preview_content = NULL;

    for (int i = 0; i < 3; i++) {
        content_boxes[i] = NULL;
    }
    for (int i = 0; i < 4; i++) {
        field_boxes[i] = NULL;
    }
    active_content_box = -1;
    active_field_valid = false;
}

void ui_page_20_set_print_on_boot_setting(const uint8_t* data, uint16_t len)
{
    uint8_t sub;
    print_config_value_t config;

    if (!data || len < 2) return;

    print_config_cancel_request();
    sub = data[0];
    print_config_get(&config);
    switch (sub) {
    case 0x01:
        if (data[1] >= PRINT_SETTING_CONTENT_LIST &&
            data[1] <= PRINT_SETTING_CONTENT_LIST_SN) {
            config.content = data[1];
            print_config_confirm(&config);
        }
        break;

    case 0x02:
        if (len >= (uint16_t)(2 + PRINT_HEAD_MAX_LEN)) {
            char text[PRINT_HEAD_MAX_LEN + 1];
            size_t copy_len = PRINT_HEAD_MAX_LEN;

            memcpy(text, &data[2], PRINT_HEAD_MAX_LEN);
            while (copy_len > 0 &&
                   (text[copy_len - 1] == ' ' || text[copy_len - 1] == '\0')) {
                copy_len--;
            }
            text[copy_len] = '\0';

            if (data[1] == 0x01) {
                lv_snprintf(config.head1, sizeof(config.head1), "%s", text);
                print_config_confirm(&config);
            } else if (data[1] == 0x02) {
                lv_snprintf(config.head2, sizeof(config.head2), "%s", text);
                print_config_confirm(&config);
            }
        }
        break;

    case 0x03:
        if (len >= 3) {
            uint8_t lines = data[2];
            if (lines > PRINT_SPACE_MAX_LINES) lines = PRINT_SPACE_MAX_LINES;

            if (data[1] == 0x01) {
                config.space_top = lines;
                print_config_confirm(&config);
            } else if (data[1] == 0x02) {
                config.space_bottom = lines;
                print_config_confirm(&config);
            }
        }
        break;

    default:
        break;
    }

    if (print_page) {
        print_refresh_view();
    }
}

void ui_page_20_set_print_on_reply(const print_config_request_result_t* result)
{
    if (!result) return;
    active_content_box = -1;
    if (print_page) print_refresh_view();
    if (!print_page) return;

    if (!result->success) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_PRINT_FAIL), lv_color_hex(0xC03A2B));
    } else {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_PRINT_SUCCESS), lv_color_hex(0x24D6A1));
    }
}
