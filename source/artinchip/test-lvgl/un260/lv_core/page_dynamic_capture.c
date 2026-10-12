#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_dynamic_capture.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_text.h"
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/stream_data_reply.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_components/lv_content_pager.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IMAGE_SOURCE_COUNT IMAGE_DATA_REPLY_SOURCE_COUNT
#define CAPTURE_SOURCE_COUNT (IMAGE_SOURCE_COUNT + WAVE_DATA_REPLY_CHANNEL_COUNT)
#define SIGNAL_POINT_MAX 256U
#define CAPTURE_ROWS_PER_PAGE 6U
#define CAPTURE_PAGE_COUNT ((CAPTURE_SOURCE_COUNT + CAPTURE_ROWS_PER_PAGE - 1U) / CAPTURE_ROWS_PER_PAGE)
#define CAPTURE_BUTTON_W 244
#define CAPTURE_BUTTON_H 44
#define CAPTURE_ROW_STEP 46
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
} capture_source_item_t;

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
static lv_settings_frame_t capture_frame;
static lv_obj_t *capture_page, *image_canvas, *image_placeholder;
static lv_obj_t *image_title_label, *image_dimensions_label, *image_status_label;
static lv_obj_t *mode_retry_button;
static capture_source_item_t source_items[CAPTURE_SOURCE_COUNT];
static image_buffer_t image_buffers[IMAGE_SOURCE_COUNT];
static uint8_t selected_image_id, captured_image_id;
static uint8_t selected_source, captured_source;
static lv_obj_t *source_pager, *source_viewport, *source_page_label;
static lv_obj_t *signal_host, *signal_line;
static lv_obj_t *signal_count_label;
static bool source_contact, source_drag_owned, source_swiping, source_moved;
static bool source_vertical, source_interrupted, source_cancelled;
static lv_point_t source_press_point;
static int source_pressed_index = -1;
static uint8_t source_painted_page;
static lv_point_t signal_points[SIGNAL_POINT_MAX];
static lv_point_t signal_grid[12][2];
static struct { uint8_t values[SIGNAL_POINT_MAX]; uint16_t count; bool complete; }
    signal_buffers[WAVE_DATA_REPLY_CHANNEL_COUNT];
static const char *signal_names[WAVE_DATA_REPLY_CHANNEL_COUNT] = {
    "MT1", "MT2", "MG1", "MG2", "MG3", "MG4", "MG5", "MG6",
    UI_N_("Upper UV"), UI_N_("Lower UV")
};
static uint16_t reported_image_width, reported_image_height;
static protocol_request_t capture_request = PROTOCOL_REQUEST_INITIALIZER(IMAGE_REQUEST_TIMEOUT_MS);
static uint32_t image_late_guard_deadline, image_feedback_deadline;
static bool image_progress_dirty, image_preview_dirty, image_hint_visible;
static bool image_canvas_binding_valid;
static lv_color_t image_empty_pixel;
static uint64_t capture_started_ms;
static uint64_t capture_entered_ms, count_started_ms;
static uint32_t count_baseline_valid;
static uint16_t count_baseline_issue;
static bool count_cumulative, count_valid_reset, count_issue_reset;
static bool count_run_active, count_run_has_note, capture_note_ready;
static bool capture_stream_started, capture_drain_only, capture_cache_reset_pending;
static uint8_t capture_preparation_token;
static bool image_sources_locked(uint32_t now);
static bool image_source_paint_valid, image_sources_were_locked;
static uint8_t image_painted_selection;
static uint32_t image_painted_completion;
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

static uint8_t capture_command(uint8_t source)
{
    return source > IMAGE_SOURCE_COUNT ? 0x48 : 0x47;
}

static uint8_t capture_wire_id(uint8_t source)
{
    return source > IMAGE_SOURCE_COUNT ? source - IMAGE_SOURCE_COUNT : source;
}

static const char *capture_name(uint8_t source)
{
    return source <= IMAGE_SOURCE_COUNT ? image_names[source - 1] :
        signal_names[source - IMAGE_SOURCE_COUNT - 1];
}

static bool capture_owns_drag(void) { return source_drag_owned; }

static bool capture_point_inside(lv_obj_t *object, const lv_point_t *point)
{
    if (!object || !point || lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) return false;
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    return point->x >= area.x1 && point->x <= area.x2 && point->y >= area.y1 && point->y <= area.y2;
}

static bool capture_layer_hit(lv_obj_t *layer, const lv_point_t *point)
{
    for (int i = (int)lv_obj_get_child_cnt(layer) - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(layer, i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        if ((lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE) && capture_point_inside(child, point)) ||
            capture_layer_hit(child, point)) return true;
    }
    return false;
}

static void capture_pointer_reset(bool cancel_drag)
{
    if (source_pressed_index >= 0 && source_items[source_pressed_index].card)
        lv_obj_clear_state(source_items[source_pressed_index].card, LV_STATE_PRESSED);
    if (cancel_drag && source_contact && source_pager)
        lv_content_pager_drag_end(source_pager, true);
    source_pressed_index = -1;
    source_contact = source_drag_owned = source_swiping = source_moved = false;
    source_vertical = source_interrupted = source_cancelled = false;
}

