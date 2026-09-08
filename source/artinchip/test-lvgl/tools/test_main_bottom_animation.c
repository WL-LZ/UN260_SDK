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
    int x, opacity, zoom;
    unsigned writes, cancelled, animations, slides, active;
} lv_obj_t;
typedef struct {
    lv_obj_t *var;
    int from, to;
    unsigned time;
    void (*exec)(void *, int32_t);
} lv_anim_t;
#define LV_OPA_COVER 255
#define LV_OPA_70 178
#define LV_OPA_60 153
enum { MODE_NONE, MODE_MDC, MODE_CNT, MODE_VER, MODE_SDC };
static bool visible = true, transitioning;
static uint32_t s_main_dirty;
static int model[5];
static lv_obj_t labels[7];
static lv_obj_t *s_bottom_a_label_mode = &labels[0];
static lv_obj_t *s_bottom_a_label_add = &labels[1];
static lv_obj_t *s_bottom_a_label_work = &labels[2];
static lv_obj_t *s_bottom_a_label_fo = &labels[3];
static lv_obj_t *s_bottom_c_label_speed = &labels[4];
static lv_obj_t *s_bottom_c_label_batch = &labels[5];
static lv_obj_t *s_bottom_c_label_cfd = &labels[6];
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
static void lv_anim_set_path_cb(lv_anim_t *animation, void (*path)(void)) { (void)animation; (void)path; }
static void lv_anim_start(lv_anim_t *animation) {
    assert(animation->time == 160 && animation->exec != NULL);
    animation->var->animations++;
    animation->var->active++;
    if (animation->from == 12 && animation->to == 0) animation->var->slides++;
}
static void lv_anim_del(lv_obj_t *object, void *callback) {
    (void)callback;
    object->cancelled += object->active;
    object->active = 0;
}
static const char *lv_label_get_text(lv_obj_t *label) { return label->text; }
static void lv_label_set_text(lv_obj_t *label, const char *text) {
    snprintf(label->text, sizeof(label->text), "%s", text); label->writes++;
}
static void lv_obj_set_style_text_opa(lv_obj_t *label, int value, int selector) { (void)selector; label->opacity = value; }
static void lv_obj_set_style_translate_x(lv_obj_t *label, int value, int selector) { (void)selector; label->x = value; }
static void lv_obj_set_style_transform_zoom(lv_obj_t *label, int value, int selector) { (void)selector; label->zoom = value; }
static int machine_state_mode(void) { return model[0]; }
static int machine_state_add_enabled(void) { return model[1]; }
static int machine_state_work_mode(void) { return model[2]; }
static int machine_state_fo_mode(void) { return model[3]; }
static int machine_state_speed(void) { return model[4]; }
static int machine_state_batch_num(void) { return 100; }
static int machine_state_batch_mode(void) { return 0; }
static int machine_state_batch_enabled(void) { return 0; }
static int machine_state_cfd_mode(void) { return 0; }
#define lv_snprintf snprintf
enum { UI_TEXT_PAGE01_BOTTOM_BATCH_VALUE_FMT, UI_TEXT_PAGE01_BOTTOM_BATCH_OFF,
       UI_TEXT_PAGE01_BOTTOM_CFD_FMT };
