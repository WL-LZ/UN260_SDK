#include "page_31_get_wave.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_text.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WAVE_SOURCE_COUNT 7
#define WAVE_POINT_MAX 256
#define WAVE_PREVIEW_W 760
#define WAVE_PREVIEW_H 210
#define WAVE_REQUEST_TIMEOUT_MS 5000U
#define WAVE_LATE_FRAME_GUARD_MS 3000U

typedef struct {
    uint8_t id;
    ui_text_id_t text_id;
    uint32_t color_hex;
} wave_source_t;

typedef struct {
    lv_obj_t* card;
    lv_obj_t* dot;
    lv_obj_t* label;
} wave_source_item_t;

static const wave_source_t g_wave_sources[WAVE_SOURCE_COUNT] = {
    { 0x01, UI_TEXT_SETTINGS_WAVE_GET_MT, 0x38BDF8 },
    { 0x02, UI_TEXT_SETTINGS_WAVE_GET_MG12, 0x0EA5E9 },
    { 0x03, UI_TEXT_SETTINGS_WAVE_GET_MG3, 0x8B5CF6 },
    { 0x04, UI_TEXT_SETTINGS_WAVE_GET_MG4, 0x6366F1 },
    { 0x05, UI_TEXT_SETTINGS_WAVE_GET_MG56, 0x24B47E },
    { 0x06, UI_TEXT_SETTINGS_WAVE_GET_UVUP, 0xF59D2A },
    { 0x07, UI_TEXT_SETTINGS_WAVE_GET_UVDOWN, 0xF04444 },
};

static lv_obj_t* wave_page = NULL;
static lv_obj_t* wave_chart_host = NULL;
static lv_obj_t* wave_line = NULL;
static lv_obj_t* wave_placeholder = NULL;
static lv_obj_t* wave_status_label = NULL;
static lv_obj_t* wave_title_label = NULL;
static lv_obj_t* wave_request_button = NULL;
static wave_source_item_t source_items[WAVE_SOURCE_COUNT] = { 0 };
static lv_point_t wave_points[WAVE_POINT_MAX];
static uint8_t wave_values[WAVE_SOURCE_COUNT][WAVE_POINT_MAX];
static uint16_t wave_value_counts[WAVE_SOURCE_COUNT] = { 0 };
static uint16_t wave_current_count = 0;
static uint8_t selected_wave_id = 1;
static bool wave_request_active = false;
static uint32_t wave_request_deadline = 0;
static uint32_t wave_late_guard_deadline = 0;

static bool wave_time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return deadline_ms != 0 && (int32_t)(now_ms - deadline_ms) >= 0;
}

static bool wave_guard_active(uint32_t now_ms)
{
    if (wave_time_reached(now_ms, wave_late_guard_deadline)) {
        wave_late_guard_deadline = 0;
    }
    return wave_late_guard_deadline != 0;
}

static void wave_refresh_request_button(uint32_t now_ms)
{
    if (!wave_request_button || !lv_obj_is_valid(wave_request_button)) return;

    if (wave_request_active || wave_guard_active(now_ms)) {
        lv_obj_add_state(wave_request_button, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(wave_request_button, LV_STATE_DISABLED);
    }
}

static void wave_request_touch(uint32_t now_ms)
{
    wave_request_deadline = now_ms + WAVE_REQUEST_TIMEOUT_MS;
}

static void wave_request_finish(uint32_t now_ms, bool guard_late_frames)
{
    wave_request_active = false;
    wave_request_deadline = 0;
    if (guard_late_frames) {
        wave_late_guard_deadline = now_ms + WAVE_LATE_FRAME_GUARD_MS;
    }
    wave_refresh_request_button(now_ms);
}

static const wave_source_t* wave_find_source(uint8_t id)
{
    for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
        if (g_wave_sources[i].id == id) {
            return &g_wave_sources[i];
        }
    }

    return &g_wave_sources[0];
}

static size_t wave_source_index(uint8_t id)
{
    for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
        if (g_wave_sources[i].id == id) {
            return i;
        }
    }

    return 0;
}

static void wave_set_status(const char* text, lv_color_t color)
{
    if (!wave_status_label || !lv_obj_is_valid(wave_status_label)) return;

    lv_label_set_text(wave_status_label, text);
    lv_obj_set_style_text_color(wave_status_label, color, 0);
}

