#ifndef CFD_H
#define CFD_H

#include <stdbool.h>
#include <stdint.h>

#include "un260/lv_system/user_cfg.h"

typedef struct {
    char currency[4];
    uint8_t levels[CFD_SCENE_COUNT][CFD_ITEM_COUNT];
} cfd_state_value_t;

void cfd_state_get(cfd_state_value_t *value);
void cfd_state_confirm(const cfd_state_value_t *value);

bool cfd_service_request_query(const char currency[4]);
void cfd_service_cancel_query(void);
bool cfd_service_take_query_result(const char currency[4]);
bool cfd_service_take_query_timeout(void);
bool cfd_service_request_update(const cfd_state_value_t *target,
                                uint8_t selected_scene);
bool cfd_service_take_update_result(const cfd_state_value_t *response);
bool cfd_service_take_update_timeout(void);
void cfd_service_cancel_update(void);
bool cfd_service_busy(void);

#endif
