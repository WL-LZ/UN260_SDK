#include "touch_calibration_service.h"
#include "un260/storage/touch_calibration_store.h"
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

#define CAL_POINT_TIMEOUT 30000U
#define CAL_SESSION_TIMEOUT 180000U
#define CAL_HOLD_MIN 80U
#define CAL_HOLD_MAX 3000U
#define CAL_JITTER_PIXELS 12U
#define CAL_VALIDATION_POINTS 3U
#define CAL_CONTROL_HEIGHT 56

static touch_calibration_source_t source;
static touch_calibration_config_t confirmed, pending;
static touch_calibration_snapshot_t state;
static touch_calibration_point_t raw_points[TOUCH_CALIBRATION_POINTS];
static touch_calibration_point_t targets[TOUCH_CALIBRATION_POINTS];
static touch_calibration_point_t validation[CAL_VALIDATION_POINTS];
static uint32_t point_tick, session_tick, contact_tick;
static int min_x, max_x, min_y, max_y;
static int64_t sum_x, sum_y;
static unsigned samples;
static bool have_config, candidate_valid, pending_valid;
static bool physical_down, wait_release, contact_owned, contact_invalid;
static bool contact_passthrough;
static pthread_t writer;
static bool writer_started;
static atomic_int writer_result = ATOMIC_VAR_INIT(0);

static bool sampling(void)
{
    return state.phase == TOUCH_CAL_COLLECT || state.phase == TOUCH_CAL_VALIDATE;
}

static void remaining(uint32_t now)
{
    uint32_t point_elapsed = now - point_tick, session_elapsed = now - session_tick;
    if (!sampling()) { state.remaining_ms = 0; return; }
    if (point_elapsed >= CAL_POINT_TIMEOUT || session_elapsed >= CAL_SESSION_TIMEOUT) {
        state.phase = TOUCH_CAL_FAILED;
        state.error = TOUCH_CAL_ERROR_TIMEOUT;
        candidate_valid = false;
        state.remaining_ms = 0;
        return;
    }
    uint32_t point_left = CAL_POINT_TIMEOUT - point_elapsed;
    uint32_t session_left = CAL_SESSION_TIMEOUT - session_elapsed;
    state.remaining_ms = point_left < session_left ? point_left : session_left;
}

static void select_target(void)
{
    touch_calibration_point_t p = state.phase == TOUCH_CAL_COLLECT ?
        targets[state.target_index] : validation[state.target_index];
    state.target_x = p.x; state.target_y = p.y;
    state.target_count = state.phase == TOUCH_CAL_COLLECT ? TOUCH_CALIBRATION_POINTS : CAL_VALIDATION_POINTS;
}

void touch_calibration_input_init(const touch_calibration_source_t *input)
{
    /* A reopened device is a new source. Never replace a snapshot still owned
     * by its writer; the caller retains the original driver mapping meanwhile. */
    if (writer_started) return;
    state = (touch_calibration_snapshot_t){0};
    have_config = candidate_valid = pending_valid = false;
    physical_down = wait_release = contact_owned = contact_invalid = contact_passthrough = false;
    if (!touch_calibration_source_valid(input)) {
        memset(&source, 0, sizeof(source));
        state.load_error = TOUCH_CAL_ERROR_UNAVAILABLE;
        return;
    }
    source = *input;
    state.available = true;
    touch_calibration_load_result_t loaded = touch_calibration_store_load(&source, &confirmed);
    if (loaded == TOUCH_CAL_STORE_LOADED) have_config = true;
    else if (loaded == TOUCH_CAL_STORE_INVALID) state.load_error = TOUCH_CAL_ERROR_FIT;
    else if (loaded == TOUCH_CAL_STORE_IO_ERROR) state.load_error = TOUCH_CAL_ERROR_STORE;
    state.calibrated = have_config;
    state.enabled = have_config && confirmed.enabled;
}

static void *write_config(void *unused)
{
    (void)unused;
    bool ok = touch_calibration_store_save(&pending);
    atomic_store_explicit(&writer_result, ok ? 1 : -1, memory_order_release);
    return NULL;
}

void touch_calibration_poll(uint32_t now_ms)
{
    if (writer_started) {
        int result = atomic_load_explicit(&writer_result, memory_order_acquire);
        if (result) {
            pthread_join(writer, NULL);
            writer_started = false;
            if (result > 0) {
                confirmed = pending;
                have_config = true;
                state.calibrated = true;
                state.enabled = confirmed.enabled;
                state.phase = TOUCH_CAL_SAVED;
                state.error = state.load_error = TOUCH_CAL_ERROR_NONE;
                pending_valid = false;
            } else {
                state.phase = TOUCH_CAL_FAILED;
                state.error = TOUCH_CAL_ERROR_STORE;
            }
        }
    }
    remaining(now_ms);
}

