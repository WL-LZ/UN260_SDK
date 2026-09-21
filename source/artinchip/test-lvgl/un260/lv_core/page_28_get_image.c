#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_28_get_image.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/app_service/work_mode_service.h"
#include <string.h>
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_text.h"

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#define IMAGE_SOURCE_COUNT 6
#define IMAGE_MAX_WIDTH 512
#define IMAGE_MAX_HEIGHT 384
#define IMAGE_PREVIEW_W 774
#define IMAGE_PREVIEW_H 174
#define IMAGE_REQUEST_TIMEOUT_MS 5000U
#define IMAGE_LATE_FRAME_GUARD_MS 3000U

typedef struct {
    lv_obj_t* card;
    lv_obj_t* label;
} image_source_item_t;

typedef struct {
    lv_color_t* buffer;
    uint16_t width;
    uint16_t height;
    uint16_t rows_received;
    bool receiving;
    uint8_t received_columns[(IMAGE_MAX_WIDTH + 7) / 8];
} image_buffer_t;

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

static const uint8_t g_image_sources[IMAGE_SOURCE_COUNT] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

static lv_settings_frame_t image_frame;
static lv_obj_t* image_page = NULL;
static const char *image_names[] = {"Upper white", "Lower white", "Upper IR reflect", "Lower IR reflect", "Upper IR transmit", "Lower IR transmit"};
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

    if (image_request_active || image_guard_active(now_ms) || !work_mode_service_diagnostic_ready()) {
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

static size_t image_source_index(uint8_t id)
{
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        if (g_image_sources[i] == id) {
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

static char image_status_text[192];
static lv_color_t image_status_color;
static void image_render_status(void)
{
    if (!image_status_label) return;
    bool ready=work_mode_service_diagnostic_ready();
    const char *text=ready ? image_status_text : work_mode_service_status_text();
    if (strcmp(lv_label_get_text(image_status_label),text)) lv_label_set_text(image_status_label,text);
    lv_obj_set_style_text_color(image_status_label,ready ? image_status_color : lv_color_hex(0x586B78),0);
}
static void image_set_status(const char* text, lv_color_t color)
{
    snprintf(image_status_text,sizeof(image_status_text),"%s",text);
    image_status_color=color;
    image_render_status();
}

static void image_refresh_sources(void)
{
    size_t selected_index = image_source_index(selected_image_id);

    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        bool selected = (i == selected_index);

        if (source_items[i].card) {
            lv_obj_set_style_bg_color(source_items[i].card,
                                      selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),
                                      0);
            lv_obj_set_style_border_color(source_items[i].card,
                                          selected ? lv_color_hex(0xC5D8ED) : lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),
                                          0);
        }

        if (source_items[i].label) {
            lv_obj_set_style_text_color(source_items[i].label,
                                        selected ? lv_color_hex(0x1462CC) : lv_color_hex(0x1D2B34),
                                        0);
        }
    }

    if (image_title_label) {
        lv_label_set_text(image_title_label, image_names[image_source_index(selected_image_id)]);
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
    memset(item->received_columns, 0, sizeof(item->received_columns));
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
                         lv_color_hex(0xB63B32));
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
                         lv_color_hex(0xB63B32));
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
                         lv_color_hex(0x1462CC));
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

    (void)row;
    if (!reported_image_length) return false;
    width = reported_image_length;
    height = (uint16_t)(pixel_len / 2U);

    image_buffer_t *existing = &image_buffers[image_source_index(image_id)];
    if (existing->buffer && (existing->width != width || existing->height != height)) {
        image_set_status("Inconsistent image dimensions. Capture again.", lv_color_hex(0xB63B32));
        return false;
    }
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

    if (!(item->received_columns[x / 8] & (1U << (x % 8)))) {
        item->received_columns[x / 8] |= (uint8_t)(1U << (x % 8));
        item->rows_received++;
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
    if (image_request_active || image_guard_active(now_ms) || !work_mode_service_diagnostic_ready()) return;

    if (image_send_request()) {
        image_clear_buffer();
        image_request_active = true;
        image_request_touch(now_ms);
        image_refresh_request_button(now_ms);
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_WAITING),
                         lv_color_hex(0x1462CC));
    } else {
        image_set_status("Could not send capture request. Try again.",lv_color_hex(0xB63B32));
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
    image_buffer_t *selected = &image_buffers[image_source_index(selected_image_id)];
    if (!selected->buffer) image_set_status("No capture for this source. Select Capture to request it.", lv_color_hex(0x586B78));
    else image_update_status(selected_image_id);
}

