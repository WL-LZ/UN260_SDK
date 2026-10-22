#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ui_display_mode.h"

typedef int lv_coord_t;
typedef int lv_event_code_t;
typedef int fault_source_t;
typedef struct { int x, y; } lv_point_t;
typedef struct { bool valid, disabled, hidden; int y; } lv_obj_t;
typedef struct { lv_point_t point; } lv_indev_t;
typedef struct { lv_event_code_t code; void *user_data; } lv_event_t;
typedef struct lv_anim_t { void *var; int from, to; void (*ready)(struct lv_anim_t *); } lv_anim_t;
typedef struct { void (*callback)(void *); } ui_deferred_action_t;
typedef struct { void *snapshot; lv_obj_t *source, *image; } lv_dma_static_surface_t;
enum { LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST,
       LV_EVENT_CANCEL, LV_EVENT_SCROLL_BEGIN, LV_EVENT_LONG_PRESSED,
       LV_EVENT_CLICKED, LV_EVENT_DELETE };
enum { LV_STATE_DISABLED, LV_OBJ_FLAG_HIDDEN, UI_PAGE_MAIN, UI_PAGE_LIST, UI_PAGE_MENU,
       UI_PAGE_SETTING, UI_PAGE_SET_PASSAGE, COUNTING_SESSION_ACTIVE };

static bool s_panel_layout, s_display_switching, s_display_rebuilding, s_counting_locked;
static ui_display_mode_t s_display_target;
static ui_deferred_action_t s_display_action;
static lv_dma_static_surface_t s_display_surface;
static lv_obj_t roots[40], image, screen, button;
static lv_obj_t *main_page;
static lv_indev_t indev;
static unsigned next_root, cycles, captures, caches, pin_destroys, releases, creates, destroys;
static int current_page = UI_PAGE_MAIN, frame_depth;
static bool visible = true, busy, held_input, navigation_blocked, aging, pin, quick, editing, fault, pending_fault;
static bool applying, number_entry, choice, settings_overlay;
static bool fail_deferred, fail_capture, fail_animation, fail_panel;
static bool registered[9];
static uint8_t fingers = 1;
static lv_anim_t running_animation;
static bool animation_active;
static bool g_no_note_motor_active, pending_start;
static struct { bool start_confirmed; int phase; } g_counting_session;