static const char *ui_text_get(int id) {
    static const char *texts[] = {"BATCH:%d", "BATCH:OFF", "CFD:%s"};
    return texts[id];
}
static const char *value_text(int value) { return value ? "NEW" : "OLD"; }
static const char *page_01_bottom_mode_text_get(uint8_t mode) { return value_text(mode); }
static const char *page_01_bottom_add_text_get(void) { return value_text(model[1]); }
static const char *page_01_bottom_work_text_get(void) { return value_text(model[2]); }
static const char *page_01_bottom_fo_text_get(void) { return value_text(model[3]); }
static const char *page_01_bottom_speed_text_get(void) { return value_text(model[4]); }
static void update_label_by_name(int objects, int len, const char *name, const char *format, ...) {
    (void)objects; (void)len; (void)name; (void)format;
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

static void reset(void)
{
    ui_frame_commit_cancel(page_01_main_commit, NULL);
    visible = true; transitioning = false; s_main_dirty = 0;
    s_main_commit_animation_flags = 0;
    memset(labels, 0, sizeof(labels));
    memset(model, 0, sizeof(model));
    for (unsigned i = 0; i < 7; i++) snprintf(labels[i].text, sizeof(labels[i].text), "OLD");
}

int main(void)
{
    void (*bottom[])(bool) = { page_01_bottom_a_refresh_mode, page_01_bottom_a_refresh_add,
        page_01_bottom_a_refresh_work, page_01_bottom_a_refresh_fo, page_01_bottom_c_refresh_speed };
    void (*refresh[])(void) = { page_01_mode_switch_refre, page_01_add_refre,
        page_01_work_refre, page_01_face_refre, page_01_speed_refre };
    const uint32_t flags[] = { PAGE_01_MAIN_DIRTY_MODE, PAGE_01_MAIN_DIRTY_ADD,
        PAGE_01_MAIN_DIRTY_WORK, PAGE_01_MAIN_DIRTY_FO, PAGE_01_MAIN_DIRTY_SPEED };
    for (unsigned i = 0; i < 5; i++) {
        reset(); model[i] = 1;
        ui_frame_commit_begin_batch();
        bottom[i](true); refresh[i](); bottom[i](false);
        ui_frame_commit_end_batch();
        assert(labels[i].writes == 0 && labels[i].slides == 0);
        ui_frame_commit_flush();
        assert(labels[i].writes == 1 && labels[i].slides == 1 && labels[i].animations == 2);
        assert(labels[i].x == 12 && labels[i].opacity == LV_OPA_70);
        assert(labels[i].active == 2 && labels[i].cancelled == 0);
        assert(s_main_commit_animation_flags == 0 && s_main_dirty == 0);
        refresh[i](); /* A normal same-state projection cannot cancel the fresh slide. */
        bottom[i](true);
        assert(labels[i].slides == 1 && labels[i].active == 2 && labels[i].cancelled == 0);
        model[i] = 0;
        ui_frame_commit_begin_batch(); refresh[i](); ui_frame_commit_end_batch();
        ui_frame_commit_flush();
        assert(labels[i].slides == 1); /* Animation metadata consumed exactly once. */

        reset(); model[i] = 1;
        ui_frame_commit_begin_batch(); refresh[i](); bottom[i](true); ui_frame_commit_end_batch();
        ui_frame_commit_flush(); /* MODE's plain-then-animated reply order. */
        assert(labels[i].slides == 1);

        reset(); model[i] = 1; visible = false;
        ui_frame_commit_begin_batch(); bottom[i](true); ui_frame_commit_end_batch();
        assert(!ui_frame_commit_pending() && s_main_dirty == flags[i]);
        visible = true; transitioning = true; refresh[i](); transitioning = false;
        assert(labels[i].slides == 0 && s_main_dirty == 0);

        reset(); model[i] = 1;
        ui_frame_commit_begin_batch(); bottom[i](true); ui_frame_commit_end_batch();
        visible = false; ui_frame_commit_flush(); /* Hidden before the queued callback runs. */
        assert(s_main_dirty == flags[i] && s_main_commit_animation_flags == 0);
        visible = true; refresh[i]();
        assert(labels[i].slides == 0 && s_main_dirty == 0);

        reset(); model[i] = 1;
        ui_frame_commit_begin_batch(); bottom[i](true); ui_frame_commit_end_batch();
        page_01_bottom_animations_stop(); /* Real suspend/destroy cancellation. */
        assert(!ui_frame_commit_pending() && s_main_dirty == flags[i]);
        transitioning = true; refresh[i](); transitioning = false;
        ui_frame_commit_flush();
        assert(labels[i].slides == 0 && s_main_dirty == 0);

        reset(); model[i] = 1; bottom[i](true); /* Keep the synchronous public API behavior. */
        assert(labels[i].slides == 1 && !ui_frame_commit_pending());
    }
    reset();
    ui_frame_commit_begin_batch();
    for (unsigned i = 0; i < 5; i++) { model[i] = 1; bottom[i](true); refresh[i](); }
    ui_frame_commit_end_batch(); ui_frame_commit_flush();
    for (unsigned i = 0; i < 5; i++) assert(labels[i].slides == 1 && labels[i].cancelled == 0);

    /* Boot prewarm is inside an ordinary data batch, not a transition. The
     * production create writes all seven labels, snapshots/clears dirty, then
     * suspend cancels this owner's queued animations/projections. Resuming an
     * unchanged model must therefore already have real text without a click. */
    reset();
    for (unsigned i = 0; i < 7; i++) strcpy(labels[i].text, "Text");
    ui_frame_commit_begin_batch();
    ui_frame_commit_begin_sync();
    for (unsigned i = 0; i < 5; i++) bottom[i](false);
    page_01_bottom_c_refresh_batch(false);
    page_01_bottom_c_refresh_cfd();
    ui_frame_commit_end_sync();
    s_main_dirty = 0; /* Actual create's completed projection snapshot. */
    page_01_bottom_animations_stop();
    visible = false;
    ui_frame_commit_end_batch();
    ui_frame_commit_flush();
    visible = true;
    assert(s_main_dirty == 0 && !ui_frame_commit_pending());
    for (unsigned i = 0; i < 7; i++) {
        assert(strcmp(labels[i].text, "Text") != 0);
        assert(labels[i].writes == 1 && labels[i].animations == 0);
    }
    assert(strcmp(labels[5].text, "BATCH:OFF") == 0);
    assert(strcmp(labels[6].text, "CFD:L") == 0);
    puts("PASS: Main MODE/ADD/WORK/FO/SPEED slide intent survives coalescing; hidden/navigation reset, one-shot consumption, same-text preservation");
    puts("PASS: all seven real Main bottom projections survive batch prewarm, suspend cancellation and clean resume without a click");
    return 0;
}
