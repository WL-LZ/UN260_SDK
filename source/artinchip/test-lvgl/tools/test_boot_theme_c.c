/* Real LVGL renderer/lifetime checks for the optional third boot theme.
 * Only navigation, file decoding, and one-shot timer failure are test fixtures.
 * Production rendering and animation state are included without test APIs. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_core/lv_page_manager.h"
#include "boot_theme_c_assets.h"
#include "un260/lv_core/boot_anim/page_00_boot_anim_theme_c.c"

static ui_page_t current_page = UI_PAGE_BOOT;
static unsigned switches;
static bool fail_next_timer;
static bool fail_background;
static bool fail_emblem;
static unsigned decode_opens;
static unsigned decode_closes;
static lv_color_t framebuffer[1280 * 400];
static lv_color_t draw_buffer[1280 * 40];
static unsigned flushes;
static uint32_t flushed_pixels;
static lv_area_t flushed_bounds;
static uint8_t *background_pixels;
static uint8_t *emblem_pixels;
static unsigned baseline_timers;
static lv_obj_t *selftest_underlay;
bool ui_page_08_curr_prepare_step(void) { return true; }
static unsigned selftest_draws;

void ui_page_08_curr_set_covered(bool covered)
{
    if (covered) lv_obj_add_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN);
}

static void underlay_draw(lv_event_t *event)
{
    LV_UNUSED(event);
    ++selftest_draws;
}

ui_page_t ui_manager_get_current_page(void)
{
    return current_page;
}

void ui_manager_switch(ui_page_t page)
{
    assert((current_page == UI_PAGE_BOOT_ANIM && page == UI_PAGE_BOOT) ||
           (current_page == UI_PAGE_BOOT && page == UI_PAGE_SENSOR));
    ++switches;
    ui_page_00_boot_anim_destroy();
    current_page = page;
}

lv_timer_t *__real_lv_timer_create(lv_timer_cb_t callback, uint32_t period,
                                  void *user_data);
lv_timer_t *__wrap_lv_timer_create(lv_timer_cb_t callback, uint32_t period,
                                  void *user_data)
{
    if (fail_next_timer) {
        fail_next_timer = false;
        return NULL;
    }
    return __real_lv_timer_create(callback, period, user_data);
}

static bool image_is(const void *source, const char *name)
{
    return source && lv_img_src_get_type(source) == LV_IMG_SRC_FILE &&
           strcmp((const char *)source, name) == 0;
}

static lv_res_t image_info(lv_img_decoder_t *decoder, const void *source,
                           lv_img_header_t *header)
{
    LV_UNUSED(decoder);
    memset(header, 0, sizeof(*header));
    header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    if (image_is(source, "L:/usr/local/share/lvgl_data/boot_theme_c/background.png")) {
        if (fail_background) return LV_RES_INV;
        header->w = BOOT_TEST_BACKGROUND_WIDTH;
        header->h = BOOT_TEST_BACKGROUND_HEIGHT;
        return LV_RES_OK;
    }
    if (image_is(source, "L:/usr/local/share/lvgl_data/boot_theme_c/emblem.png")) {
        if (fail_emblem) return LV_RES_INV;
        header->w = BOOT_TEST_EMBLEM_WIDTH;
        header->h = BOOT_TEST_EMBLEM_HEIGHT;
        return LV_RES_OK;
    }
    return LV_RES_INV;
}

static lv_res_t image_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    if (image_info(decoder, dsc->src, &dsc->header) != LV_RES_OK)
        return LV_RES_INV;
    dsc->img_data = image_is(dsc->src,
        "L:/usr/local/share/lvgl_data/boot_theme_c/background.png") ?
        background_pixels : emblem_pixels;
    ++decode_opens;
    return LV_RES_OK;
}

static void image_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    LV_UNUSED(decoder);
    LV_UNUSED(dsc);
    ++decode_closes;
}

static uint8_t *load_pixels(const char *name, size_t length)
{
    const char *directory = getenv("BOOT_THEME_C_PIXEL_INPUT");
    assert(directory);
    char path[2048];
    assert(snprintf(path, sizeof(path), "%s/%s", directory, name) < (int)sizeof(path));
    FILE *file = fopen(path, "rb");
    assert(file);
    uint8_t *pixels = malloc(length);
    assert(pixels);
    assert(fread(pixels, 1, length, file) == length);
    assert(fgetc(file) == EOF);
    fclose(file);
    return pixels;
}

static void reset_flush_stats(void)
{
    flushes = 0;
    flushed_pixels = 0;
    flushed_bounds = (lv_area_t){1280, 400, -1, -1};
}

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    const unsigned width = (unsigned)lv_area_get_width(area);
    assert(area->x1 >= 0 && area->y1 >= 0 && area->x2 < 1280 && area->y2 < 400);
    ++flushes;
    flushed_pixels += width * (unsigned)lv_area_get_height(area);
    if (area->x1 < flushed_bounds.x1) flushed_bounds.x1 = area->x1;
    if (area->y1 < flushed_bounds.y1) flushed_bounds.y1 = area->y1;
    if (area->x2 > flushed_bounds.x2) flushed_bounds.x2 = area->x2;
    if (area->y2 > flushed_bounds.y2) flushed_bounds.y2 = area->y2;
    for (int y = area->y1; y <= area->y2; ++y)
        memcpy(framebuffer + y * 1280 + area->x1,
               pixels + (y - area->y1) * width, width * sizeof(*pixels));
    lv_disp_flush_ready(driver);
}

static unsigned timer_count(void)
{
    unsigned count = 0;
    for (lv_timer_t *timer = lv_timer_get_next(NULL); timer;
         timer = lv_timer_get_next(timer)) ++count;
    return count;
}

static void render(void)
{
    lv_obj_update_layout(lv_scr_act());
    lv_obj_update_layout(lv_layer_top());
    lv_refr_now(NULL);
}

static void frame(unsigned elapsed)
{
    apply_elapsed(elapsed);
    render();
}

static void tick(unsigned elapsed)
{
    for (unsigned advanced = 0; advanced < elapsed; advanced += 20) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
}

static void screenshot(const char *name)
{
    const char *directory = getenv("BOOT_THEME_C_RASTER_OUTPUT");
    if (!directory) return;
    render();
    char path[2048];
    assert(snprintf(path, sizeof(path), "%s/%s.bmp", directory, name) < (int)sizeof(path));
    FILE *file = fopen(path, "wb");
    assert(file);
    uint8_t header[54] = {0};
    uint32_t size = 54 + sizeof(framebuffer), offset = 54, dib = 40, width = 1280;
    int32_t height = -400;
    uint16_t planes = 1, bits = 32;
    header[0] = 'B'; header[1] = 'M';
    memcpy(header + 2, &size, 4); memcpy(header + 10, &offset, 4);
    memcpy(header + 14, &dib, 4); memcpy(header + 18, &width, 4);
    memcpy(header + 22, &height, 4); memcpy(header + 26, &planes, 2);
    memcpy(header + 28, &bits, 2);
    assert(fwrite(header, 1, sizeof(header), file) == sizeof(header));
    assert(fwrite(framebuffer, 1, sizeof(framebuffer), file) == sizeof(framebuffer));
    fclose(file);
}

static void create(void)
{
    assert(!ui_page_00_boot_anim_is_active());
    current_page = UI_PAGE_BOOT;
    ui_page_00_boot_anim_create(lv_layer_top());
    assert(ui_page_00_boot_anim_is_active());
    assert(g_intro.root && g_intro.background && g_intro.icon && g_intro.brand &&
           g_intro.welcome && g_intro.dots[0] && g_intro.dots[1] && g_intro.dots[2]);
    assert(g_intro.timer);
    render();
}

static void assert_no_owner(void)
{
    assert(!ui_page_00_boot_anim_is_active());
    assert(g_intro.root == NULL);
    assert(g_intro.timer == NULL);
    assert(timer_count() == baseline_timers);
}

static void test_sequence(void)
{
    create();
    lv_obj_t *original = g_intro.root;
    unsigned count = timer_count();
    ui_page_00_boot_anim_create(lv_layer_top());
    assert(g_intro.root == original && timer_count() == count);
    const void *icon_source = lv_img_get_src(g_intro.icon);
    for (unsigned elapsed = 0; elapsed < 7875; elapsed += 25) {
        apply_elapsed(elapsed);
        assert(lv_img_get_src(g_intro.icon) == icon_source);
        assert(lv_img_get_angle(g_intro.icon) == 0);
        assert(lv_img_get_zoom(g_intro.icon) == 256);
        if (elapsed <= 2700)
            assert(lv_obj_get_style_img_opa(g_intro.icon, 0) ==
                   lv_obj_get_style_text_opa(g_intro.brand, 0));
        assert(!(lv_obj_get_style_text_opa(g_intro.brand, 0) > LV_OPA_MIN &&
                 lv_obj_get_style_text_opa(g_intro.welcome, 0) > LV_OPA_MIN));
    }
    frame(550); screenshot("icon-fade");
    frame(1000);
    assert(lv_obj_get_style_img_opa(g_intro.icon, 0) >= LV_OPA_MAX);
    assert(lv_obj_get_style_text_opa(g_intro.brand, 0) >= LV_OPA_MAX);
    frame(2000); screenshot("brand");
    assert(lv_obj_get_style_text_opa(g_intro.brand, 0) >= LV_OPA_MAX);
    frame(3325);
    assert(lv_obj_get_style_text_opa(g_intro.brand, 0) <= LV_OPA_MIN);
    assert(lv_obj_get_style_text_opa(g_intro.welcome, 0) <= LV_OPA_MIN);
    frame(4300); screenshot("welcome");
    assert(lv_obj_get_style_text_opa(g_intro.welcome, 0) >= LV_OPA_MAX);
    const char *welcome = lv_label_get_text(g_intro.welcome);
    assert(strcmp(welcome, "WELCOME") == 0 || strcmp(welcome, "WELCOM") == 0);
    frame(5000); screenshot("waiting");
    frame(8030); screenshot("retiring");
    assert(lv_obj_get_style_text_opa(g_intro.welcome, 0) > LV_OPA_MIN);
    assert(lv_obj_get_style_text_opa(g_intro.welcome, 0) < LV_OPA_MAX);
    ui_page_00_boot_anim_destroy();
    ui_page_00_boot_anim_destroy();
    assert_no_owner();
    puts("PASS sequential fades, pre-rendered nonrotating icon, duplicate create/destroy");
}

static void test_settling(void)
{
    create();
    lv_obj_t *objects[] = {g_intro.icon, g_intro.brand, g_intro.welcome};
    const unsigned starts[] = {150, 150, 3450};
    const unsigned durations[] = {800, 800, 700};
    const lv_coord_t bases[] = {82, 219, 219};
    const lv_coord_t distances[] = {6, 4, 4};
    for (unsigned i = 0; i < 3; ++i) {
        frame(starts[i]);
        assert(lv_obj_get_y(objects[i]) == bases[i] + distances[i]);
        lv_coord_t previous = lv_obj_get_y(objects[i]);
        for (unsigned t = 16; t < durations[i]; t += 16) {
            frame(starts[i] + t);
            lv_coord_t y = lv_obj_get_y(objects[i]);
            assert(y >= bases[i] && y <= previous);
            assert(previous - y <= 1);
            previous = y;
        }
        frame(starts[i] + durations[i]);
        assert(lv_obj_get_y(objects[i]) == bases[i]);
        frame(8030);
        assert(lv_obj_get_y(objects[i]) == bases[i]);
    }
    /* Sample settled dot cycles: asymmetric timing, stagger, quiet landing. */
    unsigned start = DOT_START_MS + DOT_PERIOD_MS;
    frame(start);
    assert(lv_obj_get_y(g_intro.dots[0]) == 296);
    frame(start + 240);
    assert(lv_obj_get_y(g_intro.dots[0]) == 290);
    assert(lv_obj_get_y(g_intro.dots[1]) > 290);
    frame(start + 450);
    assert(lv_obj_get_y(g_intro.dots[0]) == 293);
    frame(start + 660);
    assert(lv_obj_get_y(g_intro.dots[0]) == 296);
    frame(start + 1000);
    for (unsigned i = 0; i < 3; ++i)
        assert(lv_obj_get_y(g_intro.dots[i]) == 296);
    ui_page_00_boot_anim_destroy();
    assert_no_owner();
    puts("PASS settling: bounded monotonic motion, <=1px per 16ms, fixed fade-out position");
    puts("PASS dots: 240ms lift / 420ms landing, staggered peaks and quiet rest");
}