bool touch_calibration_storage_busy(void) { return writer_started; }
bool touch_calibration_busy(void)
{
    return writer_started || sampling() || state.phase == TOUCH_CAL_READY;
}
bool touch_calibration_calibrated(void) { return have_config; }
bool touch_calibration_enabled(void) { return have_config && confirmed.enabled; }
void touch_calibration_get_snapshot(touch_calibration_snapshot_t *snapshot)
{
    if (snapshot) *snapshot = state;
}

bool touch_calibration_begin(unsigned width, unsigned height, uint32_t now_ms)
{
    if (writer_started) return false;
    if (!state.available || width != source.width || height != source.height) {
        state.phase = TOUCH_CAL_FAILED; state.error = TOUCH_CAL_ERROR_UNAVAILABLE; return false;
    }
    state.phase = TOUCH_CAL_COLLECT; state.error = TOUCH_CAL_ERROR_NONE;
    state.target_index = state.progress = 0;
    candidate_valid = pending_valid = false;
    wait_release = physical_down;
    contact_owned = contact_invalid = contact_passthrough = false;
    state.down = false;
    point_tick = session_tick = now_ms;
    targets[0] = (touch_calibration_point_t){64, 96};
    targets[1] = (touch_calibration_point_t){(int)width - 65, 96};
    targets[2] = (touch_calibration_point_t){(int)width - 65, (int)height - 97};
    targets[3] = (touch_calibration_point_t){64, (int)height - 97};
    targets[4] = (touch_calibration_point_t){(int)width / 2, (int)height / 2};
    validation[0] = (touch_calibration_point_t){(int)width / 4, (int)height * 7 / 20};
    validation[1] = (touch_calibration_point_t){(int)width * 3 / 4, (int)height * 13 / 20};
    validation[2] = (touch_calibration_point_t){(int)width / 2, (int)height * 3 / 5};
    select_target(); remaining(now_ms);
    return true;
}

bool touch_calibration_cancel(void)
{
    if (writer_started) return false;
    state.phase = TOUCH_CAL_IDLE;
    state.error = TOUCH_CAL_ERROR_NONE;
    state.remaining_ms = state.progress = 0;
    candidate_valid = pending_valid = false;
    state.down = false;
    return true;
}

static bool start_writer(void)
{
    if (writer_started || !pending_valid) return false;
    atomic_store_explicit(&writer_result, 0, memory_order_relaxed);
    if (pthread_create(&writer, NULL, write_config, NULL)) {
        state.phase = TOUCH_CAL_FAILED; state.error = TOUCH_CAL_ERROR_STORE; return false;
    }
    writer_started = true;
    state.phase = TOUCH_CAL_SAVING; state.error = TOUCH_CAL_ERROR_NONE;
    state.remaining_ms = 0;
    return true;
}

bool touch_calibration_can_save(void)
{
    return !writer_started && ((state.phase == TOUCH_CAL_READY && candidate_valid) ||
        (state.phase == TOUCH_CAL_FAILED && state.error == TOUCH_CAL_ERROR_STORE && pending_valid));
}

bool touch_calibration_save(void)
{
    if (state.phase == TOUCH_CAL_READY && candidate_valid) pending_valid = true;
    else if (state.phase != TOUCH_CAL_FAILED || state.error != TOUCH_CAL_ERROR_STORE || !pending_valid)
        return false;
    return start_writer();
}

bool touch_calibration_set_enabled(bool enabled)
{
    if (touch_calibration_busy()) return false;
    if (!have_config) {
        if (!enabled) return true;
        state.error = TOUCH_CAL_ERROR_UNAVAILABLE; return false;
    }
    if (confirmed.enabled == enabled) return true;
    pending = confirmed; pending.enabled = enabled; pending_valid = true;
    return start_writer();
}

bool touch_calibration_transform(int raw_x, int raw_y, int *x, int *y)
{
    if (!x || !y || !touch_calibration_enabled()) return false;
    touch_calibration_map(&confirmed.matrix, raw_x, raw_y, x, y);
    return true;
}

