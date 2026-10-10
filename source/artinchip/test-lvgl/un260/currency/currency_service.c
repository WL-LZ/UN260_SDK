#include "currency_service.h"
#include "un260/protocol/protocol_request.h"
#include <string.h>

#define CURRENCY_SWITCH_TIMEOUT_MS 800U

typedef struct {
    uint8_t target_index;
    char target_code[4];
} currency_switch_request_t;

static currency_switch_request_t g_currency_switch_request;
static protocol_request_t g_currency_switch_lifecycle = PROTOCOL_REQUEST_INITIALIZER(CURRENCY_SWITCH_TIMEOUT_MS);
static bool rejection_detail_pending;
static uint64_t rejection_received_ms;

static void currency_service_copy_code(char dst[4], const char src[4])
{
    memset(dst, 0, 4);
    if (!src) return;
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

bool currency_service_request_switch(uint8_t target_index, const char target_code[4])
{
    char listed_code[4];

    if (!target_code || target_code[0] == '\0') return false;
    if (!currency_state_get_code(target_index, listed_code) ||
        strncmp(listed_code, target_code, 3) != 0) {
        return false;
    }
    if (!protocol_request_begin_for_command(&g_currency_switch_lifecycle, 0x03)) return false;

    rejection_detail_pending = false;
    memset(&g_currency_switch_request, 0, sizeof(g_currency_switch_request));
    g_currency_switch_request.target_index = target_index;
    currency_service_copy_code(g_currency_switch_request.target_code, target_code);
    return true;
}

bool currency_service_take_switch_result(uint8_t status, currency_switch_result_t* result)
{
    if (!result) return false;
    if (status != 0x01 && status != 0x02) return false;
    if (!protocol_request_take_result(&g_currency_switch_lifecycle)) return false;

    memset(result, 0, sizeof(*result));
    result->success = (status == 0x01);
    result->controller_rejected = status == 0x02;
    rejection_detail_pending = result->controller_rejected;
    rejection_received_ms = protocol_request_result_time_ms();
    result->target_index = g_currency_switch_request.target_index;
    currency_service_copy_code(result->target_code, g_currency_switch_request.target_code);
    memset(&g_currency_switch_request, 0, sizeof(g_currency_switch_request));
    return true;
}

bool currency_service_take_switch_timeout(currency_switch_result_t* result)
{
    if (!result || !protocol_request_take_timeout(&g_currency_switch_lifecycle)) return false;

    memset(result, 0, sizeof(*result));
    result->success = false;
    result->timeout = true;
    rejection_detail_pending = false;
    result->target_index = g_currency_switch_request.target_index;
    currency_service_copy_code(result->target_code, g_currency_switch_request.target_code);
    memset(&g_currency_switch_request, 0, sizeof(g_currency_switch_request));
    return true;
}

bool currency_service_switch_pending(void)
{
    return protocol_request_is_pending(&g_currency_switch_lifecycle);
}

bool currency_service_take_switch_rejection_detail(uint8_t command, uint8_t code)
{
    bool pending = rejection_detail_pending;
    rejection_detail_pending = false;
    uint64_t received_ms = protocol_request_result_time_ms();
    return pending && command == 0x0F && (code == 2 || code == 7) &&
        received_ms >= rejection_received_ms &&
        received_ms - rejection_received_ms < CURRENCY_SWITCH_TIMEOUT_MS;
}

void currency_service_cancel_switch(void)
{
    rejection_detail_pending = false;
    protocol_request_finish(&g_currency_switch_lifecycle);
    memset(&g_currency_switch_request, 0, sizeof(g_currency_switch_request));
}
