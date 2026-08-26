#include "un260/lv_components/smart_island/smart_island_internal.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/machine_state/machine_state.h"

#define SMART_ISLAND_RESULT_HOLD_MS 1800U

static void smart_island_result_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    fault_popup_clear_pending();
    fault_popup_reset_auto_retry();
    smart_island_result_stop_timer();
    smart_island_restore_idle();
}

void smart_island_result_stop_timer(void)
{
    if (g_si_ctx.lifecycle.result_timer == NULL) {
        return;
    }

    lv_timer_del(g_si_ctx.lifecycle.result_timer);
    g_si_ctx.lifecycle.result_timer = NULL;
}

void smart_island_notify_count_start(void)
{
    /* 新会话开始前先清掉上一轮残留的结束动画状态。 */
    ui_count_end_anim_cancel();

    g_si_ctx.lifecycle.count_session_active = true;
    g_si_ctx.warning.level = SMART_ISLAND_WARNING_LEVEL_WARNING;
    smart_island_warning_fault_clear();
    smart_island_warning_stop();
    g_si_ctx.text.result[0] = '\0';
    g_si_ctx.text.serial_ticker[0] = '\0';
    smart_island_set_scene(SMART_ISLAND_SCENE_COUNTING, NULL, NULL);
    smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, true);
}

void smart_island_notify_serial_number(int denomination, const char *serial_number)
{
    uint8_t mode = machine_state_mode();

    if (!g_si_ctx.lifecycle.count_session_active ||
        (mode != MODE_MDC && mode != MODE_SDC) ||
        serial_number == NULL || serial_number[0] == '\0') {
        return;
    }

    if (denomination > 0) {
        lv_snprintf(g_si_ctx.text.serial_ticker,
                    sizeof(g_si_ctx.text.serial_ticker),
                    "%d  %s", denomination, serial_number);
    } else {
        lv_snprintf(g_si_ctx.text.serial_ticker,
                    sizeof(g_si_ctx.text.serial_ticker),
                    "%s", serial_number);
    }
    smart_island_refresh_summary();
}

void smart_island_notify_count_end(const char *result_text)
{
    g_si_ctx.lifecycle.count_session_active = false;
    if (result_text && result_text[0] != '\0') {
        lv_snprintf(g_si_ctx.text.result, sizeof(g_si_ctx.text.result), "%s", result_text);
    } else {
        g_si_ctx.text.result[0] = '\0';
    }

    smart_island_set_scene(SMART_ISLAND_SCENE_RESULT, NULL, NULL);
    smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, true);
    smart_island_result_stop_timer();
    g_si_ctx.lifecycle.result_timer = lv_timer_create(
        smart_island_result_timer_cb,
        SMART_ISLAND_RESULT_HOLD_MS,
        NULL);
    if (g_si_ctx.lifecycle.result_timer == NULL) {
        smart_island_restore_idle();
    }
}

void smart_island_set_count_analysis(int valid_pcs, int suspect_pcs, int damaged_pcs)
{
    g_si_ctx.text.analysis_valid_pcs = valid_pcs > 0 ? valid_pcs : 0;
    g_si_ctx.text.analysis_suspect_pcs = suspect_pcs > 0 ? suspect_pcs : 0;
    g_si_ctx.text.analysis_damaged_pcs = damaged_pcs > 0 ? damaged_pcs : 0;
    g_si_ctx.text.analysis_valid = true;
    smart_island_refresh_summary();
}

void smart_island_clear_count_analysis(void)
{
    g_si_ctx.text.analysis_valid = false;
    g_si_ctx.text.analysis_valid_pcs = 0;
    g_si_ctx.text.analysis_suspect_pcs = 0;
    g_si_ctx.text.analysis_damaged_pcs = 0;
}
