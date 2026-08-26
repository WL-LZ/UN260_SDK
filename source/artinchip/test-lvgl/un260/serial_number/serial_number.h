#ifndef SERIAL_NUMBER_H
#define SERIAL_NUMBER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool success;
    bool target_enabled;
    uint8_t target_level;
    bool previous_enabled;
    uint8_t previous_level;
    uint8_t response_level;
} serial_number_setting_result_t;

uint8_t serial_number_state_normalize_level(uint8_t level);
void serial_number_state_confirm(bool enabled, uint8_t level);
bool serial_number_state_enabled(void);
uint8_t serial_number_state_level(void);

bool serial_number_service_request(bool target_enabled, uint8_t target_level);
void serial_number_service_cancel_request(void);
bool serial_number_service_take_reply(uint8_t level, uint8_t status, serial_number_setting_result_t *result);
bool serial_number_service_take_timeout(serial_number_setting_result_t *result);

#endif
