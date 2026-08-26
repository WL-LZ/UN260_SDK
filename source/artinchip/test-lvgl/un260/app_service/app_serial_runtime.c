#include "app_serial_runtime.h"

#include <stdio.h>

#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_drivers/uart_bridge_service.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_frame_queue.h"
#include "un260/protocol/protocol_rx_service.h"
#include "un260/protocol/protocol_send.h"

#define APP_UART_CONTROLLER_DEVICE "/dev/ttyS4"
#define APP_UART_BRIDGE_DEVICE     "/dev/ttyS5"
#define APP_UART_LOG_DEVICE        "/dev/ttyS6"
#define APP_UART_BAUD              115200

static int fd4 = -1;
static int fd5 = -1;
static int fd6 = -1;

static bool g_rx_started;
static bool g_bridge_started;

bool protocol_send_is_ready(void)
{
    return fd4 >= 0;
}

int protocol_send(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len)
{
    uint8_t buf[PROTOCOL_FRAME_MAX_SIZE];
    char log_prefix[40];
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

    snprintf(log_prefix, sizeof(log_prefix), "Send CMD 0x%02X: ", cmd_g);
    uart_log_hex(fd6, log_prefix, buf, (size_t)frame_len, (size_t)frame_len);

    return uart_send(fd4, (const char *)buf, frame_len);
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

    if (uart_config(fd4, APP_UART_BAUD, 8, 'N', 1) < 0 ||
        uart_config(fd5, APP_UART_BAUD, 8, 'N', 1) < 0 ||
        uart_config(fd6, APP_UART_BAUD, 8, 'N', 1) < 0) {
        printf("UART配置失败\n");
        app_serial_runtime_stop();
        return false;
    }
    printf("UART配置完成\n");
    uart_debug_set_fd(fd6);

    protocol_frame_queue_clear();
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

    uart_printf(fd6, "UART6 ready\n");
    return true;
}

void app_serial_runtime_stop(void)
{
    if (g_bridge_started) {
        uart_bridge_service_stop();
        g_bridge_started = false;
    }
    if (g_rx_started) {
        protocol_rx_service_stop();
        g_rx_started = false;
    }
    protocol_frame_queue_clear();
    app_serial_runtime_close_devices();
}

bool app_serial_runtime_is_started(void)
{
    return g_rx_started && g_bridge_started &&
           fd4 >= 0 && fd5 >= 0 && fd6 >= 0;
}
