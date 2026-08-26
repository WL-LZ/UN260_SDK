#include "cfd.h"

#include <stddef.h>
#include <string.h>

#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_send.h"

#define CFD_QUERY_TIMEOUT_MS 800U

static cfd_state_value_t g_cfd_state = {
    .currency = "CNY",
    .levels = {
        { 3, 3, 3, 3 },
        { 3, 3, 3, 3 },
        { 3, 3, 3, 3 },
    },
};
static protocol_request_t g_cfd_query_request = PROTOCOL_REQUEST_INITIALIZER(CFD_QUERY_TIMEOUT_MS);
static char g_cfd_query_currency[4] = "";
static protocol_request_t g_cfd_update_request = PROTOCOL_REQUEST_INITIALIZER(CFD_QUERY_TIMEOUT_MS);
static cfd_state_value_t g_cfd_update_target;

static void cfd_copy_currency(char dst[4], const char *src)
{
    size_t len = 0;

    if (src) {
        while (len < 3 && src[len] != '\0') {
            dst[len] = src[len];
            len++;
        }
    }
    dst[len] = '\0';
}

void cfd_state_get(cfd_state_value_t *value)
{
    if (!value) return;
    *value = g_cfd_state;
}

void cfd_state_confirm(const cfd_state_value_t *value)
{
    if (!value) return;
    cfd_copy_currency(g_cfd_state.currency, value->currency);
    memcpy(g_cfd_state.levels, value->levels, sizeof(g_cfd_state.levels));
}

bool cfd_service_request_query(const char currency[4])
{
    uint8_t payload[4] = { 0x01, 0, 0, 0 };

    if (!currency || currency[0] == '\0') return false;
    if (protocol_request_is_pending(&g_cfd_update_request)) return false;
    if (!protocol_request_begin(&g_cfd_query_request)) return false;
    cfd_copy_currency(g_cfd_query_currency, currency);
    payload[1] = (uint8_t)g_cfd_query_currency[0];
    payload[2] = (uint8_t)g_cfd_query_currency[1];
    payload[3] = (uint8_t)g_cfd_query_currency[2];
    if (protocol_send(0x45, payload, sizeof(payload)) < 0) {
        cfd_service_cancel_query();
        return false;
    }
    return true;
}

void cfd_service_cancel_query(void)
{
    protocol_request_finish(&g_cfd_query_request);
    g_cfd_query_currency[0] = '\0';
}

bool cfd_service_take_query_result(const char currency[4])
{
    if (!currency || strncmp(currency, g_cfd_query_currency, 3) != 0) return false;
    if (!protocol_request_take_result(&g_cfd_query_request)) return false;
    g_cfd_query_currency[0] = '\0';
    return true;
}

bool cfd_service_take_query_timeout(void)
{
    if (!protocol_request_take_timeout(&g_cfd_query_request)) return false;
    g_cfd_query_currency[0] = '\0';
    return true;
}

bool cfd_service_request_update(const cfd_state_value_t *target,
                                uint8_t selected_scene)
{
    uint8_t payload[17];
    uint8_t pos = 5;

    if (target == NULL || target->currency[0] == '\0' ||
        selected_scene >= CFD_SCENE_COUNT ||
        protocol_request_is_pending(&g_cfd_query_request) ||
        !protocol_request_begin(&g_cfd_update_request)) {
        return false;
    }

    g_cfd_update_target = *target;
    payload[0] = 0x02;
    payload[1] = (uint8_t)target->currency[0];
    payload[2] = (uint8_t)target->currency[1];
    payload[3] = (uint8_t)target->currency[2];
    payload[4] = (uint8_t)(selected_scene + 1);
    for (uint8_t scene = 0; scene < CFD_SCENE_COUNT; scene++) {
        for (uint8_t item = 0; item < CFD_ITEM_COUNT; item++) {
            payload[pos++] = target->levels[scene][item];
        }
    }

    if (protocol_send(0x45, payload, sizeof(payload)) < 0) {
        cfd_service_cancel_update();
        return false;
    }
    return true;
}

bool cfd_service_take_update_result(const cfd_state_value_t *response)
{
    bool taken;

    if (response == NULL ||
        strncmp(response->currency, g_cfd_update_target.currency, 3) != 0 ||
        memcmp(response->levels, g_cfd_update_target.levels,
               sizeof(response->levels)) != 0) {
        return false;
    }
    taken = protocol_request_take_result(&g_cfd_update_request);
    if (taken) memset(&g_cfd_update_target, 0, sizeof(g_cfd_update_target));
    return taken;
}

bool cfd_service_take_update_timeout(void)
{
    bool timed_out = protocol_request_take_timeout(&g_cfd_update_request);

    if (timed_out) memset(&g_cfd_update_target, 0, sizeof(g_cfd_update_target));
    return timed_out;
}

void cfd_service_cancel_update(void)
{
    protocol_request_finish(&g_cfd_update_request);
    memset(&g_cfd_update_target, 0, sizeof(g_cfd_update_target));
}

bool cfd_service_busy(void)
{
    return protocol_request_is_pending(&g_cfd_query_request) ||
           protocol_request_is_pending(&g_cfd_update_request);
}
