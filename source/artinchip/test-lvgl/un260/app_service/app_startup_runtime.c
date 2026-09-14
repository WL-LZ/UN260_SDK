#include "app_startup_runtime.h"
#include "app_serial_runtime.h"
#include "un260/lv_drivers/startup_devices.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/ui_history_data.h"
#include <pthread.h>
#include <stdatomic.h>

#define STARTUP_TIMEOUT_MS 15000U

static pthread_t g_thread;
static atomic_int g_worker_result = ATOMIC_VAR_INIT(APP_STARTUP_WAITING);
static atomic_bool g_cancel = ATOMIC_VAR_INIT(false);
static user_cfg_startup_snapshot_t g_snapshot;
static uint32_t g_started_ms;
static bool g_started, g_joined, g_timed_out, g_serial_ready;
static app_startup_status_t g_result = APP_STARTUP_WAITING;

static void *startup_worker(void *unused)
{
    (void)unused;
    user_cfg_startup_read(&g_snapshot);
    bool serial_ready = false;
    if (!atomic_load_explicit(&g_cancel, memory_order_acquire) &&
        startup_devices_prepare() &&
        !atomic_load_explicit(&g_cancel, memory_order_acquire))
        serial_ready = app_serial_runtime_start();
    if (atomic_load_explicit(&g_cancel, memory_order_acquire)) {
        if (serial_ready) app_serial_runtime_stop();
        serial_ready = false;
    }
    atomic_store_explicit(&g_worker_result,
        serial_ready ? APP_STARTUP_READY : APP_STARTUP_FAILED, memory_order_release);
    return NULL;
}

bool app_startup_runtime_begin(uint32_t now_ms)
{
    if (g_started) return false;
    g_started = true;
    g_started_ms = now_ms;
    ui_history_data_init_async();
    if (pthread_create(&g_thread, NULL, startup_worker, NULL) != 0) {
        g_joined = true;
        g_result = APP_STARTUP_FAILED;
        return false;
    }
    return true;
}

app_startup_status_t app_startup_runtime_poll(uint32_t now_ms)
{
    if (!g_started) return g_result;
    ui_history_data_init_poll();
    if (!g_joined) {
        int worker_result = atomic_load_explicit(&g_worker_result, memory_order_acquire);
        if (worker_result != APP_STARTUP_WAITING) {
            /* Only join after all worker I/O. Never cancel inside stdio or
             * termios, or free data still owned by a blocked worker. */
            pthread_join(g_thread, NULL);
            g_joined = true;
            g_serial_ready = worker_result == APP_STARTUP_READY;
            if (!g_timed_out) {
                user_cfg_startup_apply(&g_snapshot);
                if (!g_serial_ready) g_result = APP_STARTUP_FAILED;
            }
        }
    }
    if (g_result == APP_STARTUP_WAITING) {
        if (g_joined && g_serial_ready && ui_history_data_is_initialized())
            g_result = APP_STARTUP_READY;
        else if ((uint32_t)(now_ms - g_started_ms) >= STARTUP_TIMEOUT_MS) {
            g_timed_out = true;
            atomic_store_explicit(&g_cancel, true, memory_order_release);
            g_result = APP_STARTUP_TIMED_OUT;
        }
    }
    if (g_timed_out && g_joined && g_serial_ready) {
        /* Covers cancellation racing with the final successful publish. */
        app_serial_runtime_stop();
        g_serial_ready = false;
    }
    return g_result;
}

bool app_startup_runtime_is_settled(void) { return g_joined; }

bool app_startup_runtime_can_process(void)
{
    return g_joined && g_serial_ready &&
        (g_result == APP_STARTUP_WAITING || g_result == APP_STARTUP_READY);
}