static bool page_01_main_toggle_display_mode(void);
static void page_01_display_transition_stop(void);
static void page_01_mode_contact_event(lv_event_t *);
static void *contact_user;
static void page_01_mode_cycle(void) { ++cycles; }
static void *lv_event_get_user_data(lv_event_t *event) { return event->user_data; }
static int lv_event_get_code(lv_event_t *event) { return event->code; }
static lv_indev_t *lv_indev_get_act(void) { return &indev; }
static void lv_indev_get_point(lv_indev_t *i, lv_point_t *point) { *point = i->point; }
static uint8_t lv_port_indev_touch_count(void) { return fingers; }
static bool lv_obj_has_state(lv_obj_t *object, int state) { (void)state; return object->disabled; }
static void lv_event_send(lv_obj_t *object, int code, void *param)
{ (void)object; (void)param; lv_event_t event = {code, contact_user}; page_01_mode_contact_event(&event); }
static void lv_obj_add_event_cb(lv_obj_t *object, void (*cb)(lv_event_t *), int code, void *user)
{ (void)object; (void)cb; registered[code] = true; contact_user = user; }
static bool lv_obj_is_valid(void *object) { return object && ((lv_obj_t *)object)->valid; }
static void lv_obj_set_y(void *object, int y) { ((lv_obj_t *)object)->y = y; }
static void ui_deferred_action_cancel(ui_deferred_action_t *action) { action->callback = NULL; }
static bool ui_deferred_action_schedule(ui_deferred_action_t *action, void (*cb)(void *), void *user)
{ (void)user; if (fail_deferred) return false; action->callback = cb; return true; }
static void lv_anim_del(void *object, void (*exec)(void *,int32_t))
{ (void)object; (void)exec; animation_active = false; }
static void lv_dma_static_surface_release(lv_dma_static_surface_t *surface)
{ if (surface->snapshot) ++releases; if (surface->image) surface->image->valid = false; *surface = (lv_dma_static_surface_t){0}; }
static void ui_manager_hold_transition_input(int owner, bool hold)
{ assert(owner == UI_PAGE_MAIN); held_input = hold; }
static int ui_manager_get_current_page(void) { return current_page; }
static bool page_01_main_is_visible(void) { return visible && main_page && main_page->valid; }
static bool page_01_main_is_created(void) { return main_page && main_page->valid; }
static bool ui_manager_is_transitioning(void) { return held_input; }
static bool ui_manager_navigation_is_blocked(void) { return navigation_blocked; }
static bool app_command_runtime_count_start_busy(void);
static bool machine_state_aging_running(void) { return aging; }
static bool ui_page_05_set_password_is_open(void) { return pin; }
static void ui_page_05_set_password_destroy(void) { assert(!pin); ++pin_destroys; }
static bool workspace_service_applying(void) { return applying; }
static bool panel_entry_is_open(void) { return number_entry; }
static bool panel_choice_is_open(void) { return choice; }
static bool settings_detail_overlay_is_open(void) { return settings_overlay; }
static bool page_01_main_quick_is_open(void) { return quick; }
static bool page_01_main_layout_is_editing(void) { return editing; }
static bool fault_popup_is_showing(void) { return fault; }
static bool fault_popup_get_pending_fault(fault_source_t *a,uint8_t *b,uint8_t *c)
{ (void)a; (void)b; (void)c; return pending_fault; }
static void lv_port_indev_capture_pointer(lv_indev_t *input) { (void)input; ++captures; }
static void ui_frame_commit_begin_sync(void) { ++frame_depth; }
static void ui_frame_commit_end_sync(void) { --frame_depth; }
static bool lv_dma_transition_surface_capture(lv_dma_static_surface_t *surface,lv_obj_t *root,const char *name)
{ (void)name; if (fail_capture) return false; image=(lv_obj_t){.valid=true}; *surface=(lv_dma_static_surface_t){&image,root,&image}; root->hidden=true; return true; }
static lv_obj_t *lv_scr_act(void) { return &screen; }
static void ui_main_destroy(void)
{ if (!s_display_rebuilding) page_01_display_transition_stop(); if (main_page) main_page->valid=false; main_page=NULL; s_panel_layout=false; ++destroys; }
static void ui_main_create(lv_obj_t *parent)
{ (void)parent; assert(next_root<40); main_page=&roots[next_root++]; *main_page=(lv_obj_t){.valid=true}; s_panel_layout=ui_display_mode_is_panel(); if(fail_panel&&s_panel_layout){s_panel_layout=false;ui_display_mode_set(UI_DISPLAY_MODE_FULLSCREEN);} ++creates; }
static bool ui_manager_invalidate_page_cache(int page)
{ assert(page==UI_PAGE_LIST||page==UI_PAGE_MENU||page==UI_PAGE_SETTING); ++caches; return true; }
static void lv_obj_update_layout(lv_obj_t *o) { (void)o; }
static void lv_obj_move_foreground(lv_obj_t *o) { (void)o; }
static void lv_obj_clear_flag(lv_obj_t *o,int flag) { (void)flag; o->hidden=false; }
static void lv_anim_init(lv_anim_t *a) { memset(a,0,sizeof(*a)); }
static void lv_anim_set_var(lv_anim_t *a,void *v) { a->var=v; }
static void lv_anim_set_values(lv_anim_t *a,int from,int to) { a->from=from;a->to=to; }
static void lv_anim_set_time(lv_anim_t *a,unsigned time) { (void)a; assert(time<=250); }
static int lv_anim_path_ease_out;
static void lv_anim_set_path_cb(lv_anim_t *a,int path) { (void)a;(void)path; }
static void lv_anim_set_exec_cb(lv_anim_t *a,void (*cb)(void *,int32_t)) { (void)a;(void)cb; }
static void lv_anim_set_ready_cb(lv_anim_t *a,void (*cb)(lv_anim_t *)) { a->ready=cb; }
static lv_anim_t *lv_anim_start(lv_anim_t *a)
{ if(fail_animation)return NULL;running_animation=*a;animation_active=true;return &running_animation; }
static bool counting_action_start_pending(void) { return pending_start || busy; }

#include "main_display_mode_under_test.h"

