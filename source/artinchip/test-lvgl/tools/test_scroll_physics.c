#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "un260/lv_components/ui_scroll_physics.h"

static unsigned failures;
static unsigned scenarios;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #condition); failures++; \
} } while (0)

static bool finite_state(const ui_scroll_physics_t *s)
{
    return isfinite(s->position) && isfinite(s->velocity) && isfinite(s->target) &&
           isfinite(s->stride) && isfinite(s->max_position);
}

static void invariant(const ui_scroll_physics_t *s)
{
    CHECK(finite_state(s));
    if (!finite_state(s)) return;
    CHECK(s->stride >= 1.0f);
    CHECK(s->position >= -56.001f && s->position <= s->max_position + 56.001f);
    CHECK(s->target >= 0.0f && s->target <= s->max_position);
    CHECK(fabsf(s->target / s->stride - roundf(s->target / s->stride)) < 0.001f);
    CHECK(s->sample_count <= UI_SCROLL_SAMPLE_COUNT);
    unsigned nearest = ui_scroll_physics_nearest(s);
    CHECK(s->count ? nearest < s->count : nearest == 0);
    for (unsigned i = 0; i < s->count && i < 34; i++) CHECK(ui_scroll_physics_focus(s, i) <= 1024);
    CHECK(ui_scroll_physics_focus(s, s->count) == 0);
    CHECK(ui_scroll_physics_focus(s, UINT_MAX) == 0);
}

static void settled(const ui_scroll_physics_t *s)
{
    invariant(s);
    CHECK(s->phase == UI_SCROLL_IDLE && s->velocity == 0.0f);
    CHECK(s->position == s->target);
    CHECK(s->position == ui_scroll_physics_nearest(s) * s->stride);
}

static void finish(ui_scroll_physics_t *s, uint32_t now, unsigned dt)
{
    unsigned elapsed = 0;
    while (s->phase != UI_SCROLL_IDLE && elapsed <= 3000) {
        now += dt; elapsed += dt;
        (void)ui_scroll_physics_step(s, now);
        invariant(s);
    }
    settled(s);
}

static ui_scroll_physics_t gesture(uint32_t start, unsigned duration, float distance)
{
    ui_scroll_physics_t s;
    ui_scroll_physics_init(&s, 34, 128.0f, 10);
    float anchor = ui_scroll_physics_begin(&s, start);
    for (unsigned t = 10; t < duration; t += 10)
        ui_scroll_physics_drag(&s, anchor + distance * t / duration, start + t);
    ui_scroll_physics_drag(&s, anchor + distance, start + duration);
    ui_scroll_physics_release(&s, start + duration, false);
    invariant(&s);
    return s;
}

static void test_speed_hold_reverse(void)
{
    ui_scroll_physics_t slow = gesture(1000, 600, 120);
    ui_scroll_physics_t fast = gesture(1000, 60, 120);
    CHECK(fast.position == slow.position);
    CHECK(fast.velocity > slow.velocity * 3.0f);
    CHECK(fast.target >= slow.target + fast.stride);
    printf("same 120px: slow v=%.1f target=%.1f; fast v=%.1f target=%.1f\n",
           slow.velocity, slow.target, fast.velocity, fast.target);
    finish(&slow, 1600, 16); finish(&fast, 1060, 16);

    ui_scroll_physics_t hold;
    ui_scroll_physics_init(&hold, 34, 128, 10);
    float anchor = ui_scroll_physics_begin(&hold, 1000);
    ui_scroll_physics_drag(&hold, anchor + 60, 1030);
    ui_scroll_physics_drag(&hold, anchor + 120, 1060);
    ui_scroll_physics_release(&hold, 1150, false);
    CHECK(hold.velocity == 0.0f);
    CHECK(hold.target == ui_scroll_physics_nearest(&hold) * hold.stride);
    finish(&hold, 1150, 16);

    ui_scroll_physics_t reverse;
    ui_scroll_physics_init(&reverse, 34, 128, 10);
    anchor = ui_scroll_physics_begin(&reverse, 1000);
    const float offsets[] = { 60, 120, 180, 140, 100, 60 };
    for (unsigned i = 0; i < 6; i++) ui_scroll_physics_drag(&reverse, anchor + offsets[i], 1020 + i * 20);
    ui_scroll_physics_release(&reverse, 1120, false);
    CHECK(reverse.velocity < -500.0f && reverse.target < reverse.position);
    printf("reversal: release v=%.1f target=%.1f\n", reverse.velocity, reverse.target);
    finish(&reverse, 1120, 16);
    scenarios += 4;
}

