#ifndef GESTURE_SERVICE_H
#define GESTURE_SERVICE_H

#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_text.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    GESTURE_ACTION_EXIT_PAGE = 0,
    GESTURE_ACTION_HOME
} gesture_action_t;

typedef struct {
    uint8_t finger_count;
    bool starts_from_bottom;
    gesture_action_t action;
    ui_text_id_t title_text;
    ui_text_id_t body_text;
} gesture_definition_t;

void gesture_service_init(void);
bool gesture_service_enabled(void);
bool gesture_service_set_enabled(bool enabled);
size_t gesture_service_definition_count(void);
const gesture_definition_t *gesture_service_definition(size_t index);

#endif
