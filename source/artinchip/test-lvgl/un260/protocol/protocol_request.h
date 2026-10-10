#ifndef PROTOCOL_REQUEST_H
#define PROTOCOL_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool pending;
    uint64_t started_ms;
    uint32_t timeout_ms;
    uint8_t command;
    bool command_bound;
} protocol_request_t;

#define PROTOCOL_REQUEST_INITIALIZER(timeout) { false, 0, (timeout), 0, false }

typedef bool (*protocol_request_pending_reply_probe_t)(uint8_t command,
                                                      uint64_t started_ms,
                                                      uint64_t deadline_ms);

typedef void (*protocol_request_start_observer_t)(uint8_t command);
/* UI-thread owner observes only successful command-bound begins, before send. */
void protocol_request_set_start_observer(protocol_request_start_observer_t observer);

/* UI-thread setup and dispatch context; the probe observes pending RX without
 * consuming or reordering frames. All timestamps use CLOCK_MONOTONIC. */
void protocol_request_set_pending_reply_probe(protocol_request_pending_reply_probe_t probe);
void protocol_request_result_context_begin(uint64_t received_ms);
void protocol_request_result_context_end(void);
uint64_t protocol_request_result_time_ms(void);
uint64_t protocol_request_now_ms(void);
bool protocol_request_begin(protocol_request_t *request);
bool protocol_request_begin_for_command(protocol_request_t *request, uint8_t command);
bool protocol_request_is_pending(const protocol_request_t *request);
bool protocol_request_is_expired(const protocol_request_t *request);
bool protocol_request_can_take_result(const protocol_request_t *request);
bool protocol_request_take_result(protocol_request_t *request);
/* Extend rolling stream inactivity from an accepted result's RX arrival.
 * The request remains pending with the same command and timeout binding. */
bool protocol_request_touch_result(protocol_request_t *request);
bool protocol_request_take_timeout(protocol_request_t *request);
void protocol_request_finish(protocol_request_t *request);

#endif
