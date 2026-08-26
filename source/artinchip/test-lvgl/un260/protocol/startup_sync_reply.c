#include "startup_sync_reply.h"

#include <stddef.h>

#include "un260/currency/currency_state.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/mode_codec.h"
#include "un260/serial_number/serial_number.h"

static startup_sync_reply_result_t startup_sync_handle_user_preference(
    const uint8_t *buf,
    uint8_t len)
{
    uint8_t sub;
    uint8_t value;

    if (buf == NULL || len < 6) {
        return STARTUP_SYNC_REPLY_INVALID;
    }

    sub = buf[4];
    if (sub == 0x00) {
        uart_debug_printf("0x58 user preference receive start\n");
        return STARTUP_SYNC_REPLY_START;
    }
    if (sub == 0xFF) {
        uart_debug_printf("0x58 user preference receive end\n");
        return STARTUP_SYNC_REPLY_END;
    }

    /* 普通参数至少需要 sub、value 和校验字节，避免把校验值误当参数。 */
    if (len < 7) {
        uart_debug_printf("0x58 sub=0x%02X invalid len=%d\n", sub, len);
        return STARTUP_SYNC_REPLY_INVALID;
    }
    value = buf[5];

    switch (sub) {
    case 0x01:
    {
        uint8_t machine_mode;
        if (!mode_codec_decode(value, &machine_mode)) {
            return STARTUP_SYNC_REPLY_IGNORED;
        }
        machine_state_confirm_mode(machine_mode);
        break;
    }
    case 0x02:
        machine_state_sync_batch_num(value);
        break;
    case 0x03:
        if (len < 10) {
            return STARTUP_SYNC_REPLY_INVALID;
        }
        machine_state_confirm_batch_amount(((uint32_t)buf[5] << 24) |
                                           ((uint32_t)buf[6] << 16) |
                                           ((uint32_t)buf[7] << 8) |
                                           (uint32_t)buf[8]);
        break;
    case 0x04:
        machine_state_confirm_reject_pocket_max(value);
        break;
    case 0x05:
        machine_state_confirm_buzzer(value == 0x01);
        break;
    case 0x06:
        if (value < 1 || value > SPEED_MODE) {
            return STARTUP_SYNC_REPLY_IGNORED;
        }
        machine_state_confirm_speed((uint8_t)(value - 1));
        break;
    case 0x07:
        serial_number_state_confirm(value == 0x01,
                                    value == 0x01 ? 0x01 : SERIAL_NUMBER_LEVEL_OFF);
        break;
    case 0x08:
        currency_state_confirm_active_index(value);
        break;
    case 0x09:
        if (value == 0x01) {
            machine_state_confirm_batch_mode(PCS_BATCH_MODE);
        } else if (value == 0x02) {
            machine_state_confirm_batch_mode(AMOUNT_BATCH_MODE);
        } else {
            return STARTUP_SYNC_REPLY_IGNORED;
        }
        break;
    case 0x0A:
        if (value < DOUBLE_NOTE_LEVEL_MIN || value > DOUBLE_NOTE_LEVEL_MAX) {
            return STARTUP_SYNC_REPLY_IGNORED;
        }
        machine_state_confirm_double_note_level(value);
        break;
    default:
        return STARTUP_SYNC_REPLY_IGNORED;
    }

    return STARTUP_SYNC_REPLY_DATA;
}

static startup_sync_reply_result_t startup_sync_handle_currency_list(
    const uint8_t *buf,
    uint8_t len)
{
    uint8_t index;
    char currency_code[4];

    if (buf == NULL || len < 9) {
        return STARTUP_SYNC_REPLY_INVALID;
    }

    index = buf[4];
    currency_code[0] = (char)buf[5];
    currency_code[1] = (char)buf[6];
    currency_code[2] = (char)buf[7];
    currency_code[3] = '\0';

    if (index == 0x00 && buf[5] == 0x00 && buf[6] == 0x00 && buf[7] == 0x00) {
        currency_state_begin_list_sync();
        uart_debug_printf("0x56 currency query start\n");
        return STARTUP_SYNC_REPLY_START;
    }

    if (index == 0xFF && buf[5] == 0xFF && buf[6] == 0xFF && buf[7] == 0xFF) {
        if (!currency_state_finish_list_sync()) {
            uart_debug_printf("0x56 currency query rejected, keep previous list\n");
            return STARTUP_SYNC_REPLY_IGNORED;
        }
        uart_debug_printf("0x56 currency query end, count=%d\n",
                    currency_state_count());
        return STARTUP_SYNC_REPLY_END;
    }

    if (index == 0 || index > MAX_CURRENCIES) {
        return STARTUP_SYNC_REPLY_IGNORED;
    }

    if (!currency_state_append_list_code(index, currency_code)) {
        uart_debug_printf("0x56 currency[%d] invalid or out of sequence\n",
                    (int)index - 1);
        return STARTUP_SYNC_REPLY_IGNORED;
    }
    uart_debug_printf("0x56 currency[%d]=%s\n", (int)index - 1, currency_code);
    return STARTUP_SYNC_REPLY_DATA;
}

startup_sync_reply_result_t startup_sync_reply_dispatch(uint8_t cmd,
                                                        const uint8_t *buf,
                                                        uint8_t len)
{
    switch (cmd) {
    case 0x58:
        return startup_sync_handle_user_preference(buf, len);
    case 0x56:
        return startup_sync_handle_currency_list(buf, len);
    default:
        return STARTUP_SYNC_REPLY_INVALID;
    }
}
