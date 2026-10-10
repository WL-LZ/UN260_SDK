/* Replay exact captured wire bytes through production parser, queue and request. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_frame_queue.h"
#include "un260/protocol/protocol_frame_parser.h"

static uint64_t now_ms;
static const uint8_t captured_manual_ack[] = {0xFD,0xDF,6,0x38,0,0x83};
uint64_t app_clock_monotonic_ms(void) { return now_ms; }
void app_runtime_wakeup_notify(void) {}

static void enqueue_at(const uint8_t *bytes, unsigned count, uint64_t received_ms)
{
    protocol_frame_parser_t parser;
    protocol_frame_view_t frame;
    protocol_frame_parser_init(&parser);
    for (unsigned i = 0; i < count; i++) {
        protocol_frame_parse_result_t result = protocol_frame_parser_feed(&parser, bytes[i], &frame);
        if (i + 1U < count) assert(result == PROTOCOL_FRAME_PARSE_INCOMPLETE);
        else {
            assert(result == PROTOCOL_FRAME_PARSE_READY);
            assert(protocol_frame_queue_push_wait_at(frame.data, frame.len, 0, received_ms));
        }
    }
}

static protocol_frame_t pop(void)
{
    protocol_frame_t frame;
    assert(protocol_frame_queue_pop(&frame));
    return frame;
}

static void reset(void)
{
    protocol_frame_queue_clear();
    protocol_request_result_context_end();
    protocol_request_set_pending_reply_probe(protocol_frame_queue_has_command_between);
}

static void test_timely_ack_delayed_dispatch(void)
{
    reset();
    protocol_request_t request = PROTOCOL_REQUEST_INITIALIZER(800);
    now_ms = 1000;
    assert(protocol_request_begin_for_command(&request, 0x38));
    const uint8_t transition[] = {0xFD,0xDF,7,0x0A,1,1,0x28};
    enqueue_at(transition, sizeof(transition), 1010);
    enqueue_at(captured_manual_ack, sizeof(captured_manual_ack), 1020);
    now_ms = 1800;
    assert(protocol_request_is_expired(&request));
    assert(!protocol_request_take_timeout(&request));
    assert(protocol_request_is_pending(&request));
    /* A stalled predecessor remains first; no ACK is consumed by the probe. */
    protocol_frame_t first = pop();
    assert(first.data[3] == 0x0A && first.received_ms == 1010);
    now_ms = 2200;
    assert(!protocol_request_take_timeout(&request));
    protocol_frame_t ack = pop();
    assert(ack.data[3] == 0x38 && ack.data[4] == 0 && ack.data[5] == 0x83);
    assert(ack.received_ms == 1020);
    protocol_request_result_context_begin(ack.received_ms);
    assert(protocol_request_result_time_ms() == 1020);
    assert(protocol_request_take_result(&request));
    protocol_request_result_context_end();
    assert(protocol_request_result_time_ms() == 2200);
    assert(!protocol_request_take_timeout(&request));
}

