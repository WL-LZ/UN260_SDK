#include "counting_denom_query_service.h"

#include <stddef.h>
#include <string.h>

#include "un260/lv_drivers/lv_drivers.h"
#include "un260/protocol/protocol_send.h"

#define DENOM_QUERY_CMD           0x0B
#define DENOM_QUERY_SUBCMD        0x01
#define DENOM_QUERY_TIMEOUT_MS    1500
#define DENOM_QUERY_MAX_RETRY     2
#define DENOM_QUERY_IDLE_RETRY_MS 2500

static bool counting_denom_query_elapsed(uint32_t now_ms,
                                         uint32_t started_ms,
                                         uint32_t interval_ms)
{
    /* Signed modular subtraction tolerates the uint32_t clock wrap and also
     * rejects a timestamp sampled just before the request was started. */
    return (int32_t)(now_ms - started_ms) >= (int32_t)interval_ms;
}

static bool counting_denom_query_send(counting_detail_state_t *detail,
                                      uint32_t now_ms)
{
    const uint8_t subcmd = DENOM_QUERY_SUBCMD;

    detail->query_tick = now_ms;
    detail->query_idle_retry_tick = now_ms;
    detail->query_started = false;
    detail->query_complete = false;
    if (protocol_send(DENOM_QUERY_CMD, &subcmd, 1) < 0) {
        detail->query_pending = false;
        uart_debug_printf("request denom list send failed\n");
        return false;
    }

    detail->query_pending = true;
    uart_debug_printf("request denom list: 0x0B 0x01\n");
    return true;
}

void counting_denom_query_invalidate(counting_detail_state_t *detail)
{
    if (detail != NULL) {
        detail->query_started = false;
        detail->query_complete = false;
    }
}

bool counting_denom_query_mark_start(counting_detail_state_t *detail)
{
    if (detail == NULL || !detail->query_pending) {
        return false;
    }

    memset(detail->query_denom, 0, sizeof(detail->query_denom));
    detail->query_denom_number = 0;
    detail->query_started = true;
    return true;
}

bool counting_denom_query_accepts_data(const counting_detail_state_t *detail)
{
    return detail != NULL &&
           (!detail->query_pending || detail->query_started);
}

bool counting_denom_query_complete(counting_detail_state_t *detail)
{
    bool was_pending;

    if (detail == NULL) {
        return false;
    }

    was_pending = detail->query_pending;
    detail->query_pending = false;
    if (was_pending && detail->query_started) {
        detail->query_complete = true;
        detail->query_retry = 0;
    } else if (was_pending) {
        detail->query_complete = false;
        detail->query_idle_retry_tick = detail->query_tick;
        uart_debug_printf("0x0B query end ignored before start frame\n");
    }
    detail->query_started = false;
    return was_pending;
}

bool counting_denom_query_commit(const counting_detail_state_t *detail,
                                 counting_sim_t *sim_data)
{
    if (detail == NULL || sim_data == NULL || !detail->query_complete ||
        detail->query_denom_number > COUNTING_DENOM_MAX_ITEMS) {
        return false;
    }

    memcpy(sim_data->denom, detail->query_denom, sizeof(sim_data->denom));
    sim_data->denom_number = detail->query_denom_number;
    return true;
}

void counting_denom_query_trigger(counting_detail_state_t *detail,
                                  uint32_t now_ms,
                                  bool boot_ready)
{
    if (detail == NULL) {
        return;
    }

    detail->query_retry = 0;
    detail->query_pending = false;
    detail->query_started = false;
    detail->query_complete = false;
    if (!boot_ready) {
        detail->query_deferred = true;
        uart_debug_printf("defer denom query until boot done\n");
        return;
    }

    detail->query_deferred = false;
    counting_denom_query_send(detail, now_ms);
}

void counting_denom_query_poll(counting_detail_state_t *detail,
                               uint32_t now_ms,
                               bool boot_ready,
                               bool main_page_active)
{
    if (detail == NULL) {
        return;
    }

    if (detail->query_pending &&
        counting_denom_query_elapsed(now_ms, detail->query_tick,
                                     DENOM_QUERY_TIMEOUT_MS)) {
        bool stream_started = detail->query_started;

        detail->query_pending = false;
        detail->query_started = false;
        if (detail->query_retry < DENOM_QUERY_MAX_RETRY) {
            detail->query_retry++;
            uart_debug_printf("0x0B query %s, retry %u/%u\n",
                        stream_started ? "incomplete response" : "timeout",
                        detail->query_retry, DENOM_QUERY_MAX_RETRY);
            counting_denom_query_send(detail, now_ms);
            return;
        }

        detail->query_idle_retry_tick = now_ms;
        uart_debug_printf(                    "0x0B query %s, keep master-only mode (no local fallback)\n",
                    stream_started ? "incomplete response" : "timeout");
        return;
    }

    if (detail->query_deferred && !detail->query_pending && boot_ready) {
        detail->query_deferred = false;
        counting_denom_query_send(detail, now_ms);
        return;
    }

    if (!detail->query_pending && !detail->query_complete &&
        main_page_active && boot_ready &&
        counting_denom_query_elapsed(now_ms, detail->query_idle_retry_tick,
                                     DENOM_QUERY_IDLE_RETRY_MS)) {
        detail->query_idle_retry_tick = now_ms;
        detail->query_retry = 0;
        uart_debug_printf("0x0B idle retry on main page\n");
        counting_denom_query_send(detail, now_ms);
    }
}