static void wave_refresh_sources(void)
{
    size_t selected_index = wave_source_index(selected_wave_id);

    for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
        bool selected = (i == selected_index);
        lv_color_t color = lv_color_hex(g_wave_sources[i].color_hex);

        if (source_items[i].card) {
            lv_obj_set_style_bg_color(source_items[i].card,
                                      selected ? lv_color_hex(0xF2FBFF) : lv_color_hex(0xFFFFFF),
                                      0);
            lv_obj_set_style_border_color(source_items[i].card,
                                          selected ? color : lv_color_hex(0xDDE6EF),
                                          0);
        }

        if (source_items[i].dot) {
            lv_obj_set_style_bg_color(source_items[i].dot, color, 0);
            lv_obj_set_style_bg_opa(source_items[i].dot, selected ? LV_OPA_COVER : LV_OPA_40, 0);
        }

        if (source_items[i].label) {
            lv_obj_set_style_text_color(source_items[i].label,
                                        selected ? lv_color_hex(0x1F2937) : lv_color_hex(0x6B7A90),
                                        0);
        }
    }

    if (wave_title_label) {
        const wave_source_t* source = wave_find_source(selected_wave_id);
        lv_label_set_text(wave_title_label, ui_text_get(source->text_id));
        if (wave_line && lv_obj_is_valid(wave_line)) {
            lv_obj_set_style_line_color(wave_line, lv_color_hex(source->color_hex), 0);
        }
    }
}

static void wave_clear_data(void)
{
    wave_current_count = 0;
    if (wave_line && lv_obj_is_valid(wave_line)) {
        lv_obj_add_flag(wave_line, LV_OBJ_FLAG_HIDDEN);
    }
    if (wave_placeholder && lv_obj_is_valid(wave_placeholder)) {
        lv_obj_clear_flag(wave_placeholder, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wave_draw_values(void)
{
    lv_coord_t x_step;

    if (!wave_line || !lv_obj_is_valid(wave_line) || wave_current_count == 0) return;

    if (wave_current_count > 1) {
        x_step = (lv_coord_t)(WAVE_PREVIEW_W - 24) / (lv_coord_t)(wave_current_count - 1);
    } else {
        x_step = 0;
    }

    for (uint16_t i = 0; i < wave_current_count; i++) {
        uint8_t value = wave_values[wave_source_index(selected_wave_id)][i];
        wave_points[i].x = (lv_coord_t)(12 + i * x_step);
        wave_points[i].y = (lv_coord_t)(12 + ((255 - value) * (WAVE_PREVIEW_H - 24)) / 255);
    }

    lv_line_set_points(wave_line, wave_points, wave_current_count);
    lv_obj_clear_flag(wave_line, LV_OBJ_FLAG_HIDDEN);
    if (wave_placeholder && lv_obj_is_valid(wave_placeholder)) {
        lv_obj_add_flag(wave_placeholder, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_invalidate(wave_chart_host);
}

static void wave_show_selected_values(void)
{
    wave_current_count = wave_value_counts[wave_source_index(selected_wave_id)];
    if (wave_current_count == 0) {
        wave_clear_data();
        return;
    }

    wave_draw_values();
}

static void wave_store_values(uint8_t wave_id, const uint8_t* values, uint16_t len)
{
    uint16_t step;
    size_t index;
    uint16_t count = 0;

    if (wave_id < 1 || wave_id > WAVE_SOURCE_COUNT || !values || len == 0) {
        wave_set_status(ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_NO_DATA),
                        lv_color_hex(0xF59D2A));
        return;
    }

    index = wave_source_index(wave_id);
    wave_value_counts[index] = 0;
    step = (uint16_t)((len + WAVE_POINT_MAX - 1) / WAVE_POINT_MAX);
    if (step == 0) step = 1;

    for (uint16_t i = 0; i < len && count < WAVE_POINT_MAX; i = (uint16_t)(i + step)) {
        wave_values[index][count++] = values[i];
    }
    wave_value_counts[index] = count;

    if (wave_id == selected_wave_id) {
        wave_current_count = count;
        wave_draw_values();
    }
    if (wave_id == WAVE_SOURCE_COUNT) {
        wave_set_status(ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_DONE),
                        lv_color_hex(0x24B47E));
    }
}

static bool wave_send_request(void)
{
    uint8_t payload = 0x01;

    return settings_detail_send_command(0x48, &payload, 1);
}

static void wave_request_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    ui_page_31_get_wave_request();
}

bool ui_page_31_get_wave_request(void)
{
    uint32_t now_ms = app_clock_uptime_ms();

    if (!wave_page || !lv_obj_is_valid(wave_page)) return false;
    if (wave_request_active || wave_guard_active(now_ms)) return false;

    wave_clear_data();
    if (wave_send_request()) {
        wave_request_active = true;
        wave_request_touch(now_ms);
        wave_refresh_request_button(now_ms);
        for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
            wave_value_counts[i] = 0;
        }
        wave_set_status(ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_WAITING),
                        lv_color_hex(0x0878C8));
        return true;
    }

    return false;
}

