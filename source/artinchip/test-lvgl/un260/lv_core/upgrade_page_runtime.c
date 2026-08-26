#include "un260/lv_core/upgrade_page_runtime.h"

#include "un260/protocol/protocol_send.h"

static void upgrade_page_runtime_set_status(upgrade_page_runtime_t *runtime,
                                            const char *text,
                                            lv_color_t color)
{
    if (runtime == NULL || runtime->status_label == NULL ||
        !lv_obj_is_valid(runtime->status_label)) return;
    lv_label_set_text(runtime->status_label, text);
    lv_obj_set_style_text_color(runtime->status_label, color, 0);
}

static void upgrade_page_runtime_timeout_cb(lv_timer_t *timer)
{
    upgrade_page_runtime_t *runtime = timer != NULL ? timer->user_data : NULL;

    if (runtime == NULL || !runtime->waiting || runtime->config == NULL) return;
    if (lv_tick_elaps(runtime->wait_start_tick) < runtime->config->timeout_ms) return;
    runtime->waiting = false;
    upgrade_page_runtime_set_status(runtime, ui_text_get(runtime->config->timeout_text_id),
                                    lv_color_hex(0xC03A2B));
}

void upgrade_page_runtime_init(upgrade_page_runtime_t *runtime,
                               const upgrade_page_runtime_config_t *config,
                               lv_obj_t *status_label)
{
    if (runtime == NULL) return;
    runtime->config = config;
    runtime->status_label = status_label;
    if (runtime->timeout_timer == NULL) {
        runtime->timeout_timer = lv_timer_create(upgrade_page_runtime_timeout_cb,
                                                 200, runtime);
    }
}

bool upgrade_page_runtime_start(upgrade_page_runtime_t *runtime)
{
    const uint8_t payload = 0x01;

    if (runtime == NULL || runtime->config == NULL || !protocol_send_is_ready()) return false;
    if (protocol_send(runtime->config->command, &payload, 1) < 0) return false;
    runtime->waiting = true;
    runtime->wait_start_tick = lv_tick_get();
    return true;
}

void upgrade_page_runtime_handle_reply(upgrade_page_runtime_t *runtime,
                                       uint8_t status)
{
    const char *text;
    bool terminal;

    if (runtime == NULL || runtime->config == NULL) return;
    runtime->has_last_status = true;
    runtime->last_status = status;
    text = runtime->config->status_text(status);
    terminal = runtime->config->is_terminal(status);
    if (terminal && (status == 0x03 || status == 0x04)) {
        if (runtime->status_label != NULL && lv_obj_is_valid(runtime->status_label)) {
            lv_label_set_text_fmt(runtime->status_label, "%s: %s", runtime->config->prefix, text);
            lv_obj_set_style_text_color(runtime->status_label, lv_color_hex(0x1F9D55), 0);
        }
    } else if (terminal) {
        if (runtime->status_label != NULL && lv_obj_is_valid(runtime->status_label)) {
            lv_label_set_text_fmt(runtime->status_label, "%s: %s", runtime->config->prefix, text);
            lv_obj_set_style_text_color(runtime->status_label, lv_color_hex(0xC03A2B), 0);
        }
    } else {
        if (runtime->status_label != NULL && lv_obj_is_valid(runtime->status_label)) {
            lv_label_set_text_fmt(runtime->status_label, "%s: %s", runtime->config->prefix, text);
            lv_obj_set_style_text_color(runtime->status_label, lv_color_hex(0x2D3A4A), 0);
        }
    }
    if (terminal) runtime->waiting = false;
}

void upgrade_page_runtime_destroy(upgrade_page_runtime_t *runtime)
{
    if (runtime == NULL) return;
    if (runtime->timeout_timer != NULL) lv_timer_del(runtime->timeout_timer);
    runtime->timeout_timer = NULL;
    runtime->status_label = NULL;
    runtime->waiting = false;
}