static void sample_finished(uint32_t now)
{
    if (contact_invalid || !samples) return;
    if (now - contact_tick < CAL_HOLD_MIN) { state.error = TOUCH_CAL_ERROR_TOO_SHORT; return; }
    if (now - contact_tick > CAL_HOLD_MAX) { state.error = TOUCH_CAL_ERROR_UNSTABLE; return; }
    touch_calibration_point_t p = {(int32_t)(sum_x / samples), (int32_t)(sum_y / samples)};
    if (state.phase == TOUCH_CAL_COLLECT) {
        raw_points[state.target_index++] = p;
        state.progress++;
        if (state.target_index == TOUCH_CALIBRATION_POINTS) {
            pending.source = source; pending.enabled = true;
            if (!touch_calibration_fit(&source, raw_points, targets, &pending.matrix)) {
                state.phase = TOUCH_CAL_FAILED; state.error = TOUCH_CAL_ERROR_FIT; return;
            }
            candidate_valid = true;
            state.phase = TOUCH_CAL_VALIDATE; state.target_index = 0;
        }
    } else if (state.phase == TOUCH_CAL_VALIDATE) {
        if (!touch_calibration_point_matches(&pending.matrix, &p, &validation[state.target_index], 14)) {
            state.error = TOUCH_CAL_ERROR_VALIDATION;
            return;
        }
        state.target_index++; state.progress++;
        if (state.target_index == CAL_VALIDATION_POINTS) {
            state.phase = TOUCH_CAL_READY; state.error = TOUCH_CAL_ERROR_NONE;
            state.remaining_ms = 0; return;
        }
    }
    state.error = TOUCH_CAL_ERROR_NONE;
    point_tick = now;
    select_target(); remaining(now);
}

bool touch_calibration_input_feed(int raw_x, int raw_y, int screen_y,
                                  uint8_t count, bool input_lost, uint32_t now_ms)
{
    touch_calibration_poll(now_ms);
    bool was_down = physical_down;
    physical_down = count != 0;
    if (!sampling()) {
        state.down = state.phase == TOUCH_CAL_READY && physical_down;
        if (state.down && candidate_valid)
            touch_calibration_map(&pending.matrix, raw_x, raw_y, &state.preview_x, &state.preview_y);
        return false;
    }
    if (wait_release) {
        if (!physical_down) wait_release = false;
        return true;
    }
    if (physical_down && !was_down) {
        /* Decide ownership once. A captured finger sliding into the button
         * strip never turns into a Cancel/Save click. */
        contact_passthrough = screen_y >= (int)source.height - CAL_CONTROL_HEIGHT;
        contact_owned = !contact_passthrough;
        contact_invalid = false;
        samples = 0; sum_x = sum_y = 0;
        min_x = max_x = raw_x; min_y = max_y = raw_y;
        contact_tick = now_ms;
    }
    bool owned = contact_owned;
    if (contact_passthrough) {
        if (!physical_down) contact_passthrough = false;
        return false;
    }
    state.down = physical_down;
    if (input_lost) { contact_invalid = true; state.error = TOUCH_CAL_ERROR_INPUT_LOST; }
    if (physical_down) {
        if (count != 1) { contact_invalid = true; state.error = TOUCH_CAL_ERROR_MULTITOUCH; }
        if (raw_x < source.x_min || raw_x > source.x_max || raw_y < source.y_min || raw_y > source.y_max) {
            contact_invalid = true; state.error = TOUCH_CAL_ERROR_INPUT_LOST;
        }
        if (candidate_valid) touch_calibration_map(&pending.matrix, raw_x, raw_y,
                                                  &state.preview_x, &state.preview_y);
        if (raw_x < min_x) min_x = raw_x;
        if (raw_x > max_x) max_x = raw_x;
        if (raw_y < min_y) min_y = raw_y;
        if (raw_y > max_y) max_y = raw_y;
        if ((int64_t)(max_x - min_x) * source.width >
            (int64_t)CAL_JITTER_PIXELS * (source.x_max - source.x_min) ||
            (int64_t)(max_y - min_y) * source.height >
            (int64_t)CAL_JITTER_PIXELS * (source.y_max - source.y_min)) {
            contact_invalid = true; state.error = TOUCH_CAL_ERROR_UNSTABLE;
        }
        /* A long, stationary hold need only emit press+release; do not demand
         * repeated events Linux suppresses. Bound the averaging accumulator. */
        if (!contact_invalid && samples < 4096) { sum_x += raw_x; sum_y += raw_y; samples++; }
    } else {
        if (owned && sampling()) sample_finished(now_ms);
        contact_owned = false;
    }
    return owned;
}
