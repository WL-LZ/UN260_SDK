#ifndef UPGRADE_PAGE_RUNTIME_H
#define UPGRADE_PAGE_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_text.h"

typedef const char *(*upgrade_page_status_text_fn_t)(uint8_t status);
typedef bool (*upgrade_page_terminal_fn_t)(uint8_t status);

typedef struct {
    uint8_t command;
    uint32_t timeout_ms;
    const char *prefix;
    ui_text_id_t timeout_text_id;
    upgrade_page_status_text_fn_t status_text;
    upgrade_page_terminal_fn_t is_terminal;
} upgrade_page_runtime_config_t;

typedef struct {
    const upgrade_page_runtime_config_t *config;
    lv_obj_t *status_label;
    lv_timer_t *timeout_timer;
    bool waiting;
    uint32_t wait_start_tick;
    uint8_t last_status;
    bool has_last_status;
} upgrade_page_runtime_t;

void upgrade_page_runtime_init(upgrade_page_runtime_t *runtime,
                               const upgrade_page_runtime_config_t *config,
                               lv_obj_t *status_label);
bool upgrade_page_runtime_start(upgrade_page_runtime_t *runtime);
void upgrade_page_runtime_handle_reply(upgrade_page_runtime_t *runtime,
                                       uint8_t status);
void upgrade_page_runtime_destroy(upgrade_page_runtime_t *runtime);

#endif