static void wave_source_cb(lv_event_t* e)
{
    uint8_t wave_id;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    wave_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (wave_id < 1 || wave_id > WAVE_SOURCE_COUNT) return;

    selected_wave_id = wave_id;
    wave_refresh_sources();
    wave_show_selected_values();
}

static void wave_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

static lv_obj_t* wave_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void wave_create_source_item(lv_obj_t* parent, size_t index)
{
    const wave_source_t* source = &g_wave_sources[index];
    lv_obj_t* item = lv_obj_create(parent);
    lv_coord_t y = (lv_coord_t)(42 + index * 36);

    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, 18, y);
    lv_obj_set_size(item, 244, 32);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_translate_y(item, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, wave_source_cb, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)source->id);

    source_items[index].dot = lv_obj_create(item);
    lv_obj_remove_style_all(source_items[index].dot);
    lv_obj_set_pos(source_items[index].dot, 15, 10);
    lv_obj_set_size(source_items[index].dot, 12, 12);
    lv_obj_set_style_radius(source_items[index].dot, 6, 0);
    lv_obj_set_style_bg_color(source_items[index].dot, lv_color_hex(source->color_hex), 0);
    lv_obj_set_style_bg_opa(source_items[index].dot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(source_items[index].dot, LV_OBJ_FLAG_SCROLLABLE);

    source_items[index].label = settings_detail_create_label(item, ui_text_get(source->text_id),
                                                             &lv_font_instrument_sans_medium_14,
                                                             lv_color_hex(0x0D3440), 40, 8);
    source_items[index].card = item;
}

static void wave_create_left_panel(lv_obj_t* parent)
{
    lv_obj_t* card = wave_create_card(parent, 38, 18, 286, 306);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_SOURCE),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x0D3440), 22, 14);

    for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
        wave_create_source_item(card, i);
    }
}

static void wave_create_grid_line(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* line = lv_obj_create(parent);

    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, h);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x243047), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_70, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static void wave_create_preview(lv_obj_t* parent)
{
    lv_obj_t* card = wave_create_card(parent, 350, 18, 840, 306);
    lv_obj_t* accent;

    wave_title_label = settings_detail_create_label(card, "",
                                                    &lv_font_instrument_sans_medium_18,
                                                    lv_color_hex(0x0D3440), 32, 18);
    wave_status_label = settings_detail_create_label(card,
                                                     ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_IDLE),
                                                     &lv_font_instrument_sans_medium_14,
                                                     lv_color_hex(0x5686A5), 500, 20);
    lv_obj_set_width(wave_status_label, 300);
    lv_obj_set_style_text_align(wave_status_label, LV_TEXT_ALIGN_RIGHT, 0);

    accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 30, 50);
    lv_obj_set_size(accent, 780, 2);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0xE9EDF2), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    wave_chart_host = lv_obj_create(card);
    lv_obj_remove_style_all(wave_chart_host);
    lv_obj_set_pos(wave_chart_host, 40, 66);
    lv_obj_set_size(wave_chart_host, WAVE_PREVIEW_W, WAVE_PREVIEW_H);
    lv_obj_set_style_bg_color(wave_chart_host, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(wave_chart_host, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wave_chart_host, 1, 0);
    lv_obj_set_style_border_color(wave_chart_host, lv_color_hex(0x233249), 0);
    lv_obj_set_style_radius(wave_chart_host, 10, 0);
    lv_obj_clear_flag(wave_chart_host, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 1; i < 5; i++) {
        wave_create_grid_line(wave_chart_host, 0, (lv_coord_t)(i * WAVE_PREVIEW_H / 5),
                              WAVE_PREVIEW_W, 1);
    }
    for (int i = 1; i < 8; i++) {
        wave_create_grid_line(wave_chart_host, (lv_coord_t)(i * WAVE_PREVIEW_W / 8), 0,
                              1, WAVE_PREVIEW_H);
    }

    wave_placeholder = settings_detail_create_label(wave_chart_host,
                                                    ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_IDLE),
                                                    &lv_font_instrument_sans_medium_18,
                                                    lv_color_hex(0x94A3B8), 0, 0);
    lv_obj_center(wave_placeholder);

    wave_line = lv_line_create(wave_chart_host);
    lv_obj_set_style_line_width(wave_line, 2, 0);
    lv_obj_set_style_line_rounded(wave_line, true, 0);
    lv_obj_set_style_line_color(wave_line, lv_color_hex(0x38BDF8), 0);
    lv_obj_add_flag(wave_line, LV_OBJ_FLAG_HIDDEN);

}

