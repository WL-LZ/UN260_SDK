#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/counting/counting_action_service.h"
#include "un260/protocol/protocol_request.h"

static uint64_t now_ms = 1000;
static bool history_ready = true;
static bool send_fails;
static bool queued_reply;
static unsigned sends;
static uint8_t last_command;

uint64_t app_clock_monotonic_ms(void) { return now_ms; }
bool counting_history_can_start(void) { return history_ready; }
int protocol_send(uint8_t command, const uint8_t *payload, uint16_t length)
{
    assert(payload != NULL && length == 1 && payload[0] == 1);
    ++sends;
    last_command = command;
    return send_fails ? -1 : 6;
}

static bool pending_reply_probe(uint8_t command, uint64_t started,
                                uint64_t deadline)
{
    assert(command == 0x3B && deadline - started == 1000);
    return queued_reply;
}

static void reset(void)
{
    counting_action_cancel_all();
    protocol_request_result_context_end();
    protocol_request_set_pending_reply_probe(NULL);
    now_ms += 2000;
    history_ready = true;
    send_fails = queued_reply = false;
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
}

static void reply(uint8_t status)
{
    const uint8_t frame[] = {0xFD, 0xDF, 6, 0x3B, status, 0x0A};
    counting_action_handle_reply(0x3B, frame, sizeof(frame));
}

static void valid_and_failed_replies(void)
{
    reset();
    reply(1);
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    assert(counting_action_request_start());
    assert(counting_action_start_pending() && last_command == 0x0A);
    assert(counting_action_request_clear());
    assert(!counting_action_start_pending() && last_command == 0x3B);
    assert(counting_action_clear_pending());
    unsigned sent = sends;
    assert(!counting_action_request_clear() && sends == sent);
    assert(!counting_action_request_start() && sends == sent);
    reply(1);
    assert(!counting_action_clear_pending());
    /* Unowned duplicates cannot replace an unconsumed terminal result. */
    reply(2);
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_SUCCESS);
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    assert(counting_action_take_timeouts() == COUNTING_ACTION_TIMEOUT_NONE);
    assert(counting_action_request_start());
    const uint8_t start[] = {0xFD, 0xDF, 7, 0x0A, 1, 1, 0x0A};
    counting_action_handle_reply(0x0A, start, sizeof(start));
    assert(!counting_action_start_pending());
    assert(counting_action_request_clear());
    reply(2);
    assert(!counting_action_clear_pending());
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_FAILED);
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
}

static void malformed_replies_keep_ownership(void)
{
    reset();
    assert(counting_action_request_clear());
    uint8_t frame[] = {0xFD, 0xDF, 6, 0x3B, 1, 0x0A, 0};
    counting_action_handle_reply(0x3B, NULL, 6);
    counting_action_handle_reply(0x3B, frame, 5);
    counting_action_handle_reply(0x3B, frame, 7);
    counting_action_handle_reply(0x3A, frame, 6);
    for (unsigned index = 0; index < 4; ++index) {
        uint8_t original = frame[index];
        frame[index] ^= 1;
        counting_action_handle_reply(0x3B, frame, 6);
        frame[index] = original;
    }
    reply(0);
    reply(3);
    reply(0xFF);
    assert(counting_action_clear_pending());
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    reply(1);
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_SUCCESS);
}

static void timeout_cancel_and_send_failure(void)
{
    reset();
    assert(counting_action_request_clear());
    now_ms += 1000;
    reply(1); /* Arrival at the deadline is expired, never successful. */
    assert(counting_action_clear_pending());
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    assert(counting_action_take_timeouts() == COUNTING_ACTION_TIMEOUT_CLEAR);
    assert(!counting_action_clear_pending());
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_TIMEOUT);
    assert(counting_action_take_timeouts() == COUNTING_ACTION_TIMEOUT_NONE);
    reply(1);
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    assert(counting_action_request_clear());
    counting_action_cancel_all();
    reply(1);
    assert(!counting_action_clear_pending());
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    assert(counting_action_request_clear());
    reply(2);
    counting_action_cancel_all();
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    send_fails = true;
    assert(!counting_action_request_clear());
    assert(!counting_action_clear_pending());
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    send_fails = false;
    history_ready = false;
    assert(!counting_action_request_start());
    history_ready = true;
    assert(counting_action_request_start());
    now_ms += 5000;
    assert(counting_action_take_timeouts() == COUNTING_ACTION_TIMEOUT_START);
}

static void queued_arrival_and_stale_timestamp(void)
{
    reset();
    assert(counting_action_request_clear());
    uint64_t started = now_ms;
    now_ms += 1500;
    protocol_request_set_pending_reply_probe(pending_reply_probe);
    queued_reply = true;
    assert(counting_action_take_timeouts() == COUNTING_ACTION_TIMEOUT_NONE);
    assert(counting_action_clear_pending());
    protocol_request_result_context_begin(started + 999);
    reply(1);
    protocol_request_result_context_end();
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_SUCCESS);
    assert(!counting_action_clear_pending());

    queued_reply = false;
    assert(counting_action_request_clear());
    protocol_request_result_context_begin(now_ms - 1);
    reply(1);
    protocol_request_result_context_end();
    assert(counting_action_clear_pending());
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_NONE);
    now_ms += 1000;
    assert(counting_action_take_timeouts() == COUNTING_ACTION_TIMEOUT_CLEAR);
    assert(counting_action_take_clear_result() == COUNTING_CLEAR_RESULT_TIMEOUT);
}

int main(void)
{
    valid_and_failed_replies();
    malformed_replies_keep_ownership();
    timeout_cancel_and_send_failure();
    queued_arrival_and_stale_timestamp();
    puts("PASS clear result ownership: exact reply shape/status, one-shot success/failure, timeout/cancel/send failure, queued arrival and stale timestamp; START guards unchanged");
    return 0;
}
