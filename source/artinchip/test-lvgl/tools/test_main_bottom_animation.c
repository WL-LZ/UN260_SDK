#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_core/ui_frame_commit.h"

typedef int lv_coord_t;
typedef int lv_opa_t;
typedef struct {
    char text[64];
    int x, opacity;
    unsigned writes, cancelled, animations, slides, active;
    int from[2], to[2];
    void (*exec[2])(void *, int32_t);
} lv_obj_t;
typedef struct {
    lv_obj_t *var;
    int from, to;
    unsigned time;
    void (*exec)(void *, int32_t);
} lv_anim_t;
#define LV_OPA_COVER 255
#define LV_OPA_70 178
enum { MODE_NONE, MODE_MDC, MODE_CNT, MODE_VER, MODE_SDC };
enum { MODE, ADD, WORK, FO, SPEED, BATCH, CFD, LABEL_COUNT };
static bool visible = true, transitioning;
static uint32_t s_main_dirty;
static int model[5], batch_enabled, batch_num, cfd_mode;
static lv_obj_t labels[LABEL_COUNT];
static lv_obj_t *s_bottom_a_label_mode = &labels[MODE];
static lv_obj_t *s_bottom_a_label_add = &labels[ADD];
static lv_obj_t *s_bottom_a_label_work = &labels[WORK];
static lv_obj_t *s_bottom_a_label_fo = &labels[FO];
static lv_obj_t *s_bottom_c_label_speed = &labels[SPEED];
static lv_obj_t *s_bottom_c_label_batch = &labels[BATCH];
static lv_obj_t *s_bottom_c_label_cfd = &labels[CFD];
static int page_01_main_obj, page_01_main_len;
static bool page_01_main_is_visible(void) { return visible; }
static bool ui_manager_is_transitioning(void) { return transitioning; }
static bool lv_obj_is_valid(lv_obj_t *object) { return object != NULL; }
static void lv_anim_path_ease_out(void) {}
static void lv_anim_init(lv_anim_t *animation) { memset(animation, 0, sizeof(*animation)); }
static void lv_anim_set_var(lv_anim_t *animation, lv_obj_t *object) { animation->var = object; }
static void lv_anim_set_time(lv_anim_t *animation, unsigned time) { animation->time = time; }
static void lv_anim_set_values(lv_anim_t *animation, int from, int to) { animation->from = from; animation->to = to; }
static void lv_anim_set_exec_cb(lv_anim_t *animation, void (*exec)(void *, int32_t)) { animation->exec = exec; }
static void lv_anim_set_path_cb(lv_anim_t *animation, void (*path)(void)) { (void)animation; assert(path == lv_anim_path_ease_out); }
static void lv_anim_start(lv_anim_t *animation) {
    lv_obj_t *label = animation->var;
    assert(animation->time == 160 && animation->exec != NULL);
    assert(label && label->active < 2);
    /* Exactly opacity plus left-to-right translation, with no zoom channel. */
    assert((animation->from == LV_OPA_70 && animation->to == LV_OPA_COVER) ||
           (animation->from == -12 && animation->to == 0));
    const unsigned channel = label->active++;
    label->from[channel] = animation->from;
    label->to[channel] = animation->to;
    label->exec[channel] = animation->exec;
    label->animations++;
    if (animation->from == -12 && animation->to == 0) label->slides++;
}
static void lv_anim_del(lv_obj_t *object, void *callback) {
    assert(callback == NULL);
    object->cancelled += object->active;
    object->active = 0;
}
static const char *lv_label_get_text(lv_obj_t *label) { return label->text; }
static void lv_label_set_text(lv_obj_t *label, const char *text) {
    snprintf(label->text, sizeof(label->text), "%s", text); label->writes++;
}
static void lv_obj_set_style_text_opa(lv_obj_t *label, int value, int selector) { assert(selector == 0); label->opacity = value; }
static void lv_obj_set_style_translate_x(lv_obj_t *label, int value, int selector) { assert(selector == 0); label->x = value; }
static int machine_state_mode(void) { return model[MODE]; }
static int machine_state_add_enabled(void) { return model[ADD]; }
static int machine_state_work_mode(void) { return model[WORK]; }
static int machine_state_fo_mode(void) { return model[FO]; }
static int machine_state_speed(void) { return model[SPEED]; }
static int machine_state_batch_num(void) { return batch_num; }
static int machine_state_batch_enabled(void) { return batch_enabled; }
static int machine_state_cfd_mode(void) { return cfd_mode; }
#define lv_snprintf snprintf
enum {
    UI_TEXT_PAGE01_BOTTOM_MODE_MDC, UI_TEXT_PAGE01_BOTTOM_MODE_SDC,
    UI_TEXT_PAGE01_BOTTOM_MODE_CNT, UI_TEXT_PAGE01_BOTTOM_ADD_ON,
    UI_TEXT_PAGE01_BOTTOM_ADD_OFF, UI_TEXT_PAGE01_BOTTOM_WORK_AUTO,
    UI_TEXT_PAGE01_BOTTOM_WORK_MANUAL, UI_TEXT_PAGE01_BOTTOM_FO_OFF,
    UI_TEXT_PAGE01_BOTTOM_FO_F, UI_TEXT_PAGE01_BOTTOM_FO_O,
    UI_TEXT_PAGE01_BOTTOM_FO_FO, UI_TEXT_PAGE01_BOTTOM_SPEED_LOW,
    UI_TEXT_PAGE01_BOTTOM_SPEED_MID, UI_TEXT_PAGE01_BOTTOM_SPEED_HIGH,
    UI_TEXT_PAGE01_BOTTOM_BATCH_VALUE_FMT, UI_TEXT_PAGE01_BOTTOM_BATCH_OFF,
    UI_TEXT_PAGE01_BOTTOM_CFD_FMT
};
/* Localization is a fixture; production state-to-text selection is extracted. */
static const char *ui_text_get(int id) {
    static const char *texts[] = {"MDC", "SDC", "CNT", "ADD:ON", "ADD:OFF",
        "AUTO", "MANUAL", "SORT:OFF", "SORT:F", "SORT:O", "SORT:F/O",
        "LOW", "MID", "HIGH", "BATCH:%d", "BATCH:OFF", "CFD:%s"};
    assert(id >= 0 && (unsigned)id < sizeof(texts) / sizeof(texts[0]));
    return texts[id];
}
static void update_label_by_name(int objects, int len, const char *name, const char *format, ...) {
    (void)objects; (void)len; assert(strcmp(name, "mode_label") == 0); assert(strcmp(format, "%s") == 0);
}
void page_01_mode_switch_refre(void);
void page_01_add_refre(void);
void page_01_work_refre(void);
void page_01_face_refre(void);
void page_01_speed_refre(void);
void page_01_bottom_a_refresh_mode(bool);
void page_01_bottom_a_refresh_add(bool);
void page_01_bottom_a_refresh_work(bool);
void page_01_bottom_a_refresh_fo(bool);
void page_01_bottom_c_refresh_speed(bool);
void page_01_bottom_c_refresh_batch(bool);
void page_01_bottom_c_refresh_cfd(void);
bool page_01_main_defer_refresh(uint32_t);
void page_01_batch_refre(void);
void page_01_cfd_refre(void);
static void page_01_err_num_refre(void) {}
static void page_01_curr_img_refre(void) {}
static void page_01_update_language_texts(void) {}
static void ui_refresh_main_page(void) {}
#include "main_animation_under_test.h"

