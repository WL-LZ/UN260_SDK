#ifndef UPGRADE_PAGE_RUNTIME_H
#define UPGRADE_PAGE_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl/lvgl.h"
#include "un260/app_service/upgrade_session.h"

typedef const char *(*upgrade_page_status_text_fn_t)(uint8_t status);
typedef bool (*upgrade_page_terminal_fn_t)(uint8_t status);

typedef struct {
    uint8_t command;
    uint32_t timeout_ms;
    upgrade_page_status_text_fn_t status_text;
    upgrade_page_terminal_fn_t is_terminal;
    uint32_t page_id;
    upgrade_session_owner_t owner;
} upgrade_page_runtime_config_t;

typedef struct {
    const upgrade_page_runtime_config_t *config;
    lv_obj_t *status_label;
    lv_timer_t *timeout_timer;
    bool waiting;
    uint32_t wait_start_tick;
    uint8_t last_status;
    bool has_last_status;
    lv_obj_t *root, *back, *start, *phase_label, *steps[3], *message;
    bool timed_out, home_requested, blocked;
} upgrade_page_runtime_t;

void upgrade_page_runtime_init(upgrade_page_runtime_t *runtime,
                               const upgrade_page_runtime_config_t *config,
                               lv_obj_t *status_label);
bool upgrade_page_runtime_start(upgrade_page_runtime_t *runtime);
void upgrade_page_runtime_handle_reply(upgrade_page_runtime_t *runtime,
                                       uint8_t status);
void upgrade_page_runtime_destroy(upgrade_page_runtime_t *runtime);
/* Shared presentation for controller and image-board update workflows. */
void upgrade_page_runtime_create(upgrade_page_runtime_t *runtime,
    lv_obj_t *parent,const upgrade_page_runtime_config_t *config,
    const char *title,const char *installed_version);

#endif
