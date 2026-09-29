#include "app_ui_runtime.h"
#include "app_setting_notice.h"
#include "app_fault_recovery.h"
#include "app_boot_runtime.h"
#include "un260/lv_components/ui_notice.h"
#include "app_auto_qr.h"
#include "app_standby_runtime.h"
#include "un260/storage/standby_store.h"

#include "un260/app_service/app_setting_runtime.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/app_service/workspace_service.h"
#include "un260/storage/workspace_store.h"
#include "un260/storage/cashbook_store.h"
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
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_components/lv_fault_popup.h"

#include <stdbool.h>

#define APP_UI_UPGRADE_DETECT_INTERVAL_MS 500U

static uint32_t g_upgrade_detect_tick;
static uint32_t g_language_generation;

/* User-requested work only: startup loads and background maintenance are not
 * registered. Storage accepts one job per service, so its busy -> idle edge
 * belongs to the accepted request until this UI-thread observer consumes it. */
static struct {
    bool workspace, records, profile;
    app_ui_notice_operation_t workspace_operation;
} g_operation_notice;

static const char *operation_key(app_ui_notice_operation_t operation)
{
    switch (operation) {
    case APP_UI_NOTICE_WORKSPACE_STORE: return "workspace.store";
    case APP_UI_NOTICE_BATCH_SAVE: return "workspace.batch";
    case APP_UI_NOTICE_RECORD_STORE: return "records.store";
    case APP_UI_NOTICE_PROFILE_APPLY: return "workspace.apply";
    default: return NULL;
    }
}

static const char *operation_title(app_ui_notice_operation_t operation)
{
    switch (operation) {
    case APP_UI_NOTICE_BATCH_SAVE: return UI_N_("Batch cycle");
    case APP_UI_NOTICE_RECORD_STORE: return UI_N_("Records");
    case APP_UI_NOTICE_PROFILE_APPLY: return UI_N_("Counting profile");
    default: return UI_N_("Workspace");
    }
}

void app_ui_runtime_notice_started(app_ui_notice_operation_t operation, const char *detail)
{
    const char *key = operation_key(operation);
    if (!key || !app_setting_notice_page_allowed()) return;
    switch (operation) {
    case APP_UI_NOTICE_WORKSPACE_STORE:
    case APP_UI_NOTICE_BATCH_SAVE:
        if (!workspace_store_busy()) return;
        g_operation_notice.workspace = true;
        g_operation_notice.workspace_operation = operation;
        break;
    case APP_UI_NOTICE_RECORD_STORE:
        if (!cashbook_store_busy()) return;
        g_operation_notice.records = true;
        break;
    case APP_UI_NOTICE_PROFILE_APPLY:
        if (!workspace_service_applying()) return;
        g_operation_notice.profile = true;
        break;
    }
    /* A new accepted request is a new acknowledgement lifetime. */
    ui_notice_clear(key);
    ui_notice_post_text(UI_NOTICE_PROGRESS, key, operation_title(operation), detail);
}

static void app_ui_runtime_poll_operation_notices(void)
{
    if (g_operation_notice.workspace && !workspace_store_busy()) {
        app_ui_notice_operation_t operation = g_operation_notice.workspace_operation;
        bool success = workspace_store_last_success();
        g_operation_notice.workspace = false;
        if (!app_setting_notice_page_allowed()) ui_notice_clear(operation_key(operation));
        else ui_notice_post_message(success ? UI_NOTICE_SUCCESS : UI_NOTICE_ERROR,
            operation_key(operation), operation_title(operation),
            success && operation == APP_UI_NOTICE_BATCH_SAVE ?
                workspace_service_batch_save_message_info() : workspace_store_message_info());
    }
    if (g_operation_notice.records && !cashbook_store_busy()) {
        g_operation_notice.records = false;
        if (!app_setting_notice_page_allowed()) ui_notice_clear("records.store");
        else ui_notice_post_message(cashbook_store_last_success() ? UI_NOTICE_SUCCESS : UI_NOTICE_ERROR,
            "records.store", UI_N_("Records"), cashbook_store_message_info());
    }
    if (g_operation_notice.profile && !workspace_service_applying()) {
        workspace_apply_result_t result = workspace_service_apply_result();
        ui_notice_kind_t kind = result == WORKSPACE_APPLY_SUCCEEDED ? UI_NOTICE_SUCCESS :
            result == WORKSPACE_APPLY_FAILED ? UI_NOTICE_ERROR :
            result == WORKSPACE_APPLY_UNCONFIRMED ? UI_NOTICE_WARNING : UI_NOTICE_INFO;
        g_operation_notice.profile = false;
        if (!app_setting_notice_page_allowed()) ui_notice_clear("workspace.apply");
        else ui_notice_post_message(kind, "workspace.apply", UI_N_("Counting profile"),
            workspace_service_apply_message_info());
    }
}

static bool app_ui_runtime_confirm_fault(machine_fault_key_t key)
{
    if (app_boot_runtime_confirm_fault(key)) return true;
    app_fault_recovery_confirm(key);
    return false;
}

void app_ui_runtime_init(void)
{
    ui_notice_init();
    app_setting_notice_init();
    app_fault_recovery_init();
    fault_popup_set_confirm_handler(app_ui_runtime_confirm_fault);
    g_language_generation = ui_lang_generation();
    work_mode_service_init();
    workspace_service_init();
    cashbook_store_init();
    lv_debug_overlay_init();
    lv_debug_overlay_set_enabled(user_cfg_performance_monitor_enabled());
}

static void app_ui_runtime_poll_upgrade(uint32_t now_ms)
{
    ui_upgrade_detect_info_t detect_info;
    ui_page_t current_page = ui_manager_get_current_page();
    /* Do not offer an upgrade while a photo/config transaction owns storage. */
    if (standby_store_busy() || workspace_store_busy() || cashbook_store_busy()) return;

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
    uint32_t language_generation = ui_lang_generation();
    if (language_generation != g_language_generation) {
        g_language_generation = language_generation;
        ui_notice_language_changed();
        fault_popup_language_changed();
    }
    ui_page_t page = ui_manager_get_current_page();
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_STANDBY, page == UI_PAGE_STANDBY);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_MODAL,
        ui_page_00_boot_anim_is_active() || page == UI_PAGE_BOOT ||
        page == UI_PAGE_BOOT_ANIM || page == UI_PAGE_UI_UPGRADE || lv_upgrade_popup_is_showing());

    app_setting_runtime_poll(now_ms);
    if(cashbook_store_poll()) ui_manager_publish_data_changed(UI_DATA_TOPIC_MACHINE_SETTINGS);
    if (workspace_service_poll(now_ms))
        ui_manager_publish_data_changed(UI_DATA_TOPIC_MACHINE_SETTINGS);
    app_ui_runtime_poll_operation_notices();
    if (diagnostic_calibration_poll(now_ms)) {
        cis_calib_ui_refresh();
    }
    ui_page_28_get_image_poll(now_ms);
    ui_page_31_get_wave_poll(now_ms);
    if (!ui_page_00_boot_anim_is_active()) {
        ui_screenshot_indicator_poll();
    }
    ui_screen_recording_indicator_poll();
    ui_count_end_anim_poll();
    app_auto_qr_poll(now_ms);
    app_ui_runtime_poll_upgrade(now_ms);
    app_standby_runtime_poll(now_ms);
}