static ui_scroll_physics_t at_elapsed(ui_scroll_physics_t s, unsigned cadence, unsigned elapsed)
{
    uint32_t start = s.tick;
    unsigned t = 0;
    while (t < elapsed) {
        unsigned dt = cadence < elapsed - t ? cadence : elapsed - t;
        t += dt;
        (void)ui_scroll_physics_step(&s, start + t);
        invariant(&s);
    }
    return s;
}

static void test_elapsed_and_dropped_frames(void)
{
    const unsigned horizons[] = { 33, 64, 100, 160, 200, 330, 500, 1000 };
    const unsigned cadences[] = { 1, 7, 10, 16, 33, 64, 100, 200 };
    ui_scroll_physics_t initial = gesture(1000, 60, 120);
    float worst_regular = 0.0f, worst_dropped = 0.0f;
    for (unsigned h = 0; h < sizeof(horizons) / sizeof(horizons[0]); h++) {
        ui_scroll_physics_t reference = at_elapsed(initial, 10, horizons[h]);
        for (unsigned c = 0; c < sizeof(cadences) / sizeof(cadences[0]); c++) {
            ui_scroll_physics_t candidate = at_elapsed(initial, cadences[c], horizons[h]);
            float difference = fabsf(candidate.position - reference.position);
            if (cadences[c] == 10 || cadences[c] == 16 || cadences[c] == 33) {
                if (difference > worst_regular) worst_regular = difference;
                CHECK(difference <= 4.0f); /* <=3.2% of a 128px stride at the same real time. */
            }
            if (cadences[c] >= 64) {
                if (difference > worst_dropped) worst_dropped = difference;
                CHECK(difference <= 5.0f);
            }
            CHECK(candidate.target == initial.target);
        }
    }
    printf("cadence path delta: 10/16/33ms max %.3fpx; 64/100/200ms max %.3fpx\n",
           worst_regular, worst_dropped);
    ui_scroll_physics_t gap = initial;
    (void)ui_scroll_physics_step(&gap, gap.tick); /* dt=0 cannot advance state. */
    CHECK(gap.position == initial.position && gap.velocity == initial.velocity);
    (void)ui_scroll_physics_step(&gap, gap.tick + 3000);
    settled(&gap);
    for (unsigned c = 0; c < sizeof(cadences) / sizeof(cadences[0]); c++) {
        ui_scroll_physics_t candidate = initial;
        finish(&candidate, candidate.tick, cadences[c]);
    }
    scenarios += 10;
}

static void test_input_sampling_and_focus(void)
{
    const unsigned cadences[] = { 10, 16, 33 };
    float first_velocity = 0;
    float first_target = 0;
    for (unsigned c = 0; c < 3; c++) {
        ui_scroll_physics_t s;
        ui_scroll_physics_init(&s, 34, 128, 10);
        float anchor = ui_scroll_physics_begin(&s, 1000);
        for (unsigned elapsed = cadences[c]; elapsed < 240; elapsed += cadences[c]) {
            ui_scroll_physics_drag(&s, anchor + elapsed * 0.5f, 1000 + elapsed);
            ui_scroll_physics_drag(&s, anchor + elapsed * 0.5f, 1000 + elapsed); /* Duplicate bubble. */
        }
        ui_scroll_physics_drag(&s, anchor + 120, 1240);
        ui_scroll_physics_release(&s, 1240, false);
        CHECK(fabsf(s.velocity - 500.0f) < 0.01f);
        if (c == 0) { first_velocity = s.velocity; first_target = s.target; }
        CHECK(fabsf(s.velocity - first_velocity) < 0.01f && s.target == first_target);
        finish(&s, 1240, cadences[c]);
    }
    ui_scroll_physics_t left, right;
    ui_scroll_physics_init(&left, 34, 128, 10);
    ui_scroll_physics_init(&right, 34, 128, 10);
    float center = ui_scroll_physics_begin(&left, 0);
    ui_scroll_physics_begin(&right, 0);
    unsigned previous = 1024;
    for (unsigned distance = 0; distance <= 128; distance++) {
        ui_scroll_physics_drag(&left, center - distance, distance * 10);
        ui_scroll_physics_drag(&right, center + distance, distance * 10);
        unsigned lfocus = ui_scroll_physics_focus(&left, 10);
        unsigned rfocus = ui_scroll_physics_focus(&right, 10);
        CHECK(lfocus == rfocus && lfocus <= previous && lfocus <= 1024);
        previous = lfocus;
    }
    CHECK(previous == 0);
    scenarios += 4;
}

