#include "page_20_set_print.h"
#include "un260/lv_core/lv_page_manager.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
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

static lv_settings_frame_t print_frame;
static bool print_pending;
static lv_obj_t* print_page = NULL;
static lv_obj_t* value_space_top = NULL;
static lv_obj_t* value_head1 = NULL;
static lv_obj_t* value_head2 = NULL;
static lv_obj_t* value_space_bottom = NULL;
static lv_obj_t* field_boxes[4] = { NULL };
static lv_obj_t* status_label = NULL;
static lv_obj_t* content_boxes[3] = { NULL };
static print_field_t active_field = PRINT_FIELD_SPACE_TOP;
static bool active_field_valid = false;

static void print_refresh_view(void);

static void print_set_status(const char* text, lv_color_t color)
{
    if (!status_label) return;
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_color(status_label, color, 0);
}

static void print_set_active_field(bool active, print_field_t field)
{
    if (active_field_valid && field_boxes[active_field])
        lv_obj_clear_state(field_boxes[active_field], LV_STATE_CHECKED);
    active_field = field;
    active_field_valid = active;
    if (active && field_boxes[field]) lv_obj_add_state(field_boxes[field], LV_STATE_CHECKED);
}

static void print_request_started(void)
{
    print_pending = true;
    print_set_status("Applying change - waiting for controller.", lv_color_hex(0x586B78));
    if (print_page) print_refresh_view();
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
    print_request_started();
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
    print_request_started();
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
    print_request_started();
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

    if (lv_event_get_code(e) != LV_EVENT_CLICKED || print_pending) return;

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

    print_refresh_view();
    print_set_active_field(true, field);

    if (!settings_detail_keyboard_show_ex(title, value, max_len, mode,
                                          print_field_keyboard_done,
                                          (void*)(uintptr_t)field,
                                          print_keyboard_close_cb, NULL)) {
        print_set_active_field(false, field);
    }
}

static void print_content_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || print_pending) return;
    print_content_t content = (print_content_t)(uintptr_t)lv_event_get_user_data(event);
    print_config_value_t config;
    print_config_get(&config);
    if (config.content == content) return;
    print_set_active_field(false, active_field);
    config.content = (uint8_t)content;
    print_send_content(content, &config);
}

static void print_refresh_view(void)
{
    print_config_value_t config;
    print_config_get(&config);
    if (value_space_top) lv_label_set_text_fmt(value_space_top, "%u", (unsigned)config.space_top);
    if (value_head1) lv_label_set_text(value_head1, config.head1[0] ? config.head1 : "Not set");
    if (value_head2) lv_label_set_text(value_head2, config.head2[0] ? config.head2 : "Not set");
    if (value_space_bottom) lv_label_set_text_fmt(value_space_bottom, "%u", (unsigned)config.space_bottom);
    for (unsigned i = 0; i < 3; ++i) {
        if (!content_boxes[i]) continue;
        if (config.content == i + 1) lv_obj_add_state(content_boxes[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(content_boxes[i], LV_STATE_CHECKED);
        if (print_pending) lv_obj_add_state(content_boxes[i], LV_STATE_DISABLED);
        else lv_obj_clear_state(content_boxes[i], LV_STATE_DISABLED);
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (!field_boxes[i]) continue;
        if (print_pending) lv_obj_add_state(field_boxes[i], LV_STATE_DISABLED);
        else lv_obj_clear_state(field_boxes[i], LV_STATE_DISABLED);
    }
}

static void print_create_field(lv_obj_t *parent, int y, const char *title,
                               lv_obj_t **value, print_field_t field)
{
    lv_settings_label(parent, title, 368, y + 13,
        &lv_font_instrument_sans_medium_16, 0x1D2B34);
    lv_obj_t *button = lv_settings_button(parent, 578, y, 654, 44,
        "", false, print_input_cb, (void *)(uintptr_t)field);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xFFFFFF), 0);
    field_boxes[field] = button;
    *value = lv_settings_label(button, "", 18, 13,
        &lv_font_instrument_sans_medium_16, 0x1D2B34);
    lv_obj_set_width(*value, 614);
    lv_label_set_long_mode(*value, LV_LABEL_LONG_DOT);
}

void ui_page_20_set_print_create(lv_obj_t *parent)
{
    if (print_page) return;
    active_field_valid = false;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_PRINT_TITLE), .icon = "Settings", .back = print_esc_cb
    };
    print_frame = lv_settings_frame_create(parent, &header);
    print_page = print_frame.root;
    lv_obj_t *body = print_frame.body;
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_settings_label(body, ui_text_get(UI_TEXT_SETTINGS_PRINT_CONTENT), 0, 12,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    const ui_text_id_t names[] = { UI_TEXT_SETTINGS_PRINT_CONTENT_LIST,
        UI_TEXT_SETTINGS_PRINT_CONTENT_SN, UI_TEXT_SETTINGS_PRINT_CONTENT_LIST_SN };
    for (unsigned i = 0; i < 3; ++i) {
        content_boxes[i] = lv_settings_button(body, 0, 54 + (int)i * 64, 320, 52,
            ui_text_get(names[i]), false, print_content_cb, (void *)(uintptr_t)(i + 1));
        lv_obj_set_style_bg_color(content_boxes[i], lv_color_hex(0xFFFFFF), 0);
    }
    lv_settings_label(body, "Receipt layout", 368, 12,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    print_create_field(body, 42, ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_TOP), &value_space_top, PRINT_FIELD_SPACE_TOP);
    print_create_field(body, 92, ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD1), &value_head1, PRINT_FIELD_HEAD1);
    print_create_field(body, 142, ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD2), &value_head2, PRINT_FIELD_HEAD2);
    print_create_field(body, 192, ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_BOTTOM), &value_space_bottom, PRINT_FIELD_SPACE_BOTTOM);
    status_label = print_frame.message;
    lv_label_set_text(status_label, print_pending ? "Waiting for controller." : "Values update after controller confirmation.");
    print_refresh_view();
}

void ui_page_20_set_print_destroy(void)
{
    settings_detail_keyboard_hide();

    if (print_page && lv_obj_is_valid(print_page)) {
        lv_obj_del(print_page);
    }

    print_page = NULL;
    memset(&print_frame, 0, sizeof(print_frame));
    value_space_top = NULL;
    value_head1 = NULL;
    value_head2 = NULL;
    value_space_bottom = NULL;
    status_label = NULL;

    for (int i = 0; i < 3; i++) {
        content_boxes[i] = NULL;
    }
    for (int i = 0; i < 4; i++) {
        field_boxes[i] = NULL;
    }
    active_field_valid = false;
}

void ui_page_20_set_print_on_boot_setting(const uint8_t* data, uint16_t len)
{
    uint8_t sub;
    print_config_value_t config;

    if (!data || len < 2) return;

    print_config_cancel_request();
    print_pending = false;
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
        print_set_status("Configuration received.", lv_color_hex(0x586B78));
    }
}

void ui_page_20_set_print_on_reply(const print_config_request_result_t* result)
{
    if (!result) return;
    print_pending = false;
    if (print_page) print_refresh_view();
    if (!print_page) return;

    if (!result->success) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_PRINT_FAIL), lv_color_hex(0xC03A2B));
    } else {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_PRINT_SUCCESS), lv_color_hex(0x1462CC));
    }
}
