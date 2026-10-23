#include "ui_factory_reset_service.h"
#include "setting_service.h"
#include "work_mode_service.h"
#include "workspace_service.h"
#include "app_command_runtime.h"
#include "app_add_runtime.h"
#include "upgrade_session.h"
#include "touch_calibration_service.h"
#include "un260/counting/counting_history_service.h"
#include "un260/machine_state/machine_state.h"
#include "un260/storage/ui_factory_reset_store.h"
#include "un260/storage/storage_worker.h"
#include "un260/storage/work_mode_store.h"
#include "un260/storage/workspace_store.h"
#include "un260/storage/cashbook_store.h"
#include "un260/storage/standby_store.h"
#include "un260/lv_core/page_30_set_factory.h"
#include "un260/lv_system/ui_i18n.h"
#include <stddef.h>

static ui_factory_reset_phase_t phase;
static bool local_job_started;

ui_factory_reset_phase_t ui_factory_reset_get_phase(void) { return phase; }
bool ui_factory_reset_blocks_navigation(void) { return phase != UI_FACTORY_RESET_IDLE; }
static bool independent_storage_busy(void)
{
    return workspace_store_busy() || cashbook_store_busy() || standby_store_busy() ||
           touch_calibration_storage_busy();
}

const char *ui_factory_reset_blocker(void)
{
    if (phase != UI_FACTORY_RESET_IDLE) return UI_N_("Reset is in progress. Keep power connected.");
    if (app_add_runtime_busy() || app_command_runtime_count_start_busy() ||
        app_command_runtime_result_pending())
        return UI_N_("Finish counting and receiving results before resetting.");
    if (machine_state_aging_running() || work_mode_service_operation_active())
        return UI_N_("Stop the running test before resetting.");
    if (touch_calibration_busy()) return UI_N_("Finish or cancel touch calibration before resetting.");
    if (workspace_service_settings_busy()) return UI_N_("Wait for the profile to finish applying.");
    if (upgrade_session_owner() != UPGRADE_SESSION_NONE) return UI_N_("Wait for the update to finish.");
    work_mode_snapshot_t mode;
    work_mode_service_get_snapshot(&mode);
    if (mode.pending) return UI_N_("Waiting for controller mode");
    /* A failed/deferred restore is not an active command. Reset supersedes it. */
    if (independent_storage_busy() || work_mode_store_busy() ||
        storage_worker_pending_status() != STORAGE_JOB_SUCCEEDED || !counting_history_can_start())
        return UI_N_("Wait for local settings and records to finish saving.");
    return NULL;
}

bool ui_factory_reset_request(void)
{
    if (ui_factory_reset_blocker() || !setting_service_request_factory_reset()) return false;
    work_mode_service_set_reset_pending(true);
    phase = UI_FACTORY_RESET_WAIT_CONTROLLER;
    return true;
}

void ui_factory_reset_on_controller_result(bool success)
{
    if (phase != UI_FACTORY_RESET_WAIT_CONTROLLER) return;
    if (!success) { work_mode_service_set_reset_pending(false); phase = UI_FACTORY_RESET_IDLE; return; }
    /* A saved pre-reset mode must never be published again after 0x44 ACK.
     * Already accepted saves drain before the reset in the shared FIFO. */
    work_mode_service_stop();
    local_job_started = false;
    phase = UI_FACTORY_RESET_SAVING;
}

void ui_factory_reset_on_controller_timeout(void)
{
    if (phase == UI_FACTORY_RESET_WAIT_CONTROLLER) {
        work_mode_service_set_reset_pending(false);
        phase = UI_FACTORY_RESET_IDLE;
    }
}

bool ui_factory_reset_retry_local(void)
{
    if (phase != UI_FACTORY_RESET_LOCAL_FAILED) return false;
    local_job_started = false;
    phase = UI_FACTORY_RESET_SAVING;
    return true;
}

void ui_factory_reset_poll(void)
{
    if (phase != UI_FACTORY_RESET_SAVING) return;
    if (!local_job_started) {
        if (independent_storage_busy()) return;
        local_job_started = ui_factory_reset_store_begin();
        if (!local_job_started) {
            phase = UI_FACTORY_RESET_LOCAL_FAILED;
            ui_page_30_set_factory_on_local_result(false);
            return;
        }
    }
    bool success;
    if (!ui_factory_reset_store_poll(&success)) return;
    local_job_started = false;
    phase = success ? UI_FACTORY_RESET_RESTART_REQUIRED : UI_FACTORY_RESET_LOCAL_FAILED;
    ui_page_30_set_factory_on_local_result(success);
}
