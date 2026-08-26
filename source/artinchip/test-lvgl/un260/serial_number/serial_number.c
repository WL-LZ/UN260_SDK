#include "serial_number.h"

#include <stddef.h>

#include "un260/lv_system/user_cfg.h"
#include "un260/protocol/protocol_request.h"

#define SERIAL_NUMBER_REQUEST_TIMEOUT_MS 800U

typedef struct {
    bool target_enabled;
    uint8_t target_level;
    bool previous_enabled;
    uint8_t previous_level;
} serial_number_request_state_t;

static bool g_serial_number_enabled = false;
static uint8_t g_serial_number_level = SERIAL_NUMBER_LEVEL_OFF;
static protocol_request_t g_serial_number_request = PROTOCOL_REQUEST_INITIALIZER(SERIAL_NUMBER_REQUEST_TIMEOUT_MS);
static serial_number_request_state_t g_serial_number_request_state;

uint8_t serial_number_state_normalize_level(uint8_t level)
{
    return level <= SERIAL_NUMBER_LEVEL_MAX ? level : SERIAL_NUMBER_LEVEL_OFF;
}

void serial_number_state_confirm(bool enabled, uint8_t level)
{
    g_serial_number_enabled = enabled;
    g_serial_number_level = level;
}

bool serial_number_state_enabled(void)
{
    return g_serial_number_enabled;
}

uint8_t serial_number_state_level(void)
{
    return g_serial_number_level;
}

bool serial_number_service_request(bool target_enabled, uint8_t target_level)
{
    if (!protocol_request_begin(&g_serial_number_request)) return false;

    g_serial_number_request_state.target_enabled = target_enabled;
    g_serial_number_request_state.target_level = target_level;
    g_serial_number_request_state.previous_enabled = serial_number_state_enabled();
    g_serial_number_request_state.previous_level = serial_number_state_level();
    return true;
}

void serial_number_service_cancel_request(void)
{
    protocol_request_finish(&g_serial_number_request);
}

static void serial_number_service_fill_result(serial_number_setting_result_t *result)
{
    result->target_enabled = g_serial_number_request_state.target_enabled;
    result->target_level = g_serial_number_request_state.target_level;
    result->previous_enabled = g_serial_number_request_state.previous_enabled;
    result->previous_level = g_serial_number_request_state.previous_level;
}

bool serial_number_service_take_reply(uint8_t level, uint8_t status, serial_number_setting_result_t *result)
{
    if (result == NULL) return false;
    if (status != 0x01 && status != 0x02) return false;
    if (level != g_serial_number_request_state.target_level) return false;
    if (!protocol_request_take_result(&g_serial_number_request)) return false;

    serial_number_service_fill_result(result);
    result->success = status == 0x01;
    result->response_level = level;
    return true;
}

bool serial_number_service_take_timeout(serial_number_setting_result_t *result)
{
    if (!result || !protocol_request_take_timeout(&g_serial_number_request)) return false;

    serial_number_service_fill_result(result);
    result->success = false;
    result->response_level = g_serial_number_request_state.target_level;
    return true;
}
