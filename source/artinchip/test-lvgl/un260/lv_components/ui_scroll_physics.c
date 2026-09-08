#include "ui_scroll_physics.h"

#include <math.h>
#include <string.h>

/* Tuned for a short landscape chooser, not copied iOS private constants.
 * Exponential free coast transitions to a velocity-continuous damped spring.
 * No frame-count-based easing and no second, delayed snap animation. */
#define COAST_DECAY       6.5f
#define SPRING_OMEGA     20.0f
#define SPRING_DAMPING    0.86f
#define VELOCITY_LIMIT 3600.0f
#define RUBBER_LIMIT     56.0f
#define RUBBER_GAIN       0.42f
#define SAMPLE_WINDOW_MS 100U
#define STALE_MOTION_MS   90U
#define SETTLE_TIMEOUT_MS 2500U

static float clampf(float x, float low, float high)
{
    return x < low ? low : (x > high ? high : x);
}

static float rubber(float distance)
{
    return distance * RUBBER_GAIN /
           (1.0f + fabsf(distance) * RUBBER_GAIN / RUBBER_LIMIT);
}

static float un_rubber(float distance)
{
    float d = clampf(distance, -RUBBER_LIMIT + 0.25f,
                     RUBBER_LIMIT - 0.25f);
    return d / (RUBBER_GAIN * (1.0f - fabsf(d) / RUBBER_LIMIT));
}

static float bounded_drag(const ui_scroll_physics_t *s, float raw)
{
    if (s->count < 2) return 0.0f;
    if (raw < 0.0f) return rubber(raw);
    if (raw > s->max_position)
        return s->max_position + rubber(raw - s->max_position);
    return raw;
}

static void sample(ui_scroll_physics_t *s, uint32_t now)
{
    unsigned n = s->sample_count;
    if (n && (uint32_t)(now - s->samples[n - 1].tick) < 8U) {
        /* Preserve a time-separated history when duplicate event bubbles or
         * sub-tick reads arrive; one motion sample per input timeslice. */
        s->samples[n - 1].x = s->position;
        return;
    }
    if (n == UI_SCROLL_SAMPLE_COUNT) {
        memmove(s->samples, s->samples + 1,
                 sizeof(s->samples[0]) * (UI_SCROLL_SAMPLE_COUNT - 1));
        --n;
    }
    s->samples[n].x = s->position;
    s->samples[n].tick = now;
    s->sample_count = n + 1;
}

static float release_velocity(const ui_scroll_physics_t *s, uint32_t now)
{
    if (s->sample_count < 2 ||
        (uint32_t)(now - s->motion_tick) >= STALE_MOTION_MS) return 0.0f;

    unsigned end = s->sample_count - 1;
    unsigned first = end;
    float direction = 0.0f;
    for (unsigned i = end; i > 0; --i) {
        if ((uint32_t)(now - s->samples[i - 1].tick) > SAMPLE_WINDOW_MS)
            break;
        float dx = s->samples[i].x - s->samples[i - 1].x;
        if (fabsf(dx) > 0.1f) {
            if (direction != 0.0f && dx * direction < 0.0f) break;
            direction = dx;
        }
        first = i - 1;
    }
    uint32_t dt = now - s->samples[first].tick;
    if (dt < 12U) return 0.0f;
    return clampf((s->position - s->samples[first].x) * 1000.0f / dt,
                   -VELOCITY_LIMIT, VELOCITY_LIMIT);
}

static unsigned index_for(const ui_scroll_physics_t *s, float x)
{
    if (s->count < 2) return 0;
    x = clampf(x, 0.0f, s->max_position);
    unsigned i = (unsigned)(x / s->stride + 0.5f);
    return i < s->count ? i : s->count - 1;
}

void ui_scroll_physics_init(ui_scroll_physics_t *s, unsigned count,
                            float stride, unsigned index)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->stride = isfinite(stride) ? clampf(stride, 1.0f, 8192.0f) : 1.0f;
    /* Protect public arithmetic, while remaining well beyond this catalog. */
    s->count = count > 4096U ? 4096U : count;
    s->max_position = s->count ? (s->count - 1) * s->stride : 0.0f;
    ui_scroll_physics_snap(s, index, false, 0);
}

float ui_scroll_physics_begin(ui_scroll_physics_t *s, uint32_t now_ms)
{
    if (!s) return 0.0f;
    s->phase = UI_SCROLL_DRAG;
    s->velocity = 0.0f;
    s->tick = s->motion_tick = now_ms;
    s->sample_count = 0;
    sample(s, now_ms);
    if (s->position < 0.0f) return un_rubber(s->position);
    if (s->position > s->max_position)
        return s->max_position + un_rubber(s->position - s->max_position);
    return s->position;
}

