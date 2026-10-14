#ifndef PROTOCOL_RX_SERVICE_H
#define PROTOCOL_RX_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

/* Called on RX thread after validation, before queue publication. No LVGL calls.
 * Set before start and clear after stop. The observer must not block. */
typedef void (*protocol_rx_observer_t)(const uint8_t *frame, uint16_t len);
void protocol_rx_service_set_observer(protocol_rx_observer_t observer);
bool protocol_rx_service_start(int uart_fd, int log_fd);
void protocol_rx_service_stop(void);
/* A complete frame can wait outside a full queue. Probe before the queue to
 * observe its handoff without consuming or reordering either source. */
bool protocol_rx_service_has_command_between(uint8_t command,
                                             uint64_t started_ms,
                                             uint64_t deadline_ms);

#endif
