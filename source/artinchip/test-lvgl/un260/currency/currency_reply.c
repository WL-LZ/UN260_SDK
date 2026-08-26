#include "currency_reply.h"

#include <string.h>

#include "currency_state.h"
#include "un260/lv_drivers/lv_drivers.h"

static void currency_reply_copy_code(char destination[4], const uint8_t *source)
{
    destination[0] = (char)source[0];
    destination[1] = (char)source[1];
    destination[2] = (char)source[2];
    destination[3] = '\0';
}

currency_reply_result_t currency_reply_handle(const uint8_t *buf, uint8_t len)
{
    currency_reply_result_t reply;
    uint8_t status;

    memset(&reply, 0, sizeof(reply));
    reply.kind = CURRENCY_REPLY_INVALID;
    if (buf == NULL || len < 6) {
        return reply;
    }

    status = buf[4];
    if (status == 0x01 || status == 0x02) {
        if (!currency_service_take_switch_result(status, &reply.switch_result)) {
            uart_debug_printf("0x03 stale switch reply ignored: status=0x%02X\n", status);
            reply.kind = CURRENCY_REPLY_IGNORED;
            return reply;
        }

        if (reply.switch_result.success) {
            if (currency_state_confirm_active_selection(
                    reply.switch_result.target_index,
                    reply.switch_result.target_code)) {
                reply.kind = CURRENCY_REPLY_SWITCH_SUCCESS;
            } else {
                reply.switch_result.success = false;
                reply.kind = CURRENCY_REPLY_SWITCH_FAILURE;
            }
        } else {
            reply.kind = CURRENCY_REPLY_SWITCH_FAILURE;
        }
        currency_state_get_active_code(reply.active_code);
        return reply;
    }

    if (status == 0x03) {
        if (len < 9) {
            return reply;
        }
        currency_reply_copy_code(reply.active_code, &buf[5]);
        if (!currency_state_confirm_active_code(reply.active_code)) {
            return reply;
        }
        reply.kind = CURRENCY_REPLY_BOOT_ACTIVE;
        return reply;
    }

    reply.kind = CURRENCY_REPLY_IGNORED;
    return reply;
}
