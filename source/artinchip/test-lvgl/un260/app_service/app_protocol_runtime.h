#ifndef UN260_APP_SERVICE_APP_PROTOCOL_RUNTIME_H
#define UN260_APP_SERVICE_APP_PROTOCOL_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

/* Coordinates non-counting protocol replies with application state and UI. */
bool app_protocol_runtime_handle_reply(uint8_t cmd,
                                       const uint8_t *buf,
                                       uint8_t len);

#endif
