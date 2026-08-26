#include "protocol_request.h"

#include <stddef.h>

#include "un260/lv_system/app_clock.h"

uint64_t protocol_request_now_ms(void)
{
    return app_clock_monotonic_ms();
}

bool protocol_request_begin(protocol_request_t *request)
{
    if (!request || request->pending) return false;

    request->pending = true;
    request->started_ms = protocol_request_now_ms();
    return true;
}

bool protocol_request_is_pending(const protocol_request_t *request)
{
    return request && request->pending;
}

bool protocol_request_is_expired(const protocol_request_t *request)
{
    if (!request || !request->pending || request->timeout_ms == 0) return false;
    return protocol_request_now_ms() - request->started_ms >= request->timeout_ms;
}

bool protocol_request_can_take_result(const protocol_request_t *request)
{
    return protocol_request_is_pending(request) && !protocol_request_is_expired(request);
}

bool protocol_request_take_result(protocol_request_t *request)
{
    if (!protocol_request_can_take_result(request)) return false;
    protocol_request_finish(request);
    return true;
}

bool protocol_request_take_timeout(protocol_request_t *request)
{
    if (!protocol_request_is_expired(request)) return false;
    protocol_request_finish(request);
    return true;
}

void protocol_request_finish(protocol_request_t *request)
{
    if (!request) return;
    request->pending = false;
    request->started_ms = 0;
}
