#ifndef __BOOT_STATE_H__
#define __BOOT_STATE_H__

typedef enum {
    HANDSHAKE_IDLE = 0,
    HANDSHAKE_SENT,
    HANDSHAKE_OK,
} handshake_state_t;

typedef enum {
    SELFTEST_NONE = 0,
    SELFTEST_SENSOR = 0x01,
    SELFTEST_MOTOR  = 0x02,
    SELFTEST_MAGNET = 0x03,
    SELFTEST_CONFIG = 0x04,
    SELFTEST_IMAGE  = 0x05,
} selftest_type_t;

typedef enum {
    BOOT_STAGE_HANDSHAKE = 0,

    BOOT_STAGE_SENSOR,
    BOOT_STAGE_MOTOR,
    BOOT_STAGE_MAGNET,
    BOOT_STAGE_CONFIG,
    BOOT_STAGE_IMAGE,

    BOOT_STAGE_DONE,
    BOOT_STAGE_FAIL,
} boot_stage_t;

typedef enum {
    BOOT_SELF_TEST_EVENT_NONE = 0,
    BOOT_SELF_TEST_EVENT_SUCCESS,
    BOOT_SELF_TEST_EVENT_FAILURE,
} boot_self_test_event_t;

#endif
