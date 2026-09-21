#include "page_12_sensor.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "settings_detail_ui.h"
#include "lv_page_manager.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/app_clock.h"
#include "un260/diagnostic/diagnostic.h"
#include <string.h>

#define SENSOR_QUERY_PERIOD_MS 300

static struct {
    lv_settings_frame_t frame;
    lv_obj_t *values[SENSOR_VOLTAGE_CH_NUM];
    lv_obj_t *received;
    lv_timer_t *timer;
    uint32_t last_update, last_received_ms;
    bool initialized, query_failed;
} sensor_page;
static const char *sensor_names[SENSOR_VOLTAGE_CH_NUM] = {
    "QTH", "QTL", "RJH", "RJL", "PS1L", "PS1R", "PS2", "PS5L", "PS5R", "ST", "SD"
};
static void sensor_esc_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) ui_manager_pop_page();
}
static void sensor_refresh_view(void)
{
    sensor_voltage_snapshot_t snapshot;
    sensor_state_get_snapshot(&snapshot);
    uint32_t now = app_clock_uptime_ms();
    if (!sensor_page.initialized || sensor_page.last_update != snapshot.update_count) {
        bool changed = sensor_page.last_update != snapshot.update_count;
        sensor_page.last_update = snapshot.update_count;
        if (sensor_page.initialized && changed) sensor_page.last_received_ms = now;
        unsigned count = 0;
        for (int i = 0; i < SENSOR_VOLTAGE_CH_NUM; i++) {
            if (snapshot.valid[i]) {
                count++;
                unsigned millivolts = ((unsigned)snapshot.raw[i] * 3300U + 128U) / 256U;
                lv_label_set_text_fmt(sensor_page.values[i], "%u.%03u V",
                                      millivolts / 1000U, millivolts % 1000U);
            } else {
                lv_label_set_text(sensor_page.values[i], "Waiting");
            }
            lv_obj_set_style_text_color(sensor_page.values[i],
                                         lv_color_hex(snapshot.valid[i] ? 0x1D2B34 : 0x586B78), 0);
        }
        lv_label_set_text_fmt(sensor_page.received, "%u / %u channels", count, SENSOR_VOLTAGE_CH_NUM);
        sensor_page.initialized = true;
    }
    const char *message = sensor_page.query_failed ? "Could not send query. Retrying the controller connection." :
        !sensor_page.last_received_ms ? "Waiting for sensor readings from the controller." :
        now - sensor_page.last_received_ms > 2000 ?
        "No recent response. Readings shown are the last received values." :
        "Live readings. Values are not a pass / fail assessment.";
    /* Receiving voltages is read-only and independent of the RUN mode lease. */
    if (strcmp(lv_label_get_text(sensor_page.frame.message),message))
        lv_label_set_text(sensor_page.frame.message,message);
}
static void sensor_poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const uint8_t query[] = {1, 1};
    sensor_page.query_failed = !settings_detail_send_command(0x1D, query, sizeof(query));
    sensor_refresh_view();
}
void ui_page_12_sensor_create(lv_obj_t *parent)
{
    if (sensor_page.frame.root) return;
    lv_settings_header_t header = {"Sensors", "Maintenance / Live voltage", "Wrench", sensor_esc_cb, NULL};
    sensor_page.frame = lv_settings_frame_create(parent, &header);
    settings_detail_add_run(sensor_page.frame.root);
    lv_obj_set_style_bg_opa(sensor_page.frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sensor_page.frame.body, 0, 0);
    /* Six columns keep all eleven readings visible without tiny controls. */
    for (int i = 0; i < SENSOR_VOLTAGE_CH_NUM; i++) {
        int x = (i % 6) * 207, y = (i / 6) * 124;
        lv_obj_t *card = lv_settings_box(sensor_page.frame.body, x, y, 197, 114, 0xFFFFFF);
        lv_obj_set_style_radius(card, 14, 0);
        lv_settings_label(card, sensor_names[i], 18, 17,
                           &lv_font_instrument_sans_medium_16, 0x586B78);
        sensor_page.values[i] = lv_settings_label(card, "Waiting", 18, 57,
                           &lv_font_instrument_sans_medium_24, 0x1D2B34);
    }
    lv_obj_t *summary = lv_settings_box(sensor_page.frame.body, 1035, 124, 197, 114, 0xF1F4F5);
    lv_obj_set_style_radius(summary, 14, 0);
    lv_settings_label(summary, "RECEIVING", 18, 17, &lv_font_instrument_sans_medium_14, 0x586B78);
    sensor_page.received = lv_settings_label(summary, "0 / 11 channels", 18, 57,
                                              &lv_font_instrument_sans_medium_18, 0x1D2B34);
    lv_obj_set_width(sensor_page.frame.message,1030);
    sensor_page.last_update = 0;
    sensor_page.last_received_ms = 0;
    sensor_refresh_view();
    sensor_poll_timer_cb(NULL);
    sensor_page.timer = lv_timer_create(sensor_poll_timer_cb, SENSOR_QUERY_PERIOD_MS, NULL);
}
void ui_page_12_sensor_destroy(void)
{
    if (sensor_page.timer) lv_timer_del(sensor_page.timer);
    if (sensor_page.frame.root) lv_obj_del(sensor_page.frame.root);
    memset(&sensor_page, 0, sizeof(sensor_page));
}
