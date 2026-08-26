#include "page_28_get_image.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_text.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IMAGE_SOURCE_COUNT 6
#define IMAGE_MAX_WIDTH 512
#define IMAGE_MAX_HEIGHT 384
#define IMAGE_PREVIEW_W 760
#define IMAGE_PREVIEW_H 230
#define IMAGE_REQUEST_TIMEOUT_MS 5000U
#define IMAGE_LATE_FRAME_GUARD_MS 3000U

typedef struct {
    uint8_t id;
    ui_text_id_t text_id;
    uint32_t color_hex;
} image_source_t;

typedef struct {
    lv_obj_t* card;
    lv_obj_t* dot;
    lv_obj_t* label;
} image_source_item_t;

typedef struct {
    lv_color_t* buffer;
    uint16_t width;
    uint16_t height;
    uint16_t rows_received;
    bool receiving;
} image_buffer_t;

static const image_source_t g_image_sources[IMAGE_SOURCE_COUNT] = {
    { 0x01, UI_TEXT_SETTINGS_IMAGE_GET_UP_WHITE, 0x38BDF8 },
    { 0x02, UI_TEXT_SETTINGS_IMAGE_GET_DOWN_WHITE, 0x0EA5E9 },
    { 0x03, UI_TEXT_SETTINGS_IMAGE_GET_UP_IR_REFLECT, 0x8B5CF6 },
    { 0x04, UI_TEXT_SETTINGS_IMAGE_GET_DOWN_IR_REFLECT, 0x6366F1 },
    { 0x05, UI_TEXT_SETTINGS_IMAGE_GET_UP_IR_TRANS, 0x24B47E },
    { 0x06, UI_TEXT_SETTINGS_IMAGE_GET_DOWN_IR_TRANS, 0xF59D2A },
};

static lv_obj_t* image_page = NULL;
static lv_obj_t* image_canvas = NULL;
static lv_obj_t* image_placeholder = NULL;
static lv_obj_t* image_status_label = NULL;
static lv_obj_t* image_title_label = NULL;
static lv_obj_t* image_request_button = NULL;
static image_source_item_t source_items[IMAGE_SOURCE_COUNT] = { 0 };
static image_buffer_t image_buffers[IMAGE_SOURCE_COUNT] = { 0 };
static uint16_t reported_image_length = 0;
static uint8_t selected_image_id = 1;
static bool image_request_active = false;
static uint32_t image_request_deadline = 0;
static uint32_t image_late_guard_deadline = 0;

static bool image_time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return deadline_ms != 0 && (int32_t)(now_ms - deadline_ms) >= 0;
}

static bool image_guard_active(uint32_t now_ms)
{
    if (image_time_reached(now_ms, image_late_guard_deadline)) {
        image_late_guard_deadline = 0;
    }
    return image_late_guard_deadline != 0;
}

static void image_refresh_request_button(uint32_t now_ms)
{
    if (!image_request_button || !lv_obj_is_valid(image_request_button)) return;

    if (image_request_active || image_guard_active(now_ms)) {
        lv_obj_add_state(image_request_button, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(image_request_button, LV_STATE_DISABLED);
    }
}

static void image_request_touch(uint32_t now_ms)
{
    image_request_deadline = now_ms + IMAGE_REQUEST_TIMEOUT_MS;
}

static void image_request_finish(uint32_t now_ms, bool guard_late_frames)
{
    image_request_active = false;
    image_request_deadline = 0;
    if (guard_late_frames) {
        image_late_guard_deadline = now_ms + IMAGE_LATE_FRAME_GUARD_MS;
    }
    image_refresh_request_button(now_ms);
}

static const image_source_t* image_find_source(uint8_t id)
{
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        if (g_image_sources[i].id == id) {
            return &g_image_sources[i];
        }
    }

    return &g_image_sources[0];
}

static size_t image_source_index(uint8_t id)
{
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        if (g_image_sources[i].id == id) {
            return i;
        }
    }

    return 0;
}

static lv_coord_t image_zoom_for_size(uint16_t width, uint16_t height)
{
    uint32_t zoom_w;
    uint32_t zoom_h;
    uint32_t zoom;

    if (width == 0 || height == 0) return 256;

    zoom_w = (uint32_t)IMAGE_PREVIEW_W * 256U / width;
    zoom_h = (uint32_t)IMAGE_PREVIEW_H * 256U / height;
    zoom = zoom_w < zoom_h ? zoom_w : zoom_h;
    if (zoom == 0) zoom = 1;
    if (zoom > 256) zoom = 256;

    return (lv_coord_t)zoom;
}

