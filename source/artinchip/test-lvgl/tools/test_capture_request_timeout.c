#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_frame_queue.h"

static uint64_t now_ms;
static unsigned probes, observed, wakeups;
uint64_t app_clock_monotonic_ms(void) { return now_ms; }
void app_runtime_wakeup_notify(void) { ++wakeups; }
static void observe(uint8_t command) { assert(command == 0x47); ++observed; }
static bool probe(uint8_t command, uint64_t start, uint64_t deadline) {
    ++probes;
    return protocol_frame_queue_has_command_between(command, start, deadline);
}
static void unchanged_query(protocol_request_t *request, bool expected) {
    protocol_request_t before;
    memcpy(&before, request, sizeof(before));
    assert(protocol_request_timeout_ready(request) == expected);
    assert(!memcmp(request, &before, sizeof(before)));
}
static void enqueue(uint8_t command, uint64_t received, uint8_t marker) {
    uint8_t frame[6];
    assert(protocol_frame_build(frame, sizeof(frame), command, &marker, 1) == 6);
    assert(protocol_frame_queue_push_wait_at(frame, sizeof(frame), 0, received));
}
static void dequeue(uint8_t command, uint64_t received, uint8_t marker) {
    protocol_frame_t frame;
    assert(protocol_frame_queue_pop(&frame));
    assert(frame.len == 6 && frame.data[3] == command && frame.data[4] == marker);
    assert(frame.received_ms == received);
}
int main(void) {
    protocol_request_t request = PROTOCOL_REQUEST_INITIALIZER(10);
    protocol_request_set_start_observer(observe);
    protocol_request_set_pending_reply_probe(probe);
    assert(!protocol_request_timeout_ready(NULL));
    assert(!protocol_request_take_timeout(NULL));
    assert(!protocol_request_take_result(NULL));
    assert(!protocol_request_touch_result(NULL));
    protocol_request_finish(NULL);
    now_ms = 100;
    unchanged_query(&request, false);
    assert(!probes);
    assert(protocol_request_begin_for_command(&request, 0x47));
    assert(observed == 1 && request.pending && request.started_ms == 100);
    assert(!protocol_request_begin_for_command(&request, 0x48));
    assert(observed == 1);
    now_ms = 99; unchanged_query(&request, false);
    now_ms = 109; unchanged_query(&request, false);
    assert(!probes);
    now_ms = 110; unchanged_query(&request, true);
    assert(probes == 1 && request.pending);

    /* Real production queue: inspection neither consumes nor reorders frames. */
    enqueue(0x38, 105, 1); // Timely wrong command cannot protect.
    enqueue(0x47, 99, 2); // Old matching command cannot protect.
    enqueue(0x47, 110, 4); // At deadline is late, never protects.
    unchanged_query(&request, true);
    enqueue(0x47, 109, 3); // Last millisecond before deadline protects.
    now_ms = 1000;
    unchanged_query(&request, false);
    unchanged_query(&request, false);
    assert(!protocol_request_take_timeout(&request) && request.pending);
    dequeue(0x38, 105, 1); dequeue(0x47, 99, 2);
    dequeue(0x47, 110, 4); dequeue(0x47, 109, 3);
    assert(!protocol_frame_queue_has_pending() && wakeups == 4);

    /* Dispatch may run much later than RX. Actual arrival owns acceptance/touch. */
    protocol_request_result_context_begin(109);
    assert(protocol_request_can_take_result(&request));
    assert(protocol_request_touch_result(&request));
    assert(request.started_ms == 109 && request.timeout_ms == 10 && request.command == 0x47);
    assert(request.pending && request.command_bound);
    protocol_request_result_context_begin(108);
    assert(!protocol_request_can_take_result(&request));
    assert(!protocol_request_touch_result(&request));
    assert(request.started_ms == 109);
    protocol_request_result_context_begin(119);
    assert(!protocol_request_take_result(&request) && request.pending);
    assert(!protocol_request_touch_result(&request));
    protocol_request_result_context_begin(118);
    assert(protocol_request_take_result(&request));
    assert(!request.pending && !request.command_bound && !request.started_ms && !request.command);
    assert(request.timeout_ms == 10);
    unchanged_query(&request, false);
    assert(!protocol_request_take_timeout(&request));
    protocol_request_result_context_end();
    assert(protocol_request_result_time_ms() == now_ms);

    /* Query respects ownership: unbound calls do not probe any queued command. */
    now_ms = 200; assert(protocol_request_begin(&request));
    enqueue(0x47, 205, 5); now_ms = 210;
    unsigned probe_count = probes;
    unchanged_query(&request, true); assert(probes == probe_count);
    assert(protocol_request_take_timeout(&request));
    assert(!request.pending && request.timeout_ms == 10 && request.started_ms == 0);
    assert(!protocol_request_take_timeout(&request));
    dequeue(0x47, 205, 5);

    /* No timeout means no expiration/probe; queued fallback may be absent. */
    request.timeout_ms = 0; now_ms = 300;
    assert(protocol_request_begin_for_command(&request, 0x47));
    now_ms = UINT64_MAX; probe_count = probes;
    unchanged_query(&request, false); assert(probes == probe_count);
    assert(!protocol_request_take_timeout(&request));
    protocol_request_result_context_begin(300);
    assert(protocol_request_take_result(&request));
    protocol_request_result_context_end();
    request.timeout_ms = 10; now_ms = 400;
    assert(protocol_request_begin_for_command(&request, 0x47));
    protocol_request_set_pending_reply_probe(NULL);
    enqueue(0x47, 405, 6); now_ms = 410;
    probe_count = probes; unchanged_query(&request, true); assert(probes == probe_count);
    assert(protocol_request_take_timeout(&request));
    dequeue(0x47, 405, 6);
    assert(!protocol_frame_queue_has_pending());
    protocol_request_set_start_observer(NULL);
    puts("PASS actual request+frame queue: const timeout query, NULL/ended/timeout0, clock/deadline, bound/unbound, timely queue protection/order, actual RX/touch, genuine one-time finish");
    return 0;
}