static int capture_button_at(const lv_point_t *point)
{
    unsigned first = lv_content_pager_get_active(source_pager) * CAPTURE_ROWS_PER_PAGE;
    for (unsigned i = first; i < first + CAPTURE_ROWS_PER_PAGE && i < CAPTURE_SOURCE_COUNT; i++)
        if (capture_point_inside(source_items[i].card, point)) return (int)i;
    return -1;
}

static bool capture_pointer_observe(lv_indev_t *indev, lv_event_code_t event,
                                    const lv_point_t *point, uint8_t count)
{
    (void)indev;
    if (!source_pager || !point) return false;
    bool released = event == LV_EVENT_RELEASED || event == LV_EVENT_PRESS_LOST || !count;
    if (settings_detail_overlay_is_open() || capture_layer_hit(lv_layer_top(), point) ||
        capture_layer_hit(lv_layer_sys(), point)) {
        bool owned = source_contact;
        capture_pointer_reset(true);
        return owned;
    }
    /* A global multi-finger gesture can skip the previous release. */
    if (event == LV_EVENT_PRESSED && source_contact) capture_pointer_reset(true);
    if (!source_contact) {
        if (released || count != 1 || event != LV_EVENT_PRESSED ||
            !capture_point_inside(source_viewport, point)) return false;
        if (image_sources_locked(app_clock_uptime_ms())) return true;
        source_contact = source_drag_owned = true;
        source_swiping = source_moved = source_vertical = source_cancelled = false;
        source_interrupted = lv_content_pager_is_moving(source_pager);
        source_press_point = *point;
        source_pressed_index = source_interrupted ? -1 : capture_button_at(point);
        lv_content_pager_drag_begin(source_pager);
        if (source_pressed_index >= 0) lv_obj_add_state(source_items[source_pressed_index].card, LV_STATE_PRESSED);
        return true;
    }
    int dx = point->x - source_press_point.x, dy = point->y - source_press_point.y;
    if (source_cancelled) { if (released) capture_pointer_reset(false); return true; }
    if (event == LV_EVENT_PRESS_LOST || (!released && count != 1) ||
        image_sources_locked(app_clock_uptime_ms())) {
        if (source_pressed_index >= 0) lv_obj_clear_state(source_items[source_pressed_index].card, LV_STATE_PRESSED);
        source_pressed_index = -1;
        lv_content_pager_drag_end(source_pager, true);
        source_cancelled = true;
        if (released) capture_pointer_reset(false);
        return true;
    }
    if (LV_ABS(dx) > 8 || LV_ABS(dy) > 8) source_moved = true;
    if (!source_swiping && !source_vertical && source_moved) {
        if (LV_ABS(dx) > 12 && LV_ABS(dx) > LV_ABS(dy)) source_swiping = true;
        else if (!released && LV_ABS(dy) >= 10 && LV_ABS(dy) > LV_ABS(dx)) source_vertical = true;
    }
    if (source_moved && source_pressed_index >= 0)
        lv_obj_clear_state(source_items[source_pressed_index].card, LV_STATE_PRESSED);
    if (!released) lv_content_pager_drag_update(source_pager, source_swiping ? dx : 0);
    if (!released) return true;
    int clicked = !source_moved && !source_interrupted &&
        capture_button_at(point) == source_pressed_index ? source_pressed_index : -1;
    if (source_swiping) lv_content_pager_drag_update(source_pager, dx);
    if (source_swiping || source_interrupted) lv_content_pager_drag_end(source_pager, source_vertical);
    else lv_content_pager_finish(source_pager);
    capture_pointer_reset(false);
    if (clicked >= 0) lv_event_send(source_items[clicked].card, LV_EVENT_CLICKED, NULL);
    return true;
}

static void capture_progress_x(void *object, int32_t x)
{
    lv_obj_set_x(object, (lv_coord_t)x);
}

static void capture_source_page_changed(lv_event_t *event)
{
    (void)event;
    if (source_pager && source_page_label)
        lv_label_set_text(source_page_label,
            ui_tr(lv_content_pager_get_active(source_pager) ? "Signals" : "Images"));
}

static void signal_show_selected(void);

static void image_render_status(void)
{
    if (!image_status_label) return;
    const char *text = work_mode_service_diagnostic_ready() ? image_status_text : work_mode_service_status_text();
    if (strcmp(lv_label_get_text(image_status_label), text)) lv_label_set_text(image_status_label, text);
    const char *hint = image_hint_visible ? ui_tr("Place one note in the hopper.") : "";
    if (strcmp(lv_label_get_text(capture_frame.message), hint)) lv_label_set_text(capture_frame.message, hint);
}

static void image_set_status(const char *text)
{
    snprintf(image_status_text, sizeof(image_status_text), "%s", text);
    image_render_status();
}

