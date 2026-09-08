#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_core/ui_frame_commit.h"

typedef enum { UI_PAGE_INVALID = -1, UI_PAGE_BOOT_ANIM, MAIN_PAGE,
    OTHER_PAGE, COLD_PAGE, NESTED_PAGE, UI_PAGE_COUNT } ui_page_t;
enum { UI_PAGE_RETAINED, UI_PAGE_TRANSIENT, UI_DATA_TOPIC_NONE = 0 };
typedef struct { int id; } lv_obj_t;
typedef struct {
    void (*create)(lv_obj_t *);
    bool (*resume)(void);
    void (*suspend)(void);
    void (*refresh_data)(uint32_t);
    bool (*prewarm_ready)(void);
    bool (*prepare_static_step)(void);
    unsigned cache_policy, static_image_count;
    bool predecode_small_visible_images;
} ui_page_registration_t;
typedef struct {
    bool initialized, visible, fail_resume;
    unsigned model, captured, dirty, creates, resumes, refreshes, suspends;
    unsigned writes, capture_bad, static_bad, static_calls;
    char text[7][32];
} page_state_t;
static ui_page_registration_t g_page_registry[UI_PAGE_COUNT];
static bool g_page_cache_ready[UI_PAGE_COUNT];
static uint32_t g_page_data_dirty[UI_PAGE_COUNT];
static struct { ui_page_t current; } g_page_manager;
static ui_page_t g_page_prewarming = UI_PAGE_INVALID;
static page_state_t pages[UI_PAGE_COUNT];
static lv_obj_t screen, root;
static bool memory_allowed, ready_allowed, profiling, enable_nested_create;
static unsigned unrelated_calls, unrelated_flags, nested_observations;
static ui_page_t static_page = MAIN_PAGE;
#define UI_PAGE_PREPARE_STATIC_MAX_STEPS 12U
#define UI_PAGE_PREPARE_STATIC_BUDGET_US 230000U
static const char *create_new_page(ui_page_t page);

static lv_obj_t *lv_scr_act(void) { return &screen; }
static uint32_t lv_obj_get_child_cnt(lv_obj_t *obj) { assert(obj == &screen); return 1; }
static lv_obj_t *lv_obj_get_child(lv_obj_t *obj, int32_t index)
{ assert(obj == &screen && index == 0); return &root; }
static bool lv_obj_is_valid(lv_obj_t *obj) { return obj == &root; }
static bool image_mem_prewarm_allowed(void) { return memory_allowed; }
static bool perf_profile_is_enabled(void) { return profiling; }
static uint64_t app_clock_monotonic_us(void) { static uint64_t time; return time += 100; }
static uint32_t app_clock_elapsed_us32(uint64_t begin, uint64_t end) { return (uint32_t)(end - begin); }
static const char *ui_manager_page_name(ui_page_t page) { (void)page; return "fixture"; }
static void perf_profile_report_event_us(const char *page, const char *event, uint32_t elapsed)
{ assert(page && event && elapsed); }

static bool real_text(const page_state_t *state)
{
    char expected[32];
    for (unsigned i = 0; i < 7; ++i) {
        snprintf(expected, sizeof(expected), "VALUE_%u_%u", i, state->model);
        if (strcmp(state->text[i], expected) != 0) return false;
    }
    return true;
}
static void apply(void *context, uint32_t flags)
{
    page_state_t *state = context;
    assert(flags != 0);
    for (unsigned i = 0; i < 7; ++i)
        snprintf(state->text[i], sizeof(state->text[i]), "VALUE_%u_%u", i, state->model);
    ++state->writes;
    state->dirty = 0;
}
static void project(page_state_t *state)
{
    state->dirty = 1;
    if (!ui_frame_commit_defer(apply, state, 1)) apply(state, 1);
}
static void create_page(ui_page_t page, lv_obj_t *parent)
{
    page_state_t *state = &pages[page];
    assert(parent == &screen);
    state->initialized = true;
    state->visible = true;
    ++state->creates;
    for (unsigned i = 0; i < 7; ++i) strcpy(state->text[i], "Text");
    project(state);
    if (page == MAIN_PAGE && enable_nested_create) {
        assert(!ui_frame_commit_is_batching());
        assert(strcmp(create_new_page(NESTED_PAGE), "CREATE") == 0);
        assert(!ui_frame_commit_is_batching());
        ++nested_observations;
    }
    /* Main finalizes its dirty/model snapshot before prewarm suspends it. */
    state->capture_bad += !real_text(state);
    state->dirty = 0;
    state->captured = state->model;
}
static bool resume_page(ui_page_t page)
{
    page_state_t *state = &pages[page];
    ++state->resumes;
    if (!state->initialized || state->fail_resume) return false;
    state->visible = true;
    if (state->dirty || state->captured != state->model) project(state);
    state->dirty = 0;
    state->captured = state->model;
    return true;
}
static void suspend_page(ui_page_t page)
{
    page_state_t *state = &pages[page];
    ++state->suspends;
    ui_frame_commit_cancel(apply, state);
    state->visible = false;
}
static void refresh_page(ui_page_t page, uint32_t flags)
{
    assert(flags != 0);
    ++pages[page].refreshes;
    project(&pages[page]);
}
#define CALLBACKS(name, id) \
    static void name##_create(lv_obj_t *parent) { create_page(id, parent); } \
    static bool name##_resume(void) { return resume_page(id); } \
    static void name##_suspend(void) { suspend_page(id); } \
    static void name##_refresh(uint32_t flags) { refresh_page(id, flags); }
