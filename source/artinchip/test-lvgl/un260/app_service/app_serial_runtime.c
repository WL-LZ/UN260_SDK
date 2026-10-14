#include "app_serial_runtime.h"
#include "un260/storage/protocol_log.h"
#include "buzzer_service.h"
#include "app_command_runtime.h"
#include "runtime_guard.h"

#include <stdio.h>

#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_drivers/uart_bridge_service.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_frame_queue.h"
#include "un260/protocol/protocol_rx_service.h"
#include "un260/protocol/protocol_send.h"
#include "un260/protocol/protocol_tx_gate.h"
#include "un260/protocol/protocol_request.h"

#define APP_UART_CONTROLLER_DEVICE "/dev/ttyS4"
#define APP_UART_BRIDGE_DEVICE     "/dev/ttyS5"
#define APP_UART_LOG_DEVICE        "/dev/ttyS6"
#define APP_UART_CONTROLLER_BAUD   512000
#define APP_UART_BRIDGE_BAUD       115200
#define APP_UART_LOG_BAUD          115200

static int fd4 = -1;
static int fd5 = -1;
static int fd6 = -1;

static void app_serial_runtime_observe_reply(const uint8_t *frame, uint16_t len)
{
    protocol_tx_gate_on_reply(frame[3]);
    if(len == 6 && frame[3] == 0x4A && frame[4] <= 2)
        buzzer_service_on_reply(frame[4]);
}
static bool g_rx_started;
static bool g_bridge_started;

static bool app_serial_runtime_reply_pending(uint8_t command,
                                              uint64_t started_ms,
                                              uint64_t deadline_ms)
{
    /* UI may retain a reply after dequeuing it for history preflight. RX
     * publishes before queueing; observe every owner without reordering. */
    return app_command_runtime_has_command_between(command, started_ms, deadline_ms) ||
           protocol_rx_service_has_command_between(command, started_ms, deadline_ms) ||
           protocol_frame_queue_has_command_between(command, started_ms, deadline_ms);
}

bool protocol_send_is_ready(void)
{
    return fd4 >= 0 && uart_tx_is_healthy(fd4) && !runtime_guard_has_fault();
}

static int protocol_write_frame(const uint8_t *buf, uint16_t frame_len)
{
    char log_prefix[40];
    snprintf(log_prefix, sizeof(log_prefix), "Send CMD 0x%02X: ", buf[3]);
    uart_log_hex(fd6, log_prefix, buf, frame_len, frame_len);
    return uart_send(fd4, (const char *)buf, frame_len);
}
static int protocol_send_frame(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len, bool optional)
{
    uint8_t buf[PROTOCOL_FRAME_MAX_SIZE];
    int frame_len;

    if (!protocol_send_is_ready()) {
        uart_printf(fd6, "Send CMD: 0x%02X, UART4 not ready\n", cmd_g);
        return -1;
    }

    frame_len = protocol_frame_build(buf, sizeof(buf), cmd_g, cmd_s, cmd_s_len);
    if (frame_len < 0) {
        uart_printf(fd6, "Send CMD: 0x%02X, invalid payload len=%u\n", cmd_g, (unsigned int)cmd_s_len);
        return -1;
    }

    return protocol_tx_gate_send(buf, (uint16_t)frame_len, optional, protocol_write_frame);
}
int protocol_send(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len)
{
    return protocol_send_frame(cmd_g,cmd_s,cmd_s_len,false);
}
int protocol_send_if_idle(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len)
{
    return protocol_send_frame(cmd_g,cmd_s,cmd_s_len,true);
}

static void app_serial_runtime_close_devices(void)
{
    uart_debug_set_fd(-1);
    if (fd4 >= 0) {
        uart_close(fd4);
        fd4 = -1;
    }
    if (fd5 >= 0) {
        uart_close(fd5);
        fd5 = -1;
    }
    if (fd6 >= 0) {
        uart_close(fd6);
        fd6 = -1;
    }
}

bool app_serial_runtime_start(void)
{
    if (app_serial_runtime_is_started() || fd4 >= 0 || fd5 >= 0 || fd6 >= 0) {
        return false;
    }

    printf("=== 初始化UART4、UART5和UART6 ===\n");
    fd4 = uart_open(APP_UART_CONTROLLER_DEVICE);
    fd5 = uart_open(APP_UART_BRIDGE_DEVICE);
    fd6 = uart_open(APP_UART_LOG_DEVICE);
    if (fd4 < 0 || fd5 < 0 || fd6 < 0) {
        printf("UART打开失败: fd4=%d fd5=%d fd6=%d\n", fd4, fd5, fd6);
        app_serial_runtime_stop();
        return false;
    }

    if (uart_config(fd4, APP_UART_CONTROLLER_BAUD, 8, 'N', 1) < 0 ||
        uart_config(fd5, APP_UART_BRIDGE_BAUD, 8, 'N', 1) < 0 ||
        uart_config(fd6, APP_UART_LOG_BAUD, 8, 'N', 1) < 0) {
        printf("UART配置失败\n");
        app_serial_runtime_stop();
        return false;
    }
    uart_tx_reset(fd4);
    printf("UART配置完成\n");
    uart_debug_set_fd(fd6);
    protocol_log_init();protocol_log_ports(fd4,fd5);uart_trace_set(protocol_log_record);

    protocol_frame_queue_clear();
    protocol_tx_gate_reset();
    protocol_request_set_pending_reply_probe(app_serial_runtime_reply_pending);
    protocol_rx_service_set_observer(app_serial_runtime_observe_reply);
    if (!protocol_rx_service_start(fd4, fd6)) {
        printf("UART4接收服务启动失败\n");
        app_serial_runtime_stop();
        return false;
    }
    g_rx_started = true;

    if (!uart_bridge_service_start(fd5, fd4, fd6)) {
        printf("UART5转发服务启动失败\n");
        app_serial_runtime_stop();
        return false;
    }
    g_bridge_started = true;
    /* Sound is optional; unavailable feedback must not prevent machine I/O. */
    if(!buzzer_service_start()) uart_printf(fd6, "Touch sound worker unavailable\n");

    uart_printf(fd6, "UART6 ready\n");
    return true;
}

void app_serial_runtime_stop(void)
{
    buzzer_service_stop();
    if (g_bridge_started) {
        uart_bridge_service_stop();
        g_bridge_started = false;
    }
    if (g_rx_started) {
        protocol_rx_service_stop();
        g_rx_started = false;
    }
    protocol_rx_service_set_observer(NULL);
    protocol_request_set_pending_reply_probe(NULL);
    protocol_frame_queue_clear();
    uart_trace_set(NULL);protocol_log_shutdown();protocol_log_ports(-1,-1);
    app_serial_runtime_close_devices();
}

bool app_serial_runtime_is_started(void)
{
    return g_rx_started && g_bridge_started &&
           fd4 >= 0 && fd5 >= 0 && fd6 >= 0;
}
