#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/currency/currency_state.h"
#include "un260/lv_components/ui_scroll_physics.h"
#include "un260/lv_system/ui_state_store.h"

#define PAGE07_CURR_MAX_ITEMS MAX_CURRENCIES
#define LV_OBJ_FLAG_HIDDEN 1U
#define LV_ANIM_OFF 0
#define CURR_TEXT_SEL 0x0073FF
#include "currency_resume_types.inc"

typedef struct { unsigned flags; int index; } lv_obj_t;
typedef struct { bool paused; unsigned period, ready; } lv_timer_t;
typedef struct { ui_scroll_physics_t motion; bool enabled; } carousel_t;
static struct {
    page07_curr_model_state_t model;
    carousel_t carousel;
    struct { lv_obj_t *grid_scroll; } objects;
    struct { int abs_idx; lv_obj_t *item, *selected_mark, *img, *name; }
        grid_items[PAGE07_CURR_MAX_ITEMS];
} g_page07_curr;
static lv_obj_t page_object, grid_object, grid_nodes[PAGE07_CURR_MAX_ITEMS];
static lv_obj_t *curr_page;
static currency_state_snapshot_t g_curr_page_snapshot;
static bool g_curr_page_snapshot_valid;
static char g_curr_page_selected_code[4];
static lv_timer_t prewarm_timer;
static lv_timer_t *g_curr_snapshot_prewarm_timer = &prewarm_timer;
static int g_curr_grid_styled_abs_idx = -1;
static ui_state_page07_t saved;
static unsigned rebuilds, saves, snaps, foregrounds;
static int left_index, revealed_grid_index;
static bool profile;

void page07_curr_model_save(void);
int page07_curr_model_find_abs_idx(const char *code);
int page07_curr_model_find_visible_pos(int index);
bool page07_curr_model_is_fixed(int index);
void page07_curr_model_load(void);
void page07_curr_model_refresh_visible(void);

void ui_state_page07_get(ui_state_page07_t *state) { *state = saved; }
void ui_state_save_page07(const ui_state_page07_t *state) { saved = *state; saves++; }
static bool lv_obj_is_valid(lv_obj_t *obj) { return obj != NULL; }
static void lv_obj_clear_flag(lv_obj_t *obj, unsigned flag) { assert(obj); obj->flags &= ~flag; }
static void lv_obj_add_flag(lv_obj_t *obj, unsigned flag) { assert(obj); obj->flags |= flag; }
static void lv_obj_move_foreground(lv_obj_t *obj) { assert(obj); foregrounds++; }
static void lv_obj_update_layout(lv_obj_t *obj) { assert(obj == &grid_object); }
static void lv_obj_scroll_to_view(lv_obj_t *obj, int animate)
{
    assert(obj && animate == LV_ANIM_OFF);
    revealed_grid_index = obj->index;
}
static void lv_obj_set_style_text_color(lv_obj_t *obj, unsigned color, int selector)
{ (void)color; (void)selector; assert(obj); }
static unsigned lv_color_hex(unsigned value) { return value; }
static void page07_curr_view_set_image_selected_style(lv_obj_t *obj) { assert(obj); }
static void page07_curr_view_set_image_unselected_style(lv_obj_t *obj) { assert(obj); }
static void curr_update_grid_fav_ui(int index) { assert(index >= 0); }
static void page07_curr_overview_selection(void *ctx) { assert(ctx == &g_page07_curr); }
static void curr_set_left_info_by_abs(int index) { left_index = index; }
static void curr_refresh_left_buttons(void) {}
static bool perf_profile_is_enabled(void) { return profile; }
static uint64_t app_clock_monotonic_us(void) { return 1000; }
static uint32_t app_clock_elapsed_us32(uint64_t start, uint64_t end)
{ return (uint32_t)(end - start); }
static void perf_profile_report_event_us(const char *page, const char *event, uint32_t elapsed)
{ assert(strcmp(page, "CURRENCY") == 0 && event); (void)elapsed; }
static void lv_timer_set_period(lv_timer_t *timer, unsigned period) { timer->period = period; }
static void lv_timer_resume(lv_timer_t *timer) { timer->paused = false; }
static void lv_timer_pause(lv_timer_t *timer) { timer->paused = true; }
static void lv_timer_ready(lv_timer_t *timer) { timer->ready++; }
static void page07_curr_carousel_enable(carousel_t *carousel, bool enabled)
{
    if (!enabled) ui_scroll_physics_stop(&carousel->motion);
    carousel->enabled = enabled;
}
static void curr_scroll_to_visible_idx(int index, bool animate)
{
    assert(!animate);
    ui_scroll_physics_snap(&g_page07_curr.carousel.motion, (unsigned)index, false, 0);
    snaps++;
}
static void curr_refresh_right_views(void)
{
    char code[4];
    rebuilds++;
    page07_curr_model_refresh_visible();
    currency_state_get_selected_code(code);
    g_page07_curr.model.selected_abs_idx = page07_curr_model_find_abs_idx(code);
    g_page07_curr.model.selected_visible_idx =
        page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);
    ui_scroll_physics_init(&g_page07_curr.carousel.motion,
                          (unsigned)g_page07_curr.model.visible_count, 228.0f, 0);
    g_page07_curr.objects.grid_scroll = &grid_object;
    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        grid_nodes[i].index = g_page07_curr.model.visible_indices[i];
        g_page07_curr.grid_items[i].abs_idx = grid_nodes[i].index;
        g_page07_curr.grid_items[i].item = &grid_nodes[i];
        g_page07_curr.grid_items[i].selected_mark = &grid_nodes[i];
        g_page07_curr.grid_items[i].img = &grid_nodes[i];
        g_page07_curr.grid_items[i].name = &grid_nodes[i];
    }
}
static void page_07_curr_img_refre(void)
{
    page07_curr_model_load();
    curr_refresh_right_views();
    curr_set_left_info_by_abs(g_page07_curr.model.selected_abs_idx);
    currency_state_get_snapshot(&g_curr_page_snapshot);
    currency_state_get_selected_code(g_curr_page_selected_code);
    g_curr_page_snapshot_valid = true;
}

