#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/font/ui_message_font.h"
#include "un260/font/ui_locale_font_bank.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/upgrade_session.h"
#include "actual_upgrade_theme.h"
#define lv_color_hex settings_theme_color_hex
/* Include the complete production component, so real events exercise its
 * private state, card construction, language refresh and lifecycle together. */
#include "un260/lv_components/lv_upgrade_popup.c"
#undef lv_color_hex
#include "upgrade_ack_translations.h"

static unsigned sends, reboots, resets, starts;
static ui_upgrade_service_status_t service;
static app_ui_upgrade_readiness_t readiness = APP_UI_UPGRADE_READY;
uint32_t app_clock_uptime_ms(void) { return lv_tick_get(); }
app_ui_upgrade_readiness_t app_command_runtime_prepare_ui_upgrade(void) { return readiness; }
bool settings_detail_send_command(uint8_t cmd, const uint8_t *data, uint16_t size) {
    assert(cmd == 0x3B && size == 1 && data[0] == 1); ++sends; return true;
}
void ui_upgrade_service_reboot(void) { ++reboots; }
void ui_upgrade_service_reset(void) { assert(!service.running); ++resets; memset(&service, 0, sizeof(service)); }
void ui_upgrade_service_detect(ui_upgrade_detect_info_t *info) { memset(info, 0, sizeof(*info)); }
bool ui_upgrade_service_can_reinstall(void) { return false; }
bool ui_upgrade_service_is_preparing_data(void) { return false; }
ui_upgrade_start_result_t ui_upgrade_service_start(void) {
    ++starts; service.running = true; service.stage = UI_UPGRADE_STAGE_PREPARE;
    return UI_UPGRADE_START_OK;
}
void ui_upgrade_service_poll(ui_upgrade_service_status_t *status) { *status = service; }
void ui_notice_post_text(ui_notice_kind_t kind, const char *key, const char *title, const char *detail) {
    (void)kind; (void)key; (void)title; (void)detail;
}