static void test_interrupt_cancel_and_edges(void)
{
    ui_scroll_physics_t s = at_elapsed(gesture(1000, 60, 120), 16, 80);
    float before = s.position;
    float anchor = ui_scroll_physics_begin(&s, s.tick);
    CHECK(s.position == before && s.velocity == 0 && s.phase == UI_SCROLL_DRAG);
    ui_scroll_physics_drag(&s, anchor, s.tick);
    CHECK(fabsf(s.position - before) <= 0.3f);
    ui_scroll_physics_drag(&s, anchor + 17, s.tick + 20);
    unsigned nearest = ui_scroll_physics_nearest(&s);
    ui_scroll_physics_release(&s, s.tick, true);
    CHECK(s.velocity == 0 && s.phase != UI_SCROLL_COAST);
    CHECK(s.target == nearest * s.stride);
    finish(&s, s.tick, 16);

    s = gesture(1000, 60, 120);
    ui_scroll_physics_stop(&s);
    settled(&s);
    CHECK(!ui_scroll_physics_step(&s, 9000));
    ui_scroll_physics_init(&s, 34, 128, 20);
    ui_scroll_physics_snap(&s, 10, true, 1000);
    (void)ui_scroll_physics_step(&s, 1040);
    CHECK(s.phase == UI_SCROLL_SPRING);
    before = s.position;
    anchor = ui_scroll_physics_begin(&s, 1040);
    CHECK(s.position == before && s.velocity == 0 && anchor == before);
    ui_scroll_physics_release(&s, 1040, true); finish(&s, 1040, 16);
    const float distances[] = { 0, 1, 12, 56, 150, 1000, 1000000 };
    for (unsigned i = 0; i < sizeof(distances) / sizeof(distances[0]); i++) {
        ui_scroll_physics_t left, right;
        ui_scroll_physics_init(&left, 34, 128, 0);
        ui_scroll_physics_init(&right, 34, 128, 33);
        ui_scroll_physics_begin(&left, 1000); ui_scroll_physics_begin(&right, 1000);
        ui_scroll_physics_drag(&left, -distances[i], 1040);
        ui_scroll_physics_drag(&right, right.max_position + distances[i], 1040);
        invariant(&left); invariant(&right);
        CHECK(fabsf(left.position + right.position - right.max_position) < 0.002f);
        float left_before = left.position, right_before = right.position;
        float la = ui_scroll_physics_begin(&left, 1040);
        float ra = ui_scroll_physics_begin(&right, 1040);
        CHECK(left.position == left_before && right.position == right_before);
        ui_scroll_physics_drag(&left, la, 1040); ui_scroll_physics_drag(&right, ra, 1040);
        CHECK(fabsf(left.position - left_before) <= 0.3f);
        CHECK(fabsf(right.position - right_before) <= 0.3f);
        ui_scroll_physics_release(&left, 1050, false); ui_scroll_physics_release(&right, 1050, false);
        for (uint32_t now = 1066; now < 2000; now += 16) {
            (void)ui_scroll_physics_step(&left, now); (void)ui_scroll_physics_step(&right, now);
            invariant(&left); invariant(&right);
            CHECK(fabsf(left.position + right.position - right.max_position) < 0.02f);
        }
        settled(&left); settled(&right);
    }
    scenarios += 10;
}

