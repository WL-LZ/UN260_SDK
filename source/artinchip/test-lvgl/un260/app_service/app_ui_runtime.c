#include "app_ui_runtime.h"

#include "un260/app_service/app_setting_runtime.h"
#include "un260/diagnostic/diagnostic.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_debug_overlay.h"
#include "un260/lv_core/page_09_cis_cala.h"
#include "un260/lv_core/page_00_boot_anim.h"
#include "un260/lv_core/page_28_get_image.h"
#include "un260/lv_core/page_31_get_wave.h"
#include "un260/lv_components/lv_upgrade_popup.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/lv_system/ui_screenshot.h"
#include "un260/lv_system/ui_screen_recording.h"
#include "un260/lv_system/user_cfg.h"

#include <stdbool.h>

#define APP_UI_UPGRADE_DETECT_INTERVAL_MS 500U

static uint32_t g_upgrade_detect_tick;

void app_ui_runtime_init(void)
{
    lv_debug_overlay_init();
    lv_debug_overlay_set_enabled(user_cfg_performance_monitor_enabled());
}

static void app_ui_runtime_poll_upgrade(uint32_t now_ms)
{
    ui_upgrade_detect_info_t detect_info;
    ui_page_t current_page = ui_manager_get_current_page();

    if (current_page == UI_PAGE_BOOT_ANIM ||
        current_page == UI_PAGE_BOOT ||
        current_page == UI_PAGE_UI_UPGRADE) {
        return;
    }
    if ((now_ms - g_upgrade_detect_tick) < APP_UI_UPGRADE_DETECT_INTERVAL_MS) {
        return;
    }

    g_upgrade_detect_tick = now_ms;
    ui_upgrade_service_detect(&detect_info);
    lv_upgrade_popup_process_detect(&detect_info);
}

void app_ui_runtime_poll(uint32_t now_ms)
{
    bool stream_timed_out = false;

    app_setting_runtime_poll(now_ms);
    if (diagnostic_calibration_poll(now_ms)) {
        cis_calib_ui_refresh();
        show_communication_error_popup();
    }
    if (ui_page_28_get_image_poll(now_ms)) {
        stream_timed_out = true;
    }
    if (ui_page_31_get_wave_poll(now_ms)) {
        stream_timed_out = true;
    }
    if (stream_timed_out) {
        show_communication_error_popup();
    }
    if (!ui_page_00_boot_anim_is_active()) {
        ui_screenshot_indicator_poll();
    }
    ui_screen_recording_indicator_poll();
    ui_count_end_anim_poll();
    app_ui_runtime_poll_upgrade(now_ms);
}
