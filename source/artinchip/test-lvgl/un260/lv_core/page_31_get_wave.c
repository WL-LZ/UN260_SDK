#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_31_get_wave.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_text.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define WAVE_SOURCE_COUNT 7
#define WAVE_POINT_MAX 256
#define WAVE_PREVIEW_W 774
#define WAVE_PREVIEW_H 170
#define WAVE_REQUEST_TIMEOUT_MS 5000U
#define WAVE_LATE_FRAME_GUARD_MS 3000U

typedef struct {
    lv_obj_t* card;
    lv_obj_t* label;
} wave_source_item_t;

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

static const uint8_t g_wave_sources[WAVE_SOURCE_COUNT] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

static lv_settings_frame_t wave_frame;
static lv_obj_t* wave_page = NULL;
static const char *wave_names[]={"MT","MG 1 / 2","MG 3","MG 4","MG 5 / 6","Upper UV","Lower UV"};
static uint8_t wave_received_mask;
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

    if (wave_request_active || wave_guard_active(now_ms) || !work_mode_service_diagnostic_ready()) {
        settings_detail_action_block(wave_request_button, !work_mode_service_diagnostic_ready() ? work_mode_service_status_text() : "The previous capture is still being received. Please wait.");
    } else {
        settings_detail_action_block(wave_request_button, NULL);
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

static size_t wave_source_index(uint8_t id)
{
    for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
        if (g_wave_sources[i] == id) {
            return i;
        }
    }

    return 0;
}

static char wave_status_text[192];
static lv_color_t wave_status_color;
static void wave_render_status(void)
{
    if (!wave_status_label) return;
    bool ready=work_mode_service_diagnostic_ready();
    const char *text=ready ? wave_status_text : work_mode_service_status_text();
    if (strcmp(lv_label_get_text(wave_status_label),text)) lv_label_set_text(wave_status_label,text);
    lv_obj_set_style_text_color(wave_status_label,ready ? wave_status_color : lv_color_hex(0x586B78),0);
}
static void wave_set_status(const char* text, lv_color_t color)
{
    snprintf(wave_status_text,sizeof(wave_status_text),"%s",text);
    wave_status_color=color;
    wave_render_status();
}

