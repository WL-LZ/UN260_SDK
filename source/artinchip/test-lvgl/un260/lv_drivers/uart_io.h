#ifndef UN260_LV_DRIVERS_UART_IO_H
#define UN260_LV_DRIVERS_UART_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* All descriptors must be opened O_NONBLOCK. A failed controller write latches
 * the channel until an explicit closed-session restart; never append to a
 * potentially partial command or claim that the controller rejected it. */
void uart_tx_reset(int fd);
bool uart_tx_is_healthy(int fd);
int uart_write_all(int fd, const void *data, size_t data_len);
int uart_send(int fd, const char *send_buf, int data_len);
/* Returns bytes read, 0 on timeout/end-of-file, and -1 on error. */
int uart_recv(int fd, char *recv_buf, int data_len, int timeout_ms);
void uart_printf(int fd, const char *fmt, ...);
/* Application diagnostic output. The serial runtime owns the destination fd. */
void uart_debug_set_fd(int fd);
void uart_debug_printf(const char *fmt, ...);
void uart_log_hex(int fd,
                  const char *prefix,
                  const uint8_t *data,
                  size_t data_len,
                  size_t preview_limit);

#endif