static void image_notice(ui_notice_kind_t kind, const char *text)
{
    image_set_status(ui_tr(text));
    if ((kind == UI_NOTICE_ERROR || kind == UI_NOTICE_WARNING) &&
        !protocol_request_is_pending(&capture_request)) {
        image_canvas_binding_valid = false;
        if (image_canvas) lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
        if (signal_host) lv_obj_add_flag(signal_host, LV_OBJ_FLAG_HIDDEN);
        if (image_dimensions_label) lv_label_set_text(image_dimensions_label, "");
        if (image_placeholder) {
            lv_obj_clear_flag(image_placeholder, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(image_placeholder, ui_tr(text)); lv_obj_center(image_placeholder);
        }
    }
    ui_notice_post_text(kind, "capture.dynamic", UI_N_("Dynamic data capture"), text);
}

static void image_progress_width(void *object, int32_t width)
{
    lv_obj_set_width(object, (lv_coord_t)width);
}

static void image_reset_progress(void)
{
    for (size_t i = 0; i < CAPTURE_SOURCE_COUNT; i++) {
        if (!source_items[i].progress) continue;
        lv_anim_del(source_items[i].progress, image_progress_width);
        lv_anim_del(source_items[i].progress, capture_progress_x);
        lv_obj_set_x(source_items[i].progress, 0);
        lv_obj_add_flag(source_items[i].progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(source_items[i].progress, 0);
    }
    image_progress_dirty = false;
}

static bool image_sources_locked(uint32_t now)
{
    return protocol_request_is_pending(&capture_request) || count_run_active || image_feedback_deadline ||
           image_guard_active(now) || !work_mode_service_diagnostic_ready();
}

static void image_refresh_sources(uint32_t now)
{
    bool locked = image_sources_locked(now);
    uint32_t completed = 0;
    if (!capture_cache_reset_pending) {
        for (size_t i = 0; i < CAPTURE_SOURCE_COUNT; i++) {
            bool complete = i < IMAGE_SOURCE_COUNT ? image_buffers[i].complete :
                signal_buffers[i - IMAGE_SOURCE_COUNT].complete;
            if (complete) completed |= UINT32_C(1) << i;
        }
    }
    bool selection_changed = !image_source_paint_valid || image_painted_selection != selected_source;
    bool lock_changed = !image_source_paint_valid || image_sources_were_locked != locked;
    bool completion_changed = !image_source_paint_valid || image_painted_completion != completed;
    if (!selection_changed && !lock_changed && !completion_changed) return;
    for (size_t i = 0; i < CAPTURE_SOURCE_COUNT; i++) {
        bool selected = selected_source == i + 1;
        if (!source_items[i].card) continue;
        if (selection_changed) {
            lv_obj_set_style_bg_color(source_items[i].card,
                lv_color_hex(selected ? 0xFFFFFF : IMAGE_SOURCE_SURFACE), 0);
        }
        if (completion_changed) {
            lv_obj_set_style_text_color(source_items[i].label,
                lv_color_hex(completed & (UINT32_C(1) << i) ? 0x1462CC : 0x1D2B34), 0);
        }
        if (lock_changed) {
            if (locked) lv_obj_clear_flag(source_items[i].card, LV_OBJ_FLAG_CLICKABLE);
            else lv_obj_add_flag(source_items[i].card, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    if (selection_changed && image_title_label) lv_label_set_text(image_title_label,
        selected_source ? ui_tr(capture_name(selected_source)) : "");
    image_painted_selection = selected_source;
    image_painted_completion = completed;
    image_sources_were_locked = locked;
    image_source_paint_valid = true;
}

static void image_update_progress(bool animate)
{
    if (!captured_source || !source_items[captured_source - 1].progress) return;
    lv_obj_t *fill = source_items[captured_source - 1].progress;
    int full = lv_obj_get_style_width(source_items[captured_source - 1].card, 0);
    bool signal = capture_command(captured_source) == 0x48;
    image_buffer_t *item = image_buffer_for(captured_image_id);
    bool complete = signal ? signal_buffers[capture_wire_id(captured_source) - 1].complete : item && item->complete;
    uint16_t total = signal ? 0 : image_data_reply_scanline_count(captured_image_id, reported_image_width, reported_image_height);
    unsigned percent = total ? (unsigned)item->packets_received * 100U / total : 0;
    if (!complete && percent > 99) percent = 99;
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_HIDDEN);
    if (signal && !complete) {
        if (!lv_anim_get(fill, capture_progress_x)) {
            lv_obj_set_width(fill, full / 3);
            lv_anim_t animation; lv_anim_init(&animation);
            lv_anim_set_var(&animation, fill); lv_anim_set_values(&animation, -full / 3, full);
            lv_anim_set_time(&animation, 900); lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_exec_cb(&animation, capture_progress_x); lv_anim_start(&animation);
        }
    } else {
        lv_anim_del(fill, capture_progress_x); lv_obj_set_x(fill, 0);
        lv_anim_del(fill, image_progress_width);
        int target = complete ? full : total ? full * (int)percent / 100 : 4;
        if (target < 4) target = 4;
        if (animate && lv_obj_get_width(fill) != target) {
            lv_anim_t animation; lv_anim_init(&animation);
            lv_anim_set_var(&animation, fill); lv_anim_set_values(&animation, lv_obj_get_width(fill), target);
            lv_anim_set_time(&animation, 100); lv_anim_set_exec_cb(&animation, image_progress_width);
            lv_anim_set_path_cb(&animation, lv_anim_path_ease_out); lv_anim_start(&animation);
        } else lv_obj_set_width(fill, target);
    }
    if (protocol_request_is_pending(&capture_request)) {
        char text[96];
        if (signal) snprintf(text, sizeof(text), ui_tr("Receiving signal: %u samples"),
            signal_buffers[capture_wire_id(captured_source) - 1].count);
        else if (total) snprintf(text, sizeof(text), "%s %u%%", ui_tr("Receiving image"), percent);
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
    if (signal_host) lv_obj_add_flag(signal_host, LV_OBJ_FLAG_HIDDEN);
    if (selected_source > IMAGE_SOURCE_COUNT) { signal_show_selected(); return; }
    image_buffer_t *item = image_buffer_for(selected_image_id);
    if (!image_canvas || !image_placeholder) return;
    if (!item || !item->buffer || (!item->complete &&
        (selected_image_id != captured_image_id || !protocol_request_is_pending(&capture_request) || !item->packets_received))) {
        image_hide_canvas();
        lv_label_set_text(image_placeholder, ui_tr(selected_source ? "Receiving image" : "Select a capture source"));
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
        if (zoom > UINT16_MAX) zoom = UINT16_MAX;
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
    if (locked) {
        capture_pointer_reset(true);
        if (source_pager) lv_content_pager_finish(source_pager);
    }
    ui_manager_hold_navigation(UI_PAGE_IMAGE_GET, locked);
    gesture_service_set_navigation_blocked(UI_PAGE_IMAGE_GET, locked);
    if (capture_frame.back) {
        if (locked) lv_obj_add_state(capture_frame.back, LV_STATE_DISABLED);
        else lv_obj_clear_state(capture_frame.back, LV_STATE_DISABLED);
    }
}


static void capture_clear_run_cache(void)
{
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) image_free_one_buffer(&image_buffers[i]);
    memset(signal_buffers, 0, sizeof(signal_buffers));
    selected_source = selected_image_id = 0;
    image_hide_canvas();
    if (signal_host) lv_obj_add_flag(signal_host, LV_OBJ_FLAG_HIDDEN);
    image_canvas_binding_valid = false;
    image_preview_dirty = false;
    if (image_dimensions_label) lv_label_set_text(image_dimensions_label, "");
    if (image_placeholder) {
        lv_label_set_text(image_placeholder, ui_tr("Select a capture source"));
        lv_obj_center(image_placeholder);
    }
    image_reset_progress();
    image_feedback_deadline = 0;
    image_source_paint_valid = false;
}

static void capture_require_new_note(void)
{
    capture_note_ready = false;
    image_hint_visible = true;
}

static void capture_wait_for_end(ui_notice_kind_t kind, const char *reason)
{
    capture_drain_only = true;
    capture_require_new_note();
    image_preview_dirty = false;
    image_reset_progress();
    image_hide_canvas();
    if (signal_host) lv_obj_add_flag(signal_host, LV_OBJ_FLAG_HIDDEN);
    if (image_placeholder) { lv_label_set_text(image_placeholder, ui_tr(reason)); lv_obj_center(image_placeholder); }
    image_notice(kind, reason);
}

static void capture_show_preparation(void)
{
    if (settings_detail_overlay_is_open()) return;
    settings_detail_dialog_show(ui_tr("Dynamic data capture"),
        ui_tr("Run one note on this page before capturing."), ui_tr("OK"), NULL,
        NULL, NULL, &capture_preparation_token);
}

static void capture_request_finish(uint32_t now, bool late_guard)
{
    protocol_request_finish(&capture_request);
    capture_started_ms = 0;
    capture_stream_started = capture_drain_only = false;
    image_preview_dirty = false;
    image_buffer_t *item = image_buffer_for(captured_image_id);
    if (item && item->raw_rgb) { lv_mem_free(item->raw_rgb); item->raw_rgb = NULL; }
    image_capture_lock(false);
    if (signal_host) lv_obj_add_flag(signal_host, LV_OBJ_FLAG_HIDDEN);
    if (late_guard) image_late_guard_deadline = now + IMAGE_LATE_FRAME_GUARD_MS;
    image_feedback_deadline = 0;
    image_reset_progress();
    if (capture_cache_reset_pending) { capture_cache_reset_pending = false; capture_clear_run_cache(); }
    image_refresh_sources(now);
}

static void image_start_capture(uint8_t source, uint32_t now)
{
    image_preview_dirty = false;
    capture_stream_started = capture_drain_only = false;
    captured_source = source;
    captured_image_id = source <= IMAGE_SOURCE_COUNT ? source : 0;
    reported_image_width = reported_image_height = 0;
    uint8_t wire_id = capture_wire_id(source);
    uint8_t command = capture_command(source);
    if (command == 0x48) memset(&signal_buffers[wire_id - 1], 0, sizeof(signal_buffers[0]));
    image_set_status(ui_tr(command == 0x47 ? "Receiving image" : "Receiving signal"));
    if (!protocol_request_begin_for_command(&capture_request, command)) return;
    capture_started_ms = protocol_request_now_ms();
    image_capture_lock(true);
    if (!settings_detail_send_command(command, &wire_id, 1)) {
        capture_require_new_note();
        capture_request_finish(now, false);
        lv_label_set_text(image_placeholder, ui_tr("Capture failed. Run one note again."));
        lv_obj_center(image_placeholder);
        image_notice(UI_NOTICE_ERROR, UI_N_("Capture failed. Run one note again."));
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
    if (!source || source > CAPTURE_SOURCE_COUNT) return;
    unsigned page = (source - 1U) / CAPTURE_ROWS_PER_PAGE;
    if (!source_pager || page != lv_content_pager_get_active(source_pager) ||
        lv_content_pager_is_moving(source_pager)) return;
    bool complete = source <= IMAGE_SOURCE_COUNT ? image_buffers[source - 1].complete :
        signal_buffers[capture_wire_id(source) - 1].complete;
    if (!complete && !capture_note_ready) { capture_show_preparation(); return; }
    selected_source = source;
    selected_image_id = source <= IMAGE_SOURCE_COUNT ? source : 0;
    image_refresh_sources(now);
    image_show_selected_buffer();
    if (complete) image_set_status("");
    else image_start_capture(source, now);
}

static void mode_retry_clicked(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && !protocol_request_is_pending(&capture_request)) work_mode_service_retry();
}

static void image_esc_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && !protocol_request_is_pending(&capture_request)) ui_manager_pop_page();
}

void ui_page_dynamic_capture_create(lv_obj_t *parent)
{
    if (capture_page) return;
    selected_source = captured_source = selected_image_id = captured_image_id = 0;
    image_capture_lock(false);
    image_preview_dirty = false;
    image_hint_visible = true;
    capture_entered_ms = protocol_request_now_ms();
    count_started_ms = 0;
    count_run_active = count_run_has_note = capture_note_ready = false;
    capture_stream_started = capture_drain_only = capture_cache_reset_pending = false;
    image_source_paint_valid = false;
    image_status_text[0] = '\0';
    lv_settings_header_t header = {ui_tr("Dynamic data capture"), NULL, "Layers", image_esc_cb, NULL};
    capture_frame = lv_settings_frame_create(parent, &header);
    capture_page = capture_frame.root;
    lv_obj_set_style_bg_opa(capture_frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(capture_frame.body, 0, 0);
    lv_obj_t *heading = lv_obj_get_parent(capture_frame.back);
    lv_obj_del(capture_frame.back);
    capture_frame.back = lv_nav_back_text_button_create(heading, 1156, 7,
        &lv_font_instrument_sans_medium_16, image_esc_cb, NULL);
    lv_obj_t *sources = lv_settings_box(capture_frame.body, 0, 0, 260, 300, IMAGE_SOURCE_SURFACE);
    lv_obj_set_style_radius(sources, 14, 0);
    lv_obj_set_style_clip_corner(sources, true, 0);
    source_pager = lv_content_pager_create(sources, 8, 8, CAPTURE_BUTTON_W, 292, CAPTURE_PAGE_COUNT);
    source_viewport = lv_content_pager_get_viewport(source_pager);
    source_page_label = lv_settings_label(sources, ui_tr("Images"), 8, 283,
        &lv_font_instrument_sans_medium_12, 0x586B78);
    lv_obj_set_width(source_page_label, 170);
    lv_label_set_long_mode(source_page_label, LV_LABEL_LONG_DOT);
    if (source_viewport) {
        lv_obj_clear_flag(source_viewport, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        lv_content_pager_set_circular(source_pager, true);
        lv_content_pager_set_indicator_position(source_pager, 182, 276);
        lv_obj_add_event_cb(source_viewport, capture_source_page_changed, LV_EVENT_SCROLL_END, NULL);
        for (unsigned i = 0; i < CAPTURE_SOURCE_COUNT; i++) {
            lv_obj_t *column = lv_content_pager_get_page(source_pager, i / CAPTURE_ROWS_PER_PAGE);
            lv_obj_t *button = lv_settings_button(column, 0, (i % CAPTURE_ROWS_PER_PAGE) * CAPTURE_ROW_STEP,
                CAPTURE_BUTTON_W, CAPTURE_BUTTON_H, ui_tr(capture_name(i + 1)),
                false, image_source_cb, (void *)(uintptr_t)(i + 1));
            lv_obj_t *label = lv_obj_get_child(button, 0);
            lv_obj_set_width(label, CAPTURE_BUTTON_W - 16);
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(label);
            lv_obj_set_style_border_width(button, 0, 0);
            lv_obj_set_style_border_width(button, 0, LV_STATE_PRESSED);
            lv_obj_set_style_clip_corner(button, true, 0);
            lv_obj_t *fill = lv_settings_box(button, 0, 0, 0, CAPTURE_BUTTON_H, IMAGE_PROGRESS_COLOR);
            lv_obj_set_style_radius(fill, 0, 0);
            lv_obj_move_background(fill);
            lv_obj_add_flag(fill, LV_OBJ_FLAG_HIDDEN);
            source_items[i] = (capture_source_item_t){button, label, fill};
        }
    }
    lv_obj_t *preview = lv_settings_box(capture_frame.body, 276, 0, IMAGE_PREVIEW_W, IMAGE_PREVIEW_H, 0xFFFFFF);
    lv_obj_set_style_radius(preview, 14, 0);
    lv_obj_set_style_clip_corner(preview, true, 0);
    image_placeholder = lv_settings_label(preview, ui_tr("Select a capture source"), 0, 0,
        &lv_font_instrument_sans_medium_18, 0x586B78);
    lv_obj_set_width(image_placeholder, IMAGE_PREVIEW_W - 80);
    lv_label_set_long_mode(image_placeholder, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(image_placeholder, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(image_placeholder);
    image_canvas = lv_canvas_create(preview);
    image_canvas_binding_valid = false;
    lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
    signal_host = lv_settings_box(preview, 0, 0, IMAGE_PREVIEW_W, IMAGE_PREVIEW_H, 0xF4F6F7);
    lv_obj_set_style_radius(signal_host, 14, 0);
    for (unsigned i = 0; i < 12; i++) {
        bool horizontal = i < 5;
        int x = 32 + (int)(i >= 5 ? i - 5 : 0) * 150;
        int y = 16 + (int)i * 63;
        signal_grid[i][0] = (lv_point_t){horizontal ? 32 : x, horizontal ? y : 16};
        signal_grid[i][1] = (lv_point_t){horizontal ? 932 : x, horizontal ? y : 268};
        lv_obj_t *grid = lv_line_create(signal_host);
        lv_line_set_points(grid, signal_grid[i], 2);
        lv_obj_set_style_line_color(grid, lv_color_hex(0xE2E7EA), 0);
        lv_obj_set_style_line_width(grid, 1, 0);
        lv_obj_clear_flag(grid, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_settings_label(signal_host, "255", 3, 10, &lv_font_instrument_sans_medium_10, 0x7B8A94);
    lv_settings_label(signal_host, "128", 3, 136, &lv_font_instrument_sans_medium_10, 0x7B8A94);
    lv_settings_label(signal_host, "0", 18, 260, &lv_font_instrument_sans_medium_10, 0x7B8A94);
    signal_count_label = lv_settings_label(signal_host, "", 32, 280,
        &lv_font_instrument_sans_medium_10, 0x586B78);
    signal_line = lv_line_create(signal_host);
    lv_obj_set_style_line_color(signal_line, lv_color_hex(0x1462CC), 0);
    lv_obj_set_style_line_width(signal_line, 2, 0);
    lv_obj_set_style_line_rounded(signal_line, true, 0);
    lv_obj_add_flag(signal_host, LV_OBJ_FLAG_HIDDEN);
    image_title_label = lv_settings_label(capture_page, "", 800, 29, &lv_font_instrument_sans_medium_16, 0x1D2B34);
    lv_obj_set_width(image_title_label, 214);
    lv_obj_set_style_text_align(image_title_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(image_title_label, LV_LABEL_LONG_DOT);
    image_dimensions_label = lv_settings_label(capture_page, "", 1030, 31, &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_obj_del(capture_frame.message);
    capture_frame.message = lv_settings_label(capture_page, ui_tr("Place one note in the hopper."), 24, 65,
        &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_obj_set_width(capture_frame.message, 610);
    lv_label_set_long_mode(capture_frame.message, LV_LABEL_LONG_DOT);
    image_status_label = lv_settings_label(capture_page, "", 644, 65, &lv_font_instrument_sans_medium_12, 0x586B78);
    lv_obj_set_width(image_status_label, 610); lv_label_set_long_mode(image_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(image_status_label, LV_TEXT_ALIGN_RIGHT, 0);
    mode_retry_button = lv_settings_button(capture_frame.footer, 0, 0, 130, 46,
        ui_tr("Retry"), false, mode_retry_clicked, NULL);
    lv_obj_add_flag(mode_retry_button, LV_OBJ_FLAG_HIDDEN);
    capture_pointer_reset(false);
    source_painted_page = 0;
    if (source_viewport) {
        gesture_service_set_page_policy(UI_PAGE_IMAGE_GET, capture_owns_drag, NULL);
        gesture_service_set_pointer_policy(UI_PAGE_IMAGE_GET, capture_pointer_observe);
    }
    image_refresh_sources(app_clock_uptime_ms()); image_render_status();
    if (!source_pager) image_notice(UI_NOTICE_ERROR,
        ui_text_msgid(UI_TEXT_SETTINGS_IMAGE_GET_MEMORY_ERROR));
}

void ui_page_dynamic_capture_destroy(void)
{
    gesture_service_clear_pointer_policy(UI_PAGE_IMAGE_GET);
    gesture_service_clear_page_policy(UI_PAGE_IMAGE_GET);
    capture_pointer_reset(false);
    if (settings_detail_dialog_is_owned_by(&capture_preparation_token)) settings_detail_dialog_hide();
    count_run_active = count_run_has_note = capture_note_ready = false;
    if (protocol_request_is_pending(&capture_request)) capture_request_finish(app_clock_uptime_ms(), true);
    image_reset_progress();
    image_capture_lock(false);
    capture_started_ms = 0;
    image_preview_dirty = false;
    image_feedback_deadline = 0;
    ui_notice_clear("capture.dynamic");
    for (size_t i = 0; i < IMAGE_SOURCE_COUNT; i++) image_free_one_buffer(&image_buffers[i]);
    if (capture_page && lv_obj_is_valid(capture_page)) lv_obj_del(capture_page);
    capture_page = image_canvas = image_placeholder = NULL;
    source_pager = source_viewport = source_page_label = NULL;
    signal_host = signal_line = signal_count_label = NULL;
    memset(signal_buffers, 0, sizeof(signal_buffers));
    selected_source = captured_source = 0;
    image_title_label = image_dimensions_label = image_status_label = mode_retry_button = NULL;
    capture_frame = (lv_settings_frame_t){0};
    memset(source_items, 0, sizeof(source_items));
    image_source_paint_valid = false;
    selected_image_id = captured_image_id = 0;
    reported_image_width = reported_image_height = 0;
}

static void image_on_frame(const uint8_t *data, uint16_t length)
{
    image_data_reply_view_t reply = image_data_reply_parse(data, length);
    uint32_t now = app_clock_uptime_ms();
    if (reply.kind == IMAGE_DATA_REPLY_HEADER) {
        if (reply.image_id != captured_image_id) return;
        capture_stream_started = true;
        if (capture_drain_only) return;
        if (reported_image_width || reported_image_height) {
            /* A repeated header cannot erase an in-flight image or extend its deadline. */
            if (reply.width == reported_image_width && reply.height == reported_image_height) return;
            capture_wait_for_end(UI_NOTICE_WARNING, UI_N_("Transfer interrupted. Waiting for the end frame."));
            return;
        }
        if (!reply.width || !reply.height || reply.width > IMAGE_MAX_WIDTH || reply.height > IMAGE_MAX_HEIGHT) {
            capture_wait_for_end(UI_NOTICE_ERROR, UI_N_("Transfer interrupted. Waiting for the end frame."));
            return;
        }
        if (!image_prepare_buffer(captured_image_id, reply.width, reply.height)) {
            capture_wait_for_end(UI_NOTICE_ERROR, UI_N_("Transfer interrupted. Waiting for the end frame."));
            return;
        }
        reported_image_width = reply.width;
        reported_image_height = reply.height;
        protocol_request_touch_result(&capture_request);
        image_progress_dirty = true;
    } else if (reply.kind == IMAGE_DATA_REPLY_SCANLINE && !capture_drain_only && image_receive_block(&reply)) {
        protocol_request_touch_result(&capture_request);
    } else if (reply.kind == IMAGE_DATA_REPLY_END) {
        image_buffer_t *item = image_buffer_for(captured_image_id);
        uint16_t total = image_data_reply_scanline_count(captured_image_id, reported_image_width, reported_image_height);
        bool complete = !capture_drain_only && item && item->buffer && reported_image_width && reported_image_height && item->packets_received == total;
        if (complete) {
            capture_request_finish(now, false);
            item->complete = true;
            image_preview_dirty = false;
            image_feedback_deadline = now + IMAGE_FINISH_FEEDBACK_MS;
            image_update_progress(true);
            image_show_selected_buffer();
            image_notice(UI_NOTICE_SUCCESS, UI_N_("Capture complete"));
            image_refresh_sources(now);
        } else {
            bool newer_run = capture_cache_reset_pending;
            if (!newer_run) capture_require_new_note();
            capture_request_finish(now, false);
            image_hide_canvas();
            if (newer_run && (capture_note_ready || count_run_active)) image_set_status("");
            else image_notice(UI_NOTICE_WARNING, UI_N_("Capture failed. Run one note again."));
        }
    }
}

static void signal_show_selected(void)
{
    image_hide_canvas();
    unsigned index = selected_source - IMAGE_SOURCE_COUNT - 1;
    if (index >= WAVE_DATA_REPLY_CHANNEL_COUNT) return;
    uint16_t count = signal_buffers[index].count;
    bool visible = signal_buffers[index].complete ||
        (selected_source == captured_source && protocol_request_is_pending(&capture_request));
    if (!count || !visible) {
        lv_label_set_text(image_placeholder, ui_tr("Receiving signal"));
        lv_obj_center(image_placeholder); return;
    }
    unsigned points = count > 1 ? count : 2;
    for (unsigned i = 0; i < points; i++) {
        uint8_t value = signal_buffers[index].values[count > 1 ? i : 0];
        signal_points[i].x = (lv_coord_t)(32 + i * 900U / (points - 1));
        signal_points[i].y = (lv_coord_t)(16 + (255U - value) * 252U / 255U);
    }
    lv_line_set_points(signal_line, signal_points, points);
    lv_label_set_text_fmt(signal_count_label, ui_tr("%u samples"), (unsigned)count);
    lv_obj_clear_flag(signal_host, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(image_placeholder, LV_OBJ_FLAG_HIDDEN);
}

static void signal_on_frame(const uint8_t *data, uint16_t length)
{
    wave_data_reply_view_t reply = wave_data_reply_parse(data, length);
    uint32_t now = app_clock_uptime_ms();
    unsigned index = capture_wire_id(captured_source) - 1;
    if (reply.kind == WAVE_DATA_REPLY_CHANNEL && reply.channel == capture_wire_id(captured_source)) {
        capture_stream_started = true;
        if (capture_drain_only) return;
        if (reply.value_count > SIGNAL_POINT_MAX) return;
        memcpy(signal_buffers[index].values, reply.values, reply.value_count);
        signal_buffers[index].count = reply.value_count;
        protocol_request_touch_result(&capture_request);
        image_progress_dirty = image_preview_dirty = true;
    } else if (reply.kind == WAVE_DATA_REPLY_END) {
        bool complete = !capture_drain_only && signal_buffers[index].count > 0;
        if (complete) {
            capture_request_finish(now, false);
            signal_buffers[index].complete = true; image_preview_dirty = false;
            image_feedback_deadline = now + IMAGE_FINISH_FEEDBACK_MS;
            image_update_progress(true); image_show_selected_buffer();
            image_notice(UI_NOTICE_SUCCESS, UI_N_("Capture complete")); image_refresh_sources(now);
        } else {
            bool newer_run = capture_cache_reset_pending;
            if (!newer_run) capture_require_new_note();
            capture_request_finish(now, false); image_hide_canvas();
            lv_obj_add_flag(signal_host, LV_OBJ_FLAG_HIDDEN);
            if (newer_run && (capture_note_ready || count_run_active)) image_set_status("");
            else image_notice(UI_NOTICE_WARNING, UI_N_("Capture failed. Run one note again."));
        }
    } else if (reply.kind == WAVE_DATA_REPLY_NO_DATA) {
        if (capture_stream_started) capture_wait_for_end(UI_NOTICE_WARNING, UI_N_("Transfer interrupted. Waiting for the end frame."));
        else {
            capture_require_new_note();
            capture_request_finish(now, true);
            image_notice(UI_NOTICE_WARNING, UI_N_("Capture failed. Run one note again."));
        }
    }
}

void ui_page_dynamic_capture_on_frame(uint8_t command, const uint8_t *data, uint16_t length)
{
    if (!capture_page || !captured_source || command != capture_command(captured_source) ||
        !protocol_request_is_pending(&capture_request) ||
        protocol_request_result_time_ms() < capture_started_ms ||
        (!capture_stream_started && !protocol_request_can_take_result(&capture_request))) return;
    if (command == 0x47) image_on_frame(data, length);
    else signal_on_frame(data, length);
}

bool ui_page_dynamic_capture_is_capturing(void)
{
    return capture_page && protocol_request_is_pending(&capture_request);
}

bool ui_page_dynamic_capture_poll(uint32_t now)
{
    if (!capture_page) return false;
    bool timed_out = !capture_drain_only && (capture_stream_started ?
        protocol_request_timeout_ready(&capture_request) : protocol_request_take_timeout(&capture_request));
    if (!timed_out && !capture_drain_only && protocol_request_is_pending(&capture_request) &&
        protocol_request_now_ms() - capture_started_ms >= IMAGE_CAPTURE_LIMIT_MS) timed_out = true;
    if (timed_out) {
        if (capture_stream_started) capture_wait_for_end(UI_NOTICE_WARNING, UI_N_("Transfer interrupted. Waiting for the end frame."));
        else {
            capture_require_new_note();
            capture_request_finish(now, true);
            image_hide_canvas();
            image_notice(UI_NOTICE_WARNING, UI_N_("Capture failed. Run one note again."));
        }
    }
    if (source_pager && source_painted_page != lv_content_pager_get_active(source_pager)) {
        source_painted_page = lv_content_pager_get_active(source_pager);
        capture_source_page_changed(NULL);
    }
    if (image_preview_dirty) {
        image_preview_dirty = false;
        image_show_selected_buffer();
        lv_obj_invalidate(selected_source > IMAGE_SOURCE_COUNT ? signal_host : image_canvas);
    }
    if (image_time_reached(now, image_feedback_deadline)) {
        image_feedback_deadline = 0;
        image_reset_progress();
    } else if (image_progress_dirty) image_update_progress(false);
    work_mode_snapshot_t mode;
    work_mode_service_get_snapshot(&mode);
    if (mode.phase == WORK_MODE_FAILED && !protocol_request_is_pending(&capture_request)) {
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


void ui_page_dynamic_capture_on_count_start(uint32_t valid, uint16_t issue, bool cumulative)
{
    uint64_t received = protocol_request_result_time_ms();
    if (!capture_page || ui_manager_get_current_page() != UI_PAGE_IMAGE_GET ||
        !work_mode_service_diagnostic_ready() || received <= capture_entered_ms || count_run_active) return;
    count_started_ms = received;
    count_baseline_valid = valid;
    count_baseline_issue = issue;
    count_cumulative = cumulative;
    count_valid_reset = count_issue_reset = false;
    count_run_active = true;
    count_run_has_note = false;
    capture_require_new_note();
    if (protocol_request_is_pending(&capture_request) && capture_stream_started) {
        capture_cache_reset_pending = true;
        capture_wait_for_end(UI_NOTICE_WARNING, UI_N_("Transfer interrupted. Waiting for the end frame."));
    } else {
        if (protocol_request_is_pending(&capture_request)) capture_request_finish(app_clock_uptime_ms(), true);
        capture_clear_run_cache();
        image_set_status("");
    }
    image_refresh_sources(app_clock_uptime_ms());
    image_render_status();
}

void ui_page_dynamic_capture_on_count_info(uint16_t valid, uint8_t issue, uint8_t status)
{
    if (!capture_page || !count_run_active || ui_manager_get_current_page() != UI_PAGE_IMAGE_GET ||
        protocol_request_result_time_ms() < count_started_ms) return;
    if (!work_mode_service_diagnostic_ready()) {
        count_run_active = false;
        capture_require_new_note();
        return;
    }
    bool finished = status >= 2 && status <= 5;
    if (status == 0 || status > 5) return;
    /* END commonly carries zeros: retain evidence from this run's accepted LIVE.
     * ADD may retain counters across runs; only new values or a reset prove a note. */
    if (status == 1) {
        if (valid > 0 && valid < count_baseline_valid) count_valid_reset = true;
        if (issue > 0 && issue < count_baseline_issue) count_issue_reset = true;
    }
    bool valid_note = valid > 0 && (!count_cumulative || count_valid_reset || valid > count_baseline_valid);
    bool issue_note = issue > 0 && (!count_cumulative || count_issue_reset || issue > count_baseline_issue);
    count_run_has_note |= valid_note || issue_note;
    if (finished) {
        count_run_active = false;
        capture_note_ready = count_run_has_note;
        image_hint_visible = !capture_note_ready;
        image_refresh_sources(app_clock_uptime_ms());
        image_render_status();
    }
}
