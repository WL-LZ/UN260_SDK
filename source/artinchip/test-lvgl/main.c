#include <unistd.h>
#include "lvgl/lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "aic_ui.h"
#include "aic_dec.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_00_boot_anim.h"
#include "un260/lv_system/app_clock.h"
#include "un260/app_service/app_boot_runtime.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/app_serial_runtime.h"
#include "un260/app_service/app_setting_runtime.h"
#include "un260/app_service/app_ui_runtime.h"
#include "un260/device_info/device_info.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_system/backlight_service.h"
#include "aic_ui/perf_stats.h"
#include "un260/lv_core/ui_frame_commit.h"
#include "un260/app_service/app_runtime_wakeup.h"
#include "aic_ui/render_scratch.h"

//-------------------- 主函数 --------------------
int main(void) {
    lv_init();
    lv_img_cache_set_size(IMG_CACHE_NUM);
    aic_dec_create();

    lv_port_disp_init();
    lv_port_indev_init();
    backlight_service_init();
    user_cfg_password_load();
    user_cfg_screenshot_load();
    user_cfg_screen_recording_load();
    user_cfg_performance_monitor_load();
    user_cfg_performance_profile_load();
    user_cfg_gesture_load();
    gesture_service_init();
    device_info_init(UI_VERSION);
    ui_history_data_init();
    ui_manager_switch(UI_PAGE_BOOT );
    perf_stats_init();
    perf_profile_set_enabled(user_cfg_performance_profile_enabled());
    app_ui_runtime_init();
    ui_page_00_boot_anim_create(lv_layer_top());

    if (!app_serial_runtime_start()) {
        return -1;
    }
    uint32_t visual_commit_tick = app_clock_uptime_ms() - LV_DISP_DEF_REFR_PERIOD;
    while (1) {
        uint64_t wake_sequence = app_runtime_wakeup_snapshot();
        uint64_t loop_start_us = app_clock_monotonic_us();
        ui_page_00_boot_anim_poll();
        uint32_t now = app_clock_uptime_ms();
        ui_page_t current_page = ui_manager_get_current_page();
        uint64_t lvgl_start_us;
        uint64_t lvgl_end_us;
        uint64_t loop_end_us;
        uint32_t profile_frame_seq;
        uint32_t lvgl_delay_ms = APP_RUNTIME_MAX_WAIT_MS;
        uint32_t processed_frames;

        profile_frame_seq = perf_profile_frame_sequence();
        lvgl_start_us = app_clock_monotonic_us();
        bool display_ready = lv_port_disp_poll();
        if (display_ready) {
            if ((uint32_t)(now - visual_commit_tick) >= LV_DISP_DEF_REFR_PERIOD) {
                ui_frame_commit_flush();
                visual_commit_tick = now;
            }
            lvgl_delay_ms = lv_timer_handler();
        }
        lvgl_end_us = app_clock_monotonic_us();
        perf_stats_report_lvgl_time_us(
            app_clock_elapsed_us32(lvgl_start_us, lvgl_end_us));
        if (perf_profile_frame_sequence() != profile_frame_seq) {
            perf_profile_report_active_handler_us(
                app_clock_elapsed_us32(lvgl_start_us, lvgl_end_us));
        }
        ui_frame_commit_begin_batch();
        uint64_t command_started_us = app_clock_monotonic_us();
        processed_frames = app_command_runtime_process_frames_budget(2000U);
        perf_profile_report_command_batch(processed_frames, app_clock_elapsed_us32(
            command_started_us, app_clock_monotonic_us()), app_command_runtime_frames_pending());

        /* Frame handlers may start protocol timeouts.  Refresh the loop time
         * afterwards so pollers never compare a newly-created deadline with
         * the older timestamp captured before frame processing. */
        now = app_clock_uptime_ms();
        app_ui_runtime_poll(now);

        app_command_runtime_poll(now);

        app_boot_runtime_poll(
            now, current_page == UI_PAGE_BOOT &&
                 !ui_page_00_boot_anim_is_active());
        ui_frame_commit_end_batch();
        render_scratch_poll(app_clock_uptime_ms());

        current_page = ui_manager_get_current_page();
        perf_profile_set_page_context(
            (uint32_t)current_page, ui_manager_page_name(current_page));

        loop_end_us = app_clock_monotonic_us();
        perf_stats_report_loop_time_us(
            app_clock_elapsed_us32(loop_start_us, loop_end_us));
        perf_profile_poll(app_clock_uptime_ms());
        /* Serial arrival wakes this wait immediately. LVGL/input and the
         * existing application timers impose a hard 10ms idle ceiling. */
        uint32_t elapsed_ms = (uint32_t)((app_clock_monotonic_us() - lvgl_end_us) / 1000U);
        uint32_t wait_ms = lvgl_delay_ms > elapsed_ms ? lvgl_delay_ms - elapsed_ms : 0;
        if (wait_ms > APP_RUNTIME_MAX_WAIT_MS) wait_ms = APP_RUNTIME_MAX_WAIT_MS;
        /* A held display cannot accept this work yet.  Do not let its expired
         * visual deadline turn a retryable display fault into a busy loop. */
        if (display_ready && ui_frame_commit_pending()) {
            uint32_t age = app_clock_uptime_ms() - visual_commit_tick;
            uint32_t visual_wait = age >= LV_DISP_DEF_REFR_PERIOD ? 0 : LV_DISP_DEF_REFR_PERIOD - age;
            if (wait_ms > visual_wait) wait_ms = visual_wait;
        }
        if (!processed_frames && !app_command_runtime_frames_pending())
            app_runtime_wakeup_wait_since(wake_sequence, wait_ms);
    }
    app_setting_runtime_stop();
    app_serial_runtime_stop();

    return 0;
}
