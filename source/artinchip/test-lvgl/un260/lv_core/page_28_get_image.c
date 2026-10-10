#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_28_get_image.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_text.h"
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/stream_data_reply.h"
#include "un260/gesture/gesture_service.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IMAGE_SOURCE_COUNT IMAGE_DATA_REPLY_SOURCE_COUNT
#define IMAGE_MAX_WIDTH IMAGE_DATA_REPLY_MAX_WIDTH
#define IMAGE_MAX_HEIGHT IMAGE_DATA_REPLY_MAX_HEIGHT
#define IMAGE_MAX_PACKETS (IMAGE_MAX_HEIGHT * 2)
#define IMAGE_PREVIEW_W 956
#define IMAGE_PREVIEW_H 300
#define IMAGE_SOURCE_SURFACE 0xF4F6F7
#define IMAGE_PROGRESS_COLOR 0xB9E5CC
#define IMAGE_REQUEST_TIMEOUT_MS 5000U
#define IMAGE_CAPTURE_LIMIT_MS 120000U
#define IMAGE_LATE_FRAME_GUARD_MS 3000U
#define IMAGE_FINISH_FEEDBACK_MS 180U

typedef struct {
    lv_obj_t *card, *label, *progress;
} image_source_item_t;

typedef struct {
    lv_color_t *buffer;
    uint8_t *raw_rgb;
    uint16_t width, height, packets_received;
    bool complete;
    uint8_t received_packets[(IMAGE_MAX_PACKETS + 7) / 8];
} image_buffer_t;

static const char *image_names[IMAGE_SOURCE_COUNT] = {
    UI_N_("Upper white"), UI_N_("Lower white"),
    UI_N_("Upper IR reflect"), UI_N_("Lower IR reflect"),
    UI_N_("Upper IR transmit"), UI_N_("Lower IR transmit")
};
static lv_settings_frame_t image_frame;
static lv_obj_t *image_page, *image_canvas, *image_placeholder;
static lv_obj_t *image_title_label, *image_dimensions_label, *image_status_label;
static lv_obj_t *mode_retry_button;
static image_source_item_t source_items[IMAGE_SOURCE_COUNT];
static image_buffer_t image_buffers[IMAGE_SOURCE_COUNT];
static uint8_t selected_image_id, captured_image_id;
static uint16_t reported_image_width, reported_image_height;
static protocol_request_t image_request = PROTOCOL_REQUEST_INITIALIZER(IMAGE_REQUEST_TIMEOUT_MS);
static uint32_t image_late_guard_deadline, image_feedback_deadline;
static bool image_progress_dirty, image_preview_dirty, image_hint_visible;
static bool image_canvas_binding_valid;
static lv_color_t image_empty_pixel;
static uint64_t image_capture_started_ms;
static uint32_t image_count_end_generation;
static bool image_source_paint_valid, image_sources_were_locked;
static uint8_t image_painted_selection;
static char image_status_text[192];

static bool image_time_reached(uint32_t now, uint32_t deadline)
{
    return deadline != 0 && (int32_t)(now - deadline) >= 0;
}

static bool image_guard_active(uint32_t now)
{
    if (image_time_reached(now, image_late_guard_deadline)) image_late_guard_deadline = 0;
    return image_late_guard_deadline != 0;
}

static image_buffer_t *image_buffer_for(uint8_t id)
{
    return id >= 1 && id <= IMAGE_SOURCE_COUNT ? &image_buffers[id - 1] : NULL;
}

static void image_render_status(void)
{
    if (!image_status_label) return;
    const char *text = work_mode_service_diagnostic_ready() ? image_status_text : work_mode_service_status_text();
    if (strcmp(lv_label_get_text(image_status_label), text)) lv_label_set_text(image_status_label, text);
    const char *hint = image_hint_visible ? ui_tr("Place one note in the hopper.") : "";
    if (strcmp(lv_label_get_text(image_frame.message), hint)) lv_label_set_text(image_frame.message, hint);
}

static void image_set_status(const char *text)
{
    snprintf(image_status_text, sizeof(image_status_text), "%s", text);
    image_render_status();
}

static void image_notice(ui_notice_kind_t kind, const char *text)
{
    image_set_status(ui_tr(text));
    ui_notice_post_text(kind, "capture.image", UI_N_("Image capture"), text);
}

static void image_progress_width(void *object, int32_t width)
{
    lv_obj_set_width(object, (lv_coord_t)width);
}

