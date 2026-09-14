#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int lv_coord_t;
typedef int lv_event_code_t;
typedef struct { int x, y; } lv_point_t;
typedef struct { int x1, y1, x2, y2; } lv_area_t;
typedef struct {
    bool valid, drag;
    int x, y, width, height, opacity;
    unsigned flags, events, children;
} lv_obj_t;
typedef struct { lv_point_t point; } lv_indev_t;
typedef struct { lv_event_code_t code; } lv_event_t;
typedef struct { bool valid; } lv_timer_t;
typedef struct { int unused; } lv_anim_t;
typedef void (*lv_anim_ready_cb_t)(lv_anim_t *);
enum { LV_EVENT_PRESSED = 1, LV_EVENT_PRESSING, LV_EVENT_RELEASED,
       LV_EVENT_PRESS_LOST, LV_EVENT_CLICKED };
enum { LV_OBJ_FLAG_CLICKABLE = 1, LV_OBJ_FLAG_SCROLLABLE = 2, LV_OBJ_FLAG_HIDDEN = 4, LV_OPA_TRANSP = 0 };
enum { UI_PAGE_MAIN, UI_PAGE_INNOVATION_CENTER, UI_PAGE_LIST, UI_PAGE_OTHER };
#define LV_ABS(x) ((x) < 0 ? -(x) : (x))
#include "main_top_gesture_state.h"