static void test_refresh_budget(void)
{
    create();
    frame(1000);
    reset_flush_stats();
    for (unsigned elapsed = 1000; elapsed <= 1120; elapsed += 20) frame(elapsed);
    assert(flushes == 0 && flushed_pixels == 0);
    frame(5500);
    unsigned opened = decode_opens;
    unsigned children = lv_obj_get_child_cnt(g_intro.root);
    uint32_t max_pixels = 0;
    bool visible_motion = false;
    for (unsigned elapsed = 5520; elapsed < 6720; elapsed += 20) {
        reset_flush_stats();
        frame(elapsed);
        assert(lv_obj_get_child_cnt(g_intro.root) == children);
        assert(decode_opens == opened);
        assert(flushed_pixels < 1280U * 400U / 20U);
        if (flushed_pixels > max_pixels) max_pixels = flushed_pixels;
        if (flushes) {
            visible_motion = true;
            assert(lv_area_get_width(&flushed_bounds) < 200);
            assert(lv_area_get_height(&flushed_bounds) < 100);
            assert(flushed_bounds.x1 > 400 && flushed_bounds.x2 < 880);
            assert(flushed_bounds.y1 > 220 && flushed_bounds.y2 < 360);
        }
    }
    assert(visible_motion);
    assert(timer_count() == baseline_timers + 1);
    printf("PASS static phase: 0 redraws; waiting maximum dirty pixels: %u/512000\n",
           (unsigned)max_pixels);
    ui_page_00_boot_anim_destroy();
    assert_no_owner();
}