#include "currency_resume_under_test.inc"

static int selected_index(void)
{
    char code[4]; uint8_t index;
    currency_state_get_selected_code(code);
    assert(currency_state_find_code(code, &index));
    return index;
}
static void assert_selected_focus(void)
{
    int index = selected_index();
    assert(g_page07_curr.model.selected_abs_idx == index);
    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        unsigned focused = ui_scroll_physics_nearest(&g_page07_curr.carousel.motion);
        assert(g_page07_curr.model.visible_indices[focused] == index);
        assert(grid_nodes[focused].index == index); /* cached object index map */
        assert(g_page07_curr.carousel.motion.phase == UI_SCROLL_IDLE);
        assert(g_page07_curr.carousel.motion.position == focused * 228.0f);
    } else {
        assert(revealed_grid_index == index);
    }
}
static void setup(bool favorites, int mode)
{
    currency_state_reset();
    memset(&g_page07_curr, 0, sizeof(g_page07_curr));
    memset(&saved, 0, sizeof(saved));
    memset(&prewarm_timer, 0, sizeof(prewarm_timer));
    saved.fav_only = favorites; saved.view_mode = mode;
    saved.fav_count = 1; memcpy(saved.fav_codes[0], "USD", 4);
    curr_page = &page_object; page_object.flags = LV_OBJ_FLAG_HIDDEN;
    page_07_curr_img_refre();
    snaps = rebuilds = saves = foregrounds = 0;
    revealed_grid_index = -1;
}
static void browse_then_reenter(unsigned destination)
{
    int previous_selection = g_page07_curr.model.selected_abs_idx;
    ui_scroll_physics_snap(&g_page07_curr.carousel.motion, destination, false, 0);
    assert(g_page07_curr.model.selected_abs_idx == previous_selection);
    ui_page_07_curr_suspend();
    assert(prewarm_timer.paused && !g_page07_curr.carousel.enabled);
    assert(ui_page_07_curr_resume());
    assert_selected_focus();
    assert(!(curr_page->flags & LV_OBJ_FLAG_HIDDEN));
    assert(!prewarm_timer.paused && prewarm_timer.period == 120);
}
int main(void)
{
    setup(false, PAGE07_CURR_VIEW_CARD);
    for (unsigned destination = 0; destination < 16; destination++)
        browse_then_reenter(destination);
    assert(rebuilds == 0 && saves == 0 && snaps == 16);

    /* Leaving in the middle of a fling must not resume old motion. */
    ui_scroll_physics_snap(&g_page07_curr.carousel.motion, 12, true, 0);
    assert(g_page07_curr.carousel.motion.phase != UI_SCROLL_IDLE);
    ui_page_07_curr_suspend(); assert(ui_page_07_curr_resume());
    assert_selected_focus(); assert(rebuilds == 0);

    /* A refused/cancelled request never updates the confirmed state. */
    g_page07_curr.carousel.motion.position = 11 * 228.0f;
    assert(strcmp(g_curr_page_selected_code, "CNY") == 0);
    ui_page_07_curr_suspend(); assert(ui_page_07_curr_resume());
    assert_selected_focus(); assert(left_index == selected_index());

    /* A successful ACK while hidden is projected before reveal. */
    ui_page_07_curr_suspend(); assert(currency_state_confirm_active_code("EUR"));
    assert(ui_page_07_curr_resume()); assert_selected_focus();
    assert(left_index == selected_index() && rebuilds == 0);

    for (unsigned special = 0; special < 2; special++) {
        setup(true, PAGE07_CURR_VIEW_CARD);
        assert(special ? currency_state_confirm_multi_selection()
                       : currency_state_confirm_auto_selection());
        browse_then_reenter(2);
        assert(g_page07_curr.model.favorite_only && rebuilds == 0 && saves == 0);
        assert(g_page07_curr.model.visible_indices[0] == 0);
        assert(g_page07_curr.model.visible_indices[1] == 1);
    }

    /* FAV hides the confirmed CNY: reveal ALL, never falsely focus AUTO,
     * never alter the favorites or the persisted filter preference. */
    setup(true, PAGE07_CURR_VIEW_CARD);
    assert(ui_page_07_curr_resume()); assert_selected_focus();
    assert(!g_page07_curr.model.favorite_only && rebuilds == 1 && saves == 0);
    assert(saved.fav_only == 1 && saved.fav_count == 1);
    assert(strcmp(saved.fav_codes[0], "USD") == 0);
    assert(g_page07_curr.model.visible_count == currency_state_count());

    /* Returning to a new confirmed favorite after an automatic ALL reveal
     * must not remap retained card objects using the saved FAV preference. */
    setup(true, PAGE07_CURR_VIEW_CARD);
    memcpy(saved.fav_codes[0], "EUR", 4); page_07_curr_img_refre();
    assert(ui_page_07_curr_resume()); assert_selected_focus();
    unsigned before = rebuilds;
    ui_page_07_curr_suspend(); assert(currency_state_confirm_active_code("EUR"));
    assert(ui_page_07_curr_resume()); assert_selected_focus();
    assert(!g_page07_curr.model.favorite_only && rebuilds == before && saves == 0);

    setup(true, PAGE07_CURR_VIEW_CARD);
    assert(currency_state_confirm_active_code("USD"));
    browse_then_reenter(0);
    assert(g_page07_curr.model.favorite_only && rebuilds == 0 && saves == 0);

    setup(false, PAGE07_CURR_VIEW_GRID);
    assert(ui_page_07_curr_resume()); assert_selected_focus();
    assert(!g_page07_curr.carousel.enabled && rebuilds == 0);
    ui_page_07_curr_suspend(); assert(currency_state_confirm_active_code("EUR"));
    assert(ui_page_07_curr_resume()); assert_selected_focus();
    assert(g_curr_grid_styled_abs_idx == selected_index());

    setup(true, PAGE07_CURR_VIEW_GRID); profile = true;
    assert(ui_page_07_curr_resume()); assert_selected_focus();
    assert(!g_page07_curr.model.favorite_only && rebuilds == 1 && saves == 0);

    /* A list refresh can change indices; rebuild, then use the new map. */
    setup(false, PAGE07_CURR_VIEW_CARD); ui_page_07_curr_suspend();
    currency_state_begin_list_sync();
    assert(currency_state_append_list_code(1, "EUR"));
    assert(currency_state_append_list_code(2, "CNY"));
    assert(currency_state_finish_list_sync());
    assert(ui_page_07_curr_resume()); assert_selected_focus(); assert(rebuilds == 1);

    curr_page = NULL; assert(!ui_page_07_curr_resume()); ui_page_07_curr_suspend();
    puts("PASS Currency retained resume: confirmed focus, ALL/FAV, AUTO/MULTI, unchanged snapshots, fling cancel, ACK projection, grid reveal, catalog rebuild, no persistence writes");
    return 0;
}