static innovation_handle_gesture_t g_handle_gesture;
static lv_obj_t *g_handle_touch;
static void (*g_handle_tap_handler)(const lv_point_t *point);
static lv_timer_t *g_preview_preload_timer;
static struct { lv_obj_t *root; } g_page;
static struct { lv_obj_t *image; } g_transition_snapshot;
static bool g_page_transitioning, g_transition_snapshot_valid;
static bool s_multi_layout, main_visible, manager_transitioning, preview_available;
static lv_obj_t *s_multi_card, *s_detail_card, *menu_object;
static lv_obj_t *s_detail_btn_a, *s_detail_btn_b, *s_detail_btn_c;
static lv_obj_t tabs[3];
static lv_obj_t root, menu, detail, multi, surface, innovation_root, objects[8];
static lv_timer_t timer;
static lv_indev_t pointer, *active_indev;
static unsigned created, deleted, timer_created, timer_deleted, innovation_destroyed;
static unsigned menu_clicks, list_pushes, preview_attempts, animations, opens, cancels;
static unsigned tab_clicks[3];
static uint32_t tick;
static int current_page, animation_destination;
static lv_anim_ready_cb_t animation_ready;
static void innovation_handle_event_cb(lv_event_t *);
static void page_01_top_strip_tap(const lv_point_t *);
void page_32_innovation_handle_detach(void);
static bool lv_obj_is_valid(lv_obj_t *object) { return object && object->valid; }
static bool lv_obj_is_visible(lv_obj_t *object) { return lv_obj_is_valid(object) && !(object->flags & LV_OBJ_FLAG_HIDDEN); }
static lv_obj_t *lv_obj_create(lv_obj_t *parent) {
    assert(lv_obj_is_valid(parent) && created < 8);
    lv_obj_t *object = &objects[created++];
    *object = (lv_obj_t){ .valid = true, .flags = LV_OBJ_FLAG_SCROLLABLE };
    parent->children++;
    return object;
}
static void lv_obj_remove_style_all(lv_obj_t *object) { assert(lv_obj_is_valid(object)); }
static void lv_obj_set_pos(lv_obj_t *object, int x, int y) { object->x = x; object->y = y; }
static void lv_obj_set_size(lv_obj_t *object, int w, int h) { object->width = w; object->height = h; }
static void lv_obj_set_style_bg_opa(lv_obj_t *object, int value, int selector) { assert(selector == 0); object->opacity = value; }
static void lv_obj_add_flag(lv_obj_t *object, unsigned flag) { object->flags |= flag; }
static void lv_obj_clear_flag(lv_obj_t *object, unsigned flag) { object->flags &= ~flag; }
static void lv_port_indev_set_drag_obj(lv_obj_t *object, bool enabled) { object->drag = enabled; }
static void lv_obj_add_event_cb(lv_obj_t *object, void (*callback)(lv_event_t *), int code, void *data) {
    assert(callback == innovation_handle_event_cb && data == NULL);
    assert(code >= LV_EVENT_PRESSED && code <= LV_EVENT_PRESS_LOST);
    object->events |= 1U << code;
}
static void lv_obj_move_foreground(lv_obj_t *object) { assert(lv_obj_is_valid(object)); }
static void lv_obj_del(lv_obj_t *object) { assert(lv_obj_is_valid(object)); object->valid = false; deleted++; }
static void innovation_preview_preload_timer_cb(lv_timer_t *value) { (void)value; }
static lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *), uint32_t delay, void *data) {
    assert(callback == innovation_preview_preload_timer_cb && delay == 120 && data == NULL);
    timer.valid = true; timer_created++; return &timer;
}
static void lv_timer_del(lv_timer_t *value) { assert(value == &timer && value->valid); value->valid = false; timer_deleted++; }
static int ui_manager_get_current_page(void) { return current_page; }
static void ui_page_32_innovation_destroy(void) { innovation_destroyed++; g_page.root = NULL; }
static lv_event_code_t lv_event_get_code(lv_event_t *event) { return event->code; }
static lv_indev_t *lv_indev_get_act(void) { return active_indev; }
static void lv_indev_get_point(lv_indev_t *indev, lv_point_t *point) { assert(indev == &pointer); *point = indev->point; }
static uint32_t lv_tick_get(void) { return tick; }
static uint32_t lv_tick_elaps(uint32_t start) { return tick - start; }
static bool perf_profile_is_enabled(void) { return false; }
static void uart_debug_printf(const char *format, ...) { (void)format; }
static void innovation_set_y_if_changed(lv_obj_t *object, lv_coord_t y) { assert(object == &surface); object->y = y; }
static bool innovation_handle_preview_begin(void) {
    preview_attempts++;
    if (!preview_available || g_page_transitioning || g_handle_gesture.preview_active) return false;
    g_handle_gesture.preview_active = true; return true;
}
static void innovation_transition_open_ready(lv_anim_t *animation) {
    (void)animation; opens++; g_handle_gesture.preview_active = false; g_page_transitioning = false;
}
static void innovation_transition_cancel_ready(lv_anim_t *animation) {
    (void)animation; cancels++; g_handle_gesture.preview_active = false; g_page_transitioning = false;
}
static void innovation_transition_cancel_async(void *data) { assert(data == NULL); innovation_transition_cancel_ready(NULL); }
static bool innovation_transition_animate(int y, uint32_t duration, lv_anim_ready_cb_t ready) {
    assert(!animation_ready);
    assert((y == 0 && duration == 180 && ready == innovation_transition_open_ready) ||
           (y == -400 && duration == 150 && ready == innovation_transition_cancel_ready));
    if (!g_transition_snapshot_valid) return false;
    animations++; animation_destination = y; animation_ready = ready; return true;
}
static bool page_01_main_is_visible(void) { return main_visible; }
static bool ui_manager_is_transitioning(void) { return manager_transitioning; }
static lv_obj_t *page_01_main_find_obj(const char *name) { assert(strcmp(name, "menu_btn") == 0); return menu_object; }
static void lv_obj_get_coords(lv_obj_t *object, lv_area_t *area) {
    assert(lv_obj_is_valid(object));
    *area = (lv_area_t){ object->x, object->y, object->x + object->width - 1, object->y + object->height - 1 };
}
static void lv_event_send(lv_obj_t *object, int event, void *data) {
    assert(event == LV_EVENT_CLICKED && data == NULL);
    if(object == &menu) { menu_clicks++; return; }
    for(unsigned i=0;i<3;++i) if(object==&tabs[i]) { tab_clicks[i]++; return; }
    assert(false);
}
static void ui_manager_push_page(int page) { assert(page == UI_PAGE_LIST); list_pushes++; }
#include "main_top_gesture_under_test.h"

