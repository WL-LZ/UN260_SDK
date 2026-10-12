#include "protocol_request.h"

#include <stddef.h>

#include "un260/lv_system/app_clock.h"

static protocol_request_pending_reply_probe_t g_pending_reply_probe;
static protocol_request_start_observer_t g_start_observer;

void protocol_request_set_start_observer(protocol_request_start_observer_t observer)
{
    g_start_observer = observer;
}
static bool g_result_context_active;
static uint64_t g_result_received_ms;

void protocol_request_set_pending_reply_probe(protocol_request_pending_reply_probe_t probe)
{
    g_pending_reply_probe = probe;
}

void protocol_request_result_context_begin(uint64_t received_ms)
{
    g_result_received_ms = received_ms;
    g_result_context_active = true;
}

void protocol_request_result_context_end(void)
{
    g_result_context_active = false;
    g_result_received_ms = 0;
}

uint64_t protocol_request_result_time_ms(void)
{
    return g_result_context_active ? g_result_received_ms : protocol_request_now_ms();
}

uint64_t protocol_request_now_ms(void)
{
    return app_clock_monotonic_ms();
}

bool protocol_request_begin(protocol_request_t *request)
{
    if (!request || request->pending) return false;

    request->pending = true;
    request->started_ms = protocol_request_now_ms();
    request->command = 0;
    request->command_bound = false;
    return true;
}

bool protocol_request_begin_for_command(protocol_request_t *request, uint8_t command)
{
    if (!protocol_request_begin(request)) return false;
    request->command = command;
    request->command_bound = true;
    if (g_start_observer) g_start_observer(command);
    return true;
}

bool protocol_request_is_pending(const protocol_request_t *request)
{
    return request && request->pending;
}

bool protocol_request_is_expired(const protocol_request_t *request)
{
    if (!request || !request->pending || request->timeout_ms == 0) return false;
    uint64_t now_ms = protocol_request_now_ms();
    return now_ms >= request->started_ms &&
           now_ms - request->started_ms >= request->timeout_ms;
}

bool protocol_request_can_take_result(const protocol_request_t *request)
{
    if (!protocol_request_is_pending(request)) return false;
    uint64_t received_ms = protocol_request_result_time_ms();
    if (received_ms < request->started_ms) return false;
    return request->timeout_ms == 0 ||
           received_ms - request->started_ms < request->timeout_ms;
}

bool protocol_request_take_result(protocol_request_t *request)
{
    if (!protocol_request_can_take_result(request)) return false;
    protocol_request_finish(request);
    return true;
}

bool protocol_request_touch_result(protocol_request_t *request)
{
    if (!protocol_request_can_take_result(request)) return false;
    request->started_ms = protocol_request_result_time_ms();
    return true;
}

bool protocol_request_timeout_ready(const protocol_request_t *request)
{
    if (!protocol_request_is_expired(request)) return false;
    if (request->command_bound && g_pending_reply_probe &&
        g_pending_reply_probe(request->command, request->started_ms,
                              request->started_ms + request->timeout_ms)) return false;
    return true;
}

bool protocol_request_take_timeout(protocol_request_t *request)
{
    if (!protocol_request_timeout_ready(request)) return false;
    protocol_request_finish(request);
    return true;
}

void protocol_request_finish(protocol_request_t *request)
{
    if (!request) return;
    request->pending = false;
    request->started_ms = 0;
    request->command = 0;
    request->command_bound = false;
}
