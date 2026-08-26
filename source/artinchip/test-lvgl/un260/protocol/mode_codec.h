#ifndef MODE_CODEC_H
#define MODE_CODEC_H

#include <stdbool.h>
#include <stdint.h>

bool mode_codec_encode(uint8_t machine_mode, uint8_t *protocol_mode);
bool mode_codec_decode(uint8_t protocol_mode, uint8_t *machine_mode);

#endif