void ui_page_31_get_wave_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (wave_page) return;

    wave_page = settings_detail_create_page(parent,
                                            ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_TITLE),
                                            wave_esc_cb, &content);

    wave_create_left_panel(content);
    wave_create_preview(content);
    wave_request_button = settings_detail_create_button(
        wave_page, 1004, 10, 136, 35,
        ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_BUTTON),
        lv_color_hex(0x0878C8), wave_request_cb, NULL);
    wave_refresh_request_button(app_clock_uptime_ms());
    wave_refresh_sources();
}

void ui_page_31_get_wave_destroy(void)
{
    uint32_t now_ms = app_clock_uptime_ms();

    if (wave_request_active) {
        wave_request_finish(now_ms, true);
    }
    if (wave_page && lv_obj_is_valid(wave_page)) {
        lv_obj_del(wave_page);
    }

    wave_page = NULL;
    wave_chart_host = NULL;
    wave_line = NULL;
    wave_placeholder = NULL;
    wave_status_label = NULL;
    wave_title_label = NULL;
    wave_request_button = NULL;
    selected_wave_id = 1;
    wave_current_count = 0;
    for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
        wave_value_counts[i] = 0;
    }

    for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
        source_items[i].card = NULL;
        source_items[i].dot = NULL;
        source_items[i].label = NULL;
    }
}

void ui_page_31_get_wave_on_frame(const uint8_t* data, uint16_t len)
{
    uint8_t sub;
    uint32_t now_ms;
    const wave_source_t* source;

    if (!data || len < 1) return;
    if (!wave_page || !lv_obj_is_valid(wave_page)) return;
    if (!wave_request_active) return;

    sub = data[0];
    now_ms = app_clock_uptime_ms();
    if (sub == 0x00) {
        wave_request_finish(now_ms, false);
        wave_clear_data();
        wave_set_status(ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_NO_DATA),
                        lv_color_hex(0xF59D2A));
        return;
    }

    if (sub < 0x01 || sub > WAVE_SOURCE_COUNT) return;
    wave_request_touch(now_ms);

    if (sub == selected_wave_id) {
        source = wave_find_source(selected_wave_id);
        if (wave_line && lv_obj_is_valid(wave_line)) {
            lv_obj_set_style_line_color(wave_line, lv_color_hex(source->color_hex), 0);
        }
        wave_refresh_sources();
    }
    wave_store_values(sub, &data[1], (uint16_t)(len - 1));
    if (sub == WAVE_SOURCE_COUNT) {
        wave_request_finish(now_ms, false);
    }
}

bool ui_page_31_get_wave_poll(uint32_t now_ms)
{
    bool timed_out = false;

    if (wave_request_active && wave_time_reached(now_ms, wave_request_deadline)) {
        wave_request_finish(now_ms, true);
        wave_set_status(ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_TIMEOUT),
                        lv_color_hex(0xC03A2B));
        timed_out = true;
    } else {
        wave_refresh_request_button(now_ms);
    }

    return timed_out;
}
