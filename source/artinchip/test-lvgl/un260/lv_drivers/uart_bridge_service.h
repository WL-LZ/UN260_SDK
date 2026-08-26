#ifndef UART_BRIDGE_SERVICE_H
#define UART_BRIDGE_SERVICE_H

#include <stdbool.h>

bool uart_bridge_service_start(int source_fd, int target_fd, int log_fd);
void uart_bridge_service_stop(void);

#endif