static void fixture(void)
{
    memset(&g_handle_gesture, 0, sizeof(g_handle_gesture));
    g_handle_touch = NULL; g_handle_tap_handler = NULL; g_preview_preload_timer = NULL;
    root = (lv_obj_t){ .valid = true, .width = 1280, .height = 400 };
    menu = (lv_obj_t){ .valid = true, .x = 1168, .y = 12, .width = 96, .height = 98 };
    detail = (lv_obj_t){ .valid = true, .x = 602, .y = 12, .width = 554, .height = 320 };
    multi = (lv_obj_t){ .valid = true, .x = 108, .y = 12, .width = 1048, .height = 320 };
    surface = (lv_obj_t){ .valid = true, .y = -400 };
    innovation_root = root; g_page.root = NULL; g_transition_snapshot.image = &surface;
    s_detail_card = &detail; s_multi_card = &multi; menu_object = &menu;
    for(unsigned i=0;i<3;++i) tabs[i]=(lv_obj_t){.valid=true,.x=620+(int)i*175,.y=24,.width=168,.height=44};
    s_detail_btn_a=&tabs[0];s_detail_btn_b=&tabs[1];s_detail_btn_c=&tabs[2];
    memset(tab_clicks,0,sizeof(tab_clicks));
    s_multi_layout = manager_transitioning = g_page_transitioning = false;
    g_transition_snapshot_valid = main_visible = preview_available = true;
    active_indev = &pointer; pointer.point = (lv_point_t){1220, 20};
    created = deleted = timer_created = timer_deleted = innovation_destroyed = 0;
    menu_clicks = list_pushes = preview_attempts = animations = opens = cancels = 0;
    tick = 0; current_page = UI_PAGE_MAIN; animation_ready = NULL;
    page_32_innovation_handle_attach(&root);
    page_32_innovation_handle_set_tap_handler(page_01_top_strip_tap);
}
static void event(int code, int x, int y, uint32_t when)
{
    pointer.point = (lv_point_t){x, y}; tick = when;
    lv_event_t value = {code}; innovation_handle_event_cb(&value);
}
static void tap(int x, int y)
{ event(LV_EVENT_PRESSED, x, y, tick + 1); event(LV_EVENT_RELEASED, x, y, tick + 100); }
static void settle(void)
{
    if (animation_ready) { lv_anim_ready_cb_t callback = animation_ready; animation_ready = NULL; callback(NULL); }
}
static void no_route(void) { assert(menu_clicks == 0 && list_pushes == 0); }
static void test_invisible_attach_and_lifetime(void)
{
    fixture();
    assert(INNOVATION_PREVIEW_ARM_DY == 10);
    assert(g_handle_touch && g_handle_touch->x == 1058 && g_handle_touch->y == 0);
    assert(g_handle_touch->width == 212 && g_handle_touch->height == 44);
    assert(g_handle_touch->opacity == LV_OPA_TRANSP && g_handle_touch->drag);
    assert(g_handle_touch->flags == LV_OBJ_FLAG_CLICKABLE);
    assert(g_handle_touch->events == ((1U << 1) | (1U << 2) | (1U << 3) | (1U << 4)));
    /* Only the invisible touch object is created: no text or chevron child. */
    assert(created == 1 && root.children == 1 && g_handle_touch->children == 0);
    assert(timer_created == 1 && timer_deleted == 0);
    event(LV_EVENT_PRESSED, 1220, 20, 0); event(LV_EVENT_PRESSING, 1240, 20, 100);
    assert(g_handle_gesture.tap_moved);
    g_page.root = &innovation_root;
    page_32_innovation_handle_detach();
    assert(!g_handle_touch && !g_handle_tap_handler && !g_preview_preload_timer);
    assert(!g_handle_gesture.pressed && !g_handle_gesture.tap_moved && !g_handle_gesture.preview_active);
    assert(deleted == 1 && timer_deleted == 1 && innovation_destroyed == 1);
    event(LV_EVENT_RELEASED, 1220, 20, 120); no_route();
    page_32_innovation_handle_detach(); assert(deleted == 1 && timer_deleted == 1);
    page_32_innovation_handle_attach(NULL); assert(created == 1 && !g_handle_tap_handler);
    page_32_innovation_handle_attach(&root); assert(created == 2 && !g_handle_tap_handler);
    page_32_innovation_handle_set_tap_handler(page_01_top_strip_tap); tap(1220, 20);
    assert(menu_clicks == 1);
    page_32_innovation_handle_set_tap_handler(NULL); tap(1220, 20); assert(menu_clicks == 1);
    current_page = UI_PAGE_INNOVATION_CENTER; g_page.root = &innovation_root;
    page_32_innovation_handle_detach(); assert(innovation_destroyed == 1);
}
static void test_tap_routing(void)
{
    fixture(); tap(1220, 32); assert(menu_clicks == 1 && list_pushes == 0);
    event(LV_EVENT_RELEASED, 1220, 32, 200); assert(menu_clicks == 1);
    tap(1168, 12); tap(1263, 43); assert(menu_clicks == 3);
    tap(1120, 32); assert(list_pushes == 0 && tab_clicks[2] == 1);
    tap(1138, 32); assert(list_pushes == 0 && tab_clicks[2] == 1);
    s_multi_layout = true; s_detail_card = NULL;
    for(unsigned i=0;i<3;++i) lv_obj_add_flag(&tabs[i], LV_OBJ_FLAG_HIDDEN);
    tap(1120, 32); assert(list_pushes == 1 && tab_clicks[2] == 1);
    s_multi_card = NULL; tap(1120, 32); assert(list_pushes == 1);
    fixture(); tap(1160, 32); tap(1269, 32); tap(1220, 5); no_route();
    manager_transitioning = true; tap(1220, 32); no_route();
    manager_transitioning = false; main_visible = false; tap(1120, 32); no_route();
    main_visible = true; menu_object = NULL; tap(1220, 32); no_route();
    page_01_top_strip_tap(NULL); no_route();
    assert(!preview_attempts && !animations);
}
static void test_tap_slop_and_movement_memory(void)
{
    fixture(); event(LV_EVENT_PRESSED, 1220, 20, 0); event(LV_EVENT_PRESSING, 1229, 29, 100);
    assert(!g_handle_gesture.tap_moved && !g_handle_gesture.preview_active && !preview_attempts);
    event(LV_EVENT_RELEASED, 1229, 29, 200); assert(menu_clicks == 1 && !animations);
    for (int dx = -10; dx <= 10; dx += 20) {
        fixture(); event(LV_EVENT_PRESSED, 1220, 20, 0); event(LV_EVENT_PRESSING, 1220 + dx, 20, 50);
        assert(g_handle_gesture.tap_moved && !preview_attempts);
        event(LV_EVENT_PRESSING, 1220, 20, 100); event(LV_EVENT_RELEASED, 1220, 20, 120); no_route();
        tap(1220, 20); assert(menu_clicks == 1 && !g_handle_gesture.tap_moved);
    }
    fixture(); event(LV_EVENT_PRESSED, 1220, 20, 0); event(LV_EVENT_RELEASED, 1230, 20, 100);
    no_route(); assert(!preview_attempts && !g_handle_gesture.pressed);
    fixture(); event(LV_EVENT_PRESSED, 1220, 20, 0); event(LV_EVENT_PRESSING, 1220, 10, 100);
    event(LV_EVENT_RELEASED, 1220, 20, 200); no_route(); assert(!preview_attempts);
    fixture(); event(LV_EVENT_PRESSED, 1220, 20, 0); event(LV_EVENT_PRESSING, 1220, 30, 100);
    assert(g_handle_gesture.tap_moved && g_handle_gesture.preview_active && preview_attempts == 1);
    event(LV_EVENT_PRESSING, 1220, 20, 400); event(LV_EVENT_RELEASED, 1220, 20, 500);
    assert(animations == 1 && animation_destination == -400); settle();
    no_route(); assert(cancels == 1 && !opens);
}
static void test_original_release_thresholds(void)
{
    static const struct { int dy; uint32_t elapsed; bool open; } cases[] = {
        {44, 350, false}, {45, 350, true}, {45, 351, false},
        {89, 2000, false}, {90, 2000, true}, {400, 4000, true}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fixture(); event(LV_EVENT_PRESSED, 1220, 20, 0);
        event(LV_EVENT_PRESSING, 1220, 20 + cases[i].dy, cases[i].elapsed);
        assert(!animations && !opens && g_handle_gesture.pressed);
        event(LV_EVENT_RELEASED, 1220, 20 + cases[i].dy, cases[i].elapsed);
        assert(animations == 1 && animation_destination == (cases[i].open ? 0 : -400));
        assert(g_handle_gesture.opened == cases[i].open); settle(); no_route();
        assert(opens == (unsigned)cases[i].open && cancels == (unsigned)!cases[i].open);
    }
    fixture(); g_transition_snapshot_valid = false;
    event(LV_EVENT_PRESSED, 1220, 20, 0); event(LV_EVENT_PRESSING, 1220, 120, 2000);
    assert(!opens); event(LV_EVENT_RELEASED, 1220, 120, 2000);
    no_route(); assert(opens == 1 && animations == 0);
}
static void detach_on_tap(const lv_point_t *point)
{ assert(point && !g_handle_gesture.pressed); page_32_innovation_handle_detach(); }
static void test_loss_failure_and_callback_cleanup(void)
{
    for (unsigned preview = 0; preview < 2; preview++) {
        fixture(); event(LV_EVENT_PRESSED, 1220, 20, 0);
        if (preview) event(LV_EVENT_PRESSING, 1220, 170, 100);
        active_indev = NULL; event(LV_EVENT_PRESS_LOST, 1220, 170, 150);
        active_indev = &pointer; event(LV_EVENT_RELEASED, 1220, 20, 200);
        no_route(); assert(!g_handle_gesture.pressed && !g_handle_gesture.preview_active && !opens);
        assert(cancels == preview); tap(1220, 20); assert(menu_clicks == 1);
    }
    fixture(); preview_available = false;
    event(LV_EVENT_PRESSED, 1220, 20, 0); event(LV_EVENT_PRESSING, 1220, 30, 100);
    event(LV_EVENT_RELEASED, 1220, 20, 200); no_route();
    assert(preview_attempts == 1 && !g_handle_gesture.pressed && !animations);
    fixture(); g_page_transitioning = true; tap(1220, 20); no_route(); assert(!g_handle_gesture.pressed);
    fixture(); active_indev = NULL; tap(1220, 20); no_route(); assert(!g_handle_gesture.pressed);
    fixture(); page_32_innovation_handle_set_tap_handler(detach_on_tap); tap(1220, 20);
    assert(!g_handle_touch && !g_handle_tap_handler && deleted == 1 && timer_deleted == 1); no_route();
}
int main(void)
{
    test_invisible_attach_and_lifetime(); test_tap_routing(); test_tap_slop_and_movement_memory();
    test_original_release_thresholds(); test_loss_failure_and_callback_cleanup();
    puts("PASS: invisible 1058,0,212,44 strip, raised ABC/MENU/detail/MULTI tap routing and owner cleanup");
    puts("PASS: 9px tap/10px arm, movement memory/reset, drag reversal, PRESS_LOST without indev, failed preview and callback-side detach");
    puts("PASS: original 45px/350ms flick, 90px held-release, 180ms settle/150ms cancel, stationary fallback and no drag-to-click forwarding");
    return 0;
}