static void test_late_old_wrong_or_invalid_ack(void)
{
    reset();
    protocol_request_t request = PROTOCOL_REQUEST_INITIALIZER(800);
    now_ms = 1000;
    assert(protocol_request_begin_for_command(&request, 0x38));
    enqueue_at(captured_manual_ack, sizeof(captured_manual_ack), 1800);
    now_ms = 1800;
    assert(!protocol_frame_queue_has_command_between(0x38, 1000, 1800));
    protocol_frame_t late = pop();
    protocol_request_result_context_begin(late.received_ms);
    assert(!protocol_request_take_result(&request));
    protocol_request_result_context_end();
    assert(protocol_request_take_timeout(&request));

    /* A queued ACK predating a retry cannot keep it pending or acknowledge it. */
    now_ms = 2000;
    assert(protocol_request_begin_for_command(&request, 0x38));
    enqueue_at(captured_manual_ack, sizeof(captured_manual_ack), 1999);
    now_ms = 2800;
    assert(protocol_request_take_timeout(&request));
    protocol_frame_t old = pop();
    now_ms = 3000;
    assert(protocol_request_begin_for_command(&request, 0x38));
    protocol_request_result_context_begin(old.received_ms);
    assert(!protocol_request_take_result(&request) && request.pending);
    protocol_request_result_context_end();
    enqueue_at(captured_manual_ack, sizeof(captured_manual_ack), 3010);
    protocol_frame_t fresh = pop();
    protocol_request_result_context_begin(fresh.received_ms);
    assert(protocol_request_take_result(&request));
    protocol_request_result_context_end();

    now_ms = 4000;
    assert(protocol_request_begin_for_command(&request, 0x38));
    const uint8_t wrong[] = {0xFD,0xDF,6,0x41,1,4};
    enqueue_at(wrong, sizeof(wrong), 4020);
    now_ms = 4800;
    assert(protocol_request_take_timeout(&request));
    assert(pop().data[3] == 0x41);

    /* Command matching postpones only queued dispatch; payload validation
     * remains with its service, and rejection leaves a real timeout due. */
    now_ms = 5000;
    assert(protocol_request_begin_for_command(&request, 0x38));
    const uint8_t invalid_payload[] = {0xFD,0xDF,6,0x38,0xFE,0};
    enqueue_at(invalid_payload, sizeof(invalid_payload), 5020);
    now_ms = 5800;
    assert(!protocol_request_take_timeout(&request));
    protocol_frame_t invalid = pop();
    assert(invalid.data[4] == 0xFE); /* service rejects it, taking no result */
    assert(protocol_request_take_timeout(&request));
    assert(!protocol_request_take_timeout(&request));
}

static void test_backpressure_timestamp_and_compatibility(void)
{
    reset();
    protocol_request_t request = PROTOCOL_REQUEST_INITIALIZER(800);
    now_ms = 6000;
    assert(protocol_request_begin_for_command(&request, 0x38));
    const uint8_t unrelated[] = {0xFD,0xDF,6,0x41,1,4};
    for (unsigned i = 0; i < PROTOCOL_FRAME_QUEUE_CAPACITY; i++)
        assert(protocol_frame_queue_push_wait_at(unrelated, sizeof(unrelated), 0, 6001));
    assert(!protocol_frame_queue_push_wait_at(captured_manual_ack, sizeof(captured_manual_ack), 0, 6020));
    now_ms = 7000;
    assert(pop().data[3] == 0x41);
    /* The RX retry preserves the first complete-frame arrival, not retry time. */
    assert(protocol_frame_queue_push_wait_at(captured_manual_ack, sizeof(captured_manual_ack), 0, 6020));
    assert(!protocol_request_take_timeout(&request));
    for (unsigned i = 1; i < PROTOCOL_FRAME_QUEUE_CAPACITY; i++) assert(pop().data[3] == 0x41);
    protocol_frame_t ack = pop();
    assert(ack.received_ms == 6020);
    protocol_request_result_context_begin(ack.received_ms);
    assert(protocol_request_take_result(&request));
    protocol_request_result_context_end();

    now_ms = 8000;
    assert(protocol_request_begin(&request)); /* legacy unbound timeout semantics */
    enqueue_at(captured_manual_ack, sizeof(captured_manual_ack), 8020);
    now_ms = 8800;
    assert(protocol_request_take_timeout(&request));
    (void)pop();
    protocol_request_set_pending_reply_probe(NULL);
    now_ms = 9000;
    assert(protocol_request_begin_for_command(&request, 0x38));
    now_ms = 9800;
    assert(protocol_request_take_timeout(&request));

    protocol_request_t no_timeout = PROTOCOL_REQUEST_INITIALIZER(0);
    assert(protocol_request_begin_for_command(&no_timeout, 0x38));
    now_ms = 100000;
    assert(!protocol_request_take_timeout(&no_timeout));
    assert(protocol_request_take_result(&no_timeout));
}