static lv_color_t pixels[1280 * 400], buffer[1280 * 30];
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *color) {
    for (int y = area->y1; y <= area->y2; ++y)
        memcpy(pixels + y * 1280 + area->x1,
               color + (y - area->y1) * (area->x2 - area->x1 + 1),
               (area->x2 - area->x1 + 1) * sizeof(lv_color_t));
    lv_disp_flush_ready(driver);
}
static void tick(unsigned ms) { while (ms) { unsigned n = ms > 10 ? 10 : ms; lv_tick_inc(n); lv_timer_handler(); ms -= n; } }
static void snapshot(const char *tag) {
    lv_obj_update_layout(lv_layer_top()); lv_obj_invalidate(lv_layer_top()); lv_refr_now(NULL);
    char path[512]; snprintf(path, sizeof(path), "%s/%s.bgra", getenv("UPGRADE_ACK_OUTPUT"), tag);
    FILE *f = fopen(path, "wb"); assert(f); assert(fwrite(pixels, sizeof(lv_color_t), 1280 * 400, f) == 1280 * 400); assert(!fclose(f));
}
static void assert_label_text(lv_obj_t *label, const char *source_text) {
    /* Arabic labels retain LVGL presentation forms, not the source message. */
    lv_obj_t *expected = lv_label_create(lv_layer_top());
    lv_label_set_text(expected, source_text);
    assert(!strcmp(lv_label_get_text(label), lv_label_get_text(expected)));
    lv_obj_del(expected);
}
static void assert_success(const char *description) {
    assert(g_upgrade_popup.state == UPGRADE_POPUP_STATE_SUCCESS);
    assert(lv_obj_has_flag(g_upgrade_popup.success_btn_later, LV_OBJ_FLAG_HIDDEN));
    lv_obj_update_layout(lv_layer_top());
    lv_obj_t *button = g_upgrade_popup.success_btn, *group = g_upgrade_popup.success_content_group;
    assert(lv_obj_get_width(button) == 228 && lv_obj_get_height(button) == 50);
    assert(lv_obj_get_x(button) == 66 && lv_obj_get_y(button) == 56);
    assert(lv_obj_get_width(group) == 360);
    assert_label_text(lv_obj_get_child(button, 0), ui_tr("Confirm"));
    assert_label_text(g_upgrade_popup.success_desc, description);
    assert(lv_obj_get_y(g_upgrade_popup.success_desc) + lv_obj_get_height(g_upgrade_popup.success_desc) <= lv_obj_get_y(button));
    assert(lv_obj_get_width(lv_obj_get_child(button, 0)) <= lv_obj_get_width(button));
    assert(!g_upgrade_popup.reboot_timer && !g_upgrade_popup.reboot_reserved);
}
static void acknowledge(void) {
    unsigned sent = sends, rebooted = reboots, reset = resets;
    lv_obj_t *button = g_upgrade_popup.success_btn;
    lv_event_send(button, LV_EVENT_CLICKED, NULL);
    lv_event_send(button, LV_EVENT_CLICKED, NULL);
    assert(resets == reset + 1 && sends == sent && reboots == rebooted);
    assert(!g_upgrade_popup.reboot_timer && !g_upgrade_popup.reboot_reserved);
    tick(UPGRADE_POPUP_HIDE_TIME_MS + 100);
    assert(!lv_upgrade_popup_is_showing());
    tick(1500); assert(sends == sent && reboots == rebooted);
}
int main(void) {
    lv_init(); ui_lang_init(); ui_message_font_set_provider(ui_locale_font_compose);
    static lv_disp_draw_buf_t draw; static lv_disp_drv_t display;
    lv_disp_draw_buf_init(&draw, buffer, NULL, 1280 * 30); lv_disp_drv_init(&display);
    display.hor_res = 1280; display.ver_res = 400; display.draw_buf = &draw; display.flush_cb = flush;
    assert(lv_disp_drv_register(&display));
    size_t tested = 0;
    for (size_t i = 0; i < sizeof(ack_translations) / sizeof(ack_translations[0]); ++i) {
        const ui_locale_t *locale = ui_lang_find(ack_translations[i].tag); assert(locale);
        if (getenv("UPGRADE_ACK_EN_ONLY") && locale->id != LANGUAGE_EN) continue;
        assert(ui_locale_font_prepare(locale->id)); ui_lang_set(locale->id);
        assert(!strcmp(ui_tr("Restart the machine later to use the update."), ack_translations[i].text));
        lv_upgrade_popup_show_result(true, "Backend success detail"); tick(500);
        assert_success(ack_translations[i].text); snapshot(locale->tag);
        lv_upgrade_popup_refresh_text(); assert_success(ack_translations[i].text);
        acknowledge();
        /* Recreated popup and shared success/restart card must restore both layouts. */
        lv_upgrade_popup_show_result(true, NULL); tick(100); assert_success(ack_translations[i].text);
        lv_upgrade_popup_show_restart_required(NULL); tick(500);
        assert(!lv_obj_has_flag(g_upgrade_popup.success_btn_later, LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_get_x(g_upgrade_popup.success_btn) == 132);
        assert_label_text(lv_obj_get_child(g_upgrade_popup.success_btn, 0),
                          ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_BTN_CONFIRM));
        lv_upgrade_popup_refresh_text();
        assert(!lv_obj_has_flag(g_upgrade_popup.success_btn_later, LV_OBJ_FLAG_HIDDEN));
        readiness = APP_UI_UPGRADE_WAIT;
        lv_event_send(g_upgrade_popup.success_btn, LV_EVENT_CLICKED, NULL); tick(150);
        assert(g_upgrade_popup.reboot_reserved && g_upgrade_popup.reboot_timer);
        assert(!sends && !reboots);
        lv_event_send(g_upgrade_popup.success_btn_later, LV_EVENT_CLICKED, NULL);
        lv_event_send(g_upgrade_popup.success_btn, LV_EVENT_CLICKED, NULL);
        assert(!g_upgrade_popup.reboot_reserved && !g_upgrade_popup.reboot_timer);
        tick(400); assert(!lv_upgrade_popup_is_showing() && upgrade_session_owner() == UPGRADE_SESSION_NONE);
        readiness = APP_UI_UPGRADE_READY;
        lv_upgrade_popup_show_restart_required(NULL); tick(100);
        lv_upgrade_popup_show_result(true, NULL); tick(500); assert_success(ack_translations[i].text);
        acknowledge();
        /* A new automatic transfer still guards progress and reaches acknowledgment. */
        upgrade_popup_show_prompt(); tick(500); lv_event_send(g_upgrade_popup.prompt_btn_confirm, LV_EVENT_CLICKED, NULL);
        assert(g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROGRESS && service.running);
        upgrade_popup_hide(); assert(g_upgrade_popup.root && !g_upgrade_popup.prompt_hiding);
        service.running = false; service.finished = true; service.success = true;
        service.stage = UI_UPGRADE_STAGE_SUCCESS; service.progress = 100;
        tick(1000); assert_success(ack_translations[i].text); acknowledge();
        ++tested;
    }
    assert(tested && starts == tested && !sends && !reboots);
    printf("PASS complete production popup on real LVGL: %zu locales, centered single Confirm, refresh/recreate, restart restoration and cancellation, subsequent automatic success, duplicate acknowledgment, zero commands/reboots\n", tested);
    lv_deinit();
}