static void test_scanout_cadence(void)
{
    /* Exercise real LVGL timer dispatch at a nominal 75 Hz scanout cadence,
     * rather than the 20 ms ticks used by the long lifecycle tests. The old
     * 16 ms theme timer skipped every other 13/14 ms service opportunity.
     * This is a scheduling regression test, not a physical FPS measurement. */
    const unsigned starts[] = {400U, 2780U, 3560U,
                               DOT_START_MS + DOT_PERIOD_MS + 40U};
    for (unsigned phase = 0; phase < sizeof(starts) / sizeof(starts[0]); ++phase) {
        create();
        assert(g_intro.timer->period == LV_DISP_DEF_REFR_PERIOD);
        ui_page_00_boot_anim_set_startup_ready(false);
        ui_page_00_boot_anim_adopt_elapsed(starts[phase]);
        render();
        unsigned opened = decode_opens;
        unsigned children = lv_obj_get_child_cnt(g_intro.root);
        unsigned elapsed = starts[phase];
        for (unsigned scanout = 0; scanout < 18; ++scanout) {
            unsigned step = scanout % 3U == 2U ? 14U : 13U;
            elapsed += step;
            lv_tick_inc(step);
            reset_flush_stats();
            lv_timer_handler();
            assert(g_intro.timer->last_run == lv_tick_get());
            assert(lv_tick_elaps(g_intro.start_tick) == elapsed);
            if (phase == 0) {
                lv_opa_t expected = opacity(ease(progress(elapsed, 150U, 800U)));
                assert(lv_obj_get_style_img_opa(g_intro.icon, 0) == expected);
                assert(lv_obj_get_style_text_opa(g_intro.brand, 0) == expected);
            } else if (phase == 1) {
                assert(lv_obj_get_style_text_opa(g_intro.brand, 0) ==
                    opacity(1.0f - ease(progress(elapsed, 2700U, 600U))));
            } else if (phase == 2) {
                assert(lv_obj_get_style_text_opa(g_intro.welcome, 0) ==
                    opacity(ease(progress(elapsed, 3450U, 700U))));
            }
            if (phase < 3) assert(flushes != 0);
            assert(lv_obj_get_child_cnt(g_intro.root) == children);
            assert(decode_opens == opened);
            assert(timer_count() == baseline_timers + 1U);
        }
        ui_page_00_boot_anim_destroy();
        assert_no_owner();
    }
    /* Faster service must not shorten the required three visible dot rounds. */
    create();
    ui_page_00_boot_anim_set_startup_ready(true);
    for (unsigned scanout = 0; ui_page_00_boot_anim_is_active(); ++scanout) {
        lv_tick_inc(scanout % 3U == 2U ? 14U : 13U);
        lv_timer_handler();
        if (ui_page_00_boot_anim_is_active()) {
            unsigned elapsed = lv_tick_elaps(g_intro.start_tick);
            if (elapsed < DOT_START_MS + 2U * DOT_PERIOD_MS + DOT_LANDED_MS)
                assert(!g_intro.revealing);
            if (g_intro.revealing) assert(g_intro.ready_dot_rounds == READY_DOT_ROUNDS);
        }
        assert(scanout < 800U);
    }
    assert_no_owner();
    puts("PASS 13/14 ms real-LVGL dispatch: no 16 ms visual skips, no extra resources, three ready rounds retained");
}

