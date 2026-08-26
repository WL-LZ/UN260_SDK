#ifndef PROTOCOL_REQUEST_H
#define PROTOCOL_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool pending;
    uint64_t started_ms;
    uint32_t timeout_ms;
} protocol_request_t;

#define PROTOCOL_REQUEST_INITIALIZER(timeout) { false, 0, (timeout) }

uint64_t protocol_request_now_ms(void);
bool protocol_request_begin(protocol_request_t *request);
bool protocol_request_is_pending(const protocol_request_t *request);
bool protocol_request_is_expired(const protocol_request_t *request);
bool protocol_request_can_take_result(const protocol_request_t *request);
bool protocol_request_take_result(protocol_request_t *request);
bool protocol_request_take_timeout(protocol_request_t *request);
void protocol_request_finish(protocol_request_t *request);

#endif
