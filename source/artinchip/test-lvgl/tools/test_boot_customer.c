#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_core/boot_anim/boot_customer_art.h"
static language_t language;
static uint32_t language_generation = 1;
const lv_font_t *ui_message_font(const lv_font_t *font) { return font; }
const lv_font_t *ui_message_font_sized(const lv_font_t *font, unsigned pixels)
{ (void)pixels; return font; }
const char *ui_tr(const char *value)
{
    if (language == 6 && !strcmp(value, "Configuration")) return "Configuration FR";
    return value;
}
language_t ui_lang_get(void) { return language; }
uint32_t ui_lang_generation(void) { return language_generation; }
static bool fail_art, fail_timer, fail_object, fail_malloc;
static unsigned allocations, file_opens, art_acquires, input_holds;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{ ++allocations; if (fail_malloc) { fail_malloc=false; return NULL; } return __real_malloc(size); }
void *__real_calloc(size_t, size_t);
void *__wrap_calloc(size_t n, size_t size) { ++allocations; return __real_calloc(n, size); }
void *__real_realloc(void *, size_t);
void *__wrap_realloc(void *p, size_t size) { ++allocations; return __real_realloc(p, size); }
FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *name, const char *mode) { ++file_opens; return __real_fopen(name, mode); }
bool __real_boot_customer_art_acquire(const char *);
bool __wrap_boot_customer_art_acquire(const char *tag)
{ ++art_acquires; return !fail_art && __real_boot_customer_art_acquire(tag); }
lv_timer_t *__real_lv_timer_create(lv_timer_cb_t, uint32_t, void *);
lv_timer_t *__wrap_lv_timer_create(lv_timer_cb_t cb, uint32_t ms, void *data)
{ if (fail_timer) { fail_timer=false; return NULL; } return __real_lv_timer_create(cb, ms, data); }
lv_obj_t *__real_lv_obj_create(lv_obj_t *);
lv_obj_t *__wrap_lv_obj_create(lv_obj_t *parent)
{ if (fail_object) { fail_object=false; return NULL; } return __real_lv_obj_create(parent); }
#include "un260/lv_core/boot_anim/page_08_boot_customer.c"
#include "un260/lv_core/page_00_boot_anim.h"
static boot_snapshot_t snapshot;
static ui_page_t current = UI_PAGE_BOOT;
void boot_service_snapshot(boot_snapshot_t *value) { *value = snapshot; }
ui_page_t ui_manager_get_current_page(void) { return current; }
void ui_manager_switch(ui_page_t page) { current = page; }
void ui_manager_hold_transition_input(ui_page_t owner, bool hold)
{ (void)owner; if (hold) ++input_holds; }
static lv_color_t framebuffer[1280 * 400], draw_buffer[1280 * 40];
static unsigned pixels, baseline_timers;
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *data)
{
    assert(area->x1 >= 0 && area->y1 >= 0 && area->x2 < 1280 && area->y2 < 400);
    unsigned width = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; ++y)
        memcpy(framebuffer + y * 1280 + area->x1, data + (y - area->y1) * width, width * 4);
    pixels += width * (area->y2 - area->y1 + 1);
    lv_disp_flush_ready(driver);
}
static void frame(unsigned milliseconds)
{ lv_tick_inc(milliseconds); lv_timer_handler(); lv_refr_now(NULL); }
static unsigned timers(void)
{ unsigned n=0; for (lv_timer_t *t=lv_timer_get_next(NULL); t; t=lv_timer_get_next(t)) ++n; return n; }
static void dump(const char *name)
{
    const char *directory = getenv("BOOT_CUSTOMER_RASTER_OUTPUT");
    if (!directory) return;
    char path[2048]; snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE *f = fopen(path, "wb"); assert(f); fprintf(f, "P6\n1280 400\n255\n");
    for (unsigned i=0; i<1280*400; ++i) {
        unsigned color=framebuffer[i].full; fputc(color>>16,f); fputc(color>>8,f); fputc(color,f);
    }
    fclose(f);
}
static lv_obj_t *find_text(lv_obj_t *root, const char *value)
{
    if (lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),value)) return root;
    for (unsigned i=0; i<lv_obj_get_child_cnt(root); ++i) {
        lv_obj_t *found=find_text(lv_obj_get_child(root,i),value); if (found) return found;
    }
    return NULL;
}
static void reset(void)
{
    ui_page_00_boot_anim_destroy(); ui_page_08_curr_destroy();
    assert(timers()==baseline_timers && !boot_customer_art_image(BOOT_CUSTOMER_ART_BG));
    memset(&snapshot,0,sizeof(snapshot)); snapshot.stage=BOOT_STAGE_HANDSHAKE;
    current=UI_PAGE_BOOT; language=0; ++language_generation;
    fail_art=fail_timer=fail_object=fail_malloc=false;
}
static void complete_all(void)
{
    snapshot.connected=true; snapshot.requested_count=5; snapshot.completed_count=5;
    for (unsigned i=0;i<5;++i) { snapshot.items[i].received=true; snapshot.items[i].result=1; }
    snapshot.stage=BOOT_STAGE_DONE; boot_selftest_list_finish();
}
static void unchanged_work(unsigned count)
{
    unsigned before_allocations=allocations, before_files=file_opens, before_art=art_acquires;
    for (unsigned i=0;i<count;++i) frame(10);
    assert(allocations==before_allocations && file_opens==before_files && art_acquires==before_art);
}
int main(void)
{
    lv_init();
    lv_disp_draw_buf_t draw; lv_disp_draw_buf_init(&draw,draw_buffer,NULL,1280*40);
    lv_disp_drv_t driver; lv_disp_drv_init(&driver); driver.hor_res=1280; driver.ver_res=400;
    driver.draw_buf=&draw; driver.flush_cb=flush; assert(lv_disp_drv_register(&driver));
    baseline_timers=timers(); reset();
    ui_page_08_curr_create(NULL); frame(10);
    assert(view.art_acquired && view.version && view.powered && !view.prepare_failed);
    assert(view.phase[0]==CHECK && view.gesture==5 && !view.done);
    for (unsigned i=1;i<5;++i) assert(view.phase[i]==WAIT);
    boot_progress_set(100); boot_selftest_list_sync_step(5); boot_selftest_list_set_result(0,1); boot_selftest_list_finish();
    assert(!view.done && !snapshot.items[0].received && view.phase[0]==CHECK);
    dump("handshake"); unchanged_work(200);
    snapshot.connected=true;
    for (unsigned i=0;i<5;++i) {
        snapshot.requested_count=i+1; boot_selftest_list_sync_step(i); frame(0);
        assert(view.phase[i]==CHECK && view.gesture==5-i && !view.done);
        char step_name[48]; snprintf(step_name,sizeof(step_name),"selfcheck-hand-%u",5-i); dump(step_name);
        unchanged_work(45);
        frame((450U + 1800U - (lv_tick_get() - view.active_at[i]) % 1800U) % 1800U);
        boot_progress_reset(); frame(0);
        assert(view.hand.width==1 && view.hand.sampled.header.w==2 && view.hand.sampled.header.h==220);
        snapshot.items[i].received=true; snapshot.items[i].result=1; snapshot.completed_count=i+1;
        boot_selftest_list_set_result(i,0); /* A caller value cannot override received success. */
        assert(view.phase[i]==PASS && !view.done);
    }
    snapshot.stage=BOOT_STAGE_DONE; boot_selftest_list_finish(); frame(0);
    assert(view.done && !ui_page_08_curr_visual_is_quiet() && !ui_page_08_curr_ready_hold_complete());
    const lv_img_dsc_t *ready=boot_customer_art_image(BOOT_CUSTOMER_ART_READY_GRAY);
    assert(view.ready_width==ready->header.w*171U/256U);
    unchanged_work(100); dump("ready-mid");
    unchanged_work(117); assert(ui_page_08_curr_visual_is_quiet());
    assert(view.ready_width==ready->header.w && view.ready_height==ready->header.h);
    assert(lv_img_get_zoom(view.ready_gray)==256 && lv_img_get_zoom(view.ready_active)==256);
    frame(179); assert(!ui_page_08_curr_ready_hold_complete()); frame(1);
    assert(ui_page_08_curr_ready_hold_complete()); dump("ready-final");
    pixels=0; unchanged_work(30); assert(pixels==0);
    ui_page_08_curr_start_handoff(); ui_page_08_curr_start_handoff();
    assert(input_holds==0 && timers()==baseline_timers+1);
    reset();

    /* Only genuine results own completion, including fast and partial failures. */
    complete_all(); ui_page_08_curr_create(NULL); frame(10); assert(view.done);
    frame(2350); assert(ui_page_08_curr_ready_hold_complete()); reset();
    ui_page_08_curr_create(NULL); complete_all(); snapshot.items[2].result=2;
    boot_selftest_list_finish(); assert(view.failed && !view.done && view.phase[2]==FAIL);
    assert(lv_obj_has_flag(view.powered,LV_OBJ_FLAG_HIDDEN));
    frame(5000); assert(!ui_page_08_curr_ready_hold_complete()); dump("selftest-failure"); reset();
    ui_page_08_curr_create(NULL); snapshot.connected=true; snapshot.requested_count=3;
    for (unsigned i=0;i<2;++i) { snapshot.items[i].received=true; snapshot.items[i].result=1; }
    snapshot.stage=BOOT_STAGE_FAIL; boot_progress_reset(); frame(0);
    assert(view.phase[2]==FAIL && view.phase[3]==WAIT && !view.done);
    assert(!strcmp(lv_label_get_text(view.caption),"Check the reported fault before continuing."));
    reset();

    /* Covers pause presentation clocks; results received under the cover are consumed on reveal. */
    ui_page_08_curr_defer_next_create(); ui_page_08_curr_create(NULL); ui_page_08_curr_set_covered(true);
    while (!ui_page_08_curr_prepare_step()) assert(!ui_page_08_curr_prepare_failed());
    assert(view.timer->paused); complete_all(); frame(5000); assert(!view.done);
    language=6; ++language_generation; ui_page_08_curr_set_covered(false); frame(10);
    assert(view.done && !strcmp(lv_label_get_text(view.rows[0].gray),"Configuration FR"));
    assert(!ui_page_08_curr_ready_hold_complete()); frame(2400); assert(ui_page_08_curr_ready_hold_complete());
    ui_page_08_curr_set_covered(true); frame(3000); assert(!ui_page_08_curr_ready_hold_complete());
    ui_page_08_curr_set_covered(false); assert(!ui_page_08_curr_ready_hold_complete()); reset();

    /* Art, transform-buffer and animation-timer failures do not block real completion. */
    fail_art=true; ui_page_08_curr_create(NULL); frame(10);
    assert(!view.art_acquired && !view.prepare_failed && !lv_obj_has_flag(view.init_text,LV_OBJ_FLAG_HIDDEN));
    complete_all(); frame(2400); assert(ui_page_08_curr_ready_hold_complete()); dump("art-fallback"); reset();
    ui_page_08_curr_defer_next_create(); ui_page_08_curr_create(NULL);
    for (unsigned i=0;i<6;++i) assert(!ui_page_08_curr_prepare_step());
    fail_malloc=true; assert(!ui_page_08_curr_prepare_step()); assert(ui_page_08_curr_prepare_step());
    assert(!view.ready_images && !view.prepare_failed); complete_all(); frame(2400);
    assert(ui_page_08_curr_ready_hold_complete()); reset();
    fail_timer=true; ui_page_08_curr_create(NULL); assert(!view.timer && !view.prepare_failed);
    complete_all(); lv_tick_inc(2350); assert(ui_page_08_curr_ready_hold_complete()); reset();
    fail_object=true; ui_page_08_curr_create(NULL); assert(ui_page_08_curr_prepare_failed()); reset();
    lv_obj_t *parent=lv_obj_create(lv_scr_act()); ui_page_08_curr_create(parent); frame(10);
    lv_obj_del(parent); assert(!view.root && timers()==baseline_timers && !boot_customer_art_image(1));
    reset();

    /* Existing early/intro/self-check ownership: no restart while startup is delayed. */
    ui_page_08_curr_defer_next_create(); ui_page_08_curr_create(NULL);
    ui_page_00_boot_anim_create(lv_layer_top()); ui_page_00_boot_anim_set_startup_ready(false);
    frame(10); for (unsigned i=0;i<65;++i) frame(10); dump("intro-first-screen");
    for (unsigned i=0;i<40 && view.step;++i) frame(LV_DISP_DEF_REFR_PERIOD);
    assert(view.covered && view.timer && view.timer->paused);
    frame(4000); assert(ui_page_00_boot_anim_is_active() && !view.done);
    ui_page_00_boot_anim_set_startup_ready(true); frame(LV_DISP_DEF_REFR_PERIOD);
    assert(!ui_page_00_boot_anim_is_active() && !view.covered && view.art_acquired);
    reset();
    ui_page_08_curr_defer_next_create(); ui_page_08_curr_create(NULL);
    ui_page_00_boot_anim_create(lv_layer_top()); ui_page_00_boot_anim_set_startup_ready(true);
    for (unsigned i=0;i<219;++i) frame(10);
    assert(ui_page_00_boot_anim_is_active()); frame(LV_DISP_DEF_REFR_PERIOD);
    assert(!ui_page_00_boot_anim_is_active() && !view.covered); reset();
    fail_art=true; ui_page_08_curr_defer_next_create(); ui_page_08_curr_create(NULL);
    ui_page_00_boot_anim_create(lv_layer_top()); ui_page_00_boot_anim_set_startup_ready(true);
    assert(find_text(lv_layer_top(),"Initialization"));
    for (unsigned i=0;i<225;++i) frame(10);
    assert(!ui_page_00_boot_anim_is_active() && !view.covered && !view.prepare_failed); reset();
    ui_page_08_curr_defer_next_create(); ui_page_08_curr_create(NULL); ui_page_00_boot_anim_create(lv_layer_top());
    ui_page_00_boot_anim_set_startup_error(false); frame(5000);
    assert(ui_page_00_boot_anim_is_active() && find_text(lv_layer_top(),"STARTUP ERROR"));
    ui_page_00_boot_anim_set_startup_error(true);
    lv_obj_t *button_text=find_text(lv_layer_top(),"DIAGNOSTICS"); assert(button_text);
    lv_event_send(lv_obj_get_parent(button_text),LV_EVENT_CLICKED,NULL);
    assert(current==UI_PAGE_SENSOR && !ui_page_00_boot_anim_is_active()); reset();

    fail_art=true; ui_page_08_curr_defer_next_create(); ui_page_08_curr_create(NULL);
    ui_page_00_boot_anim_create(lv_layer_top()); ui_page_00_boot_anim_set_startup_error(true); frame(10);
    lv_obj_t *old_fallback=find_text(lv_layer_top(),"Initialization");
    assert(old_fallback && lv_obj_has_flag(old_fallback,LV_OBJ_FLAG_HIDDEN));
    assert(find_text(lv_layer_top(),"STARTUP ERROR") && find_text(lv_layer_top(),"DIAGNOSTICS"));
    dump("intro-error-fallback"); reset();

    /* All packaged title locales are exercised through real catalog/art/blob. */
    for (unsigned i=0;i<10;++i) {
        language=i; ++language_generation; ui_page_08_curr_create(NULL); complete_all(); frame(2400);
        ready=boot_customer_art_image(BOOT_CUSTOMER_ART_READY_GRAY);
        assert(ready && view.ready_width==ready->header.w && view.ready_height==ready->header.h);
        const lv_img_dsc_t *hand=boot_customer_art_image(BOOT_CUSTOMER_ART_HAND_1);
        int ink_right=0;
        for (unsigned y=0;y<hand->header.h;++y) for (unsigned x=0;x<hand->header.w;++x)
            if (hand->data[(y*hand->header.w+x)*4+3] && (int)x>ink_right) ink_right=x;
        lv_coord_t hx=0,hy=0,rx=0,ry=0;
        assert(boot_customer_art_position(BOOT_CUSTOMER_ART_HAND_1,&hx,&hy));
        assert(boot_customer_art_position(BOOT_CUSTOMER_ART_READY_GRAY,&rx,&ry));
        assert(lv_obj_get_x(view.ready_active_clip)==rx && lv_obj_get_y(view.ready_active_clip)==ry);
        assert(rx>hx+ink_right && rx+(int)view.ready_width<=1280 && ry+(int)view.ready_height<=400);
        char name[64]; snprintf(name,sizeof(name),"ready-locale-%s",boot_welcome_tags[i]); dump(name); reset();
    }
    assert(input_holds==0 && timers()==baseline_timers);
    puts("PASS real LVGL customer boot: authoritative five results, no fake success, real failures/timeouts, fast completion, 2350ms Ready bound, no extra handoff hold, covers, allocation/art/timer fallbacks, 10 catalog locales, intro 2200ms/delay/error/diagnostics, timer/art cleanup, no frame-time malloc or file I/O.");
    return 0;
}
