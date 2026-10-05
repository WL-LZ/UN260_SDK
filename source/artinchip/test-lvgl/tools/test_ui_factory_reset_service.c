#include <assert.h>
#include <stdio.h>
#include "un260/app_service/ui_factory_reset_service.c"
static bool running, result_pending, aging, diagnostic, settings_busy, touch_busy, touch_storage_busy;
static bool workspace_busy, cashbook_busy, standby_busy, mode_store_busy, history_ready = true;
static bool send_ok = true, submit_ok = true, store_ready, store_success;
static unsigned sends, local_begins, stops, view_results;
static bool view_success;
static work_mode_snapshot_t mode;
static storage_job_status_t lane_status = STORAGE_JOB_SUCCEEDED;
static upgrade_session_owner_t upgrade_owner = UPGRADE_SESSION_NONE;
bool setting_service_request_factory_reset(void) { ++sends; return send_ok; }
bool app_command_runtime_count_start_busy(void) { return running; }
bool app_command_runtime_result_pending(void) { return result_pending; }
bool machine_state_aging_running(void) { return aging; }
bool work_mode_service_diagnostic_active(void) { return diagnostic; }
bool workspace_service_settings_busy(void) { return settings_busy; }
bool workspace_store_busy(void) { return workspace_busy; }
bool cashbook_store_busy(void) { return cashbook_busy; }
bool standby_store_busy(void) { return standby_busy; }
bool touch_calibration_busy(void) { return touch_busy || touch_storage_busy; }
bool touch_calibration_storage_busy(void) { return touch_storage_busy; }
bool work_mode_store_busy(void) { return mode_store_busy; }
void work_mode_service_stop(void) { ++stops; }
void work_mode_service_get_snapshot(work_mode_snapshot_t *snapshot) { *snapshot = mode; }
storage_job_status_t storage_worker_pending_status(void) { return lane_status; }
bool counting_history_can_start(void) { return history_ready; }
upgrade_session_owner_t upgrade_session_owner(void) { return upgrade_owner; }
bool ui_factory_reset_store_begin(void) { ++local_begins; return submit_ok; }
bool ui_factory_reset_store_poll(bool *success)
{ if (!store_ready) return false; *success = store_success; store_ready = false; return true; }
void ui_page_30_set_factory_on_local_result(bool success) { ++view_results; view_success = success; }
int main(void)
{
    assert(ui_factory_reset_get_phase() == UI_FACTORY_RESET_IDLE && !ui_factory_reset_blocks_navigation());
    ui_factory_reset_on_controller_result(true); ui_factory_reset_poll();
    assert(local_begins == 0 && stops == 0); /* No unsolicited/duplicate ACK. */
    bool *gates[] = {&running, &result_pending, &aging, &diagnostic, &settings_busy, &touch_busy,
        &workspace_busy, &cashbook_busy, &standby_busy, &touch_storage_busy, &mode_store_busy};
    for (unsigned i = 0; i < sizeof(gates) / sizeof(gates[0]); ++i) {
        *gates[i] = true; assert(!ui_factory_reset_request() && ui_factory_reset_blocker()); *gates[i] = false;
    }
    lane_status = STORAGE_JOB_PENDING; assert(!ui_factory_reset_request());
    lane_status = STORAGE_JOB_FAILED; assert(!ui_factory_reset_request());
    lane_status = STORAGE_JOB_SUCCEEDED;
    history_ready = false; assert(!ui_factory_reset_request()); history_ready = true;
    mode.pending = true; assert(!ui_factory_reset_request()); mode.pending = false;
    mode.restore_pending = true; assert(!ui_factory_reset_request()); mode.restore_pending = false;
    upgrade_owner = UPGRADE_SESSION_UI; assert(!ui_factory_reset_request());
    upgrade_owner = UPGRADE_SESSION_CONTROLLER; assert(!ui_factory_reset_request());
    upgrade_owner = UPGRADE_SESSION_NONE;
    assert(sends == 0 && local_begins == 0);
    send_ok = false; assert(!ui_factory_reset_request() && !ui_factory_reset_blocks_navigation()); send_ok = true;
    assert(ui_factory_reset_request() && ui_factory_reset_blocks_navigation());
    assert(!ui_factory_reset_request()); ui_factory_reset_on_controller_result(false); ui_factory_reset_poll();
    assert(!ui_factory_reset_blocks_navigation() && local_begins == 0 && stops == 0);
    assert(ui_factory_reset_request()); ui_factory_reset_on_controller_timeout(); ui_factory_reset_poll();
    assert(!ui_factory_reset_blocks_navigation() && local_begins == 0);
    ui_factory_reset_on_controller_result(true); assert(local_begins == 0); /* Late timeout ACK ignored. */
    assert(ui_factory_reset_request()); unsigned confirmed_sends = sends;
    ui_factory_reset_on_controller_result(true);
    assert(stops == 1 && ui_factory_reset_get_phase() == UI_FACTORY_RESET_SAVING);
    ui_factory_reset_on_controller_result(true); assert(stops == 1);
    touch_storage_busy = true; ui_factory_reset_poll(); assert(local_begins == 0); touch_storage_busy = false;
    submit_ok = false; ui_factory_reset_poll(); assert(view_results == 1 && !view_success);
    assert(ui_factory_reset_get_phase() == UI_FACTORY_RESET_LOCAL_FAILED && ui_factory_reset_blocks_navigation());
    assert(ui_factory_reset_retry_local()); submit_ok = true; ui_factory_reset_poll();
    assert(local_begins == 2 && view_results == 1);
    store_ready = true; store_success = false; ui_factory_reset_poll();
    assert(ui_factory_reset_get_phase() == UI_FACTORY_RESET_LOCAL_FAILED && view_results == 2);
    assert(ui_factory_reset_retry_local()); ui_factory_reset_poll();
    store_ready = store_success = true; ui_factory_reset_poll();
    assert(ui_factory_reset_get_phase() == UI_FACTORY_RESET_RESTART_REQUIRED && ui_factory_reset_blocks_navigation());
    assert(view_results == 3 && view_success && sends == confirmed_sends && stops == 1);
    assert(!ui_factory_reset_retry_local() && !ui_factory_reset_request());
    ui_factory_reset_on_controller_timeout(); ui_factory_reset_poll();
    assert(ui_factory_reset_get_phase() == UI_FACTORY_RESET_RESTART_REQUIRED && view_results == 3);
    puts("PASS factory reset request/storage/count/upgrade/touch guards, matched-success-only local mutation, failed local retry without resending controller reset, and restart lifetime");
}
