#ifndef PROTOCOL_RX_SERVICE_H
#define PROTOCOL_RX_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

bool protocol_rx_service_start(int uart_fd, int log_fd);
void protocol_rx_service_stop(void);
/* A complete frame can wait outside a full queue. Probe before the queue to
 * observe its handoff without consuming or reordering either source. */
bool protocol_rx_service_has_command_between(uint8_t command,
                                             uint64_t started_ms,
                                             uint64_t deadline_ms);

#endif
