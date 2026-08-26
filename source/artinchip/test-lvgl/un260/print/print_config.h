#ifndef PRINT_CONFIG_H
#define PRINT_CONFIG_H

#include "un260/lv_system/user_cfg.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t space_top;
    char head1[PRINT_SETTING_HEAD_MAX_LEN + 1];
    char head2[PRINT_SETTING_HEAD_MAX_LEN + 1];
    uint8_t content;
    uint8_t space_bottom;
} print_config_value_t;

typedef struct {
    uint8_t sub_command;
    bool success;
    bool timeout;
} print_config_request_result_t;

void print_config_get(print_config_value_t *value);
void print_config_confirm(const print_config_value_t *value);
bool print_config_request(uint8_t sub_command,
                          const uint8_t *payload,
                          uint16_t payload_len,
                          const print_config_value_t *target);
bool print_config_take_reply(uint8_t sub_command,
                             uint8_t status,
                             print_config_request_result_t *result);
bool print_config_take_timeout(print_config_request_result_t *result);
void print_config_cancel_request(void);

#endif
