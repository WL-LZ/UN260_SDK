#ifndef PROTOCOL_SEND_H
#define PROTOCOL_SEND_H

#include <stdint.h>
#include <stdbool.h>

#include "protocol_frame.h"

/* cmd_s_len must not exceed PROTOCOL_FRAME_MAX_PAYLOAD.
 * cmd_s may be NULL only when cmd_s_len is zero. */
int protocol_send(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len);
bool protocol_send_is_ready(void);

#endif
