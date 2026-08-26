#include "counting_control_reply.h"

#include <stddef.h>

#include "un260/counting/counting_history_service.h"
#include "un260/lv_drivers/lv_drivers.h"

static void counting_control_report_error(const counting_control_reply_hooks_t *hooks,
                                          const char *tag,
                                          const uint8_t *buf,
                                          uint8_t len)
{
    if (hooks != NULL && hooks->on_error_frame != NULL) {
        hooks->on_error_frame(tag, buf, len);
    }
}

static void counting_control_report_start_failure(const counting_control_reply_hooks_t *hooks,
                                                  uint8_t type,
                                                  uint8_t code)
{
    if (hooks != NULL && hooks->on_start_failure != NULL) {
        hooks->on_start_failure(type, code);
    }
}

static void counting_control_report_runtime_fault(
    const counting_control_reply_hooks_t *hooks, uint8_t code)
{
    if (hooks != NULL && hooks->on_runtime_fault != NULL) {
        hooks->on_runtime_fault(code);
    }
}

static void counting_control_handle_runtime_fault(const uint8_t *buf,
                                                  uint8_t len,
                                                  const counting_control_reply_hooks_t *hooks)
{
    uint8_t fault;

    if (buf == NULL || len < 6) {
        return;
    }

    fault = buf[4];
    if (fault == 0x00) {
        counting_control_report_runtime_fault(hooks, fault);
        return;
    }

    counting_control_report_error(hooks, "0x0F", buf, len);
    counting_control_report_runtime_fault(hooks, fault);
}

static void counting_control_handle_start(const uint8_t *buf,
                                          uint8_t len,
                                          counting_session_state_t *session,
                                          const counting_control_reply_hooks_t *hooks)
{
    uint8_t type;
    uint8_t value;

    if (session == NULL || buf == NULL || len < 7) {
        return;
    }

    type = buf[4];
    value = buf[5];

    if (type == 0x01 && value == 0x01) {
        if (counting_history_discard_pending(session)) {
            uart_debug_printf("pending history discarded by new counting session\n");
        }
        session->phase = COUNTING_SESSION_IDLE;
        session->end_anim_wait_detail = false;
        session->expected_issue = 0;
        if (hooks != NULL && hooks->on_start_success != NULL) {
            hooks->on_start_success(buf, len);
        }
        return;
    }

    if (type == 0x01 && value == 0x02) {
        counting_control_report_error(hooks, "0x0A", buf, len);
        counting_control_report_start_failure(hooks, type, value);
        return;
    }

    counting_control_report_error(hooks, "0x0A", buf, len);

    if (type == 0x01) {
        counting_control_report_start_failure(hooks, type, value);
    } else if (type == 0x02) {
        counting_control_report_start_failure(hooks, type, value);
    } else {
        uart_debug_printf("0x0A start fail (unknown type): type=%02X val=%02X\n",
                    type, value);
        counting_control_report_start_failure(hooks, type, value);
    }
}

bool counting_control_reply_dispatch(uint8_t cmd,
                                     counting_session_state_t *session,
                                     const uint8_t *buf,
                                     uint8_t len,
                                     const counting_control_reply_hooks_t *hooks)
{
    switch (cmd) {
    case 0x0A:
        counting_control_handle_start(buf, len, session, hooks);
        return true;
    case 0x0F:
        counting_control_handle_runtime_fault(buf, len, hooks);
        return true;
    default:
        return false;
    }
}
