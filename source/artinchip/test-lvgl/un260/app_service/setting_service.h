#ifndef SETTING_SERVICE_H
#define SETTING_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SETTING_BATCH_REQUEST_NONE = 0,
    SETTING_BATCH_REQUEST_NUMBER,
    SETTING_BATCH_REQUEST_SWITCH,
} setting_batch_request_type_t;

typedef struct {
    bool enable;
    uint8_t num;
} setting_batch_snapshot_t;

typedef struct {
    setting_batch_request_type_t type;
    setting_batch_snapshot_t target;
    setting_batch_snapshot_t previous;
} setting_batch_result_t;

typedef struct {
    uint8_t target;
    uint8_t previous;
    bool success;
    bool timeout;
} setting_value_result_t;

typedef struct {
    bool success;
    bool timeout;
} setting_action_result_t;

typedef enum {
    SETTING_REQUEST_TIMEOUT_NONE = 0,
    SETTING_REQUEST_TIMEOUT_MODE = 1U << 0,
    SETTING_REQUEST_TIMEOUT_ADD = 1U << 1,
    SETTING_REQUEST_TIMEOUT_FO_MODE = 1U << 2,
    SETTING_REQUEST_TIMEOUT_SPEED = 1U << 3,
    SETTING_REQUEST_TIMEOUT_WORK_MODE = 1U << 4,
    SETTING_REQUEST_TIMEOUT_BEEP = 1U << 5,
} setting_request_timeout_t;

/* Result tag for 0x04/0x01. It is a currency selection, not a UI mode. */
#define SETTING_MODE_TARGET_AUTO_CURRENCY 0xF0U

bool setting_service_request_mode(uint8_t target);
bool setting_service_request_auto_currency(void);
bool setting_service_mode_is_pending(void);
bool setting_service_take_mode_result(uint8_t *target);
void setting_service_cancel_mode_request(void);

bool setting_service_request_add(bool target);
bool setting_service_take_add_result(bool *target);

bool setting_service_request_fo_mode(uint8_t target);
bool setting_service_take_fo_mode_result(uint8_t *target);

bool setting_service_request_speed(uint8_t target);
bool setting_service_take_speed_result(uint8_t *target);

bool setting_service_request_work_mode(uint8_t target);
bool setting_service_take_work_mode_result(uint8_t *target);

bool setting_service_request_beep(bool target);
bool setting_service_take_beep_result(bool *target);
uint32_t setting_service_take_basic_timeouts(void);

bool setting_service_request_batch_number(uint8_t num, bool previous_enable, uint8_t previous_num);
bool setting_service_request_batch_switch(bool target_enable, uint8_t sent_num, bool previous_enable, uint8_t previous_num);
bool setting_service_batch_take_result(uint8_t status, setting_batch_result_t *result);
bool setting_service_batch_take_timeout(setting_batch_result_t *result);

bool setting_service_request_double_note_level(uint8_t target, uint8_t previous);
bool setting_service_take_double_note_level_result(uint8_t response_level,
                                                   uint8_t status,
                                                   setting_value_result_t *result);
bool setting_service_take_double_note_level_timeout(setting_value_result_t *result);
void setting_service_clear_double_note_level_request(void);

bool setting_service_request_flap_position(uint8_t target, uint8_t previous);
bool setting_service_take_flap_position_result(uint8_t status, setting_value_result_t *result);
bool setting_service_take_flap_position_timeout(setting_value_result_t *result);

bool setting_service_request_reject_pocket_max(uint8_t target, uint8_t previous);
bool setting_service_take_reject_pocket_max_result(uint8_t status, setting_value_result_t *result);
bool setting_service_take_reject_pocket_max_timeout(setting_value_result_t *result);
void setting_service_clear_reject_pocket_max_request(void);

bool setting_service_request_aging_start(void);
bool setting_service_take_aging_result(uint8_t status,
                                       setting_action_result_t *result);
bool setting_service_take_aging_timeout(setting_action_result_t *result);

bool setting_service_request_factory_reset(void);
bool setting_service_take_factory_result(uint8_t status,
                                         setting_action_result_t *result);
bool setting_service_take_factory_timeout(setting_action_result_t *result);
void setting_service_cancel_all(void);

#endif
