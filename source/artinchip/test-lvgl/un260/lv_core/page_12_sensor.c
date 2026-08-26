#include "page_12_sensor.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/ui_text.h"
#include "un260/diagnostic/diagnostic.h"

#include <stdbool.h>
#include <string.h>

#define SENSOR_SCALE_Y_DEFAULT 330
#define SENSOR_QUERY_PERIOD_MS 300

typedef struct {
    lv_obj_t *page;
    lv_obj_t *value_labels[SENSOR_VOLTAGE_CH_NUM];
    lv_timer_t *poll_timer;
    uint32_t last_update_count;
    bool values_initialized;
    bool displayed_valid[SENSOR_VOLTAGE_CH_NUM];
    uint8_t displayed_raw[SENSOR_VOLTAGE_CH_NUM];
} sensor_page_context_t;

static sensor_page_context_t g_sensor_page;

static void sensor_page_context_reset(void)
{
    memset(&g_sensor_page, 0, sizeof(g_sensor_page));
}

static const char* sensor_names[SENSOR_VOLTAGE_CH_NUM] = {
    "QTH", "QTL", "RJH", "RJL", "PS1L", "PS1R", "PS2", "PS5L", "PS5R", "ST", "SD"
};

static float sensor_raw_to_volt(uint8_t raw)
{
    return ((float)raw / 256.0f) * ((float)SENSOR_SCALE_Y_DEFAULT / 100.0f);
}

static void sensor_send_query(void)
{
    const uint8_t req[2] = { 0x01, 0x01 };
    settings_detail_send_command(0x1D, req, 2);
}

static void sensor_refresh_view(void)
{
    sensor_voltage_snapshot_t snapshot;

    sensor_state_get_snapshot(&snapshot);
    if (g_sensor_page.last_update_count == snapshot.update_count) return;
    g_sensor_page.last_update_count = snapshot.update_count;

    for (int i = 0; i < SENSOR_VOLTAGE_CH_NUM; i++) {
        lv_obj_t *value_label = g_sensor_page.value_labels[i];

        if (!value_label || !lv_obj_is_valid(value_label)) continue;
        if (g_sensor_page.values_initialized &&
            g_sensor_page.displayed_valid[i] == snapshot.valid[i] &&
            (!snapshot.valid[i] ||
             g_sensor_page.displayed_raw[i] == snapshot.raw[i])) {
            continue;
        }

        if (snapshot.valid[i]) {
            const uint8_t raw = snapshot.raw[i];
            const float v = sensor_raw_to_volt(raw);
            lv_label_set_text_fmt(value_label, "%.3f V", v);
            lv_obj_set_style_text_color(value_label, lv_color_hex(0x1C8E4D), 0);
        } else {
            lv_label_set_text(value_label, "-- V");
            lv_obj_set_style_text_color(value_label, lv_color_hex(0xdee2de), 0);
        }
        g_sensor_page.displayed_valid[i] = snapshot.valid[i];
        g_sensor_page.displayed_raw[i] = snapshot.raw[i];
    }
    g_sensor_page.values_initialized = true;
}

static void sensor_poll_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    sensor_send_query();
    sensor_refresh_view();
}

static void sensor_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

void ui_page_12_sensor_create(lv_obj_t* parent)
{
    if (g_sensor_page.page && lv_obj_is_valid(g_sensor_page.page)) return;

    ui_page_12_sensor_destroy();

    lv_obj_t* content = NULL;
    g_sensor_page.page = settings_detail_create_page(
        parent, ui_text_get(UI_TEXT_SETTINGS_SENSOR_VOLTAGE),
        sensor_esc_cb, &content);

    lv_obj_t* grid = lv_obj_create(content);
    lv_obj_set_pos(grid, 20, 20);
    lv_obj_set_size(grid, 1240, 300);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(grid, 6, 0);
    lv_obj_set_style_pad_gap(grid, 10, 0);

    for (int i = 0; i < SENSOR_VOLTAGE_CH_NUM; i++) {
        lv_obj_t* card = lv_obj_create(grid);
        lv_obj_set_size(card, 236, 84);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xD6E6F5), 0);
        lv_obj_set_style_pad_all(card, 10, 0);

        lv_obj_t* name = lv_label_create(card);
        lv_label_set_text(name, sensor_names[i]);
        lv_obj_set_style_text_font(name, &lv_font_instrument_sans_medium_16, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(0x355779), 0);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 0);

        g_sensor_page.value_labels[i] = lv_label_create(card);
        lv_label_set_text(g_sensor_page.value_labels[i], "-- V");
        lv_obj_set_style_text_font(g_sensor_page.value_labels[i],
                                   &lv_font_instrument_sans_medium_20, 0);
        lv_obj_set_style_text_color(g_sensor_page.value_labels[i],
                                    lv_color_hex(0x8B9AAF), 0);
        lv_obj_align(g_sensor_page.value_labels[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }

    sensor_refresh_view();
    sensor_send_query();
    g_sensor_page.poll_timer = lv_timer_create(sensor_poll_timer_cb,
                                               SENSOR_QUERY_PERIOD_MS, NULL);
}

void ui_page_12_sensor_destroy(void)
{
    if (g_sensor_page.poll_timer) {
        lv_timer_del(g_sensor_page.poll_timer);
        g_sensor_page.poll_timer = NULL;
    }

    if (g_sensor_page.page && lv_obj_is_valid(g_sensor_page.page)) {
        lv_obj_del(g_sensor_page.page);
    }

    sensor_page_context_reset();
}
