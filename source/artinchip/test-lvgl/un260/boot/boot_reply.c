#include "boot_reply.h"

#include <string.h>

static boot_reply_result_t boot_reply_handshake_handle(const uint8_t *buf,
                                                       uint8_t len)
{
    boot_reply_result_t reply;

    memset(&reply, 0, sizeof(reply));
    reply.kind = BOOT_REPLY_INVALID;
    /* 回复值后还应保留校验字节。 */
    if (buf == NULL || len < 6) {
        return reply;
    }
    if (buf[4] != 0x01 || boot_service_handshake_state() != HANDSHAKE_SENT) {
        reply.kind = BOOT_REPLY_IGNORED;
        return reply;
    }

    boot_service_confirm_handshake();
    boot_service_reset_self_test();
    boot_service_reset_self_test_results();
    boot_service_set_stage(BOOT_STAGE_SENSOR);
    reply.kind = BOOT_REPLY_HANDSHAKE_ACCEPTED;
    return reply;
}

static boot_reply_result_t boot_reply_self_test_handle(const uint8_t *buf,
                                                       uint8_t len)
{
    boot_reply_result_t reply;
    boot_stage_t stage;

    memset(&reply, 0, sizeof(reply));
    reply.kind = BOOT_REPLY_INVALID;
    /* 测试类型、结果之后还应保留校验字节。 */
    if (buf == NULL || len < 7) {
        return reply;
    }

    stage = boot_service_get_stage();
    if (boot_service_handshake_state() != HANDSHAKE_OK ||
        stage < BOOT_STAGE_SENSOR || stage > BOOT_STAGE_IMAGE) {
        reply.kind = BOOT_REPLY_IGNORED;
        return reply;
    }

    if (!boot_service_record_self_test_result(buf[4], buf[5],
                                               &reply.self_test_index)) {
        reply.kind = BOOT_REPLY_IGNORED;
        return reply;
    }

    reply.self_test_result = buf[5];
    boot_service_advance_stage();
    reply.self_test_event = boot_service_take_self_test_event(
        &reply.first_failure_step,
        &reply.first_failure_result);
    if (reply.self_test_event == BOOT_SELF_TEST_EVENT_FAILURE) {
        boot_service_set_stage(BOOT_STAGE_FAIL);
    }
    reply.kind = BOOT_REPLY_SELF_TEST_RECORDED;
    return reply;
}

boot_reply_result_t boot_reply_dispatch(uint8_t cmd,
                                        const uint8_t *buf,
                                        uint8_t len)
{
    boot_reply_result_t reply;

    switch (cmd) {
    case 0x01:
        return boot_reply_handshake_handle(buf, len);
    case 0x37:
        return boot_reply_self_test_handle(buf, len);
    default:
        memset(&reply, 0, sizeof(reply));
        reply.kind = BOOT_REPLY_INVALID;
        return reply;
    }
}