static void test_lifetimes(void)
{
    create();
    lv_obj_del(g_intro.root);
    assert_no_owner();
    tick(200);
    create();
    ui_page_00_boot_anim_destroy();
    assert_no_owner();

    lv_obj_t *parent = lv_obj_create(lv_layer_top());
    ui_page_00_boot_anim_create(parent);
    assert(ui_page_00_boot_anim_is_active());
    lv_obj_del(parent);
    assert_no_owner();
    tick(200);

    fail_next_timer = true;
    ui_page_00_boot_anim_create(lv_layer_top());
    assert(!fail_next_timer);
    assert_no_owner();
    unsigned overlay_switches = switches;
    ui_page_00_boot_anim_poll();
    assert(switches == overlay_switches && current_page == UI_PAGE_BOOT);

    create();
    unsigned before = switches;
    tick(8240);
    assert_no_owner();
    assert(switches == before && current_page == UI_PAGE_BOOT);

    current_page = UI_PAGE_BOOT_ANIM;
    ui_page_00_boot_anim_create(lv_layer_top());
    assert(ui_page_00_boot_anim_is_active());
    tick(8240);
    assert_no_owner();
    assert(switches == before + 1 && current_page == UI_PAGE_BOOT);

    current_page = UI_PAGE_BOOT_ANIM;
    fail_next_timer = true;
    ui_page_00_boot_anim_create(lv_layer_top());
    assert_no_owner();
    assert(switches == before + 1 && current_page == UI_PAGE_BOOT_ANIM);
    ui_page_00_boot_anim_poll();
    assert(switches == before + 2 && current_page == UI_PAGE_BOOT);
    ui_page_00_boot_anim_poll();
    assert(switches == before + 2);

    /* Page create runs before the manager commits current. Never navigate
     * reentrantly from an allocation failure inside that create stack. */
    current_page = UI_PAGE_MAIN;
    fail_next_timer = true;
    ui_page_00_boot_anim_create(lv_layer_top());
    assert_no_owner();
    assert(switches == before + 2 && current_page == UI_PAGE_MAIN);
    current_page = UI_PAGE_BOOT_ANIM;
    ui_page_00_boot_anim_poll();
    assert_no_owner();
    assert(switches == before + 3 && current_page == UI_PAGE_BOOT);
    create();
    current_page = UI_PAGE_SENSOR;
    tick(40);
    assert_no_owner();
    assert(current_page == UI_PAGE_SENSOR && switches == before + 3);

    lv_tick_inc(UINT32_MAX - lv_tick_get() - 2000U);
    create();
    tick(8240);
    assert_no_owner();
    assert(current_page == UI_PAGE_BOOT && switches == before + 3);
    puts("PASS root/parent deletion, recreate, timer failure, finish and registered-page routing");
    puts("PASS failed creation defers BOOT_ANIM fallback until manager commit/poll");
    puts("PASS early navigation retains SENSOR; elapsed timeline crosses tick wrap");
}