static void test_rolling_stream_arrival_window(void)
{
    reset();
    protocol_request_t stream = PROTOCOL_REQUEST_INITIALIZER(5000);
    now_ms = 10000;
    assert(protocol_request_begin_for_command(&stream, 0x47));
    const uint8_t header[] = {0xFD,0xDF,11,0x47,0,1,0,2,0,2,0x31};
    const uint8_t line[] = {0xFD,0xDF,10,0x47,1,0,1,0xF8,0,0x55};
    const uint8_t end[] = {0xFD,0xDF,6,0x47,0xFF,0x63};
    enqueue_at(header, sizeof(header), 11000);
    enqueue_at(line, sizeof(line), 15500);
    enqueue_at(end, sizeof(end), 20499);
    now_ms = 30000;
    assert(protocol_request_is_expired(&stream));
    assert(!protocol_request_take_timeout(&stream));

    protocol_frame_t frame = pop();
    protocol_request_result_context_begin(frame.received_ms);
    assert(protocol_request_touch_result(&stream));
    protocol_request_result_context_end();
    assert(stream.pending && stream.started_ms == 11000);
    assert(stream.timeout_ms == 5000 && stream.command_bound && stream.command == 0x47);
    /* The next frame became timely only after the header's RX arrival rolled
     * the inactivity window. Dispatch time must never be the new start. */
    assert(!protocol_request_take_timeout(&stream));
    frame = pop();
    protocol_request_result_context_begin(frame.received_ms);
    assert(protocol_request_touch_result(&stream));
    protocol_request_result_context_end();
    assert(stream.started_ms == 15500);
    assert(!protocol_request_take_timeout(&stream));
    frame = pop();
    protocol_request_result_context_begin(frame.received_ms);
    assert(protocol_request_touch_result(&stream));
    assert(stream.started_ms == 20499);
    assert(protocol_request_take_result(&stream));
    protocol_request_result_context_end();
    assert(!protocol_request_touch_result(&stream));
    assert(!protocol_request_touch_result(NULL));

    now_ms = 40000;
    assert(protocol_request_begin_for_command(&stream, 0x47));
    protocol_request_result_context_begin(41000);
    assert(protocol_request_touch_result(&stream));
    protocol_request_result_context_end();
    protocol_request_result_context_begin(40999);
    assert(!protocol_request_touch_result(&stream)); /* old result after rollover */
    protocol_request_result_context_end();
    assert(stream.started_ms == 41000);
    protocol_request_result_context_begin(46000);
    assert(!protocol_request_touch_result(&stream)); /* exactly at deadline */
    protocol_request_result_context_end();
    assert(stream.started_ms == 41000);
    enqueue_at(line, sizeof(line), 46000);
    now_ms = 50000;
    assert(protocol_request_take_timeout(&stream)); /* late queue cannot defer */
    (void)pop();

    now_ms = 60000;
    assert(protocol_request_begin_for_command(&stream, 0x47));
    enqueue_at(captured_manual_ack, sizeof(captured_manual_ack), 60001);
    now_ms = 65000;
    assert(protocol_request_take_timeout(&stream)); /* wrong command probe */
    (void)pop();

    protocol_request_t unlimited = PROTOCOL_REQUEST_INITIALIZER(0);
    now_ms = 70000;
    assert(protocol_request_begin(&unlimited));
    now_ms = 80000;
    assert(protocol_request_touch_result(&unlimited)); /* legacy no context */
    assert(unlimited.pending && unlimited.started_ms == 80000 &&
           !unlimited.command_bound && unlimited.timeout_ms == 0);
    protocol_request_finish(&unlimited);
}

int main(void)
{
    test_timely_ack_delayed_dispatch();
    test_late_old_wrong_or_invalid_ack();
    test_backpressure_timestamp_and_compatibility();
    test_rolling_stream_arrival_window();
    puts("PASS exact CRC83 ACK: timely RX survives delayed ordered dispatch; late/old/wrong/rejected replies cannot succeed");
    puts("PASS RX backpressure preserves arrival time; no duplicate timeout; unbound/no-probe/no-timeout compatibility");
    puts("PASS rolling stream: RX arrival extends inactivity; pending FIFO probe survives late dispatch; old/late results cannot extend");
    return 0;
}
