#ifndef UN260_TOUCH_CALIBRATION_SERVICE_H
#define UN260_TOUCH_CALIBRATION_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
#include "un260/input/touch_calibration_math.h"

typedef enum {
    TOUCH_CAL_IDLE, TOUCH_CAL_COLLECT, TOUCH_CAL_VALIDATE, TOUCH_CAL_READY,
    TOUCH_CAL_SAVING, TOUCH_CAL_SAVED, TOUCH_CAL_FAILED
} touch_calibration_phase_t;
typedef enum {
    TOUCH_CAL_ERROR_NONE, TOUCH_CAL_ERROR_UNAVAILABLE, TOUCH_CAL_ERROR_TOO_SHORT,
    TOUCH_CAL_ERROR_UNSTABLE, TOUCH_CAL_ERROR_MULTITOUCH, TOUCH_CAL_ERROR_INPUT_LOST,
    TOUCH_CAL_ERROR_FIT, TOUCH_CAL_ERROR_VALIDATION, TOUCH_CAL_ERROR_TIMEOUT,
    TOUCH_CAL_ERROR_STORE
} touch_calibration_error_t;
typedef struct {
    touch_calibration_phase_t phase;
    touch_calibration_error_t error, load_error;
    unsigned target_index, target_count, progress, remaining_ms;
    int target_x, target_y, preview_x, preview_y;
    bool available, calibrated, enabled, down;
} touch_calibration_snapshot_t;

/* Driver supplies hardware identity/axis geometry once; an invalid source
 * disables calibration and preserves the existing input mapping. UI thread. */
void touch_calibration_input_init(const touch_calibration_source_t *source);
bool touch_calibration_begin(unsigned width, unsigned height, uint32_t now_ms);
bool touch_calibration_cancel(void);
bool touch_calibration_save(void);
bool touch_calibration_can_save(void);
bool touch_calibration_set_enabled(bool enabled);
bool touch_calibration_calibrated(void);
bool touch_calibration_enabled(void);
bool touch_calibration_storage_busy(void);
bool touch_calibration_busy(void);
void touch_calibration_poll(uint32_t now_ms);
void touch_calibration_get_snapshot(touch_calibration_snapshot_t *snapshot);

/* Complete, uncalibrated input report. UI controls in the final 56 screen
 * pixels retain the confirmed matrix; owned samples stay captured to lift.
 * Returns true while this physical contact belongs to calibration. */
bool touch_calibration_input_feed(int raw_x, int raw_y, int screen_y,
                                  uint8_t count, bool input_lost, uint32_t now_ms);
/* Returns false for the original driver mapping. Never clips raw samples. */
bool touch_calibration_transform(int raw_x, int raw_y, int *x, int *y);
#endif