static void image_set_status(const char* text, lv_color_t color)
{
    if (!image_status_label || !lv_obj_is_valid(image_status_label)) return;

    lv_label_set_text(image_status_label, text);
    lv_obj_set_style_text_color(image_status_label, color, 0);
}

static void image_refresh_sources(void)
{
    size_t selected_index = image_source_index(selected_image_id);

    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        bool selected = (i == selected_index);
        lv_color_t color = lv_color_hex(g_image_sources[i].color_hex);

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

    if (image_title_label) {
        const image_source_t* source = image_find_source(selected_image_id);
        lv_label_set_text(image_title_label, ui_text_get(source->text_id));
    }

}

static void image_free_one_buffer(image_buffer_t* item)
{
    if (!item) return;

    if (item->buffer) {
        lv_mem_free(item->buffer);
    }

    item->buffer = NULL;
    item->width = 0;
    item->height = 0;
    item->rows_received = 0;
    item->receiving = false;
}

static void image_hide_canvas(void)
{
    if (image_canvas && lv_obj_is_valid(image_canvas)) {
        lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
    }

    if (image_placeholder && lv_obj_is_valid(image_placeholder)) {
        lv_obj_clear_flag(image_placeholder, LV_OBJ_FLAG_HIDDEN);
    }
}

static void image_show_selected_buffer(void)
{
    image_buffer_t* current = &image_buffers[image_source_index(selected_image_id)];

    if (!image_canvas || !lv_obj_is_valid(image_canvas) ||
        !image_placeholder || !lv_obj_is_valid(image_placeholder)) {
        return;
    }

    if (!current->buffer || current->width == 0 || current->height == 0) {
        image_hide_canvas();
        return;
    }

    lv_canvas_set_buffer(image_canvas, current->buffer,
                         current->width, current->height,
                         LV_IMG_CF_TRUE_COLOR);
    lv_img_set_zoom(image_canvas, image_zoom_for_size(current->width, current->height));
    lv_obj_center(image_canvas);
    lv_obj_clear_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(image_placeholder, LV_OBJ_FLAG_HIDDEN);
}

static void image_clear_buffer(void)
{
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        image_free_one_buffer(&image_buffers[i]);
    }

    reported_image_length = 0;

    image_hide_canvas();
}

static bool image_prepare_one_buffer(uint8_t image_id, uint16_t width, uint16_t height)
{
    image_buffer_t* item;
    size_t pixel_count;

    if (image_id < 1 || image_id > IMAGE_SOURCE_COUNT) return false;
    if (width == 0 || height == 0 ||
        width > IMAGE_MAX_WIDTH || height > IMAGE_MAX_HEIGHT) {
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_SIZE_ERROR),
                         lv_color_hex(0xC03A2B));
        return false;
    }

    item = &image_buffers[image_source_index(image_id)];
    if (item->buffer && item->width == width && item->height == height) {
        item->receiving = true;
        return true;
    }

    image_free_one_buffer(item);

    pixel_count = (size_t)width * height;
    item->buffer = (lv_color_t*)lv_mem_alloc(pixel_count * sizeof(lv_color_t));
    if (!item->buffer) {
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_MEMORY_ERROR),
                         lv_color_hex(0xC03A2B));
        return false;
    }

    item->width = width;
    item->height = height;
    item->rows_received = 0;
    item->receiving = true;

    for (size_t i = 0; i < pixel_count; i++) {
        item->buffer[i] = lv_color_hex(0xEFF4F8);
    }

    if (image_id == selected_image_id) {
        image_show_selected_buffer();
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_RECEIVING),
                         lv_color_hex(0x0878C8));
    }

    return true;
}

static lv_color_t image_rgb565_to_color(uint8_t high, uint8_t low)
{
    uint16_t rgb = (uint16_t)(((uint16_t)high << 8) | low);
    uint8_t r = (uint8_t)((((rgb >> 11) & 0x1F) * 255U) / 31U);
    uint8_t g = (uint8_t)((((rgb >> 5) & 0x3F) * 255U) / 63U);
    uint8_t b = (uint8_t)(((rgb & 0x1F) * 255U) / 31U);

    return lv_color_make(r, g, b);
}

static bool image_prepare_for_row(uint8_t image_id, uint16_t row, uint16_t pixel_len)
{
    uint16_t width;
    uint16_t height;

    if (image_id < 1 || image_id > IMAGE_SOURCE_COUNT) return false;
    if (pixel_len < 2 || (pixel_len & 0x01U) != 0) return false;

    width = reported_image_length ? reported_image_length : row;
    height = (uint16_t)(pixel_len / 2U);

    return image_prepare_one_buffer(image_id, width, height);
}

