#include "un260/lv_components/smart_island/smart_island_internal.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/counting/counting_data_store.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/machine_state/machine_state.h"

#define SMART_ISLAND_RESULT_HOLD_MS 1200U
#define SMART_ISLAND_RESULT_SWITCH_MS 140U

static void smart_island_result_present(void);

static void smart_island_result_source_opa_cb(void *var, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)value, 0);
}

static void smart_island_result_cancel_transition(void)
{
    if (!g_si_ctx.lifecycle.result_transition_pending) return;
    if (g_si_ctx.objects.counting_root &&
        lv_obj_is_valid(g_si_ctx.objects.counting_root)) {
        lv_anim_del(g_si_ctx.objects.counting_root,
                    smart_island_result_source_opa_cb);
        lv_obj_set_style_opa(g_si_ctx.objects.counting_root, LV_OPA_COVER, 0);
    }
    g_si_ctx.lifecycle.result_transition_pending = false;
}

static void smart_island_result_source_fade_finish_cb(lv_anim_t *animation)
{
    lv_obj_t *counting_root = g_si_ctx.objects.counting_root;

    LV_UNUSED(animation);
    if (counting_root && lv_obj_is_valid(counting_root)) {
        lv_obj_set_style_opa(counting_root, LV_OPA_COVER, 0);
    }

    if (!g_si_ctx.lifecycle.result_transition_pending) {
        return;
    }
    g_si_ctx.lifecycle.result_transition_pending = false;

    /* A warning or a new count session may supersede the short fade. */
    if (g_si_ctx.lifecycle.count_session_active ||
        g_si_ctx.view.scene != SMART_ISLAND_SCENE_COUNTING) {
        return;
    }
    smart_island_result_present();
}

static void smart_island_result_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    fault_popup_clear_pending();
    fault_popup_reset_auto_retry();
    smart_island_result_stop_timer();
    smart_island_view_result_collapse();
}

void smart_island_result_stop_timer(void)
{
    if (g_si_ctx.lifecycle.result_timer == NULL) {
        return;
    }

    lv_timer_del(g_si_ctx.lifecycle.result_timer);
    g_si_ctx.lifecycle.result_timer = NULL;
}

static void smart_island_result_present(void)
{
    smart_island_set_scene(SMART_ISLAND_SCENE_RESULT, NULL, NULL);
    /* COUNTING and RESULT deliberately share one geometry.  Only content
     * changes here, so the shell never performs a second stretch. */
    smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, false);
    smart_island_view_result_enter();

    smart_island_result_stop_timer();
    g_si_ctx.lifecycle.result_timer = lv_timer_create(
        smart_island_result_timer_cb,
        SMART_ISLAND_RESULT_HOLD_MS,
        NULL);
    if (g_si_ctx.lifecycle.result_timer == NULL) {
        smart_island_restore_idle();
    }
}

void smart_island_notify_count_start(void)
{
    /* 0x0E is emitted for every accepted note.  Starting an already active
     * counting scene again would clear the live serial, rebuild the complete
     * island and restart its animation once per banknote. */
    if (g_si_ctx.lifecycle.count_session_active &&
        g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING) {
        return;
    }

    /* 新会话开始前先清掉上一轮残留的结束动画状态。 */
    ui_count_end_anim_cancel();

    smart_island_result_cancel_transition();

    g_si_ctx.lifecycle.count_session_active = true;
    g_si_ctx.warning.level = SMART_ISLAND_WARNING_LEVEL_WARNING;
    g_si_ctx.warning.resume_animation_pending = false;
    g_si_ctx.warning.resume_counting = false;
    smart_island_warning_fault_clear();
    smart_island_warning_stop();
    g_si_ctx.text.result[0] = '\0';
    g_si_ctx.text.serial_ticker[0] = '\0';
    g_si_ctx.counting.pcs = counting_data_current()->total_pcs;
    g_si_ctx.counting.amount = (int)counting_data_current()->total_amount;
    g_si_ctx.counting.denomination = 0;
    g_si_ctx.counting.mode = machine_state_mode();
    g_si_ctx.counting.serial[0] = '\0';
    g_si_ctx.counting.value_initialized = false;
    smart_island_set_scene(SMART_ISLAND_SCENE_COUNTING, NULL, NULL);
    smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, true);
    smart_island_view_update_counting();
    smart_island_view_message_pulse();
}