static void test_selftest_cover(void)
{
    create();
    assert(lv_obj_has_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN));
    selftest_draws = 0;
    tick(980);
    assert(lv_obj_has_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN));
    assert(selftest_draws == 0);
    tick(80);
    render();
    assert(!lv_obj_has_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN));
    assert(selftest_draws > 0);
    ui_page_00_boot_anim_destroy();
    create();
    lv_obj_del(g_intro.root);
    assert(!lv_obj_has_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN));
    assert_no_owner();
    fail_next_timer = true;
    ui_page_00_boot_anim_create(lv_layer_top());
    assert(!lv_obj_has_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN));
    assert_no_owner();
    ui_page_00_boot_anim_poll();
    current_page = UI_PAGE_BOOT;
    ui_page_00_boot_anim_create(lv_layer_top());
    lv_tick_inc(1500);
    timer_cb(g_intro.timer);
    assert(lv_obj_has_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN));
    render();
    timer_cb(g_intro.timer);
    assert(!lv_obj_has_flag(selftest_underlay, LV_OBJ_FLAG_HIDDEN));
    ui_page_00_boot_anim_destroy();
    puts("PASS covered self-test: no first-frame underlay draw, quiet-hold restore, deletion/failure cleanup");
}

static void test_real_startup(void)
{
    create();
    ui_page_00_boot_anim_set_startup_ready(false);
    ui_page_00_boot_anim_adopt_elapsed(5000);
    assert(lv_tick_elaps(g_intro.start_tick) == 5000);
    assert(lv_obj_get_style_text_opa(g_intro.brand, 0) == LV_OPA_TRANSP);
    assert(lv_obj_get_style_text_opa(g_intro.welcome, 0) == LV_OPA_COVER);
    ui_page_00_boot_anim_destroy();
    create();
    ui_page_00_boot_anim_set_startup_ready(false);
    tick(12000);
    assert(ui_page_00_boot_anim_is_active() && !g_intro.revealing);
    assert(lv_obj_get_style_text_opa(g_intro.welcome, 0) == LV_OPA_COVER);
    lv_coord_t y = lv_obj_get_y(g_intro.dots[0]);
    tick(200);
    assert(lv_obj_get_y(g_intro.dots[0]) != y);
    ui_page_00_boot_anim_set_startup_ready(true);
    tick(5100);
    assert(g_intro.ready_dot_rounds == 2 && !g_intro.revealing);
    ui_page_00_boot_anim_set_startup_ready(true); /* Duplicate must not restart. */
    tick(160);
    assert(g_intro.revealing && ui_page_00_boot_anim_is_active());
    tick(400);
    assert_no_owner();
    create();
    ui_page_00_boot_anim_set_startup_ready(true);
    tick(4800);
    assert(!g_intro.revealing);
    tick(3560);
    assert(g_intro.ready_dot_rounds == 3 && g_intro.revealing);
    tick(400);
    assert_no_owner();
    create();
    ui_page_00_boot_anim_set_startup_ready(true);
    lv_tick_inc(14000); /* No rendered jumps: elapsed time alone is insufficient. */
    timer_cb(g_intro.timer);
    assert(g_intro.ready_dot_rounds == 0 && !g_intro.revealing);
    tick(5600);
    assert_no_owner();
    create();
    ui_page_00_boot_anim_set_startup_ready(false);
    tick(1200);
    ui_page_00_boot_anim_set_startup_error(false);
    assert(g_intro.failed && !g_intro.timer && !g_intro.diagnostics);
    assert(strcmp(lv_label_get_text(g_intro.welcome), "STARTUP ERROR") == 0);
    ui_page_00_boot_anim_set_startup_ready(true);
    tick(1000);
    assert(ui_page_00_boot_anim_is_active());
    ui_page_00_boot_anim_set_startup_error(true);
    lv_obj_t *button = g_intro.diagnostics;
    assert(button);
    ui_page_00_boot_anim_set_startup_error(true);
    assert(g_intro.diagnostics == button);
    screenshot("startup-error");
    lv_event_send(button, LV_EVENT_CLICKED, NULL);
    assert(current_page == UI_PAGE_SENSOR);
    assert_no_owner();
    puts("PASS real startup: prolonged moving wait, ready-controlled dissolve, minimum visual sequence, latched failure and SENSOR action");
}