static void test_counts_focus_and_inputs(void)
{
    const unsigned counts[] = { 0, 1, 2, 34 };
    for (unsigned c = 0; c < 4; c++) {
        ui_scroll_physics_t s;
        ui_scroll_physics_init(&s, counts[c], 128, UINT_MAX);
        invariant(&s);
        ui_scroll_physics_snap(&s, UINT_MAX, true, 0); finish(&s, 0, 16);
        ui_scroll_physics_begin(&s, 100);
        ui_scroll_physics_drag(&s, -100000, 116);
        if (s.count < 2) CHECK(s.position == 0);
        float position = s.position;
        ui_scroll_physics_drag(&s, NAN, 120);
        ui_scroll_physics_drag(&s, INFINITY, 130);
        CHECK(s.position == position);
        ui_scroll_physics_release(&s, 132, false); finish(&s, 132, 16);
        for (unsigned i = 0; i < counts[c]; i++) {
            ui_scroll_physics_snap(&s, i, true, 1000); finish(&s, 1000, 33);
            CHECK(ui_scroll_physics_focus(&s, i) == 1024);
        }
        ui_scroll_physics_snap(&s, UINT_MAX, false, 2000); settled(&s);
    }
    const float bad_strides[] = { NAN, INFINITY, -INFINITY, -10, 0, 0.5f };
    for (unsigned i = 0; i < sizeof(bad_strides) / sizeof(bad_strides[0]); i++) {
        ui_scroll_physics_t s;
        ui_scroll_physics_init(&s, 34, bad_strides[i], 4);
        invariant(&s); CHECK(s.stride == 1.0f);
    }
    ui_scroll_physics_t huge;
    ui_scroll_physics_init(&huge, UINT_MAX, FLT_MAX, UINT_MAX);
    CHECK(finite_state(&huge)); /* Public finite inputs must not overflow into NaN/Inf. */
    if (finite_state(&huge)) invariant(&huge);
    ui_scroll_physics_init(NULL, 34, 128, 1);
    CHECK(ui_scroll_physics_begin(NULL, 0) == 0);
    ui_scroll_physics_drag(NULL, 10, 0); ui_scroll_physics_release(NULL, 0, false);
    CHECK(!ui_scroll_physics_step(NULL, 0)); ui_scroll_physics_snap(NULL, 0, true, 0);
    ui_scroll_physics_stop(NULL);
    CHECK(ui_scroll_physics_nearest(NULL) == 0 && ui_scroll_physics_focus(NULL, 0) == 0);
    scenarios += 11;
}

static void test_wraparound(void)
{
    uint32_t ordinary = 1000, wrapped = UINT32_MAX - 35U;
    ui_scroll_physics_t a = gesture(ordinary, 60, 120);
    ui_scroll_physics_t b = gesture(wrapped, 60, 120);
    CHECK(a.position == b.position && a.velocity == b.velocity && a.target == b.target);
    for (unsigned elapsed = 16; elapsed <= 3000; elapsed += 16) {
        (void)ui_scroll_physics_step(&a, ordinary + 60 + elapsed);
        (void)ui_scroll_physics_step(&b, wrapped + 60 + elapsed);
        CHECK(a.position == b.position && a.velocity == b.velocity && a.phase == b.phase);
        invariant(&a); invariant(&b);
    }
    settled(&a); settled(&b);
    scenarios++;
}

static uint32_t random_state = 0x4A913B27U;
static uint32_t next_random(void)
{
    random_state = random_state * 1664525U + 1013904223U;
    return random_state;
}

static void test_deterministic_trajectories(void)
{
    const unsigned dts[] = { 0, 1, 4, 7, 8, 10, 16, 33, 64, 125, 500 };
    for (unsigned run = 0; run < 250; run++) {
        ui_scroll_physics_t s;
        unsigned count = next_random() % 35;
        uint32_t now = UINT32_MAX - (next_random() % 1000);
        ui_scroll_physics_init(&s, count, 32 + next_random() % 481, next_random() % 40);
        float raw = ui_scroll_physics_begin(&s, now);
        for (unsigned sample_index = 0; sample_index < 30; sample_index++) {
            now += dts[next_random() % (sizeof(dts) / sizeof(dts[0]))];
            raw += (int)(next_random() % 801) - 400;
            ui_scroll_physics_drag(&s, raw, now);
            invariant(&s);
        }
        bool cancelled = (run % 4) == 0;
        ui_scroll_physics_release(&s, now, cancelled);
        if (cancelled) CHECK(s.velocity == 0 && s.phase != UI_SCROLL_COAST);
        for (unsigned elapsed = 0; s.phase != UI_SCROLL_IDLE && elapsed < 4000;) {
            unsigned dt = dts[next_random() % (sizeof(dts) / sizeof(dts[0]))];
            now += dt; elapsed += dt;
            (void)ui_scroll_physics_step(&s, now);
            invariant(&s);
        }
        settled(&s);
        scenarios++;
    }
}

int main(void)
{
    test_speed_hold_reverse();
    test_elapsed_and_dropped_frames();
    test_input_sampling_and_focus();
    test_interrupt_cancel_and_edges();
    test_counts_focus_and_inputs();
    test_wraparound();
    test_deterministic_trajectories();
    if (failures) {
        fprintf(stderr, "FAIL: %u checks across %u deterministic scenarios\n", failures, scenarios);
        return EXIT_FAILURE;
    }
    printf("PASS: %u deterministic physics scenarios, finite/bounded/legal snap, focus, time and interruption invariants\n", scenarios);
    return EXIT_SUCCESS;
}
