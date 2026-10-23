#include "app_add_runtime.h"

#include "app_command_runtime.h"
#include "setting_service.h"
#include "un260/counting/counting_action_service.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/machine_state/machine_state.h"

#include <stdint.h>

#define APP_ADD_CLEAR_ADMISSION_TIMEOUT_MS 2000U
#define APP_ADD_COMMIT_RETRY_MS 100U

typedef enum {
    APP_ADD_IDLE = 0,
    APP_ADD_SETTING,
    APP_ADD_WAIT_TO_CLEAR,
    APP_ADD_WAIT_CLEAR,
    APP_ADD_RETRY_CLEAR,
    APP_ADD_COMMIT,
} app_add_phase_t;

static app_add_phase_t phase;
static bool setting_target;
static bool setting_interrupted;
static uint32_t clear_wait_started_ms;
static bool commit_retry_waiting;
static uint32_t commit_retry_tick;

static void warning(const char *title, const char *detail)
{
    ui_notice_post_text(UI_NOTICE_WARNING, "count.add-reset", title, detail);
}

static void waiting_for_clear(void)
{
    ui_notice_post_text(UI_NOTICE_PROGRESS, "count.add-reset", UI_N_("Clear"),
                        UI_N_("Please wait"));
}

static void finish(void)
{
    phase = APP_ADD_IDLE;
    setting_target = false;
    setting_interrupted = false;
    clear_wait_started_ms = 0;
    commit_retry_waiting = false;
    commit_retry_tick = 0;
    ui_notice_clear("count.add-reset");
}

static bool send_clear(void)
{
    /* A previous ordinary CLEAR or interrupted attempt cannot complete this
     * new transaction. Request timing still belongs to the wire service. */
    (void)counting_action_take_clear_result();
    phase = APP_ADD_WAIT_CLEAR;
    waiting_for_clear();
    if (!counting_action_request_clear()) {
        phase = APP_ADD_RETRY_CLEAR;
        warning(UI_N_("Clear"),
                UI_N_("The command could not be sent. Please try again."));
        return false;
    }
    return true;
}

bool app_add_runtime_request(bool target)
{
    if (phase == APP_ADD_RETRY_CLEAR) return app_add_runtime_retry_clear();
    if (phase != APP_ADD_IDLE) return false;
    if (target == machine_state_add_enabled()) return true;
    if (!app_command_runtime_prepare_add_reset()) {
        warning(UI_N_("ADD"),
                UI_N_("Finish the current operation, then try again."));
        return false;
    }
    setting_target = target;
    setting_interrupted = false;
    phase = APP_ADD_SETTING;
    ui_notice_clear("count.add-reset");
    if (!setting_service_request_add(target)) {
        finish();
        warning(UI_N_("ADD"),
                UI_N_("The command could not be sent. Please try again."));
        return false;
    }
    return true;
}

bool app_add_runtime_busy(void)
{
    return phase != APP_ADD_IDLE;
}

bool app_add_runtime_waiting_setting(void)
{
    return phase == APP_ADD_SETTING;
}

bool app_add_runtime_retry_clear(void)
{
    if (phase != APP_ADD_RETRY_CLEAR) return false;
    if (!app_command_runtime_add_clear_allowed()) {
        warning(UI_N_("Clear"),
                UI_N_("Finish the current operation, then try again."));
        return false;
    }
    return send_clear();
}

void app_add_runtime_setting_result(bool target, bool success)
{
    if (phase != APP_ADD_SETTING || (success && target != setting_target)) return;
    if (!success) {
        finish();
        warning(UI_N_("ADD"), UI_N_("The controller rejected the request. Check the machine before retrying."));
        return;
    }
    /* The caller has already published the confirmed ADD state. A physical
     * START during the setting exchange makes automatic clearing unsafe. */
    if (setting_interrupted) {
        phase = APP_ADD_RETRY_CLEAR;
        warning(UI_N_("Clear"),
                UI_N_("Finish the current operation, then try again."));
        return;
    }
    phase = APP_ADD_WAIT_TO_CLEAR;
    clear_wait_started_ms = app_clock_uptime_ms();
    waiting_for_clear();
    app_add_runtime_poll();
}

void app_add_runtime_setting_timeout(void)
{
    if (phase != APP_ADD_SETTING) return;
    finish();
    warning(UI_N_("ADD"), UI_N_("No response received. Check device status."));
}

bool app_add_runtime_before_count_start(void)
{
    if (phase == APP_ADD_COMMIT) return false;
    if (phase == APP_ADD_SETTING) {
        setting_interrupted = true;
    } else if (phase == APP_ADD_WAIT_TO_CLEAR || phase == APP_ADD_WAIT_CLEAR) {
        phase = APP_ADD_RETRY_CLEAR;
        warning(UI_N_("Clear"),
                UI_N_("Finish the current operation, then try again."));
    }
    return true;
}

void app_add_runtime_poll(void)
{
    if (phase == APP_ADD_SETTING && !setting_service_add_is_pending()) {
        finish();
        warning(UI_N_("ADD"), UI_N_("Please try again."));
        return;
    }
    if (phase == APP_ADD_WAIT_TO_CLEAR) {
        if ((uint32_t)(app_clock_uptime_ms() - clear_wait_started_ms) >=
                APP_ADD_CLEAR_ADMISSION_TIMEOUT_MS) {
            phase = APP_ADD_RETRY_CLEAR;
            warning(UI_N_("Clear"),
                    UI_N_("Finish the current operation, then try again."));
        } else if (app_command_runtime_add_clear_allowed()) {
            (void)send_clear();
        }
        return;
    }
    if (phase == APP_ADD_WAIT_CLEAR) {
        counting_clear_result_t result = counting_action_take_clear_result();
        if (result == COUNTING_CLEAR_RESULT_SUCCESS) {
            phase = APP_ADD_COMMIT;
            commit_retry_waiting = false;
        } else if (result == COUNTING_CLEAR_RESULT_FAILED) {
            phase = APP_ADD_RETRY_CLEAR;
            warning(UI_N_("Clear"), UI_N_("The controller rejected the request. Check the machine before retrying."));
        } else if (result == COUNTING_CLEAR_RESULT_TIMEOUT) {
            phase = APP_ADD_RETRY_CLEAR;
            warning(UI_N_("Clear"), UI_N_("No response received. Check device status."));
        } else if (!counting_action_clear_pending()) {
            phase = APP_ADD_RETRY_CLEAR;
            warning(UI_N_("Clear"), UI_N_("Please try again."));
        }
    }
    if (phase == APP_ADD_COMMIT) {
        const uint32_t now = app_clock_uptime_ms();
        if (commit_retry_waiting &&
            (uint32_t)(now - commit_retry_tick) < APP_ADD_COMMIT_RETRY_MS) return;
        if (app_command_runtime_commit_add_reset()) finish();
        else {
            commit_retry_waiting = true;
            commit_retry_tick = now;
        }
    }
}
