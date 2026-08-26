#include "mode_codec.h"
#include <stddef.h>
#include "un260/lv_system/user_cfg.h"

bool mode_codec_encode(uint8_t machine_mode, uint8_t *protocol_mode)
{
    if (protocol_mode == NULL) return false;

    if (machine_mode == MODE_MDC) {
        *protocol_mode = 0x03;
        return true;
    }
    if (machine_mode == MODE_SDC) {
        *protocol_mode = 0x04;
        return true;
    }
    if (machine_mode == MODE_CNT) {
        *protocol_mode = 0x05;
        return true;
    }

    return false;
}

bool mode_codec_decode(uint8_t protocol_mode, uint8_t *machine_mode)
{
    if (machine_mode == NULL) return false;

    if (protocol_mode == 0x03) {
        *machine_mode = MODE_MDC;
        return true;
    }
    if (protocol_mode == 0x04) {
        *machine_mode = MODE_SDC;
        return true;
    }
    if (protocol_mode == 0x05) {
        *machine_mode = MODE_CNT;
        return true;
    }

    return false;
}