static void test_resource_fallback(void)
{
    for (unsigned variant = 1; variant <= 3; ++variant) {
        fail_background = (variant & 1U) != 0;
        fail_emblem = (variant & 2U) != 0;
        create();
        assert(g_intro.background_ready == !fail_background);
        assert(g_intro.icon_ready == !fail_emblem);
        if (fail_background) {
            assert(lv_obj_has_flag(g_intro.background, LV_OBJ_FLAG_HIDDEN));
            assert(lv_obj_get_style_bg_opa(g_intro.root, 0) == LV_OPA_COVER);
        }
        if (fail_emblem) assert(lv_obj_has_flag(g_intro.icon, LV_OBJ_FLAG_HIDDEN));
        frame(5600);
        assert(lv_obj_get_style_text_opa(g_intro.welcome, 0) >= LV_OPA_MAX);
        if (variant == 3) screenshot("missing-assets-fallback");
        tick(8240);
        assert_no_owner();
    }
    fail_background = fail_emblem = false;
    create();
    assert(g_intro.background_ready && g_intro.icon_ready);
    ui_page_00_boot_anim_destroy();
    assert_no_owner();
    puts("PASS missing background/icon/both fallback and successful recreation");
}

int main(void)
{
    assert(sizeof(lv_color_t) == 4);
    background_pixels = load_pixels("background.bgra",
        BOOT_TEST_BACKGROUND_WIDTH * BOOT_TEST_BACKGROUND_HEIGHT * 4U);
    emblem_pixels = load_pixels("emblem.bgra",
        BOOT_TEST_EMBLEM_WIDTH * BOOT_TEST_EMBLEM_HEIGHT * 4U);
    lv_init();
    lv_img_cache_set_size(4);
    lv_img_decoder_t *decoder = lv_img_decoder_create();
    assert(decoder);
    lv_img_decoder_set_info_cb(decoder, image_info);
    lv_img_decoder_set_open_cb(decoder, image_open);
    lv_img_decoder_set_close_cb(decoder, image_close);
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, draw_buffer, NULL, 1280 * 40);
    lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = 1280; driver.ver_res = 400;
    driver.draw_buf = &buffer; driver.flush_cb = flush;
    assert(lv_disp_drv_register(&driver));
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xBCD3DE), 0);
    selftest_underlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(selftest_underlay);
    lv_obj_set_size(selftest_underlay, 1280, 400);
    lv_obj_add_event_cb(selftest_underlay, underlay_draw, LV_EVENT_DRAW_MAIN, NULL);
    render();
    baseline_timers = timer_count();
    test_sequence();
    test_settling();
    test_refresh_budget();
    test_scanout_cadence();
    test_lifetimes();
    test_resource_fallback();
    test_selftest_cover();
    test_real_startup();
    lv_img_cache_invalidate_src(NULL);
    assert(decode_closes == decode_opens);
    lv_img_decoder_delete(decoder);
    free(emblem_pixels);
    free(background_pixels);
    puts("PASS theme C host rendering and ownership (not board GE/frame-rate validation)");
    return 0;
}
