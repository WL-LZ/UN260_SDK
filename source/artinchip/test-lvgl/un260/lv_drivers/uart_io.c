#include "uart_io.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <poll.h>
#include <time.h>
#include "un260/app_service/runtime_guard.h"
#include <sys/time.h>
#include <unistd.h>

static pthread_mutex_t g_uart_tx_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_uart_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_uart_debug_fd = -1;

#define UART_HEX_LOG_BUFFER_SIZE 1024U
#define UART_IO_DEADLINE_MS 50
static int g_failed_fd = -1;
static uart_trace_t trace_sink;
void uart_trace_set(uart_trace_t trace){__atomic_store_n(&trace_sink,trace,__ATOMIC_RELEASE);}
static void trace_bytes(int fd,bool tx,const void *p,size_t n){uart_trace_t trace=__atomic_load_n(&trace_sink,__ATOMIC_ACQUIRE);if(trace&&n)trace(fd,tx,p,n);}
static int64_t monotonic_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return -1;
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static int remaining_ms(int64_t deadline)
{
    int64_t now = monotonic_ms();
    return now < 0 || now >= deadline ? 0 : (int)(deadline - now);
}
static int bounded_lock(pthread_mutex_t *mutex, int64_t deadline)
{
    int result;
    while ((result = pthread_mutex_trylock(mutex)) == EBUSY) {
        if (!remaining_ms(deadline)) { errno = ETIMEDOUT; return -1; }
        struct timespec pause = {0, 1000000};
        nanosleep(&pause, NULL);
    }
    if (result) { errno = result; return -1; }
    return 0;
}
void uart_tx_reset(int fd) { (void)fd; __atomic_store_n(&g_failed_fd, -1, __ATOMIC_RELEASE); }
bool uart_tx_is_healthy(int fd) { return __atomic_load_n(&g_failed_fd, __ATOMIC_ACQUIRE) != fd; }
static int write_until(int fd, const void *data, size_t size, int64_t deadline, bool trace)
{
    size_t done = 0;
    if (fd < 0 || (!data && size) || size > INT_MAX) { errno = EINVAL; return -1; }
    while (done < size) {
        ssize_t n = write(fd, (const uint8_t *)data + done, size - done);
        if (n > 0) {if(trace)trace_bytes(fd,true,(const uint8_t *)data+done,(size_t)n);done += (size_t)n;}
        else if (!n) { errno = EIO; return -1; }
        else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) return -1;
        if (done == size) return (int)done;
        int left = remaining_ms(deadline);
        if (!left) { errno = ETIMEDOUT; return -1; }
        struct pollfd p = {fd, POLLOUT, 0};
        int ready = poll(&p, 1, left);
        if (!ready) { errno = ETIMEDOUT; return -1; }
        if (ready < 0 && errno != EINTR) return -1;
        if (ready > 0 && (p.revents & (POLLERR | POLLHUP | POLLNVAL))) { errno = EIO; return -1; }
    }
    return (int)done;
}

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
    if (bounded_lock(&g_uart_log_mutex, monotonic_ms() + UART_IO_DEADLINE_MS)) return;
    uart_log_write_locked(fd, data, data_len);
    pthread_mutex_unlock(&g_uart_log_mutex);
}

int uart_write_all(int fd, const void *data, size_t data_len)
{
    return write_until(fd, data, data_len, monotonic_ms() + UART_IO_DEADLINE_MS, false);
}

int uart_send(int fd, const char *send_buf, int data_len)
{
    if (fd < 0 || data_len < 0 || (!send_buf && data_len)) { errno = EINVAL; return -1; }
    if (runtime_guard_has_fault() || !uart_tx_is_healthy(fd)) { errno = EIO; return -1; }
    int64_t deadline = monotonic_ms() + UART_IO_DEADLINE_MS;
    if (bounded_lock(&g_uart_tx_mutex, deadline)) goto fault;
    if (runtime_guard_has_fault() || !uart_tx_is_healthy(fd)) { pthread_mutex_unlock(&g_uart_tx_mutex); errno = EIO; return -1; }
    runtime_guard_tx(send_buf, (size_t)data_len, 0);
    int result = write_until(fd, send_buf, (size_t)data_len, deadline, true);
    int saved_errno = errno;
    runtime_guard_tx(send_buf, (size_t)data_len, result == data_len ? 1 : -1);
    if (result != data_len) __atomic_store_n(&g_failed_fd, fd, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&g_uart_tx_mutex);
    errno = saved_errno;
    if (result == data_len) return result;
fault:
    __atomic_store_n(&g_failed_fd, fd, __ATOMIC_RELEASE);
    runtime_guard_fault(RUNTIME_FAULT_UART_TX);
    return -1;
}

int uart_recv(int fd, char *recv_buf, int data_len, int timeout_ms)
{
    if (fd < 0 || !recv_buf || data_len <= 0 || timeout_ms < 0) { errno = EINVAL; return -1; }
    int64_t deadline = monotonic_ms() + timeout_ms;
    for (;;) {
        struct pollfd p = {fd, POLLIN, 0};
        int ready = poll(&p, 1, remaining_ms(deadline));
        if (!ready) return 0;
        if (ready < 0) { if (errno != EINTR) return -1; }
        else {
            if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) { errno = EIO; return -1; }
            ssize_t n = read(fd, recv_buf, (size_t)data_len);
            if (n >= 0) {if(n)trace_bytes(fd,false,recv_buf,(size_t)n);return (int)n;}
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) return -1;
        }
        if (!remaining_ms(deadline)) return 0;
    }
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
    __atomic_store_n(&g_uart_debug_fd, fd, __ATOMIC_RELEASE);
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

    if (bounded_lock(&g_uart_log_mutex, monotonic_ms() + UART_IO_DEADLINE_MS)) return;
    int fd = __atomic_load_n(&g_uart_debug_fd, __ATOMIC_ACQUIRE);
    if (fd >= 0) {
        uart_log_write_locked(fd, buf, output_len);
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