void ui_scroll_physics_drag(ui_scroll_physics_t *s, float raw_position,
                            uint32_t now_ms)
{
    if (!s || s->phase != UI_SCROLL_DRAG || !isfinite(raw_position)) return;
    float position = bounded_drag(s, raw_position);
    if (fabsf(position - s->position) > 0.1f) s->motion_tick = now_ms;
    s->position = position;
    s->tick = now_ms;
    sample(s, now_ms);
}

void ui_scroll_physics_release(ui_scroll_physics_t *s, uint32_t now_ms,
                               bool cancelled)
{
    if (!s || s->phase != UI_SCROLL_DRAG) return;
    s->velocity = cancelled ? 0.0f : release_velocity(s, now_ms);
    float projection = s->position + s->velocity / COAST_DECAY;
    s->target = index_for(s, projection) * s->stride;
    s->phase = UI_SCROLL_COAST;
    if (cancelled || s->position < 0.0f || s->position > s->max_position) {
        s->target = index_for(s, s->position) * s->stride;
        s->phase = UI_SCROLL_SPRING;
    }
    s->tick = s->release_tick = now_ms;
    if (fabsf(s->position - s->target) < 0.35f && fabsf(s->velocity) < 4.0f) {
        s->position = s->target;
        s->velocity = 0.0f;
        s->phase = UI_SCROLL_IDLE;
    }
}

bool ui_scroll_physics_step(ui_scroll_physics_t *s, uint32_t now_ms)
{
    if (!s || s->phase == UI_SCROLL_IDLE) return false;
    if (s->phase == UI_SCROLL_DRAG) return true;
    uint32_t elapsed = now_ms - s->tick;
    s->tick = now_ms;
    /* Subdivide long frames instead of discarding elapsed time: deceleration
     * must not slow down simply because a frame took 100 or 200ms. An owner
     * suspended for multiple seconds resumes at its valid final slot. */
    if ((uint32_t)(now_ms - s->release_tick) >= SETTLE_TIMEOUT_MS) {
        s->position = s->target;
        s->velocity = 0.0f;
        s->phase = UI_SCROLL_IDLE;
        return false;
    }

    while (elapsed) {
        unsigned ms = elapsed > 8U ? 8U : elapsed;
        float dt = ms * 0.001f;
        float remaining = s->target - s->position;
        if (s->phase == UI_SCROLL_COAST &&
            (fabsf(remaining) <= s->stride * 0.55f ||
             remaining * s->velocity <= 0.0f || fabsf(s->velocity) < 80.0f))
            s->phase = UI_SCROLL_SPRING;

        if (s->phase == UI_SCROLL_COAST) {
            float decay = expf(-COAST_DECAY * dt);
            s->position += s->velocity * (1.0f - decay) / COAST_DECAY;
            s->velocity *= decay;
        } else {
            float acceleration = SPRING_OMEGA * SPRING_OMEGA * remaining -
                2.0f * SPRING_DAMPING * SPRING_OMEGA * s->velocity;
            s->velocity += acceleration * dt;
            s->position += s->velocity * dt;
        }
        if (s->position < -RUBBER_LIMIT) {
            s->position = -RUBBER_LIMIT;
            if (s->velocity < 0.0f) s->velocity = 0.0f;
        } else if (s->position > s->max_position + RUBBER_LIMIT) {
            s->position = s->max_position + RUBBER_LIMIT;
            if (s->velocity > 0.0f) s->velocity = 0.0f;
        }
        elapsed -= ms;
    }
    if (fabsf(s->position - s->target) < 0.35f && fabsf(s->velocity) < 4.0f) {
        s->position = s->target;
        s->velocity = 0.0f;
        s->phase = UI_SCROLL_IDLE;
    }
    return s->phase != UI_SCROLL_IDLE;
}

void ui_scroll_physics_snap(ui_scroll_physics_t *s, unsigned index,
                            bool animate, uint32_t now_ms)
{
    if (!s) return;
    if (s->count == 0) index = 0;
    else if (index >= s->count) index = s->count - 1;
    s->target = index * s->stride;
    s->velocity = 0.0f;
    s->sample_count = 0;
    s->tick = s->release_tick = now_ms;
    s->phase = animate ? UI_SCROLL_SPRING : UI_SCROLL_IDLE;
    if (!animate) s->position = s->target;
}

void ui_scroll_physics_stop(ui_scroll_physics_t *s)
{
    if (s) ui_scroll_physics_snap(s, index_for(s, s->position), false, s->tick);
}

unsigned ui_scroll_physics_nearest(const ui_scroll_physics_t *s)
{
    return s ? index_for(s, s->position) : 0;
}

unsigned ui_scroll_physics_focus(const ui_scroll_physics_t *s, unsigned index)
{
    if (!s || index >= s->count) return 0;
    float t = 1.0f - fabsf(index * s->stride - s->position) / (s->stride * 0.8f);
    t = clampf(t, 0.0f, 1.0f);
    return (unsigned)(1024.0f * t * t * (3.0f - 2.0f * t) + 0.5f);
}