static void image_update_status(uint8_t image_id)
{
    image_buffer_t* item;

    if (image_id != selected_image_id) return;
    if (!image_status_label || !lv_obj_is_valid(image_status_label)) return;

    item = &image_buffers[image_source_index(image_id)];
    if (item->receiving && item->width > 0) {
        lv_label_set_text_fmt(image_status_label, "%s %u/%u",
                              ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_RECEIVING),
                              item->rows_received, item->width);
    }
}

static void image_draw_row(uint8_t image_id, uint16_t row,
                           const uint8_t* pixels, uint16_t pixel_len)
{
    image_buffer_t* item;
    uint16_t x;
    uint16_t copy_len;

    if (!pixels || image_id < 1 || image_id > IMAGE_SOURCE_COUNT) return;
    if (row == 0) return;
    if (!image_prepare_for_row(image_id, row, pixel_len)) return;

    item = &image_buffers[image_source_index(image_id)];
    if (!item->buffer || item->width == 0 || item->height == 0) return;

    x = (uint16_t)(row - 1);
    if (x >= item->width) return;

    copy_len = (uint16_t)(pixel_len / 2U);
    if (copy_len > item->height) copy_len = item->height;

    for (uint16_t y = 0; y < copy_len; y++) {
        item->buffer[(size_t)y * item->width + x] =
            image_rgb565_to_color(pixels[y * 2U], pixels[y * 2U + 1U]);
    }

    if (row > item->rows_received) {
        item->rows_received = row;
    }

    if (image_id == selected_image_id) {
        image_show_selected_buffer();
        lv_obj_invalidate(image_canvas);
    }
    image_update_status(image_id);
}

static bool image_send_request(void)
{
    uint8_t payload = selected_image_id;

    return settings_detail_send_command(0x47, &payload, 1);
}

static void image_request_cb(lv_event_t* e)
{
    uint32_t now_ms;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    now_ms = app_clock_uptime_ms();
    if (image_request_active || image_guard_active(now_ms)) return;

    image_clear_buffer();
    if (image_send_request()) {
        image_request_active = true;
        image_request_touch(now_ms);
        image_refresh_request_button(now_ms);
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_WAITING),
                         lv_color_hex(0x0878C8));
    }
}

static void image_source_cb(lv_event_t* e)
{
    uint8_t image_id;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    image_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (image_id < 1 || image_id > IMAGE_SOURCE_COUNT) return;

    selected_image_id = image_id;
    image_refresh_sources();
    image_show_selected_buffer();
}

static void image_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

static lv_obj_t* image_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                   lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void image_create_source_item(lv_obj_t* parent, size_t index)
{
    const image_source_t* source = &g_image_sources[index];
    lv_obj_t* item = lv_obj_create(parent);
    lv_coord_t y = (lv_coord_t)(44 + index * 41);

    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, 18, y);
    lv_obj_set_size(item, 244, 36);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_translate_y(item, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, image_source_cb, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)source->id);

    source_items[index].dot = lv_obj_create(item);
    lv_obj_remove_style_all(source_items[index].dot);
    lv_obj_set_pos(source_items[index].dot, 15, 12);
    lv_obj_set_size(source_items[index].dot, 12, 12);
    lv_obj_set_style_radius(source_items[index].dot, 6, 0);
    lv_obj_set_style_bg_color(source_items[index].dot, lv_color_hex(source->color_hex), 0);
    lv_obj_set_style_bg_opa(source_items[index].dot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(source_items[index].dot, LV_OBJ_FLAG_SCROLLABLE);

    source_items[index].label = settings_detail_create_label(item, ui_text_get(source->text_id),
                                                             &lv_font_instrument_sans_medium_14,
                                                             lv_color_hex(0x0D3440), 40, 10);
    source_items[index].card = item;
}

static void image_create_left_panel(lv_obj_t* parent)
{
    lv_obj_t* card = image_create_card(parent, 38, 18, 286, 306);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_SOURCE),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x0D3440), 22, 16);

    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        image_create_source_item(card, i);
    }
}

