#include "app_runtime_wakeup.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

static pthread_mutex_t g_wakeup_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t g_wakeup_once = PTHREAD_ONCE_INIT;
static pthread_cond_t g_wakeup_cond;
static bool g_wakeup_ready;
static uint64_t g_wakeup_sequence;

static void app_runtime_wakeup_init(void)
{
    pthread_condattr_t attr;

    if (pthread_condattr_init(&attr) != 0) return;
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0 &&
        pthread_cond_init(&g_wakeup_cond, &attr) == 0) {
        g_wakeup_ready = true;
    }
    pthread_condattr_destroy(&attr);
}

uint64_t app_runtime_wakeup_snapshot(void)
{
    uint64_t sequence;

    pthread_mutex_lock(&g_wakeup_mutex);
    sequence = g_wakeup_sequence;
    pthread_mutex_unlock(&g_wakeup_mutex);
    return sequence;
}

void app_runtime_wakeup_notify(void)
{
    pthread_once(&g_wakeup_once, app_runtime_wakeup_init);
    pthread_mutex_lock(&g_wakeup_mutex);
    g_wakeup_sequence++;
    if (g_wakeup_ready) pthread_cond_broadcast(&g_wakeup_cond);
    pthread_mutex_unlock(&g_wakeup_mutex);
}

void app_runtime_wakeup_wait_since(uint64_t sequence, uint32_t max_wait_ms)
{
    struct timespec deadline;

    if (max_wait_ms == 0U) return;
    if (max_wait_ms > APP_RUNTIME_MAX_WAIT_MS) {
        max_wait_ms = APP_RUNTIME_MAX_WAIT_MS;
    }
    pthread_once(&g_wakeup_once, app_runtime_wakeup_init);
    /* Failing open preserves responsiveness if condition setup is unavailable;
     * never substitute a wall-clock deadline that an RTC change can extend. */
    if (!g_wakeup_ready || clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return;
    deadline.tv_nsec += (long)max_wait_ms * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&g_wakeup_mutex);
    while (g_wakeup_sequence == sequence) {
        int result = pthread_cond_timedwait(&g_wakeup_cond, &g_wakeup_mutex,
                                            &deadline);
        if (result != 0 && result != EINTR) break;
    }
    pthread_mutex_unlock(&g_wakeup_mutex);
}
