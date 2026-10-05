#include "touch_calibration_math.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>

bool touch_calibration_source_valid(const touch_calibration_source_t *s)
{
    return s && s->width >= 320 && s->width <= 4096 &&
        s->height >= 240 && s->height <= 4096 && s->device_signature &&
        s->x_min >= -65536 && s->x_max <= 65536 && s->x_max > s->x_min &&
        s->y_min >= -65536 && s->y_max <= 65536 && s->y_max > s->y_min;
}

static int fixed_round(int64_t value)
{
    value = value >= 0 ? (value + TOUCH_CALIBRATION_SCALE / 2) / TOUCH_CALIBRATION_SCALE :
        -((-value + TOUCH_CALIBRATION_SCALE / 2) / TOUCH_CALIBRATION_SCALE);
    return value > INT_MAX ? INT_MAX : value < INT_MIN ? INT_MIN : (int)value;
}

void touch_calibration_map(const touch_calibration_matrix_t *m, int raw_x,
                           int raw_y, int *x, int *y)
{
    *x = fixed_round((int64_t)m->a[0] * raw_x + (int64_t)m->a[1] * raw_y + m->a[2]);
    *y = fixed_round((int64_t)m->a[3] * raw_x + (int64_t)m->a[4] * raw_y + m->a[5]);
}

bool touch_calibration_matrix_valid(const touch_calibration_source_t *s,
                                    const touch_calibration_matrix_t *m)
{
    if (!touch_calibration_source_valid(s) || !m) return false;
    double rx = (double)(s->x_max - s->x_min) / s->width;
    double ry = (double)(s->y_max - s->y_min) / s->height;
    double ax = m->a[0] * rx / TOUCH_CALIBRATION_SCALE;
    double ay = m->a[1] * ry / TOUCH_CALIBRATION_SCALE;
    double bx = m->a[3] * rx / TOUCH_CALIBRATION_SCALE;
    double by = m->a[4] * ry / TOUCH_CALIBRATION_SCALE;
    double determinant = fabs(ax * by - ay * bx);
    if (determinant < 0.25 || determinant > 4.0) return false;
    /* A corrupt but invertible matrix must not map the entire sensor outside
     * the display. Rotation/inversion remain valid affine transformations. */
    int min_x = INT_MAX, max_x = INT_MIN, min_y = INT_MAX, max_y = INT_MIN;
    for (unsigned i = 0; i < 4; ++i) {
        int x, y;
        touch_calibration_map(m, i & 1 ? s->x_max : s->x_min,
                              i & 2 ? s->y_max : s->y_min, &x, &y);
        if (x < -(int)s->width / 2 || x > (int)s->width * 3 / 2 ||
            y < -(int)s->height / 2 || y > (int)s->height * 3 / 2) return false;
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    return max_x - min_x >= (int)s->width / 2 &&
           max_y - min_y >= (int)s->height / 2;
}

bool touch_calibration_point_matches(const touch_calibration_matrix_t *m,
                                     const touch_calibration_point_t *raw,
                                     const touch_calibration_point_t *target,
                                     unsigned tolerance)
{
    int x, y;
    if (!m || !raw || !target || tolerance > 4096) return false;
    touch_calibration_map(m, raw->x, raw->y, &x, &y);
    int64_t dx = (int64_t)x - target->x, dy = (int64_t)y - target->y;
    if (dx < -(int64_t)tolerance || dx > tolerance ||
        dy < -(int64_t)tolerance || dy > tolerance) return false;
    return dx * dx + dy * dy <= (int64_t)tolerance * tolerance;
}

bool touch_calibration_fit(const touch_calibration_source_t *s,
                           const touch_calibration_point_t raw[TOUCH_CALIBRATION_POINTS],
                           const touch_calibration_point_t target[TOUCH_CALIBRATION_POINTS],
                           touch_calibration_matrix_t *matrix)
{
    if (!touch_calibration_source_valid(s) || !raw || !target || !matrix) return false;
    /* Five-point least-squares affine fit, following the tslib calibration
     * procedure. Center/normalize first, then solve with pivoted elimination;
     * no upstream implementation is copied. */
    double cx = 0, cy = 0;
    for (unsigned i = 0; i < TOUCH_CALIBRATION_POINTS; ++i) {
        if (raw[i].x < s->x_min || raw[i].x > s->x_max ||
            raw[i].y < s->y_min || raw[i].y > s->y_max ||
            target[i].x < 0 || target[i].x >= (int)s->width ||
            target[i].y < 0 || target[i].y >= (int)s->height) return false;
        cx += raw[i].x; cy += raw[i].y;
    }
    cx /= TOUCH_CALIBRATION_POINTS; cy /= TOUCH_CALIBRATION_POINTS;
    double sx = s->x_max - s->x_min, sy = s->y_max - s->y_min;
    double normal[3][5] = {{0}};
    for (unsigned i = 0; i < TOUCH_CALIBRATION_POINTS; ++i) {
        double row[3] = {(raw[i].x - cx) / sx, (raw[i].y - cy) / sy, 1};
        for (unsigned r = 0; r < 3; ++r) {
            for (unsigned c = 0; c < 3; ++c) normal[r][c] += row[r] * row[c];
            normal[r][3] += row[r] * target[i].x;
            normal[r][4] += row[r] * target[i].y;
        }
    }
    for (unsigned column = 0; column < 3; ++column) {
        unsigned pivot = column;
        for (unsigned r = column + 1; r < 3; ++r)
            if (fabs(normal[r][column]) > fabs(normal[pivot][column])) pivot = r;
        if (fabs(normal[pivot][column]) < 1e-7) return false;
        if (pivot != column) for (unsigned c = 0; c < 5; ++c) {
            double swap = normal[pivot][c]; normal[pivot][c] = normal[column][c]; normal[column][c] = swap;
        }
        double div = normal[column][column];
        for (unsigned c = column; c < 5; ++c) normal[column][c] /= div;
        for (unsigned r = 0; r < 3; ++r) if (r != column) {
            double factor = normal[r][column];
            for (unsigned c = column; c < 5; ++c) normal[r][c] -= factor * normal[column][c];
        }
    }
    touch_calibration_matrix_t result;
    for (unsigned axis = 0; axis < 2; ++axis) {
        double a = normal[0][3 + axis] / sx, b = normal[1][3 + axis] / sy;
        double values[3] = {a, b, normal[2][3 + axis] - a * cx - b * cy};
        for (unsigned c = 0; c < 3; ++c) {
            double fixed = values[c] * TOUCH_CALIBRATION_SCALE;
            if (!isfinite(fixed) || fixed < INT32_MIN || fixed > INT32_MAX) return false;
            result.a[axis * 3 + c] = (int32_t)llround(fixed);
        }
    }
    if (!touch_calibration_matrix_valid(s, &result)) return false;
    for (unsigned i = 0; i < TOUCH_CALIBRATION_POINTS; ++i)
        if (!touch_calibration_point_matches(&result, &raw[i], &target[i], 10)) return false;
    *matrix = result;
    return true;
}
