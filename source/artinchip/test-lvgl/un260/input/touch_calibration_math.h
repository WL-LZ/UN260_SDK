#ifndef UN260_TOUCH_CALIBRATION_MATH_H
#define UN260_TOUCH_CALIBRATION_MATH_H
#include <stdbool.h>
#include <stdint.h>

#define TOUCH_CALIBRATION_POINTS 5U
#define TOUCH_CALIBRATION_SCALE 1048576
typedef struct { int32_t x, y; } touch_calibration_point_t;
typedef struct {
    uint32_t width, height;
    int32_t x_min, x_max, y_min, y_max;
    uint32_t device_signature;
} touch_calibration_source_t;
/* x'=(a0*x+a1*y+a2)/SCALE, y'=(a3*x+a4*y+a5)/SCALE. */
typedef struct { int32_t a[6]; } touch_calibration_matrix_t;
bool touch_calibration_source_valid(const touch_calibration_source_t *source);
bool touch_calibration_matrix_valid(const touch_calibration_source_t *source,
                                    const touch_calibration_matrix_t *matrix);
bool touch_calibration_fit(const touch_calibration_source_t *source,
                           const touch_calibration_point_t raw[TOUCH_CALIBRATION_POINTS],
                           const touch_calibration_point_t target[TOUCH_CALIBRATION_POINTS],
                           touch_calibration_matrix_t *matrix);
void touch_calibration_map(const touch_calibration_matrix_t *matrix, int raw_x,
                           int raw_y, int *x, int *y);
bool touch_calibration_point_matches(const touch_calibration_matrix_t *matrix,
                                     const touch_calibration_point_t *raw,
                                     const touch_calibration_point_t *target,
                                     unsigned tolerance);
#endif
