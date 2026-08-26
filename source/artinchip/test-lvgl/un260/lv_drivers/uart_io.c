#include "uart_io.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

static pthread_mutex_t g_uart_tx_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_uart_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_uart_debug_fd = -1;

#define UART_HEX_LOG_BUFFER_SIZE 1024U

static void uart_log_write_locked(int fd, const char *data, size_t data_len)
{
    int write_result;

    write_result = uart_write_all(fd, data, data_len);
    if (write_result != (int)data_len) {
        fprintf(stderr, "UART log write failed: %d/%zu\n", write_result, data_len);
    }
}

static void uart_log_write(int fd, const char *data, size_t data_len)
{
    pthread_mutex_lock(&g_uart_log_mutex);
    uart_log_write_locked(fd, data, data_len);
    pthread_mutex_unlock(&g_uart_log_mutex);
}

int uart_write_all(int fd, const void *data, size_t data_len)
{
    const uint8_t *bytes = data;
    size_t written = 0;

    if (fd < 0 || (data == NULL && data_len > 0) || data_len > INT_MAX) {
        errno = EINVAL;
        return -1;
    }

    while (written < data_len) {
        ssize_t result = write(fd, bytes + written, data_len - written);

        if (result > 0) {
            written += (size_t)result;
            continue;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }

        if (result == 0) {
            errno = EIO;
        }
        return -1;
    }

    return (int)written;
}

int uart_send(int fd, const char *send_buf, int data_len)
{
    int result;

    if (data_len < 0) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&g_uart_tx_mutex);
    result = uart_write_all(fd, send_buf, (size_t)data_len);
    pthread_mutex_unlock(&g_uart_tx_mutex);
    return result;
}

int uart_recv(int fd, char *recv_buf, int data_len, int timeout_ms)
{
    fd_set read_fds;
    struct timeval timeout;
    int select_result;
    ssize_t read_result;

    if (fd < 0 || fd >= FD_SETSIZE || recv_buf == NULL ||
        data_len <= 0 || timeout_ms < 0) {
        errno = EINVAL;
        return -1;
    }

    do {
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        select_result = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    } while (select_result < 0 && errno == EINTR);

    if (select_result <= 0) {
        return select_result;
    }

    do {
        read_result = read(fd, recv_buf, (size_t)data_len);
    } while (read_result < 0 && errno == EINTR);

    if (read_result < 0) {
        return -1;
    }
    return (int)read_result;
}

void uart_printf(int fd, const char *fmt, ...)
{
    char buf[256];
    va_list args;
    int formatted_len;
    size_t output_len;

    if (fd < 0 || fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    formatted_len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (formatted_len <= 0) {
        return;
    }

    output_len = (size_t)formatted_len;
    if (output_len >= sizeof(buf)) {
        output_len = sizeof(buf) - 1;
    }

    uart_log_write(fd, buf, output_len);
}

void uart_debug_set_fd(int fd)
{
    pthread_mutex_lock(&g_uart_log_mutex);
    g_uart_debug_fd = fd;
    pthread_mutex_unlock(&g_uart_log_mutex);
}

void uart_debug_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    int formatted_len;
    size_t output_len;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    formatted_len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (formatted_len <= 0) {
        return;
    }

    output_len = (size_t)formatted_len;
    if (output_len >= sizeof(buf)) {
        output_len = sizeof(buf) - 1;
    }

    pthread_mutex_lock(&g_uart_log_mutex);
    if (g_uart_debug_fd >= 0) {
        uart_log_write_locked(g_uart_debug_fd, buf, output_len);
    }
    pthread_mutex_unlock(&g_uart_log_mutex);
}

void uart_log_hex(int fd,
                  const char *prefix,
                  const uint8_t *data,
                  size_t data_len,
                  size_t preview_limit)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[UART_HEX_LOG_BUFFER_SIZE];
    size_t pos = 0;
    size_t shown;
    size_t rendered = 0;
    bool truncated;

    if (fd < 0 || (data == NULL && data_len > 0)) {
        return;
    }

    if (prefix != NULL) {
        while (*prefix != '\0' && pos + 1 < sizeof(buf)) {
            buf[pos++] = *prefix++;
        }
    }

    shown = data_len < preview_limit ? data_len : preview_limit;
    while (rendered < shown && pos + 3 < sizeof(buf)) {
        if (rendered > 0) {
            buf[pos++] = ' ';
        }
        buf[pos++] = hex[data[rendered] >> 4];
        buf[pos++] = hex[data[rendered] & 0x0F];
        rendered++;
    }

    truncated = rendered < data_len;
    if (truncated && pos + 4 < sizeof(buf)) {
        buf[pos++] = ' ';
        buf[pos++] = '.';
        buf[pos++] = '.';
        buf[pos++] = '.';
    }
    if (pos + 1 < sizeof(buf)) {
        buf[pos++] = '\n';
    }

    uart_log_write(fd, buf, pos);
}
