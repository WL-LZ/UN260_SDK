#include "protocol_rx_service.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "protocol_frame_parser.h"
#include "protocol_frame_queue.h"
#include "un260/lv_drivers/lv_drivers.h"

static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_thread;
static bool g_started;
static bool g_running;
static int g_uart_fd = -1;
static int g_log_fd = -1;

#define PROTOCOL_RX_LOG_PREVIEW_BYTES 32U
#define PROTOCOL_RX_DROP_REPORT_STEP  100U
#define PROTOCOL_RX_INVALID_REPORT_STEP 100U
#define PROTOCOL_RX_ERROR_REPORT_STEP 100U
#define PROTOCOL_RX_ERROR_BACKOFF_US  10000U

static bool protocol_rx_service_should_run(void)
{
    bool running;

    pthread_mutex_lock(&g_state_mutex);
    running = g_running;
    pthread_mutex_unlock(&g_state_mutex);
    return running;
}

static void *protocol_rx_service_thread(void *arg)
{
    protocol_frame_parser_t parser;
    protocol_frame_view_t frame;
    uint8_t byte;
    unsigned int dropped_frames = 0;
    unsigned int invalid_frames = 0;
    unsigned int receive_errors = 0;

    (void)arg;
    protocol_frame_parser_init(&parser);

    while (protocol_rx_service_should_run()) {
        int len = uart_recv(g_uart_fd, (char *)&byte, 1, 10);

        if (len < 0) {
            int error_code = errno;

            receive_errors++;
            if (receive_errors == 1 ||
                (receive_errors % PROTOCOL_RX_ERROR_REPORT_STEP) == 0) {
                uart_printf(g_log_fd,
                            "UART4: receive failed errno=%d count=%u\n",
                            error_code,
                            receive_errors);
            }
            usleep(PROTOCOL_RX_ERROR_BACKOFF_US);
            continue;
        }
        if (receive_errors > 0) {
            uart_printf(g_log_fd,
                        "UART4: receive recovered, errors=%u\n",
                        receive_errors);
            receive_errors = 0;
        }

        if (len > 0) {
            protocol_frame_parse_result_t result;

            result = protocol_frame_parser_feed(&parser, byte, &frame);
            if (result == PROTOCOL_FRAME_PARSE_READY) {
                if (!protocol_frame_queue_push(frame.data, frame.len)) {
                    dropped_frames++;
                    if (dropped_frames == 1 ||
                        (dropped_frames % PROTOCOL_RX_DROP_REPORT_STEP) == 0) {
                        uart_printf(g_log_fd,
                                    "UART4: queue full, dropped=%u\n",
                                    dropped_frames);
                    }
                } else {
                    char prefix[32];

                    snprintf(prefix, sizeof(prefix), "RX[%u]: ",
                             (unsigned int)frame.len);
                    uart_log_hex(g_log_fd, prefix, frame.data, frame.len,
                                 PROTOCOL_RX_LOG_PREVIEW_BYTES);
                    if (dropped_frames > 0) {
                        uart_printf(g_log_fd,
                                    "UART4: queue recovered, dropped=%u\n",
                                    dropped_frames);
                        dropped_frames = 0;
                    }
                }
            } else if (result != PROTOCOL_FRAME_PARSE_INCOMPLETE) {
                invalid_frames++;
                if (invalid_frames == 1 ||
                    (invalid_frames % PROTOCOL_RX_INVALID_REPORT_STEP) == 0) {
                    uart_printf(g_log_fd,
                                "UART4: invalid frame result=%d count=%u\n",
                                result,
                                invalid_frames);
                }
            }
        }
        usleep(100);
    }

    if (dropped_frames > 0) {
        uart_printf(g_log_fd, "UART4: receive stopped, dropped=%u\n", dropped_frames);
    }
    if (invalid_frames > 0) {
        uart_printf(g_log_fd, "UART4: receive stopped, invalid=%u\n", invalid_frames);
    }
    if (receive_errors > 0) {
        uart_printf(g_log_fd, "UART4: receive stopped, errors=%u\n", receive_errors);
    }

    return NULL;
}

bool protocol_rx_service_start(int uart_fd, int log_fd)
{
    int create_result;

    if (uart_fd < 0 || log_fd < 0) {
        return false;
    }

    pthread_mutex_lock(&g_state_mutex);
    if (g_started) {
        pthread_mutex_unlock(&g_state_mutex);
        return false;
    }

    g_uart_fd = uart_fd;
    g_log_fd = log_fd;
    g_running = true;
    create_result = pthread_create(&g_thread, NULL, protocol_rx_service_thread, NULL);
    if (create_result != 0) {
        g_running = false;
        g_uart_fd = -1;
        g_log_fd = -1;
        pthread_mutex_unlock(&g_state_mutex);
        return false;
    }
    g_started = true;
    pthread_mutex_unlock(&g_state_mutex);

    return true;
}

void protocol_rx_service_stop(void)
{
    pthread_t thread;

    pthread_mutex_lock(&g_state_mutex);
    if (!g_started) {
        pthread_mutex_unlock(&g_state_mutex);
        return;
    }
    g_running = false;
    thread = g_thread;
    pthread_mutex_unlock(&g_state_mutex);

    pthread_join(thread, NULL);

    pthread_mutex_lock(&g_state_mutex);
    g_started = false;
    g_uart_fd = -1;
    g_log_fd = -1;
    pthread_mutex_unlock(&g_state_mutex);
}