CALLBACKS(main, MAIN_PAGE)
CALLBACKS(other, OTHER_PAGE)
CALLBACKS(cold, COLD_PAGE)
CALLBACKS(nested, NESTED_PAGE)
static bool prewarm_ready(void) { return ready_allowed; }
static bool static_step(void)
{
    page_state_t *state = &pages[static_page];
    assert(state->visible);
    state->static_bad += !real_text(state);
    return state->static_calls++ == 0;
}
static uint8_t ui_manager_predecode_static_images(const ui_page_registration_t *entry)
{
    pages[static_page].static_bad += !real_text(&pages[static_page]);
    return (uint8_t)entry->static_image_count;
}
static uint8_t ui_manager_predecode_small_visible_images(lv_obj_t *obj)
{
    assert(obj == &root);
    pages[static_page].static_bad += !real_text(&pages[static_page]);
    return 1;
}
#include "page_manager_under_test.h"

static void unrelated(void *context, uint32_t flags)
{ assert(context == &unrelated_calls); ++unrelated_calls; unrelated_flags |= flags; }
static void reset(void)
{
    assert(!ui_frame_commit_is_batching() && !ui_frame_commit_pending());
    memset(pages, 0, sizeof(pages));
    memset(g_page_registry, 0, sizeof(g_page_registry));
    memset(g_page_cache_ready, 0, sizeof(g_page_cache_ready));
    memset(g_page_data_dirty, 0, sizeof(g_page_data_dirty));
    g_page_manager.current = UI_PAGE_BOOT_ANIM;
    g_page_prewarming = UI_PAGE_INVALID;
    memory_allowed = ready_allowed = profiling = true;
    enable_nested_create = false;
    unrelated_calls = unrelated_flags = nested_observations = 0;
    static_page = MAIN_PAGE;
#define REGISTER(name, id) \
    g_page_registry[id] = (ui_page_registration_t){ \
        name##_create, name##_resume, name##_suspend, name##_refresh, \
        prewarm_ready, NULL, UI_PAGE_RETAINED, 0, false }
    REGISTER(main, MAIN_PAGE);
    REGISTER(other, OTHER_PAGE);
    REGISTER(cold, COLD_PAGE);
    REGISTER(nested, NESTED_PAGE);
    g_page_registry[COLD_PAGE].cache_policy = UI_PAGE_TRANSIENT;
    for (unsigned i = 0; i < UI_PAGE_COUNT; ++i) pages[i].model = 1;
}
static void finish_batch(void)
{
    assert(ui_frame_commit_is_batching());
    ui_frame_commit_end_batch();
    assert(!ui_frame_commit_is_batching());
    ui_frame_commit_flush();
    assert(!ui_frame_commit_pending());
}
static void boot_prewarm(bool mutation)
{
    reset();
    g_page_registry[MAIN_PAGE].static_image_count = 2;
    g_page_registry[MAIN_PAGE].predecode_small_visible_images = true;
    g_page_registry[MAIN_PAGE].prepare_static_step = static_step;
    ui_frame_commit_begin_batch();
    assert(ui_frame_commit_defer(unrelated, &unrelated_calls, 1));
    assert(ui_manager_prewarm_page(MAIN_PAGE));
    assert(g_page_prewarming == UI_PAGE_INVALID && g_page_cache_ready[MAIN_PAGE]);
    assert(!pages[MAIN_PAGE].visible && pages[MAIN_PAGE].suspends == 1);
    assert(unrelated_calls == 0 && ui_frame_commit_pending());
    assert(ui_frame_commit_is_batching());
    assert(strcmp(create_new_page(MAIN_PAGE), "RESUME") == 0);
    assert(pages[MAIN_PAGE].creates == 1 && pages[MAIN_PAGE].writes == (mutation ? 0U : 1U));
    if (mutation) {
        assert(pages[MAIN_PAGE].capture_bad == 1 && pages[MAIN_PAGE].static_bad > 0);
        for (unsigned i = 0; i < 7; ++i) assert(strcmp(pages[MAIN_PAGE].text[i], "Text") == 0);
    } else {
        assert(real_text(&pages[MAIN_PAGE]));
        assert(pages[MAIN_PAGE].capture_bad == 0 && pages[MAIN_PAGE].static_bad == 0);
    }
    assert(ui_frame_commit_defer(unrelated, &unrelated_calls, 2));
    finish_batch();
    assert(unrelated_calls == 1 && unrelated_flags == 3);
    /* Cancelled page projection cannot be resurrected by flushing the batch. */
    assert(real_text(&pages[MAIN_PAGE]) != mutation);
}
static void cold_and_cached(void)
{
    reset();
    ui_frame_commit_begin_batch();
    assert(ui_frame_commit_defer(unrelated, &unrelated_calls, 4));
    assert(strcmp(create_new_page(COLD_PAGE), "CREATE") == 0);
    assert(real_text(&pages[COLD_PAGE]) && pages[COLD_PAGE].capture_bad == 0);
    assert(strcmp(create_new_page(OTHER_PAGE), "CREATE") == 0);
    assert(real_text(&pages[OTHER_PAGE]));
    assert(unrelated_calls == 0 && ui_frame_commit_is_batching());
    suspend_page(OTHER_PAGE);
    pages[OTHER_PAGE].model = 2;
    g_page_data_dirty[OTHER_PAGE] = 7;
    assert(strcmp(create_new_page(OTHER_PAGE), "RESUME") == 0);
    assert(real_text(&pages[OTHER_PAGE]) && pages[OTHER_PAGE].refreshes == 1);
    assert(g_page_data_dirty[OTHER_PAGE] == UI_DATA_TOPIC_NONE);
    suspend_page(OTHER_PAGE);
    pages[OTHER_PAGE].model = 3;
    pages[OTHER_PAGE].fail_resume = true;
    g_page_data_dirty[OTHER_PAGE] = 8;
    assert(strcmp(create_new_page(OTHER_PAGE), "CREATE") == 0);
    assert(real_text(&pages[OTHER_PAGE]) && pages[OTHER_PAGE].creates == 2);
    assert(pages[OTHER_PAGE].capture_bad == 0 && g_page_data_dirty[OTHER_PAGE] == 0);
    assert(unrelated_calls == 0);
    finish_batch();
    assert(unrelated_calls == 1 && unrelated_flags == 4);
}
static void nested_scopes(void)
{
    reset();
    enable_nested_create = true;
    ui_frame_commit_begin_batch();
    ui_frame_commit_begin_batch();
    ui_frame_commit_begin_sync();
    assert(strcmp(create_new_page(MAIN_PAGE), "CREATE") == 0);
    assert(nested_observations == 1 && real_text(&pages[NESTED_PAGE]));
    assert(!ui_frame_commit_is_batching());
    assert(ui_manager_prewarm_page(OTHER_PAGE));
    assert(!ui_frame_commit_is_batching());
    ui_frame_commit_end_sync();
    assert(ui_frame_commit_is_batching());
    ui_frame_commit_end_batch();
    assert(ui_frame_commit_is_batching());
    assert(ui_frame_commit_defer(unrelated, &unrelated_calls, 16));
    finish_batch();
    assert(unrelated_calls == 1 && unrelated_flags == 16);
}
static void early_returns(void)
{
    reset();
    ui_frame_commit_begin_batch();
    assert(!ui_manager_prewarm_page(UI_PAGE_INVALID));
    assert(!ui_manager_prewarm_page(UI_PAGE_COUNT));
    assert(!ui_manager_prewarm_page(UI_PAGE_BOOT_ANIM));
    g_page_manager.current = MAIN_PAGE;
    assert(!ui_manager_prewarm_page(MAIN_PAGE));
    g_page_manager.current = UI_PAGE_BOOT_ANIM;
    g_page_cache_ready[MAIN_PAGE] = true;
    assert(ui_manager_prewarm_page(MAIN_PAGE));
    g_page_cache_ready[MAIN_PAGE] = false;
    memory_allowed = false;
    assert(!ui_manager_prewarm_page(MAIN_PAGE));
    memory_allowed = true;
    assert(!ui_manager_prewarm_page(COLD_PAGE));
    g_page_registry[MAIN_PAGE].suspend = NULL;
    assert(!ui_manager_prewarm_page(MAIN_PAGE));
    g_page_registry[MAIN_PAGE].suspend = main_suspend;
    ready_allowed = false;
    assert(!ui_manager_prewarm_page(MAIN_PAGE));
    assert(ui_frame_commit_is_batching() && !ui_frame_commit_pending());
    ready_allowed = true;
    profiling = false;
    assert(ui_manager_prewarm_page(MAIN_PAGE));
    assert(real_text(&pages[MAIN_PAGE]));
    assert(ui_frame_commit_defer(unrelated, &unrelated_calls, 32));
    finish_batch();
    assert(unrelated_calls == 1);
}
int main(int argc, char **argv)
{
    bool mutation = argc == 2 && strcmp(argv[1], "--sync-guard-mutation") == 0;
    assert(argc == 1 || mutation);
    boot_prewarm(mutation);
    if (mutation) {
        puts("PASS negative control: removing only manager sync guards reproduces all seven Text labels after prewarm/suspend/unchanged resume");
        return 0;
    }
    cold_and_cached();
    nested_scopes();
    early_returns();
    puts("PASS: real manager prewarm/create/dirty resume/fallback create initialize before capture/suspend and preserve unrelated pending work");
    puts("PASS: nested synchronous activations, nested batches, early returns and non-profiled prewarm leave no sync/batch scope leak");
    return 0;
}