static void emit(int code) { lv_event_t event={code,contact_user}; page_01_mode_contact_event(&event); }
static void tap(void) { emit(LV_EVENT_PRESSED);emit(LV_EVENT_RELEASED);emit(LV_EVENT_CLICKED); }
static void commit(void)
{ assert(s_display_action.callback); void (*cb)(void *)=s_display_action.callback;s_display_action.callback=NULL;cb(NULL);assert(frame_depth==0); }
static void settle(void)
{ assert(animation_active);running_animation.ready(&running_animation);assert(!held_input&&!s_display_switching&&!animation_active&&!s_display_surface.snapshot); }
int main(void)
{
    assert(ui_display_mode_get()==UI_DISPLAY_MODE_FULLSCREEN);
    assert(!ui_display_mode_set((ui_display_mode_t)8));
    ui_main_create(&screen);button.valid=true;page_01_mode_button_bind(&button);
    assert(registered[LV_EVENT_CLICKED]&&registered[LV_EVENT_LONG_PRESSED]&&registered[LV_EVENT_RELEASED]);
    tap();assert(cycles==1&&!held_input);
    emit(LV_EVENT_PRESSED);emit(LV_EVENT_LONG_PRESSED);
    assert(held_input&&s_display_switching&&ui_display_mode_get()==UI_DISPLAY_MODE_FULLSCREEN);
    emit(LV_EVENT_LONG_PRESSED);emit(LV_EVENT_RELEASED);emit(LV_EVENT_CLICKED);assert(cycles==1&&captures==1);
    commit();assert(ui_display_mode_is_panel()&&s_panel_layout&&animation_active&&held_input&&caches==3&&pin_destroys==1);
    assert(running_animation.to==-400&&!page_01_main_toggle_display_mode());settle();assert(releases==1);
    assert(page_01_main_toggle_display_mode());commit();assert(!ui_display_mode_is_panel()&&running_animation.to==400);settle();

    fail_deferred=true;emit(LV_EVENT_PRESSED);emit(LV_EVENT_LONG_PRESSED);emit(LV_EVENT_RELEASED);emit(LV_EVENT_CLICKED);
    assert(cycles==1&&!held_input&&!s_display_switching);fail_deferred=false;
    emit(LV_EVENT_PRESSED);indev.point.x=11;emit(LV_EVENT_PRESSING);indev.point.x=0;emit(LV_EVENT_LONG_PRESSED);emit(LV_EVENT_RELEASED);emit(LV_EVENT_CLICKED);assert(cycles==1&&!held_input);
    emit(LV_EVENT_PRESSED);indev.point.x=11;emit(LV_EVENT_RELEASED);emit(LV_EVENT_CLICKED);assert(cycles==1);indev.point.x=0;
    fingers=2;tap();assert(cycles==1);fingers=1;
    button.disabled=true;tap();assert(cycles==1);button.disabled=false;
    emit(LV_EVENT_PRESSED);emit(LV_EVENT_PRESS_LOST);emit(LV_EVENT_CLICKED);assert(cycles==1);
    emit(LV_EVENT_PRESSED);emit(LV_EVENT_SCROLL_BEGIN);emit(LV_EVENT_LONG_PRESSED);emit(LV_EVENT_CLICKED);assert(cycles==1);
    tap();assert(cycles==2);

    bool *guards[]={&busy,&s_counting_locked,&navigation_blocked,&aging,&pin,&quick,&editing,&fault,&pending_fault,&applying,&number_entry,&choice,&settings_overlay};
    for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);++i){*guards[i]=true;assert(!page_01_main_toggle_display_mode());*guards[i]=false;}
    current_page=UI_PAGE_LIST;assert(!page_01_main_toggle_display_mode());current_page=UI_PAGE_MAIN;
    assert(page_01_main_toggle_display_mode());busy=true;commit();assert(!held_input&&!s_display_switching&&!ui_display_mode_is_panel());busy=false;
    assert(page_01_main_toggle_display_mode());pending_fault=true;commit();assert(!held_input&&!s_display_switching&&!ui_display_mode_is_panel());pending_fault=false;
    fail_capture=true;assert(page_01_main_toggle_display_mode());commit();assert(ui_display_mode_is_panel()&&!held_input&&!animation_active);fail_capture=false;
    fail_animation=true;assert(page_01_main_toggle_display_mode());commit();assert(!ui_display_mode_is_panel()&&!held_input&&!s_display_surface.snapshot);fail_animation=false;
    fail_panel=true;assert(page_01_main_toggle_display_mode());commit();assert(!ui_display_mode_is_panel()&&!held_input&&!animation_active);fail_panel=false;
    assert(page_01_main_toggle_display_mode());page_01_display_transition_stop();assert(!s_display_action.callback&&!held_input&&!s_display_switching);
    assert(page_01_main_toggle_display_mode());commit();ui_main_destroy();assert(!held_input&&!s_display_surface.snapshot&&!animation_active);

    pending_start=true;assert(app_command_runtime_count_start_busy()&&!app_command_runtime_counting_confirmed());pending_start=false;
    g_no_note_motor_active=true;assert(app_command_runtime_count_start_busy()&&!app_command_runtime_counting_confirmed());g_no_note_motor_active=false;
    g_counting_session.start_confirmed=true;assert(app_command_runtime_count_start_busy()&&app_command_runtime_counting_confirmed());
    g_counting_session.start_confirmed=false;assert(!app_command_runtime_counting_confirmed());
    emit(LV_EVENT_DELETE);assert(!s_mode_contacts[0].button);
    printf("PASS: contact, double-action suppression, transition cleanup/fallback, display preference, confirmed-vs-pending (%u creates/%u destroys)\n",creates,destroys);
}
