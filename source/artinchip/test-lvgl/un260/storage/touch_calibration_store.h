#ifndef UN260_TOUCH_CALIBRATION_STORE_H
#define UN260_TOUCH_CALIBRATION_STORE_H
#include "un260/input/touch_calibration_math.h"
typedef struct {
    touch_calibration_source_t source;
    touch_calibration_matrix_t matrix;
    bool enabled;
} touch_calibration_config_t;
typedef enum {
    TOUCH_CAL_STORE_MISSING, TOUCH_CAL_STORE_LOADED,
    TOUCH_CAL_STORE_INVALID, TOUCH_CAL_STORE_IO_ERROR
} touch_calibration_load_result_t;
touch_calibration_load_result_t touch_calibration_store_load(
    const touch_calibration_source_t *source, touch_calibration_config_t *config);
/* Success requires the data file and rename directory to reach stable storage. */
bool touch_calibration_store_save(const touch_calibration_config_t *config);
#endif