static void image_reset_progress(void)
{
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        if (!source_items[i].progress) continue;
        lv_anim_del(source_items[i].progress, image_progress_width);
        lv_obj_add_flag(source_items[i].progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(source_items[i].progress, 0);
    }
    image_progress_dirty = false;
}

static bool image_sources_locked(uint32_t now)
{
    return protocol_request_is_pending(&image_request) || image_feedback_deadline ||
           image_guard_active(now) || !work_mode_service_diagnostic_ready();
}

static void image_refresh_sources(uint32_t now)
{
    bool locked = image_sources_locked(now);
    bool selection_changed = !image_source_paint_valid || image_painted_selection != selected_image_id;
    bool lock_changed = !image_source_paint_valid || image_sources_were_locked != locked;
    if (!selection_changed && !lock_changed) return;
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        bool selected = selected_image_id == i + 1;
        if (!source_items[i].card) continue;
        if (selection_changed) {
            lv_obj_set_style_bg_color(source_items[i].card,
                lv_color_hex(selected ? 0xFFFFFF : IMAGE_SOURCE_SURFACE), 0);
            lv_obj_set_style_text_color(source_items[i].label,
                lv_color_hex(selected ? 0x1462CC : 0x1D2B34), 0);
        }
        if (lock_changed) {
            if (locked) lv_obj_clear_flag(source_items[i].card, LV_OBJ_FLAG_CLICKABLE);
            else lv_obj_add_flag(source_items[i].card, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    if (selection_changed && image_title_label) lv_label_set_text(image_title_label,
        selected_image_id ? ui_tr(image_names[selected_image_id - 1]) : "");
    image_painted_selection = selected_image_id;
    image_sources_were_locked = locked;
    image_source_paint_valid = true;
}

static void image_update_progress(bool animate)
{
    image_buffer_t *item = image_buffer_for(captured_image_id);
    if (!item || !source_items[captured_image_id - 1].progress) return;
    lv_obj_t *fill = source_items[captured_image_id - 1].progress;
    uint16_t total = image_data_reply_scanline_count(captured_image_id, reported_image_width, reported_image_height);
    unsigned percent = total ? (unsigned)item->packets_received * 100U / total : 0;
    if (!item->complete && percent > 99) percent = 99;
    int target = item->complete ? 244 : (total ? (int)(244U * percent / 100U) : 4);
    if (target < 4) target = 4;
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(fill, image_progress_width);
    if (animate && lv_obj_get_width(fill) != target) {
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, fill);
        lv_anim_set_values(&animation, lv_obj_get_width(fill), target);
        lv_anim_set_time(&animation, 100);
        lv_anim_set_exec_cb(&animation, image_progress_width);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    } else lv_obj_set_width(fill, target);
    if (protocol_request_is_pending(&image_request)) {
        char text[96];
        if (total) snprintf(text, sizeof(text), "%s %u%%", ui_tr("Receiving image"), percent);
        else snprintf(text, sizeof(text), "%s", ui_tr("Receiving image"));
        image_set_status(text);
    }
    image_progress_dirty = false;
}

static void image_hide_canvas(void)
{
    image_canvas_binding_valid = false;
    if (image_canvas) lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
    if (image_dimensions_label) lv_label_set_text(image_dimensions_label, "");
    if (image_placeholder) lv_obj_clear_flag(image_placeholder, LV_OBJ_FLAG_HIDDEN);
}

static void image_show_selected_buffer(void)
{
    image_buffer_t *item = image_buffer_for(selected_image_id);
    if (!image_canvas || !image_placeholder) return;
    if (!item || !item->buffer || (!item->complete &&
        (selected_image_id != captured_image_id || !protocol_request_is_pending(&image_request) || !item->packets_received))) {
        image_hide_canvas();
        lv_label_set_text(image_placeholder, ui_tr(selected_image_id ? "Receiving image" : "Select an image source"));
        lv_obj_center(image_placeholder);
        return;
    }
    const lv_img_dsc_t *shown = lv_canvas_get_img(image_canvas);
    if (!image_canvas_binding_valid || !shown || shown->data != (const uint8_t *)item->buffer ||
        shown->header.w != item->width || shown->header.h != item->height) {
        lv_canvas_set_buffer(image_canvas, item->buffer, item->width, item->height, LV_IMG_CF_TRUE_COLOR);
        uint32_t zoom_w = IMAGE_PREVIEW_W * 256U / item->width;
        uint32_t zoom_h = IMAGE_PREVIEW_H * 256U / item->height;
        uint32_t zoom = zoom_w < zoom_h ? zoom_w : zoom_h;
        if (!zoom) zoom = 1;
        lv_img_set_zoom(image_canvas, (uint16_t)zoom);
        lv_obj_center(image_canvas);
        lv_label_set_text_fmt(image_dimensions_label, "%u x %u", item->width, item->height);
        image_canvas_binding_valid = true;
    }
    lv_obj_clear_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(image_placeholder, LV_OBJ_FLAG_HIDDEN);
}

static void image_free_one_buffer(image_buffer_t *item)
{
    if (!item) return;
    if (item->buffer && image_canvas) {
        const lv_img_dsc_t *shown = lv_canvas_get_img(image_canvas);
        if (shown && shown->data == (const uint8_t *)item->buffer) {
            image_hide_canvas();
            /* Detach the owned storage and invalidate LVGL's decoder cache before free. */
            lv_canvas_set_buffer(image_canvas, &image_empty_pixel, 1, 1, LV_IMG_CF_TRUE_COLOR);
        }
    }
    if (item->buffer) lv_mem_free(item->buffer);
    if (item->raw_rgb) lv_mem_free(item->raw_rgb);
    memset(item, 0, sizeof(*item));
}

static bool image_prepare_buffer(uint8_t source, uint16_t width, uint16_t height)
{
    image_buffer_t *item = image_buffer_for(source);
    uint16_t total = image_data_reply_scanline_count(source, width, height);
    if (!item || !width || !height || width > IMAGE_MAX_WIDTH || height > IMAGE_MAX_HEIGHT ||
        !total || total > IMAGE_MAX_PACKETS) return false;
    image_hide_canvas();
    image_free_one_buffer(item);
    item->buffer = lv_mem_alloc((size_t)width * height * sizeof(lv_color_t));
    if (!item->buffer) return false;
    if (image_data_reply_pixel_format(source) == IMAGE_DATA_REPLY_PIXEL_RGB565_BE) {
        item->raw_rgb = lv_mem_alloc((size_t)width * height * 2U);
        if (!item->raw_rgb) { image_free_one_buffer(item); return false; }
    }
    for (size_t i = 0; i < (size_t)width * height; i++)
        item->buffer[i] = lv_color_hex(0xEFF4F8);
    item->width = width;
    item->height = height;
    return true;
}

static lv_color_t image_rgb565_to_color(uint8_t high, uint8_t low)
{
    uint16_t rgb = ((uint16_t)high << 8) | low;
    uint8_t red = (uint8_t)(((rgb >> 11) & 31U) * 255U / 31U);
    uint8_t green = (uint8_t)(((rgb >> 5) & 63U) * 255U / 63U);
    uint8_t blue = (uint8_t)((rgb & 31U) * 255U / 31U);
    return lv_color_make(red, green, blue);
}

static bool image_packet_received(const image_buffer_t *item, uint16_t packet)
{
    return (item->received_packets[packet / 8] & (1U << (packet % 8))) != 0;
}

static bool image_receive_block(const image_data_reply_view_t *reply)
{
    image_buffer_t *item = image_buffer_for(captured_image_id);
    uint16_t count = image_data_reply_scanline_count(captured_image_id, reported_image_width, reported_image_height);
    uint16_t bytes = image_data_reply_scanline_bytes(captured_image_id, reported_image_width, reported_image_height);
    if (!item || !item->buffer || reply->image_id != captured_image_id || !reply->scanline ||
        reply->scanline > count || reply->sample_count != bytes) return false;
    uint16_t packet = reply->scanline - 1U;
    if (image_packet_received(item, packet)) return false;
    uint32_t offset = image_data_reply_scanline_offset(captured_image_id,
        reported_image_width, reported_image_height, reply->scanline);
    if (offset == UINT32_MAX) return false;
    size_t pixels = (size_t)item->width * item->height;
    image_data_reply_pixel_format_t format = image_data_reply_pixel_format(captured_image_id);
    if (format == IMAGE_DATA_REPLY_PIXEL_RGB565_BE) {
        if (!item->raw_rgb || offset + bytes > pixels * 2U) return false;
        memcpy(item->raw_rgb + offset, reply->samples, bytes);
    } else if (format == IMAGE_DATA_REPLY_PIXEL_GRAY8) {
        if (offset + bytes > pixels) return false;
        for (size_t i = 0; i < bytes; i++) {
            uint8_t intensity = reply->samples[i];
            item->buffer[offset + i] = lv_color_make(intensity, intensity, intensity);
        }
    } else return false;
    item->received_packets[packet / 8] |= (uint8_t)(1U << (packet % 8));
    if (format == IMAGE_DATA_REPLY_PIXEL_RGB565_BE) {
        /* Odd-width packets can split a pixel. Decode it only after both bytes
         * have arrived; the unreceived area remains the initialized neutral fill. */
        size_t last = (offset + bytes - 1U) / 2U;
        for (size_t pixel = offset / 2U; pixel <= last; pixel++) {
            uint16_t first = (uint16_t)(pixel * 2U / bytes);
            uint16_t second = (uint16_t)((pixel * 2U + 1U) / bytes);
            if (image_packet_received(item, first) && image_packet_received(item, second))
                item->buffer[image_data_reply_pixel_offset(captured_image_id,
                    item->width, item->height, (uint32_t)pixel)] =
                    image_rgb565_to_color(item->raw_rgb[pixel * 2U], item->raw_rgb[pixel * 2U + 1U]);
        }
    }
    item->packets_received++;
    image_progress_dirty = image_preview_dirty = true;
    return true;
}

static void image_capture_lock(bool locked)
{
    ui_manager_hold_navigation(UI_PAGE_IMAGE_GET, locked);
    gesture_service_set_navigation_blocked(UI_PAGE_IMAGE_GET, locked);
    if (image_frame.back) {
        if (locked) lv_obj_add_state(image_frame.back, LV_STATE_DISABLED);
        else lv_obj_clear_state(image_frame.back, LV_STATE_DISABLED);
    }
}

static void image_request_finish(uint32_t now, bool late_guard)
{
    protocol_request_finish(&image_request);
    image_capture_started_ms = 0;
    image_preview_dirty = false;
    image_buffer_t *item = image_buffer_for(captured_image_id);
    if (item && item->raw_rgb) { lv_mem_free(item->raw_rgb); item->raw_rgb = NULL; }
    image_capture_lock(false);
    if (late_guard) image_late_guard_deadline = now + IMAGE_LATE_FRAME_GUARD_MS;
    image_feedback_deadline = 0;
    image_reset_progress();
    image_refresh_sources(now);
}

static void image_start_capture(uint8_t source, uint32_t now)
{
    image_preview_dirty = false;
    captured_image_id = source;
    reported_image_width = reported_image_height = 0;
    image_set_status(ui_tr("Receiving image"));
    if (!protocol_request_begin_for_command(&image_request, 0x47)) return;
    image_capture_started_ms = protocol_request_now_ms();
    image_capture_lock(true);
    if (!settings_detail_send_command(0x47, &source, 1)) {
        image_request_finish(now, false);
        lv_label_set_text(image_placeholder, ui_tr("Could not send capture request. Try again."));
        lv_obj_center(image_placeholder);
        image_notice(UI_NOTICE_ERROR, UI_N_("Could not send capture request. Try again."));
        return;
    }
    image_update_progress(false);
    image_refresh_sources(now);
}

static void image_source_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    uint32_t now = app_clock_uptime_ms();
    if (image_sources_locked(now)) return;
    uint8_t source = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    image_buffer_t *item = image_buffer_for(source);
    if (!item) return;
    selected_image_id = source;
    image_refresh_sources(now);
    image_show_selected_buffer();
    if (item->complete) image_set_status("");
    else image_start_capture(source, now);
}

