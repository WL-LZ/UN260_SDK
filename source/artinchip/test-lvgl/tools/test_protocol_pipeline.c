#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "un260/app_service/app_runtime_wakeup.h"
#include "un260/lv_system/app_clock.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_frame_queue.h"
#include "un260/protocol/protocol_rx_service.h"
#include "un260/protocol/protocol_request.h"

#define TEST_FRAMES 600U
static uint8_t g_input[TEST_FRAMES * 7U];
static size_t g_input_len;
static size_t g_input_pos;
static unsigned g_read_calls;
static unsigned g_logged_frames;
static pthread_mutex_t g_test_mutex = PTHREAD_MUTEX_INITIALIZER;

static void sleep_ms(unsigned ms)
{
    struct timespec delay = { ms / 1000U, (long)(ms % 1000U) * 1000000L };
    nanosleep(&delay, NULL);
}

/* UART mock deliberately splits and joins frames at varying byte boundaries.
 * It also verifies that RX requests a chunk, not one syscall per byte. */
int uart_recv(int fd, char *output, int capacity, int timeout_ms)
{
    static const size_t chunks[] = { 1, 2, 5, 7, 256, 13, 97, 3 };
    size_t count;

    (void)fd;
    assert(capacity > 1);
    pthread_mutex_lock(&g_test_mutex);
    count = chunks[g_read_calls++ % (sizeof(chunks) / sizeof(chunks[0]))];
    if (count > (size_t)capacity) count = (size_t)capacity;
    if (count > g_input_len - g_input_pos) count = g_input_len - g_input_pos;
    memcpy(output, g_input + g_input_pos, count);
    g_input_pos += count;
    pthread_mutex_unlock(&g_test_mutex);
    if (count == 0U) sleep_ms((unsigned)timeout_ms);
    return (int)count;
}

void uart_printf(int fd, const char *format, ...)
{
    (void)fd;
    (void)format;
}

void uart_log_hex(int fd, const char *prefix, const uint8_t *data,
                  size_t len, size_t preview_limit)
{
    (void)fd;
    assert(prefix != NULL && data != NULL && (len == 6U || len == 7U) && preview_limit == 32U);
    pthread_mutex_lock(&g_test_mutex);
    g_logged_frames++;
    pthread_mutex_unlock(&g_test_mutex);
}

static void reset_input(unsigned frames)
{
    protocol_frame_queue_clear();
    g_input_len = 0U;
    g_input_pos = 0U;
    g_read_calls = 0U;
    g_logged_frames = 0U;
    for (unsigned i = 0; i < frames; i++) {
        uint8_t payload[] = { (uint8_t)i, (uint8_t)(i >> 8) };
        int size = protocol_frame_build(g_input + g_input_len,
                                        sizeof(g_input) - g_input_len,
                                        0x49, payload, sizeof(payload));
        assert(size == 7);
        g_input_len += (size_t)size;
    }
}

static void *notify_thread(void *arg)
{
    (void)arg;
    sleep_ms(2);
    app_runtime_wakeup_notify();
    return NULL;
}

static void test_wakeup(void)
{
    uint64_t token = app_runtime_wakeup_snapshot();
    uint64_t started;
    pthread_t producer;

    app_runtime_wakeup_notify();
    started = app_clock_monotonic_us();
    app_runtime_wakeup_wait_since(token, 1000U);
    assert(app_clock_monotonic_us() - started < 100000U);

    token = app_runtime_wakeup_snapshot();
    assert(pthread_create(&producer, NULL, notify_thread, NULL) == 0);
    app_runtime_wakeup_wait_since(token, 10U);
    pthread_join(producer, NULL);
    assert(app_runtime_wakeup_snapshot() != token);

    token = app_runtime_wakeup_snapshot();
    started = app_clock_monotonic_us();
    app_runtime_wakeup_wait_since(token, 1000U);
    /* Scheduling tolerance is intentionally wide; this is a safety bound test,
     * not a target-device performance measurement. */
    assert(app_clock_monotonic_us() - started < 200000U);
}

static void test_queue_capacity(void)
{
    uint8_t frame[7];
    uint8_t payload[] = { 3, 4 };
    protocol_frame_t received;
    uint64_t token;

    assert(protocol_frame_build(frame, sizeof(frame), 0x49, payload, 2) == 7);
    protocol_frame_queue_clear();
    assert(!protocol_frame_queue_has_pending());
    token = app_runtime_wakeup_snapshot();
    for (unsigned i = 0; i < PROTOCOL_FRAME_QUEUE_CAPACITY; i++) {
        assert(protocol_frame_queue_push(frame, sizeof(frame)));
    }
    assert(app_runtime_wakeup_snapshot() != token);
    assert(!protocol_frame_queue_push(frame, sizeof(frame)));
    assert(!protocol_frame_queue_push_wait(frame, sizeof(frame), 1U));
    assert(protocol_frame_queue_pop(&received));
    assert(protocol_frame_queue_push_wait(frame, sizeof(frame), 1U));
    assert(!protocol_frame_queue_push(NULL, 7));
    protocol_frame_queue_clear();
}