void smart_island_update_counting(int pcs, float amount)
{
    int amount_integer = amount > 0.0f ? (int)(amount + 0.5f) : 0;
    uint8_t mode = machine_state_mode();
    bool mode_changed = g_si_ctx.counting.mode != mode;

    if (pcs < 0) pcs = 0;
    if (g_si_ctx.counting.pcs == pcs &&
        g_si_ctx.counting.amount == amount_integer &&
        g_si_ctx.counting.mode == mode) {
        return;
    }

    g_si_ctx.counting.pcs = pcs;
    g_si_ctx.counting.amount = amount_integer;
    g_si_ctx.counting.mode = mode;
    if (mode_changed && g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING &&
        !g_si_ctx.lifecycle.suspended) {
        smart_island_view_apply_visual(SMART_ISLAND_VISUAL_COMPACT, true);
    }
    smart_island_view_update_counting();
}

void smart_island_notify_serial_number(int denomination, const char *serial_number)
{
    uint8_t mode = machine_state_mode();

    if (!g_si_ctx.lifecycle.count_session_active ||
        mode == MODE_CNT ||
        serial_number == NULL || serial_number[0] == '\0') {
        return;
    }

    if (denomination < 0) {
        denomination = 0;
    }
    g_si_ctx.counting.denomination = denomination;
    lv_snprintf(g_si_ctx.text.serial_ticker,
                sizeof(g_si_ctx.text.serial_ticker),
                "%.13s", serial_number);
    lv_snprintf(g_si_ctx.counting.serial,
                sizeof(g_si_ctx.counting.serial),
                "%.13s", serial_number);
    /* Denomination and serial originate from the same note frame.  Refresh
     * them together so the island can never pair a serial with total amount. */
    g_si_ctx.counting.value_initialized = false;
    smart_island_view_update_counting();
}

void smart_island_notify_count_end(const char *result_text)
{
    lv_anim_t animation;
    /* Detail completion may arrive after the transport-finished frame.  The
     * first end event owns the transition; delayed duplicates must not replay
     * COMPLETE after READY has already returned. */
    if (!g_si_ctx.lifecycle.count_session_active) {
        return;
    }

    g_si_ctx.lifecycle.count_session_active = false;
    if (result_text && result_text[0] != '\0') {
        lv_snprintf(g_si_ctx.text.result, sizeof(g_si_ctx.text.result), "%s", result_text);
    } else {
        g_si_ctx.text.result[0] = '\0';
    }

    /* Keep the 310x44 counting geometry unchanged.  Fade its content first,
     * then reveal COMPLETE in the same shell, avoiding an abrupt hard swap. */
    if (!g_si_ctx.lifecycle.suspended &&
        g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING &&
        g_si_ctx.objects.counting_root &&
        lv_obj_is_valid(g_si_ctx.objects.counting_root)) {
        g_si_ctx.lifecycle.result_transition_pending = true;
        lv_anim_del(g_si_ctx.objects.counting_root,
                    smart_island_result_source_opa_cb);
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.counting_root);
        lv_anim_set_exec_cb(&animation, smart_island_result_source_opa_cb);
        lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&animation, SMART_ISLAND_RESULT_SWITCH_MS);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
        lv_anim_set_ready_cb(&animation,
                             smart_island_result_source_fade_finish_cb);
        if (lv_anim_start(&animation) == NULL) {
            /* End state must not depend on allocating a cosmetic animation.
             * Duplicated transport ends are intentionally ignored above. */
            g_si_ctx.lifecycle.result_transition_pending = false;
            smart_island_result_present();
        }
    } else {
        g_si_ctx.lifecycle.result_transition_pending = false;
        smart_island_result_present();
    }
}

void smart_island_notify_count_reset(void)
{
    bool from_result = g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT;

    g_si_ctx.lifecycle.count_session_active = false;
    g_si_ctx.warning.resume_counting = false;
    smart_island_result_cancel_transition();
    smart_island_result_stop_timer();
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING ||
        from_result) {
        smart_island_restore_idle();
        /* A reset can interrupt RESULT before its normal collapse. In that
         * case restore_idle deliberately skips geometry, so settle it here. */
        if (from_result) smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, true);
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
