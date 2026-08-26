#ifndef PROTOCOL_RX_SERVICE_H
#define PROTOCOL_RX_SERVICE_H

#include <stdbool.h>

bool protocol_rx_service_start(int uart_fd, int log_fd);
void protocol_rx_service_stop(void);

#endif
