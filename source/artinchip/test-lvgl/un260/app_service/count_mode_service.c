#include "count_mode_service.h"
#include "setting_service.h"
#include "app_add_runtime.h"
#include "app_command_runtime.h"
#include "workspace_service.h"
#include "un260/boot/boot_service.h"
#include "un260/currency/currency_state.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_system/ui_i18n.h"

static enum { IDLE, WAIT_CURRENCY, SEND_MODE, WAIT_MODE } phase;
static uint8_t target_mode;
static bool mode_result_from_currency;
static bool mode_run_started;
static bool mode_known, currency_unknown, mode_seen, currency_seen, boot_checked;

bool count_mode_service_currency_mode(void) { return phase == SEND_MODE || phase == WAIT_MODE; }
bool count_mode_service_confirmed(void) { return mode_known; }
bool count_mode_service_result_may_clear(void) { return !mode_result_from_currency && !mode_run_started && !app_command_runtime_count_start_busy(); }
bool count_mode_service_busy(void)
{
    return phase != IDLE || setting_service_mode_is_pending() || currency_service_switch_pending();
}
static bool operation_busy(void)
{
    return app_command_runtime_count_start_busy() || app_add_runtime_busy();
}
bool count_mode_service_request(uint8_t mode)
{
    char code[4]; currency_state_get_selected_code(code);
    if (!count_mode_allowed(code, mode) || count_mode_service_busy()) return false;
    if (mode_known && machine_state_mode() == mode) return true;
    if (operation_busy()) return false;
    if (!setting_service_request_mode(mode)) return false;
    mode_run_started = false;
    mode_result_from_currency = false;
    return true;
}
static void mode_not_sent(void)
{
    phase = IDLE;
    ui_notice_post_text(UI_NOTICE_ERROR, "settings.mode", UI_N_("Count mode"),
                        UI_N_("Mode request not sent. Retry."));
}
count_currency_request_t count_mode_service_select_currency(uint8_t index, const char code[4])
{
    char selected[4], listed[4];
    if (!code || !currency_state_get_code(index, listed) || strncmp(code, listed, 4) ||
        count_mode_service_busy() || workspace_service_applying()) return COUNT_CURRENCY_REJECTED;
    currency_state_get_selected_code(selected);
    bool same = !strncmp(selected, code, 4);
    if (same && !currency_unknown && mode_known && count_mode_allowed(code, machine_state_mode()))
        return COUNT_CURRENCY_UNCHANGED;
    if (operation_busy()) return COUNT_CURRENCY_REJECTED;
    target_mode = mode_known ? count_mode_resolve(code, machine_state_mode()) : MODE_MDC;
    if (!currency_service_request_switch(index, code)) return COUNT_CURRENCY_REJECTED;
    if (protocol_send(0x03, (const uint8_t *)code, 3) < 0) {
        currency_service_cancel_switch();
        ui_notice_post_text(UI_NOTICE_ERROR, "settings.currency", UI_N_("Currency not sent"),
                            UI_N_("Previous currency retained."));
        return COUNT_CURRENCY_REJECTED;
    }
    phase = WAIT_CURRENCY;
    return COUNT_CURRENCY_PENDING;
}
void count_mode_service_mode_result(bool success, bool timeout)
{
    mode_result_from_currency = phase == WAIT_MODE;
    if (mode_result_from_currency) phase = IDLE;
    if (success) { mode_known = true; mode_seen = true; }
    else if (timeout) mode_known = false;
}
void count_mode_service_mode_sync(void)
{
    mode_known = mode_seen = true;
    mode_result_from_currency = false;
    if (phase == WAIT_MODE) phase = IDLE;
}
void count_mode_service_currency_result(const currency_switch_result_t *result)
{
    if (!result) return;
    if (result->success) { currency_unknown = false; currency_seen = true; }
    else if (result->timeout) currency_unknown = true;
    if (phase == WAIT_CURRENCY)
        phase = result->success && (!mode_known || machine_state_mode() != target_mode) ? SEND_MODE : IDLE;
}
void count_mode_service_currency_sync(void)
{
    currency_unknown = false; currency_seen = true;
    if (phase != IDLE) { phase = IDLE; ui_notice_clear("settings.currency"); }
}
void count_mode_service_count_started(void)
{
    /* A physical run cancels an unsent follow-on; never change mode mid-run. */
    if (setting_service_mode_is_pending()) mode_run_started = true;
    if (phase == WAIT_CURRENCY || phase == SEND_MODE) phase = IDLE;
}
void count_mode_service_poll(void)
{
    if (phase == WAIT_CURRENCY && !currency_service_switch_pending()) phase = IDLE;
    if (phase == WAIT_MODE && !setting_service_mode_is_pending()) phase = IDLE;
    if (phase == SEND_MODE) {
        if (operation_busy() || !setting_service_request_mode(target_mode)) mode_not_sent();
        else { phase = WAIT_MODE; mode_run_started = false; }
    }
    if (boot_checked || !mode_seen || !currency_seen || count_mode_service_busy() || operation_busy() || workspace_service_applying()) return;
    boot_stage_t stage = boot_service_get_stage();
    if (stage != BOOT_STAGE_DONE && stage != BOOT_STAGE_FAIL) return;
    boot_checked = true;
    char code[4]; currency_state_get_selected_code(code);
    if (!count_mode_allowed(code, machine_state_mode()) && !count_mode_service_request(MODE_MDC))
        ui_notice_post_text(UI_NOTICE_WARNING, "settings.mode", UI_N_("Count mode"), UI_N_("Please try again."));
}
