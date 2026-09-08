#include "protocol_frame_queue.h"

#include <pthread.h>
#include <string.h>
#include <time.h>

#include "protocol_frame.h"
#include "un260/app_service/app_runtime_wakeup.h"

static protocol_frame_t g_frames[PROTOCOL_FRAME_QUEUE_CAPACITY];
static int g_head;
static int g_tail;
static int g_count;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t g_space_once = PTHREAD_ONCE_INIT;
static pthread_cond_t g_space_cond;
static bool g_space_ready;

static void protocol_frame_queue_space_init(void)
{
    pthread_condattr_t attr;

    if (pthread_condattr_init(&attr) != 0) return;
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0 &&
        pthread_cond_init(&g_space_cond, &attr) == 0) {
        g_space_ready = true;
    }
    pthread_condattr_destroy(&attr);
}

bool protocol_frame_queue_push(const uint8_t *data, int len)
{
    return protocol_frame_queue_push_wait(data, len, 0U);
}

bool protocol_frame_queue_push_wait(const uint8_t *data, int len,
                                    uint32_t timeout_ms)
{
    bool pushed = false;
    struct timespec deadline;

    if (len < 0 || !protocol_frame_is_valid(data, (size_t)len)) {
        return false;
    }

    pthread_once(&g_space_once, protocol_frame_queue_space_init);
    if (timeout_ms > APP_RUNTIME_MAX_WAIT_MS) timeout_ms = APP_RUNTIME_MAX_WAIT_MS;
    if (!g_space_ready || clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
        timeout_ms = 0U;
    } else {
        deadline.tv_nsec += (long)timeout_ms * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
    }

    pthread_mutex_lock(&g_mutex);
    while (g_count >= PROTOCOL_FRAME_QUEUE_CAPACITY && timeout_ms > 0U) {
        if (pthread_cond_timedwait(&g_space_cond, &g_mutex, &deadline) != 0) break;
    }
    if (g_count < PROTOCOL_FRAME_QUEUE_CAPACITY) {
        memcpy(g_frames[g_tail].data, data, (size_t)len);
        g_frames[g_tail].len = (uint8_t)len;
        g_tail = (g_tail + 1) % PROTOCOL_FRAME_QUEUE_CAPACITY;
        g_count++;
        pushed = true;
    }
    pthread_mutex_unlock(&g_mutex);

    if (pushed) app_runtime_wakeup_notify();

    return pushed;
}

bool protocol_frame_queue_pop(protocol_frame_t *frame)
{
    bool popped = false;

    if (frame == NULL) {
        return false;
    }

    pthread_mutex_lock(&g_mutex);
    if (g_count > 0) {
        *frame = g_frames[g_head];
        g_head = (g_head + 1) % PROTOCOL_FRAME_QUEUE_CAPACITY;
        g_count--;
        popped = true;
        if (g_space_ready) pthread_cond_signal(&g_space_cond);
    }
    pthread_mutex_unlock(&g_mutex);

    return popped;
}

bool protocol_frame_queue_has_pending(void)
{
    bool pending;

    pthread_mutex_lock(&g_mutex);
    pending = g_count > 0;
    pthread_mutex_unlock(&g_mutex);
    return pending;
}

void protocol_frame_queue_clear(void)
{
    pthread_once(&g_space_once, protocol_frame_queue_space_init);
    pthread_mutex_lock(&g_mutex);
    g_head = 0;
    g_tail = 0;
    g_count = 0;
    if (g_space_ready) pthread_cond_broadcast(&g_space_cond);
    pthread_mutex_unlock(&g_mutex);
}