static void image_create_preview(lv_obj_t* parent)
{
    lv_obj_t* card = image_create_card(parent, 350, 18, 840, 306);
    lv_obj_t* film;
    lv_obj_t* accent;

    image_title_label = settings_detail_create_label(card, "",
                                                     &lv_font_instrument_sans_medium_18,
                                                     lv_color_hex(0x0D3440), 32, 18);
    image_status_label = settings_detail_create_label(card,
                                                      ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_IDLE),
                                                      &lv_font_instrument_sans_medium_14,
                                                      lv_color_hex(0x5686A5), 520, 20);
    lv_obj_set_width(image_status_label, 280);
    lv_obj_set_style_text_align(image_status_label, LV_TEXT_ALIGN_RIGHT, 0);

    accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 30, 50);
    lv_obj_set_size(accent, 780, 2);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0xE9EDF2), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    film = lv_obj_create(card);
    lv_obj_remove_style_all(film);
    lv_obj_set_pos(film, 40, 66);
    lv_obj_set_size(film, IMAGE_PREVIEW_W, IMAGE_PREVIEW_H);
    lv_obj_set_style_bg_color(film, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(film, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(film, 1, 0);
    lv_obj_set_style_border_color(film, lv_color_hex(0x233249), 0);
    lv_obj_set_style_radius(film, 10, 0);
    lv_obj_clear_flag(film, LV_OBJ_FLAG_SCROLLABLE);

    image_placeholder = settings_detail_create_label(film,
                                                     ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_EMPTY),
                                                     &lv_font_instrument_sans_medium_18,
                                                     lv_color_hex(0x94A3B8), 0, 0);
    lv_obj_center(image_placeholder);

    image_canvas = lv_canvas_create(film);
    lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);

}

void ui_page_28_get_image_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (image_page) return;

    image_page = settings_detail_create_page(parent,
                                             ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_TITLE),
                                             image_esc_cb, &content);

    image_create_left_panel(content);
    image_create_preview(content);
    image_request_button = settings_detail_create_button(
        image_page, 1004, 10, 136, 35,
        ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_BUTTON),
        lv_color_hex(0x0878C8), image_request_cb, NULL);
    image_refresh_request_button(app_clock_uptime_ms());
    image_refresh_sources();
}

void ui_page_28_get_image_destroy(void)
{
    uint32_t now_ms = app_clock_uptime_ms();

    if (image_request_active) {
        image_request_finish(now_ms, true);
    }
    image_clear_buffer();

    if (image_page && lv_obj_is_valid(image_page)) {
        lv_obj_del(image_page);
    }

    image_page = NULL;
    image_canvas = NULL;
    image_placeholder = NULL;
    image_status_label = NULL;
    image_title_label = NULL;
    image_request_button = NULL;
    selected_image_id = 1;

    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        source_items[i].card = NULL;
        source_items[i].dot = NULL;
        source_items[i].label = NULL;
    }
}

void ui_page_28_get_image_on_frame(const uint8_t* data, uint16_t len)
{
    uint8_t sub;
    uint32_t now_ms;

    if (!data || len < 1) return;
    if (!image_page || !lv_obj_is_valid(image_page)) return;
    if (!image_request_active) return;

    sub = data[0];
    now_ms = app_clock_uptime_ms();
    if (sub == 0x00) {
        uint8_t image_id;
        uint16_t length;

        if (len < 6) return;
        image_id = data[1];
        if (image_id < 1 || image_id > IMAGE_SOURCE_COUNT) return;
        image_request_touch(now_ms);
        length = (uint16_t)(((uint16_t)data[2] << 8) | data[3]);
        image_clear_buffer();
        reported_image_length = length;
        selected_image_id = image_id;
        image_refresh_sources();
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_RECEIVING),
                         lv_color_hex(0x0878C8));
        return;
    }

    if (sub == 0xFF) {
        image_request_finish(now_ms, false);
        for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
            image_buffers[i].receiving = false;
        }
        if (image_canvas && lv_obj_is_valid(image_canvas)) {
            lv_obj_invalidate(image_canvas);
        }
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_DONE),
                         lv_color_hex(0x24B47E));
        return;
    }

    if (sub >= 0x01 && sub <= IMAGE_SOURCE_COUNT) {
        uint16_t row;

        if (len < 4) return;
        image_request_touch(now_ms);
        row = (uint16_t)(((uint16_t)data[1] << 8) | data[2]);
        image_draw_row(sub, row, &data[3], (uint16_t)(len - 3));
    }
}

bool ui_page_28_get_image_poll(uint32_t now_ms)
{
    bool timed_out = false;

    if (image_request_active && image_time_reached(now_ms, image_request_deadline)) {
        image_request_finish(now_ms, true);
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_TIMEOUT),
                         lv_color_hex(0xC03A2B));
        timed_out = true;
    } else {
        image_refresh_request_button(now_ms);
    }

    return timed_out;
}
