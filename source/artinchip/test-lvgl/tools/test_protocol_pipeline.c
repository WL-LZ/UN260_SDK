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
    assert(prefix != NULL && data != NULL && len == 7U && preview_limit == 32U);
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
        i++;
    }
    protocol_rx_service_stop();
    assert(!protocol_frame_queue_has_pending());
    assert(g_logged_frames == TEST_FRAMES);
    assert(g_read_calls < g_input_len / 2U);
}

static void test_stop_when_full(void)
{
    uint64_t started;

    reset_input(TEST_FRAMES);
    assert(protocol_rx_service_start(1, 2));
    sleep_ms(35);
    started = app_clock_monotonic_us();
    protocol_rx_service_stop();
    assert(app_clock_monotonic_us() - started < 200000U);
    protocol_frame_queue_clear();
}

int main(void)
{
    test_wakeup();
    test_queue_capacity();
    test_rx_order_and_backpressure();
    test_stop_when_full();
    puts("protocol pipeline tests passed");
    return 0;
}