static void wave_refresh_sources(void)
{
    size_t selected_index = wave_source_index(selected_wave_id);

    for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
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

    if (wave_title_label) {
        lv_label_set_text(wave_title_label, wave_names[wave_source_index(selected_wave_id)]);
        if (wave_line && lv_obj_is_valid(wave_line)) {
            lv_obj_set_style_line_color(wave_line, lv_color_hex(0x79AEF2), 0);
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
    if (!wave_line || !lv_obj_is_valid(wave_line) || wave_current_count == 0) return;

    for (uint16_t i = 0; i < wave_current_count; i++) {
        uint8_t value = wave_values[wave_source_index(selected_wave_id)][i];
        wave_points[i].x = (lv_coord_t)(12 + (wave_current_count > 1 ?
            (uint32_t)i * (WAVE_PREVIEW_W - 24) / (wave_current_count - 1) : 0));
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
                        lv_color_hex(0xA35B12));
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
    wave_received_mask |= (uint8_t)(1U << index);

    if (wave_id == selected_wave_id) {
        wave_current_count = count;
        wave_draw_values();
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
    if (wave_request_active || wave_guard_active(now_ms) || !work_mode_service_diagnostic_ready()) return false;

    if (wave_send_request()) {
        wave_clear_data();
        wave_received_mask = 0;
        wave_request_active = true;
        wave_request_touch(now_ms);
        wave_refresh_request_button(now_ms);
        for (size_t i = 0; i < WAVE_SOURCE_COUNT; i++) {
            wave_value_counts[i] = 0;
        }
        wave_set_status(ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_WAITING),
                        lv_color_hex(0x1462CC));
        return true;
    }

    wave_set_status("Could not send capture request. Try again.",lv_color_hex(0xB63B32));
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

void ui_page_31_get_wave_create(lv_obj_t *parent)
{
    if (wave_page) return;
    lv_settings_header_t header={"Waveform","Maintenance / Magnetic and UV channels","Layers",wave_esc_cb,NULL};
    wave_frame=lv_settings_frame_create(parent,&header);wave_page=wave_frame.root;
    settings_detail_add_run(wave_page);
    lv_obj_set_style_bg_opa(wave_frame.body,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(wave_frame.body,0,0);
    lv_obj_t *sources=lv_settings_box(wave_frame.body,0,0,410,242,0xF1F4F5);
    lv_obj_set_style_radius(sources,14,0);
    for(unsigned i=0;i<WAVE_SOURCE_COUNT;i++) {
        lv_obj_t *item=lv_settings_button(sources,8+(i%2)*201,8+(i/2)*58,193,52,
                                          wave_names[i],false,wave_source_cb,(void *)(uintptr_t)(i+1));
        lv_obj_set_style_border_width(item,1,0);
        source_items[i].card=item;source_items[i].label=lv_obj_get_child(item,0);
    }
    lv_settings_label(sources,"7 channels",235,203,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_obj_t *preview=lv_settings_box(wave_frame.body,426,0,806,242,0xFFFFFF);
    lv_obj_set_style_radius(preview,14,0);
    wave_title_label=lv_settings_label(preview,"",16,15,&lv_font_instrument_sans_medium_18,0x1D2B34);
    lv_settings_label(preview,"RAW VALUE  0 - 255",572,19,&lv_font_instrument_sans_medium_12,0x586B78);
    wave_chart_host=lv_settings_box(preview,16,47,WAVE_PREVIEW_W,WAVE_PREVIEW_H,0x1D2B34);
    lv_obj_set_style_radius(wave_chart_host,10,0);
    for(unsigned i=1;i<5;i++)lv_settings_box(wave_chart_host,12,i*WAVE_PREVIEW_H/5,WAVE_PREVIEW_W-24,1,0x334550);
    for(unsigned i=1;i<8;i++)lv_settings_box(wave_chart_host,i*WAVE_PREVIEW_W/8,12,1,WAVE_PREVIEW_H-24,0x334550);
    wave_placeholder=lv_settings_label(wave_chart_host,"Capture a note to view its signal",0,0,
                                       &lv_font_instrument_sans_medium_18,0xC8D3DA);
    lv_obj_center(wave_placeholder);
    wave_line=lv_line_create(wave_chart_host);
    lv_obj_set_style_line_width(wave_line,2,0);
    lv_obj_set_style_line_rounded(wave_line,true,0);
    lv_obj_set_style_line_color(wave_line,lv_color_hex(0x79AEF2),0);
    lv_obj_add_flag(wave_line,LV_OBJ_FLAG_HIDDEN);
    lv_settings_label(preview,"Sample position",331,221,&lv_font_instrument_sans_medium_12,0x586B78);
    wave_status_label=wave_frame.message;
    wave_set_status("Place a note as required, then select Capture.",lv_color_hex(0x586B78));
    mode_retry_button=lv_settings_button(wave_frame.footer,904,0,130,46,"Retry",false,mode_retry_clicked,NULL);
    lv_obj_add_flag(mode_retry_button,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(wave_frame.message,884);
    wave_request_button=lv_settings_button(wave_frame.footer,1050,0,182,46,"Capture",true,wave_request_cb,NULL);
    wave_refresh_sources();wave_refresh_request_button(app_clock_uptime_ms());
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
    wave_frame = (lv_settings_frame_t){0};
    mode_retry_button=NULL;
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
        source_items[i].label = NULL;
    }
}

void ui_page_31_get_wave_on_frame(const uint8_t* data, uint16_t len)
{
    uint8_t sub;
    uint32_t now_ms;

    if (!data || len < 1) return;
    if (!wave_page || !lv_obj_is_valid(wave_page)) return;
    if (!wave_request_active) return;

    sub = data[0];
    now_ms = app_clock_uptime_ms();
    if (sub == 0x00) {
        wave_request_finish(now_ms, false);
        wave_clear_data();
        wave_set_status(ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_NO_DATA),
                        lv_color_hex(0xA35B12));
        return;
    }

    if (sub < 0x01 || sub > WAVE_SOURCE_COUNT) return;
    wave_request_touch(now_ms);

    if (sub == selected_wave_id) {
        if (wave_line && lv_obj_is_valid(wave_line)) {
            lv_obj_set_style_line_color(wave_line, lv_color_hex(0x79AEF2), 0);
        }
        wave_refresh_sources();
    }
    wave_store_values(sub, &data[1], (uint16_t)(len - 1));
    if (sub == WAVE_SOURCE_COUNT) {
        wave_request_finish(now_ms, false);
        bool complete = wave_received_mask == (1U << WAVE_SOURCE_COUNT) - 1U;
        wave_set_status(complete ? "Capture complete - 7 channels received" :
                        "Transfer ended with missing channels. Capture again.",
                        lv_color_hex(complete ? 0x247650 : 0xA35B12));
    }
}

bool ui_page_31_get_wave_poll(uint32_t now_ms)
{
    bool timed_out = false;

    if (wave_request_active && wave_time_reached(now_ms, wave_request_deadline)) {
        wave_request_finish(now_ms, true);
        wave_set_status(ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_TIMEOUT),
                        lv_color_hex(0xB63B32));
        timed_out = true;
    } else {
        wave_refresh_request_button(now_ms);
    }

    mode_retry_refresh();
    wave_render_status();
    return timed_out;
}
