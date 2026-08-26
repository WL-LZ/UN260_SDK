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
#include "aic_ui/perf_stats.h"

//-------------------- 主函数 --------------------
int main(void) {
    lv_init();
    lv_img_cache_set_size(IMG_CACHE_NUM);
    aic_dec_create();

    lv_port_disp_init();
    lv_port_indev_init();
    user_cfg_password_load();
    user_cfg_screenshot_load();
    user_cfg_screen_recording_load();
    user_cfg_performance_monitor_load();
    user_cfg_performance_profile_load();
    user_cfg_gesture_load();
    gesture_service_init();
    device_info_init(UI_VERSION);
    ui_history_data_init();
    ui_manager_switch(UI_PAGE_BOOT);
    perf_stats_init();
    perf_profile_set_enabled(user_cfg_performance_profile_enabled());
    app_ui_runtime_init();
    ui_page_00_boot_anim_create(lv_layer_top());

    if (!app_serial_runtime_start()) {
        return -1;
    }
    while (1) {
        uint64_t loop_start_us = app_clock_monotonic_us();
        uint32_t now = app_clock_uptime_ms();
        ui_page_t current_page = ui_manager_get_current_page();
        uint64_t lvgl_start_us;
        uint64_t lvgl_end_us;
        uint64_t loop_end_us;
        uint32_t profile_frame_seq;

        profile_frame_seq = perf_profile_frame_sequence();
        lvgl_start_us = app_clock_monotonic_us();
        lv_timer_handler();
        lvgl_end_us = app_clock_monotonic_us();
        perf_stats_report_lvgl_time_us(
            app_clock_elapsed_us32(lvgl_start_us, lvgl_end_us));
        if (perf_profile_frame_sequence() != profile_frame_seq) {
            perf_profile_report_active_handler_us(
                app_clock_elapsed_us32(lvgl_start_us, lvgl_end_us));
        }
        app_command_runtime_process_frames();

        /* Frame handlers may start protocol timeouts.  Refresh the loop time
         * afterwards so pollers never compare a newly-created deadline with
         * the older timestamp captured before frame processing. */
        now = app_clock_uptime_ms();
        app_ui_runtime_poll(now);

        app_command_runtime_poll(now);

        app_boot_runtime_poll(
            now, current_page == UI_PAGE_BOOT &&
                 !ui_page_00_boot_anim_is_active());

        current_page = ui_manager_get_current_page();
        perf_profile_set_page_context(
            (uint32_t)current_page, ui_manager_page_name(current_page));

        loop_end_us = app_clock_monotonic_us();
        perf_stats_report_loop_time_us(
            app_clock_elapsed_us32(loop_start_us, loop_end_us));
        perf_profile_poll(app_clock_uptime_ms());
        usleep(1000);
    }
    app_setting_runtime_stop();
    app_serial_runtime_stop();

    return 0;
}
