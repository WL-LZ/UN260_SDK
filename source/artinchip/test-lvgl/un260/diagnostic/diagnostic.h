#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_VOLTAGE_CH_NUM 11
#define DIAGNOSTIC_CALIBRATION_TIMEOUT_MS 300000U

typedef enum {
    CIS_CALIB_IDLE = 0,
    CIS_CALIB_RUNNING,
    CIS_CALIB_SUCCESS,
    CIS_CALIB_FAIL_UPPER,
    CIS_CALIB_FAIL_LOWER,
    CIS_CALIB_FAIL_IR
} cis_calib_state_t;

typedef enum {
    CB_CALIB_IDLE = 0,
    CB_CALIB_RUNNING,
    CB_CALIB_SUCCESS,
    CB_CALIB_FAIL_IR
} cb_calib_state_t;

typedef enum {
    CALIB_TARGET_CIS = 0,
    CALIB_TARGET_CB
} calib_target_t;

typedef struct {
    cis_calib_state_t cis_state;
    cb_calib_state_t cb_state;
    calib_target_t target;
    bool session_active;
    bool timed_out;
} calibration_state_snapshot_t;

typedef struct {
    uint8_t raw[SENSOR_VOLTAGE_CH_NUM];
    bool valid[SENSOR_VOLTAGE_CH_NUM];
    uint32_t update_count;
} sensor_voltage_snapshot_t;

typedef enum {
    DIAGNOSTIC_REPLY_INVALID = 0,
    DIAGNOSTIC_REPLY_IGNORED,
    DIAGNOSTIC_REPLY_SENSOR_START,
    DIAGNOSTIC_REPLY_SENSOR_DATA,
    DIAGNOSTIC_REPLY_SENSOR_END,
    DIAGNOSTIC_REPLY_CALIBRATION_UPDATED,
} diagnostic_reply_result_t;

typedef struct {
    void (*on_calibration_changed)(void);
} diagnostic_reply_hooks_t;

void sensor_state_clear(void);
void sensor_state_set_voltage(uint8_t channel, uint8_t raw);
void sensor_state_get_snapshot(sensor_voltage_snapshot_t *snapshot);

void diagnostic_calibration_get_snapshot(calibration_state_snapshot_t *snapshot);
bool diagnostic_calibration_begin(calib_target_t target, uint32_t now_ms);
void diagnostic_calibration_end_session(void);
bool diagnostic_calibration_poll(uint32_t now_ms);

diagnostic_reply_result_t diagnostic_reply_dispatch(
    uint8_t cmd, const uint8_t *buf, uint8_t len, uint32_t now_ms,
    const diagnostic_reply_hooks_t *hooks);

#endif