static void image_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

void ui_page_28_get_image_create(lv_obj_t *parent)
{
    if (image_page) return;
    lv_settings_header_t header={"Image capture","Maintenance / Optical channels","Layers",image_esc_cb,NULL};
    image_frame=lv_settings_frame_create(parent,&header);
    settings_detail_add_run(image_frame.root);
    image_page=image_frame.root;
    lv_obj_set_style_bg_opa(image_frame.body,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(image_frame.body,0,0);
    lv_obj_t *sources=lv_settings_box(image_frame.body,0,0,410,242,0xF1F4F5);
    lv_obj_set_style_radius(sources,14,0);
    for(unsigned i=0;i<IMAGE_SOURCE_COUNT;i++) {
        lv_obj_t *item=lv_settings_button(sources,8+(i%2)*201,8+(i/2)*78,193,70,
                                          image_names[i],false,image_source_cb,(void *)(uintptr_t)(i+1));
        lv_obj_t *label=lv_obj_get_child(item,0);
        lv_obj_set_width(label,170);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(label);
        lv_obj_set_style_border_width(item,1,0);
        source_items[i].card=item;source_items[i].label=label;
    }
    lv_obj_t *preview=lv_settings_box(image_frame.body,426,0,806,242,0xFFFFFF);
    lv_obj_set_style_radius(preview,14,0);
    image_title_label=lv_settings_label(preview,"",16,16,&lv_font_instrument_sans_medium_18,0x1D2B34);
    lv_settings_label(preview,"OPTICAL CAPTURE",594,19,&lv_font_instrument_sans_medium_12,0x586B78);
    lv_obj_t *film=lv_settings_box(preview,16,52,IMAGE_PREVIEW_W,IMAGE_PREVIEW_H,0x1D2B34);
    lv_obj_set_style_radius(film,10,0);
    image_placeholder=lv_settings_label(film,"Select a source, then Capture",0,0,
                                        &lv_font_instrument_sans_medium_18,0xC8D3DA);
    lv_obj_center(image_placeholder);
    image_canvas=lv_canvas_create(film);lv_obj_add_flag(image_canvas,LV_OBJ_FLAG_HIDDEN);
    image_status_label=image_frame.message;
    image_set_status("Place the note as required before requesting an image.",lv_color_hex(0x586B78));
    mode_retry_button=lv_settings_button(image_frame.footer,904,0,130,46,"Retry",false,mode_retry_clicked,NULL);
    lv_obj_add_flag(mode_retry_button,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(image_frame.message,884);
    image_request_button=lv_settings_button(image_frame.footer,1050,0,182,46,"Capture",true,image_request_cb,NULL);
    image_refresh_sources();
    image_refresh_request_button(app_clock_uptime_ms());
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
    image_frame = (lv_settings_frame_t){0};
    mode_retry_button=NULL;
    image_canvas = NULL;
    image_placeholder = NULL;
    image_status_label = NULL;
    image_title_label = NULL;
    image_request_button = NULL;
    selected_image_id = 1;

    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        source_items[i].card = NULL;
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
        if (!length || length > IMAGE_MAX_WIDTH) {
            image_request_finish(now_ms, true);
            image_set_status("Unsupported image size", lv_color_hex(0xB63B32));
            return;
        }
        reported_image_length = length;
        selected_image_id = image_id;
        image_refresh_sources();
        image_set_status(ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_RECEIVING),
                         lv_color_hex(0x1462CC));
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
        image_buffer_t *selected = &image_buffers[image_source_index(selected_image_id)];
        bool complete = selected->buffer && selected->width && selected->rows_received == selected->width;
        image_set_status(complete ? "Capture complete" : "Transfer ended with missing image data. Capture again.",
                         lv_color_hex(complete ? 0x247650 : 0xA35B12));
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
                         lv_color_hex(0xB63B32));
        timed_out = true;
    } else {
        image_refresh_request_button(now_ms);
    }

    mode_retry_refresh();
    image_render_status();
    return timed_out;
}
