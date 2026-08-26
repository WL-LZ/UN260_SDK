#include "counting_action_service.h"

#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_send.h"

#define COUNTING_ACTION_TIMEOUT_MS 1000U
#define COUNTING_START_CMD         0x0A
#define COUNTING_CLEAR_CMD         0x3B
#define COUNTING_ACTION_REQUEST    0x01
#define COUNTING_START_REPLY_LEN   7U
#define COUNTING_CLEAR_REPLY_LEN   6U

static protocol_request_t g_start_request =
    PROTOCOL_REQUEST_INITIALIZER(COUNTING_ACTION_TIMEOUT_MS);
static protocol_request_t g_clear_request =
    PROTOCOL_REQUEST_INITIALIZER(COUNTING_ACTION_TIMEOUT_MS);

static bool counting_action_send(protocol_request_t *request, uint8_t cmd)
{
    const uint8_t payload = COUNTING_ACTION_REQUEST;

    if (!protocol_request_begin(request)) {
        return false;
    }
    if (protocol_send(cmd, &payload, 1) < 0) {
        protocol_request_finish(request);
        return false;
    }
    return true;
}

bool counting_action_request_start(void)
{
    if (protocol_request_is_pending(&g_clear_request)) {
        return false;
    }
    return counting_action_send(&g_start_request, COUNTING_START_CMD);
}

bool counting_action_request_clear(void)
{
    protocol_request_finish(&g_start_request);
    return counting_action_send(&g_clear_request, COUNTING_CLEAR_CMD);
}

void counting_action_handle_reply(uint8_t cmd, const uint8_t *frame,
                                  uint8_t frame_len)
{
    if (frame == NULL) {
        return;
    }
    if (cmd == COUNTING_START_CMD && frame_len >= COUNTING_START_REPLY_LEN) {
        protocol_request_take_result(&g_start_request);
    } else if (cmd == COUNTING_CLEAR_CMD &&
               frame_len >= COUNTING_CLEAR_REPLY_LEN) {
        protocol_request_take_result(&g_clear_request);
    }
}

uint32_t counting_action_take_timeouts(void)
{
    uint32_t timeouts = COUNTING_ACTION_TIMEOUT_NONE;

    if (protocol_request_take_timeout(&g_start_request)) {
        timeouts |= COUNTING_ACTION_TIMEOUT_START;
    }
    if (protocol_request_take_timeout(&g_clear_request)) {
        timeouts |= COUNTING_ACTION_TIMEOUT_CLEAR;
    }
    return timeouts;
}

bool counting_action_clear_pending(void)
{
    return protocol_request_is_pending(&g_clear_request);
}

void counting_action_cancel_all(void)
{
    protocol_request_finish(&g_start_request);
    protocol_request_finish(&g_clear_request);
}