static void mode_retry_clicked(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && !protocol_request_is_pending(&image_request)) work_mode_service_retry();
}

static void image_esc_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && !protocol_request_is_pending(&image_request)) ui_manager_pop_page();
}

void ui_page_28_get_image_create(lv_obj_t *parent)
{
    if (image_page) return;
    selected_image_id = captured_image_id = 0;
    image_capture_lock(false);
    image_preview_dirty = false;
    image_hint_visible = true;
    image_count_end_generation = app_command_runtime_count_end_generation();
    image_source_paint_valid = false;
    image_status_text[0] = '\0';
    lv_settings_header_t header = {ui_tr("Image capture"), NULL, "Layers", image_esc_cb, NULL};
    image_frame = lv_settings_frame_create(parent, &header);
    image_page = image_frame.root;
    lv_obj_set_style_bg_opa(image_frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(image_frame.body, 0, 0);
    lv_obj_t *heading = lv_obj_get_parent(image_frame.back);
    lv_obj_del(image_frame.back);
    image_frame.back = lv_nav_back_text_button_create(heading, 1156, 7,
        &lv_font_instrument_sans_medium_16, image_esc_cb, NULL);
    lv_obj_t *sources = lv_settings_box(image_frame.body, 0, 0, 260, 300, IMAGE_SOURCE_SURFACE);
    lv_obj_set_style_radius(sources, 14, 0);
    for (unsigned i = 0; i < IMAGE_SOURCE_COUNT; i++) {
        lv_obj_t *button = lv_settings_button(sources, 8, 8 + i * 48, 244, 46,
            ui_tr(image_names[i]), false, image_source_cb, (void *)(uintptr_t)(i + 1));
        lv_obj_t *label = lv_obj_get_child(button, 0);
        lv_obj_set_width(label, 224);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
        lv_obj_set_style_border_width(button, 0, 0);
        lv_obj_remove_local_style_prop(button, LV_STYLE_BORDER_WIDTH, LV_STATE_CHECKED);
        lv_obj_remove_local_style_prop(button, LV_STYLE_BORDER_WIDTH, LV_STATE_DISABLED);
        lv_obj_set_style_clip_corner(button, true, 0);
        lv_obj_t *fill = lv_settings_box(button, 0, 0, 0, 46, IMAGE_PROGRESS_COLOR);
        lv_obj_set_style_radius(fill, 0, 0);
        lv_obj_move_background(fill);
        lv_obj_add_flag(fill, LV_OBJ_FLAG_HIDDEN);
        source_items[i] = (image_source_item_t){button, label, fill};
    }
    lv_obj_t *preview = lv_settings_box(image_frame.body, 276, 0, IMAGE_PREVIEW_W, IMAGE_PREVIEW_H, 0);
    lv_obj_set_style_bg_opa(preview, LV_OPA_TRANSP, 0);
    image_placeholder = lv_settings_label(preview, ui_tr("Select an image source"), 0, 0,
        &lv_font_instrument_sans_medium_18, 0x586B78);
    lv_obj_center(image_placeholder);
    image_canvas = lv_canvas_create(preview);
    image_canvas_binding_valid = false;
    lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
    image_title_label = lv_settings_label(image_page, "", 740, 29, &lv_font_instrument_sans_medium_16, 0x1D2B34);
    lv_obj_set_width(image_title_label, 274);
    lv_obj_set_style_text_align(image_title_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(image_title_label, LV_LABEL_LONG_DOT);
    image_dimensions_label = lv_settings_label(image_page, "", 1030, 31, &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_obj_del(image_frame.message);
    image_frame.message = lv_settings_label(image_page, ui_tr("Place one note in the hopper."), 24, 65,
        &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_obj_set_width(image_frame.message, 610);
    lv_label_set_long_mode(image_frame.message, LV_LABEL_LONG_DOT);
    image_status_label = lv_settings_label(image_page, "", 644, 65, &lv_font_instrument_sans_medium_12, 0x586B78);
    lv_obj_set_width(image_status_label, 610);
    lv_label_set_long_mode(image_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(image_status_label, LV_TEXT_ALIGN_RIGHT, 0);
    mode_retry_button = lv_settings_button(image_frame.footer, 0, 0, 130, 46,
        ui_tr("Retry"), false, mode_retry_clicked, NULL);
    lv_obj_add_flag(mode_retry_button, LV_OBJ_FLAG_HIDDEN);
    image_refresh_sources(app_clock_uptime_ms());
    image_render_status();
}

void ui_page_28_get_image_destroy(void)
{
    if (protocol_request_is_pending(&image_request)) image_request_finish(app_clock_uptime_ms(), true);
    image_reset_progress();
    image_capture_lock(false);
    image_capture_started_ms = 0;
    image_preview_dirty = false;
    image_feedback_deadline = 0;
    ui_notice_clear("capture.image");
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) image_free_one_buffer(&image_buffers[i]);
    if (image_page && lv_obj_is_valid(image_page)) lv_obj_del(image_page);
    image_page = image_canvas = image_placeholder = NULL;
    image_title_label = image_dimensions_label = image_status_label = mode_retry_button = NULL;
    image_frame = (lv_settings_frame_t){0};
    memset(source_items, 0, sizeof(source_items));
    image_source_paint_valid = false;
    selected_image_id = captured_image_id = 0;
    reported_image_width = reported_image_height = 0;
}

void ui_page_28_get_image_on_frame(const uint8_t *data, uint16_t length)
{
    if (!image_page || !protocol_request_can_take_result(&image_request)) return;
    image_data_reply_view_t reply = image_data_reply_parse(data, length);
    uint32_t now = app_clock_uptime_ms();
    if (reply.kind == IMAGE_DATA_REPLY_HEADER) {
        if (reply.image_id != captured_image_id) return;
        if (reported_image_width || reported_image_height) {
            /* A repeated header cannot erase an in-flight image or extend its deadline. */
            if (reply.width == reported_image_width && reply.height == reported_image_height) return;
            image_request_finish(now, true);
            image_hide_canvas();
            image_notice(UI_NOTICE_WARNING, UI_N_("Image data incomplete. Tap the source to retry."));
            return;
        }
        if (!reply.width || !reply.height || reply.width > IMAGE_MAX_WIDTH || reply.height > IMAGE_MAX_HEIGHT) {
            image_request_finish(now, true);
            image_notice(UI_NOTICE_ERROR, UI_N_("Unsupported image size"));
            return;
        }
        if (!image_prepare_buffer(captured_image_id, reply.width, reply.height)) {
            image_request_finish(now, true);
            image_notice(UI_NOTICE_ERROR, ui_text_msgid(UI_TEXT_SETTINGS_IMAGE_GET_MEMORY_ERROR));
            return;
        }
        reported_image_width = reply.width;
        reported_image_height = reply.height;
        protocol_request_touch_result(&image_request);
        image_progress_dirty = true;
    } else if (reply.kind == IMAGE_DATA_REPLY_SCANLINE && image_receive_block(&reply)) {
        protocol_request_touch_result(&image_request);
    } else if (reply.kind == IMAGE_DATA_REPLY_END) {
        image_buffer_t *item = image_buffer_for(captured_image_id);
        uint16_t total = image_data_reply_scanline_count(captured_image_id, reported_image_width, reported_image_height);
        bool complete = item && item->buffer && reported_image_width && reported_image_height && item->packets_received == total;
        protocol_request_finish(&image_request);
        image_capture_started_ms = 0;
        image_capture_lock(false);
        if (complete) {
            item->complete = true;
            if (item->raw_rgb) { lv_mem_free(item->raw_rgb); item->raw_rgb = NULL; }
            image_preview_dirty = false;
            image_feedback_deadline = now + IMAGE_FINISH_FEEDBACK_MS;
            image_update_progress(true);
            image_show_selected_buffer();
            image_notice(UI_NOTICE_SUCCESS, UI_N_("Capture complete"));
            image_refresh_sources(now);
        } else {
            image_request_finish(now, false);
            image_hide_canvas();
            lv_label_set_text(image_placeholder, ui_tr("Image data incomplete. Tap the source to retry."));
            lv_obj_center(image_placeholder);
            image_notice(UI_NOTICE_WARNING, UI_N_("Image data incomplete. Tap the source to retry."));
        }
    }
}

bool ui_page_28_get_image_is_capturing(void)
{
    return image_page && protocol_request_is_pending(&image_request);
}

bool ui_page_28_get_image_poll(uint32_t now)
{
    if (!image_page) return false;
    if (app_command_runtime_count_end_generation() != image_count_end_generation) image_hint_visible = false;
    bool timed_out = protocol_request_take_timeout(&image_request);
    if (!timed_out && protocol_request_is_pending(&image_request) &&
        protocol_request_now_ms() - image_capture_started_ms >= IMAGE_CAPTURE_LIMIT_MS) timed_out = true;
    if (timed_out) {
        image_request_finish(now, true);
        image_hide_canvas();
        lv_label_set_text(image_placeholder, ui_tr("Image data incomplete. Tap the source to retry."));
        lv_obj_center(image_placeholder);
        image_notice(UI_NOTICE_WARNING, ui_text_msgid(UI_TEXT_SETTINGS_IMAGE_GET_TIMEOUT));
    }
    if (image_preview_dirty) {
        image_preview_dirty = false;
        image_show_selected_buffer();
        lv_obj_invalidate(image_canvas);
    }
    if (image_time_reached(now, image_feedback_deadline)) {
        image_feedback_deadline = 0;
        image_reset_progress();
    } else if (image_progress_dirty) image_update_progress(false);
    work_mode_snapshot_t mode;
    work_mode_service_get_snapshot(&mode);
    if (mode.phase == WORK_MODE_FAILED) {
        lv_obj_clear_flag(mode_retry_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(image_title_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(image_dimensions_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mode_retry_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(image_title_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(image_dimensions_label, LV_OBJ_FLAG_HIDDEN);
    }
    image_refresh_sources(now);
    image_render_status();
    return timed_out;
}