static void (*const bottom[])(bool) = { page_01_bottom_a_refresh_mode,
    page_01_bottom_a_refresh_add, page_01_bottom_a_refresh_work,
    page_01_bottom_a_refresh_fo, page_01_bottom_c_refresh_speed,
    page_01_bottom_c_refresh_batch };
static void (*const refresh[])(void) = { page_01_mode_switch_refre,
    page_01_add_refre, page_01_work_refre, page_01_face_refre,
    page_01_speed_refre, page_01_batch_refre };
static const uint32_t flags[] = { PAGE_01_MAIN_DIRTY_MODE,
    PAGE_01_MAIN_DIRTY_ADD, PAGE_01_MAIN_DIRTY_WORK, PAGE_01_MAIN_DIRTY_FO,
    PAGE_01_MAIN_DIRTY_SPEED, PAGE_01_MAIN_DIRTY_BATCH };
static const char *const initial[] = { "MDC", "ADD:OFF", "AUTO", "SORT:OFF",
    "LOW", "BATCH:OFF", "CFD:L" };
static const char *const changed[] = { "SDC", "ADD:ON", "MANUAL", "SORT:F",
    "MID", "BATCH:100" };
static void reset(void)
{
    assert(!ui_frame_commit_is_batching());
    ui_frame_commit_cancel(page_01_main_commit, NULL);
    assert(!ui_frame_commit_pending());
    visible = true; transitioning = false; s_main_dirty = 0;
    s_main_commit_animation_flags = 0;
    memset(labels, 0, sizeof(labels));
    memset(model, 0, sizeof(model));
    model[MODE] = MODE_MDC;
    batch_num = 100; batch_enabled = cfd_mode = 0;
    for (unsigned i = 0; i < LABEL_COUNT; i++) {
        snprintf(labels[i].text, sizeof(labels[i].text), "%s", initial[i]);
        labels[i].opacity = LV_OPA_COVER;
    }
}
static void change(unsigned index, bool enabled)
{
    if (index == BATCH) batch_enabled = enabled;
    else model[index] = index == MODE ? (enabled ? MODE_SDC : MODE_MDC) : enabled;
}
static void expect_text(unsigned index, const char *text)
{
    if (strcmp(labels[index].text, text) != 0)
        fprintf(stderr, "label %u: expected %s, got %s\n", index, text, labels[index].text);
    assert(strcmp(labels[index].text, text) == 0);
}
static void step_animation(lv_obj_t *label, unsigned elapsed)
{
    assert(elapsed <= 160 && label->active == 2);
    for (unsigned i = 0; i < label->active; i++)
        label->exec[i](label, label->from[i] +
            (label->to[i] - label->from[i]) * (int)elapsed / 160);
    if (elapsed == 160) label->active = 0;
}
static void test_coalescing_and_visibility(void)
{
    for (unsigned i = 0; i < BATCH + 1; i++) {
        reset(); change(i, true);
        ui_frame_commit_begin_batch();
        bottom[i](true); refresh[i](); bottom[i](false);
        ui_frame_commit_end_batch();
        assert(labels[i].writes == 0 && labels[i].slides == 0);
        ui_frame_commit_flush();
        expect_text(i, changed[i]);
        assert(labels[i].writes == 1 && labels[i].slides == 1 && labels[i].animations == 2);
        assert(labels[i].x == -12 && labels[i].opacity == LV_OPA_70);
        assert(labels[i].active == 2 && labels[i].cancelled == 0);
        assert(s_main_commit_animation_flags == 0 && s_main_dirty == 0);
        step_animation(&labels[i], 80);
        assert(labels[i].x == -6 && labels[i].opacity > LV_OPA_70);
        refresh[i](); bottom[i](true);
        assert(labels[i].writes == 1 && labels[i].slides == 1);
        assert(labels[i].active == 2 && labels[i].cancelled == 0 && labels[i].x == -6);
        change(i, false);
        ui_frame_commit_begin_batch(); refresh[i](); ui_frame_commit_end_batch();
        ui_frame_commit_flush();
        expect_text(i, initial[i]);
        assert(labels[i].slides == 1 && labels[i].cancelled == 2);
        assert(labels[i].x == 0 && labels[i].opacity == LV_OPA_COVER);
        assert(labels[i].active == 0); /* Interaction intent consumed only once. */

        reset(); change(i, true);
        ui_frame_commit_begin_batch(); refresh[i](); bottom[i](true); ui_frame_commit_end_batch();
        ui_frame_commit_flush(); /* Plain-then-animated reply order. */
        expect_text(i, changed[i]); assert(labels[i].slides == 1);

        reset(); change(i, true); visible = false;
        ui_frame_commit_begin_batch(); bottom[i](true); ui_frame_commit_end_batch();
        assert(!ui_frame_commit_pending() && s_main_dirty == flags[i]);
        assert(labels[i].writes == 0);
        visible = true; transitioning = true; refresh[i](); transitioning = false;
        expect_text(i, changed[i]); assert(labels[i].slides == 0 && s_main_dirty == 0);

        reset(); change(i, true);
        ui_frame_commit_begin_batch(); bottom[i](true); ui_frame_commit_end_batch();
        visible = false; ui_frame_commit_flush();
        assert(s_main_dirty == flags[i] && s_main_commit_animation_flags == 0);
        assert(labels[i].writes == 0);
        visible = true; refresh[i]();
        expect_text(i, changed[i]); assert(labels[i].slides == 0 && s_main_dirty == 0);

        reset(); change(i, true);
        ui_frame_commit_begin_batch(); bottom[i](true); ui_frame_commit_end_batch();
        page_01_bottom_animations_stop();
        assert(!ui_frame_commit_pending() && s_main_dirty == flags[i]);
        transitioning = true; refresh[i](); transitioning = false;
        ui_frame_commit_flush();
        expect_text(i, changed[i]); assert(labels[i].slides == 0 && s_main_dirty == 0);

        reset(); change(i, true); bottom[i](true);
        assert(labels[i].slides == 1 && !ui_frame_commit_pending());
        step_animation(&labels[i], 160);
        assert(labels[i].x == 0 && labels[i].opacity == LV_OPA_COVER && labels[i].active == 0);

        reset(); change(i, true); transitioning = true; bottom[i](true); transitioning = false;
        expect_text(i, changed[i]); assert(labels[i].slides == 0 && labels[i].writes == 1);
    }
}
static void test_rapid_updates(void)
{
    for (unsigned i = 0; i < BATCH + 1; i++) {
        reset();
        ui_frame_commit_begin_batch();
        for (unsigned n = 0; n < 9; n++) { change(i, n % 2 == 0); bottom[i](true); refresh[i](); }
        ui_frame_commit_end_batch(); ui_frame_commit_flush();
        expect_text(i, changed[i]); assert(labels[i].writes == 1 && labels[i].slides == 1);
        step_animation(&labels[i], 80);
        change(i, false); bottom[i](true);
        expect_text(i, initial[i]);
        assert(labels[i].writes == 2 && labels[i].slides == 2);
        assert(labels[i].active == 2 && labels[i].cancelled == 2);
        assert(labels[i].x == -12 && labels[i].opacity == LV_OPA_70);
        page_01_bottom_animations_stop();
        assert(labels[i].active == 0 && labels[i].cancelled == 4);
        assert(labels[i].x == 0 && labels[i].opacity == LV_OPA_COVER);

        reset(); ui_frame_commit_begin_batch();
        change(i, true); bottom[i](true); change(i, false); bottom[i](true);
        ui_frame_commit_end_batch(); ui_frame_commit_flush();
        expect_text(i, initial[i]);
        assert(labels[i].writes == 0 && labels[i].animations == 0 && s_main_dirty == 0);
    }
    reset(); ui_frame_commit_begin_batch();
    for (unsigned i = 0; i < BATCH + 1; i++) { change(i, true); bottom[i](true); refresh[i](); }
    ui_frame_commit_end_batch(); ui_frame_commit_flush();
    for (unsigned i = 0; i < BATCH + 1; i++) {
        expect_text(i, changed[i]); assert(labels[i].slides == 1 && labels[i].cancelled == 0);
    }
    page_01_bottom_animations_stop();
    for (unsigned i = 0; i < BATCH + 1; i++)
        assert(labels[i].active == 0 && labels[i].cancelled == 2 && labels[i].x == 0);
}
static void test_actual_state_text(void)
{
    static const struct { unsigned index; int value; const char *text; } states[] = {
        {MODE, MODE_MDC, "MDC"}, {MODE, MODE_SDC, "SDC"}, {MODE, MODE_CNT, "CNT"},
        {MODE, MODE_NONE, "MDC"}, {MODE, MODE_VER, "MDC"}, {MODE, 255, "MDC"},
        {ADD, 0, "ADD:OFF"}, {ADD, 1, "ADD:ON"}, {WORK, 0, "AUTO"}, {WORK, 1, "MANUAL"},
        {FO, 0, "SORT:OFF"}, {FO, 1, "SORT:F"}, {FO, 2, "SORT:O"},
        {FO, 3, "SORT:F/O"}, {FO, 255, "SORT:OFF"},
        {SPEED, 0, "LOW"}, {SPEED, 1, "MID"}, {SPEED, 2, "HIGH"}, {SPEED, 255, "LOW"}
    };
    reset();
    for (unsigned i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        model[states[i].index] = states[i].value; bottom[states[i].index](false);
        expect_text(states[i].index, states[i].text);
        assert(labels[states[i].index].animations == 0);
    }
    for (unsigned value = 0; value < 4; value++) {
        cfd_mode = (int)value;
        ui_frame_commit_begin_batch(); page_01_cfd_refre(); ui_frame_commit_end_batch();
        ui_frame_commit_flush();
        expect_text(CFD, (const char *[]) {"CFD:L", "CFD:M", "CFD:H", "CFD:L"}[value]);
        assert(labels[CFD].animations == 0 && labels[CFD].x == 0);
    }
    batch_enabled = 1;
    for (unsigned i = 0; i < 3; i++) {
        static const int values[] = {0, 100, 99999};
        static const char *texts[] = {"BATCH:0", "BATCH:100", "BATCH:99999"};
        batch_num = values[i]; page_01_bottom_c_refresh_batch(false); expect_text(BATCH, texts[i]);
    }
    batch_enabled = 0; page_01_batch_refre(); expect_text(BATCH, "BATCH:OFF");
    reset(); visible = false; cfd_mode = 2; page_01_cfd_refre();
    assert(s_main_dirty == PAGE_01_MAIN_DIRTY_CFD && labels[CFD].writes == 0);
    visible = true; transitioning = true; page_01_cfd_refre(); transitioning = false;
    expect_text(CFD, "CFD:H"); assert(s_main_dirty == 0 && labels[CFD].animations == 0);
}
static void test_initial_projection(void)
{
    /* Real writers are synchronous inside an ordinary prewarm data batch,
     * before create snapshots/clears dirtiness and suspend cancels this owner. */
    for (int level = 0; level < 3; level++) {
        reset(); cfd_mode = level;
        for (unsigned i = 0; i < LABEL_COUNT; i++) strcpy(labels[i].text, "Text");
        ui_frame_commit_begin_batch(); ui_frame_commit_begin_sync();
        for (unsigned i = 0; i < BATCH + 1; i++) bottom[i](false);
        page_01_bottom_c_refresh_cfd(); ui_frame_commit_end_sync();
        s_main_dirty = 0; page_01_bottom_animations_stop(); visible = false;
        ui_frame_commit_end_batch(); ui_frame_commit_flush(); visible = true;
        assert(s_main_dirty == 0 && !ui_frame_commit_pending());
        for (unsigned i = 0; i < LABEL_COUNT; i++) {
            expect_text(i, i == CFD ? (const char *[]) {"CFD:L", "CFD:M", "CFD:H"}[level] : initial[i]);
            assert(labels[i].writes == 1 && labels[i].animations == 0);
        }
    }
}
static unsigned unrelated_calls;
static void unrelated(void *context, uint32_t request)
{ assert(context == &unrelated_calls && request == 8); unrelated_calls++; }
static void test_owner_cleanup(void)
{
    reset();
    page_01_bottom_label_anim_run(NULL, "test", PAGE_01_BOTTOM_TEXT_ANIM_SLIDE);
    page_01_bottom_label_anim_run(&labels[MODE], NULL, PAGE_01_BOTTOM_TEXT_ANIM_SLIDE);
    assert(labels[MODE].writes == 0);
    change(MODE, true); bottom[MODE](true); ui_frame_commit_begin_batch();
    assert(ui_frame_commit_defer(unrelated, &unrelated_calls, 8));
    change(BATCH, true); bottom[BATCH](true); ui_frame_commit_end_batch();
    page_01_bottom_animations_stop();
    assert(labels[MODE].cancelled == 2 && labels[MODE].x == 0);
    assert(ui_frame_commit_pending() && s_main_dirty == PAGE_01_MAIN_DIRTY_BATCH);
    ui_frame_commit_flush(); assert(unrelated_calls == 1);
    assert(labels[BATCH].writes == 0 && !ui_frame_commit_pending());
    s_bottom_a_label_mode = s_bottom_a_label_add = s_bottom_a_label_work = NULL;
    s_bottom_a_label_fo = s_bottom_c_label_speed = s_bottom_c_label_batch = s_bottom_c_label_cfd = NULL;
    page_01_bottom_animations_stop(); /* Repeated destroy after globals were cleared. */
}
int main(void)
{
    test_coalescing_and_visibility(); test_rapid_updates(); test_actual_state_text();
    test_initial_projection(); test_owner_cleanup();
    puts("PASS: MODE/ADD/WORK/FO/SPEED/BATCH preserve coalesced left-to-right -12->0 / 160ms slide intent, same-text progress and one-shot consumption");
    puts("PASS: rapid replies, synchronous/transition projection, hidden/queued-hidden refresh, owner suspend/destroy and null cleanup");
    puts("PASS: real state-to-text mapping, CFD L/M/H/fallback, Batch values, and all seven initial projections before prewarm suspension");
    return 0;
}