static void test_rx_order_and_backpressure(void)
{
    uint64_t started = app_clock_monotonic_us();

    reset_input(TEST_FRAMES);
    assert(protocol_rx_service_start(1, 2));
    /* Allow a real full queue, then release the consumer. The pending parser
     * frame must survive this interval without overwrite, duplication or loss. */
    sleep_ms(35);
    uint64_t pending_observed_ms = app_clock_monotonic_ms();
    assert(protocol_rx_service_has_command_between(0x49, 0, pending_observed_ms + 1));
    assert(!protocol_rx_service_has_command_between(0x38, 0, pending_observed_ms + 1));
    assert(!protocol_rx_service_has_command_between(0x49, pending_observed_ms + 1, pending_observed_ms + 100));
    bool overflow_frame_seen = false;
    for (unsigned i = 0; i < TEST_FRAMES;) {
        protocol_frame_t frame;
        uint64_t token = app_runtime_wakeup_snapshot();

        if (!protocol_frame_queue_pop(&frame)) {
            assert(app_clock_monotonic_us() - started < 5000000U);
            app_runtime_wakeup_wait_since(token, 10U);
            continue;
        }
        assert(frame.len == 7U && frame.data[3] == 0x49);
        assert(frame.data[4] == (uint8_t)i);
        assert(frame.data[5] == (uint8_t)(i >> 8));
        if (i == PROTOCOL_FRAME_QUEUE_CAPACITY) {
            overflow_frame_seen = true;
            assert(frame.received_ms <= pending_observed_ms);
        }
        i++;
    }
    protocol_rx_service_stop();
    assert(!protocol_frame_queue_has_pending());
    assert(g_logged_frames == TEST_FRAMES && overflow_frame_seen);
    assert(g_read_calls < g_input_len / 2U);
    assert(!protocol_rx_service_has_command_between(0x49, 0, UINT64_MAX));
}

static void test_stop_when_full(void)
{
    uint64_t started;

    reset_input(TEST_FRAMES);
    assert(protocol_rx_service_start(1, 2));
    sleep_ms(35);
    assert(protocol_rx_service_has_command_between(0x49, 0, UINT64_MAX));
    started = app_clock_monotonic_us();
    protocol_rx_service_stop();
    assert(app_clock_monotonic_us() - started < 200000U);
    assert(!protocol_rx_service_has_command_between(0x49, 0, UINT64_MAX));
    protocol_frame_queue_clear();
    /* An empty restarted service must not expose its predecessor's metadata. */
    reset_input(0);
    assert(protocol_rx_service_start(1, 2));
    assert(!protocol_rx_service_has_command_between(0x49, 0, UINT64_MAX));
    protocol_rx_service_stop();
}

static bool pending_reply(uint8_t command, uint64_t started_ms, uint64_t deadline_ms)
{
    /* Probe the producer before the queue to preserve its handoff observation. */
    return protocol_rx_service_has_command_between(command, started_ms, deadline_ms) ||
           protocol_frame_queue_has_command_between(command, started_ms, deadline_ms);
}

static void test_waiting_rx_ack_deadline(void)
{
    const uint8_t captured[] = {0xFD,0xDF,6,0x38,0,0x83};
    protocol_request_t request = PROTOCOL_REQUEST_INITIALIZER(5000);
    reset_input(PROTOCOL_FRAME_QUEUE_CAPACITY);
    memcpy(g_input + g_input_len, captured, sizeof(captured));
    g_input_len += sizeof(captured);
    protocol_request_set_pending_reply_probe(pending_reply);
    assert(protocol_request_begin_for_command(&request, 0x38));
    assert(protocol_rx_service_start(1, 2));
    uint64_t observed_ms;
    for (;;) {
        observed_ms = app_clock_monotonic_ms();
        if (protocol_rx_service_has_command_between(0x38, request.started_ms, observed_ms + 1)) break;
        assert(observed_ms - request.started_ms < 1000);
        sleep_ms(1);
    }
    /* Use the observed complete-frame bound to avoid scheduler timing assumptions. */
    request.timeout_ms = (uint32_t)(observed_ms - request.started_ms + 1);
    sleep_ms(5);
    assert(protocol_request_is_expired(&request));
    assert(!protocol_frame_queue_has_command_between(0x38, request.started_ms,
                                                     request.started_ms + request.timeout_ms));
    assert(!protocol_request_take_timeout(&request)); /* only waiting RX can defer */
    for (unsigned i = 0; i < PROTOCOL_FRAME_QUEUE_CAPACITY; i++) {
        protocol_frame_t frame;
        assert(protocol_frame_queue_pop(&frame));
        assert(frame.data[3] == 0x49 && frame.data[4] == (uint8_t)i);
    }
    protocol_frame_t ack;
    for (;;) {
        if (protocol_frame_queue_pop(&ack)) break;
        assert(app_clock_monotonic_ms() - observed_ms < 1000);
        assert(!protocol_request_take_timeout(&request));
        sleep_ms(1);
    }
    assert(ack.len == sizeof(captured) && !memcmp(ack.data, captured, sizeof(captured)));
    assert(ack.received_ms <= observed_ms);
    protocol_request_result_context_begin(ack.received_ms);
    assert(protocol_request_take_result(&request));
    protocol_request_result_context_end();
    protocol_rx_service_stop();
    protocol_request_set_pending_reply_probe(NULL);
    assert(!protocol_frame_queue_has_pending());
    assert(!protocol_rx_service_has_command_between(0x38, 0, UINT64_MAX));
    puts("PASS full queue: timely CRC83 ACK waits in RX, defers timeout, retains FIFO and original arrival time");
}

int main(void)
{
    test_wakeup();
    test_queue_capacity();
    test_rx_order_and_backpressure();
    test_stop_when_full();
    test_waiting_rx_ack_deadline();
    puts("protocol pipeline tests passed");
    return 0;
}
