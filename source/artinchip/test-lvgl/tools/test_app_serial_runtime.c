/* Production runtime with hardware/service edges stubbed; no real UART opens. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "un260/app_service/app_serial_runtime.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_drivers/uart_bridge_service.h"
#include "un260/protocol/protocol_rx_service.h"
#include "un260/protocol/protocol_frame_queue.h"
#include "un260/protocol/protocol_send.h"
#include "un260/protocol/protocol_request.h"
#include "un260/app_service/buzzer_service.h"

static uint64_t fake_ms = 1000;
uint64_t app_clock_monotonic_ms(void){return fake_ms;}
static protocol_rx_observer_t observer;
void protocol_rx_service_set_observer(protocol_rx_observer_t cb){observer=cb;}
bool buzzer_service_start(void){return true;}
void buzzer_service_stop(void){}
void buzzer_service_on_reply(uint8_t status){(void)status;}
static unsigned opened, configured, closed, rx_starts, bridge_starts, rx_stops, bridge_stops;
static int fail_config_fd = -1, debug_fd = -1;
static bool rx_ok = true, bridge_ok = true;

int uart_open(const char *device)
{
    const char *names[] = {"/dev/ttyS4", "/dev/ttyS5", "/dev/ttyS6"};
    assert(opened < 3 && !strcmp(device, names[opened]));
    return 4 + opened++;
}
int uart_config(int fd, int baud, int bits, char parity, int stop)
{
    assert(fd == 4 + (int)configured++);
    assert(baud == (fd == 4 ? 512000 : 115200));
    assert(bits == 8 && parity == 'N' && stop == 1);
    return fd == fail_config_fd ? -1 : 0;
}
void uart_close(int fd) { assert(fd == 4 + (int)closed++); }
void uart_debug_set_fd(int fd) { debug_fd = fd; }
void uart_printf(int fd, const char *fmt, ...) { (void)fd; (void)fmt; }
void uart_log_hex(int fd, const char *prefix, const uint8_t *data, size_t len, size_t limit)
{ (void)fd; (void)prefix; (void)data; (void)len; (void)limit; }
void uart_tx_reset(int fd) { assert(fd == 4); }
bool uart_tx_is_healthy(int fd) { return fd == 4; }
int uart_send(int fd, const char *data, int len)
{ assert(fd == 4 && len >= 6 && (uint8_t)data[0] == 0xFD); return len; }
static protocol_request_pending_reply_probe_t pending_probe;
static bool deferred_reply, queue_reply, rx_reply;
static unsigned deferred_probes, rx_probes, queue_probes;
bool runtime_guard_has_fault(void) { return false; }
void protocol_request_set_pending_reply_probe(protocol_request_pending_reply_probe_t probe)
{ pending_probe = probe; }
bool app_command_runtime_has_command_between(uint8_t command, uint64_t started_ms, uint64_t deadline_ms)
{ (void)command; (void)started_ms; (void)deadline_ms; deferred_probes++; return deferred_reply; }
bool protocol_frame_queue_has_command_between(uint8_t command, uint64_t started_ms, uint64_t deadline_ms)
{ (void)command; (void)started_ms; (void)deadline_ms; queue_probes++; return queue_reply; }
bool protocol_rx_service_has_command_between(uint8_t command, uint64_t started_ms, uint64_t deadline_ms)
{ (void)command; (void)started_ms; (void)deadline_ms; rx_probes++; return rx_reply; }
void protocol_frame_queue_clear(void) {}
bool protocol_rx_service_start(int fd, int log_fd)
{ assert(fd == 4 && log_fd == 6 && configured == 3); ++rx_starts; return rx_ok; }
void protocol_rx_service_stop(void) { ++rx_stops; }
bool uart_bridge_service_start(int source, int target, int log_fd)
{ assert(source == 5 && target == 4 && log_fd == 6); ++bridge_starts; return bridge_ok; }
void uart_bridge_service_stop(void) { ++bridge_stops; }

static void reset(void)
{
    assert(!pending_probe);
    assert(!app_serial_runtime_is_started() && !protocol_send_is_ready());
    opened = configured = closed = rx_starts = bridge_starts = rx_stops = bridge_stops = 0;
    fail_config_fd = -1;
    rx_ok = bridge_ok = true;
}

int main(void)
{
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        reset();
        assert(app_serial_runtime_start());
        assert(pending_probe && !pending_probe(0x38, 100, 200));
        deferred_probes = rx_probes = queue_probes = 0;
        deferred_reply = true; assert(pending_probe(0x0A, 100, 5100));
        assert(deferred_probes == 1 && rx_probes == 0 && queue_probes == 0);
        deferred_reply = false;
        rx_reply = true; assert(pending_probe(0x38, 100, 200)); rx_reply = false;
        assert(deferred_probes == 2 && rx_probes == 1 && queue_probes == 0);
        queue_reply = true; assert(pending_probe(0x38, 100, 200)); queue_reply = false;
        assert(app_serial_runtime_is_started() && protocol_send_is_ready() && debug_fd == 6);
        assert(opened == 3 && configured == 3 && rx_starts == 1 && bridge_starts == 1);
        assert(!app_serial_runtime_start() && opened == 3);
        const uint8_t sub = 1;
        /* Startup resets the lane, each business request gates optional ON,
           and only the matching RX reply releases it after settling. */
        assert(protocol_send_if_idle(0x4A, &sub, 1) == 6);
        assert(protocol_send(0x01, &sub, 1) == 6);
        fake_ms += 50;
        assert(protocol_send_if_idle(0x4A, &sub, 1) == 0);
        uint8_t reply[]={0xFD,0xDF,6,0x01,1,0x0A};assert(observer);observer(reply,6);
        assert(protocol_send_if_idle(0x4A, &sub, 1) == 0);
        fake_ms += 10;assert(protocol_send_if_idle(0x4A, &sub, 1) == 6);
        app_serial_runtime_stop();
        assert(!observer);
        assert(closed == 3 && rx_stops == 1 && bridge_stops == 1 && debug_fd == -1 && !pending_probe);
        app_serial_runtime_stop();
        assert(closed == 3);
    }
    for (int fd = 4; fd <= 6; ++fd) {
        reset(); fail_config_fd = fd;
        assert(!app_serial_runtime_start());
        assert(closed == 3 && configured == (unsigned)(fd - 3));
        assert(rx_starts == 0 && bridge_starts == 0 && debug_fd == -1);
    }
    reset(); rx_ok = false;
    assert(!app_serial_runtime_start() && closed == 3 && bridge_starts == 0);
    reset(); bridge_ok = false;
    assert(!app_serial_runtime_start() && closed == 3 && rx_stops == 1);
    reset();
    puts("PASS serial runtime: ttyS4 requests 512000, ttyS5/6=115200, restart and failure cleanup without silent baud fallback");
    return 0;
}
