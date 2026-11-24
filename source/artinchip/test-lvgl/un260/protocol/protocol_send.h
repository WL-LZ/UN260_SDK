#ifndef PROTOCOL_SEND_H
#define PROTOCOL_SEND_H

#include <stdint.h>
#include <stdbool.h>

#include "protocol_frame.h"

/* ON/OFF feedback use the same bound in the sound owner and wire gate. */
#define PROTOCOL_FEEDBACK_REPLY_MS 120U

/* cmd_s_len must not exceed PROTOCOL_FRAME_MAX_PAYLOAD.
 * cmd_s may be NULL only when cmd_s_len is zero. */
int protocol_send(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len);
/* Optional feedback only: 0 means busy (nothing written), negative means
 * failure, positive is the written frame length. Caller owns bounded retry. */
int protocol_send_if_idle(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len);
/* Business owners with an unsent state retry only a zero result. A positive
 * result is a completed wire write, a negative result is a real failure. */
int protocol_send_when_feedback_clear(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len);
/* A pending motor-test stop gets the next wire opportunity after feedback. */
int protocol_send_stop_when_feedback_clear(uint8_t cmd_g, const uint8_t *cmd_s, uint16_t cmd_s_len);
bool protocol_send_is_ready(void);

#endif
