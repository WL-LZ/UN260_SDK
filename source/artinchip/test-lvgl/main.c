#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "lvgl/lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "aic_ui.h"
#include "aic_dec.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_00_boot_anim.h"
#include "un260/lv_core/page_08_boot.h"
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
#include "un260/app_service/app_startup_trace.h"
#include "un260/app_service/app_startup_runtime.h"
#include "un260/lv_drivers/startup_devices.h"
#include "un260/lv_drivers/startup_visual.h"
#include "aic_ui/render_scratch.h"

//-------------------- 主函数 --------------------
int main(void) {
    app_startup_trace_mark("main_enter");
#if UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_C
    if (!startup_devices_prepare()) return 1;
#endif
    lv_init();
    lv_img_cache_set_size(IMG_CACHE_NUM);
    aic_dec_create();

    lv_port_disp_init();
    app_startup_trace_mark("display_initialized");
    backlight_service_init();
    app_startup_trace_mark("backlight_initialized");
#if UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_C
    lv_port_indev_init();
    app_startup_trace_mark("input_initialized");
    user_cfg_password_load();
    user_cfg_screenshot_load();
    user_cfg_screen_recording_load();
    user_cfg_performance_monitor_load();
    user_cfg_performance_profile_load();
    user_cfg_gesture_load();
    gesture_service_init();
#endif
    device_info_init(UI_VERSION);
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C
    ui_page_08_curr_defer_next_create();
#endif
    ui_manager_switch(UI_PAGE_BOOT );
    app_startup_trace_mark("selftest_created");
    perf_stats_init();
#if UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_C
    perf_profile_set_enabled(user_cfg_performance_profile_enabled());
    app_ui_runtime_init();
#endif
    ui_page_00_boot_anim_create(lv_layer_top());
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C
    if (!ui_page_00_boot_anim_is_active())
        while (!ui_page_08_curr_prepare_step()) { }
#endif
    app_startup_trace_mark("intro_assets_ready");

#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C
    /* A single renderer owns the display throughout startup. No preferences,
     * serial state or business callbacks are consumed until the I/O worker
     * publishes readiness; even input registration waits for that boundary. */
    ui_page_00_boot_anim_set_startup_ready(false);
    bool startup_started = false, startup_ready = false, startup_input_ready = false;
    bool startup_error_reported = false;
#else
    if (!app_serial_runtime_start()) {
        app_startup_trace_mark("serial_failed");
        return -1;
    }
    app_startup_trace_mark("serial_ready");
    ui_history_data_init_async();
    app_startup_trace_mark("history_load_started");
#endif
    bool first_frame_presented = false;
    bool boot_frames_active = ui_page_00_boot_anim_is_active() && app_startup_frames_begin();
    if (boot_frames_active)
        lv_port_disp_set_present_observer(app_startup_frames_present);
    uint32_t early_elapsed = 0;
    int early_visual = startup_visual_acquire(&early_elapsed);
    if (early_visual < 0) {
        fprintf(stderr,"Startup display ownership unavailable; refusing concurrent draw\n");
        return 1;
    }
    if (getenv("UN260_BOOT_LIGHT_ACTIVE")) {
        bool adopted_scanout = lv_port_disp_adopt_scanout();
        if (early_visual > 0 && !adopted_scanout) return 1;
    }
    if (early_visual > 0) {
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C
        ui_page_00_boot_anim_adopt_elapsed(early_elapsed);
#endif
        app_startup_trace_mark("early_visual_adopted");
    }
    bool history_load_reported = false;
    uint32_t visual_commit_tick = app_clock_uptime_ms() - LV_DISP_DEF_REFR_PERIOD;
    while (1) {
        uint64_t wake_sequence = app_runtime_wakeup_snapshot();
        uint64_t loop_start_us = app_clock_monotonic_us();
        ui_page_00_boot_anim_poll();
        if (boot_frames_active && !ui_page_00_boot_anim_is_active()) {
            lv_port_disp_set_present_observer(NULL);
            app_startup_frames_finish();
            boot_frames_active = false;
        }
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
        if (!first_frame_presented && fbdev_present_sequence() != 0U) {
            first_frame_presented = true;
            app_startup_trace_mark("first_frame_presented");
        }
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C
        if (!startup_started && first_frame_presented) {
            startup_started = true;
            (void)app_startup_runtime_begin(app_clock_uptime_ms());
            app_startup_trace_mark("startup_io_started");
        }
        if (startup_started && !startup_ready) {
            app_startup_status_t status = app_startup_runtime_poll(app_clock_uptime_ms());
            if (app_startup_runtime_is_settled() && !startup_input_ready) {
                lv_port_indev_init();
                gesture_service_init();
                perf_profile_set_enabled(user_cfg_performance_profile_enabled());
                app_ui_runtime_init();
                startup_input_ready = true;
                app_startup_trace_mark("input_initialized");
            }
            if (status == APP_STARTUP_READY) {
                startup_ready = true;
                ui_page_00_boot_anim_set_startup_ready(true);
                app_startup_trace_mark("startup_ready");
            } else if (status == APP_STARTUP_FAILED || status == APP_STARTUP_TIMED_OUT) {
                if (!startup_error_reported) {
                    fprintf(stderr, "Startup initialization %s; restart required\n",
                            status == APP_STARTUP_TIMED_OUT ? "timed out" : "failed");
                    app_startup_trace_mark("startup_failed");
                    startup_error_reported = true;
                }
                ui_page_00_boot_anim_set_startup_error(startup_input_ready);
            }
        }
        /* Preferences and UART must be published before any consumer runs.
         * Once they are ready, keep draining unrelated replies while history
         * loads; the existing counting preflight retains record backpressure. */
        if (!app_startup_runtime_can_process()) {
            uint32_t delay_ms = lvgl_delay_ms > 5U ? 5U : lvgl_delay_ms;
            if (delay_ms == 0U) delay_ms = 1U;
            usleep(delay_ms * 1000U);
            continue;
        }
#else
        ui_history_data_init_poll();
#endif
        if (!history_load_reported && ui_history_data_is_initialized()) {
            history_load_reported = true;
            app_startup_trace_mark("history_load_finished");
        }
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
                 !ui_page_00_boot_anim_is_active() &&
                 ui_history_data_is_initialized());
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
