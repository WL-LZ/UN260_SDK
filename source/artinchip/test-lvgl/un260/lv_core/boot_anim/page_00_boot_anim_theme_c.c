#include "un260/lv_core/page_00_boot_anim.h"

#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C

#include <string.h>
#include "un260/font/manrope_fonts.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_08_boot.h"

/* Theme B's 7.875s hold + 320ms cubic dissolve, without its terminal layer. */
#define INTRO_REVEAL_MS 7875U
#define INTRO_FADE_MS 320U
/* Use the existing display update budget, not a separate 16 ms animation
 * ceiling. The board's nominal scanout is about 13.4 ms; a 16 ms callback
 * can otherwise leave alternate scanouts holding the previous visual state.
 * VSYNC still limits presentation; this timer exists only while intro owns it. */
#define INTRO_TIMER_MS LV_DISP_DEF_REFR_PERIOD
#define DOT_START_MS 4350U
#define DOT_PERIOD_MS 1500U
#define INTRO_MIN_REVEAL_MS 5000U
#define READY_DOT_ROUNDS 3U
#define DOT_LANDED_MS (320U + 660U)

static const char background_src[] =
    "L:/usr/local/share/lvgl_data/boot_theme_c/background.png";
static const char emblem_src[] =
    "L:/usr/local/share/lvgl_data/boot_theme_c/emblem.png";

static struct {
    lv_obj_t *root, *background, *icon, *brand, *welcome, *dots[3];
    lv_obj_t *diagnostics;
    lv_timer_t *timer;
    uint32_t start_tick;
    bool background_ready, icon_ready, selftest_covered, first_drawn;
    bool managed, startup_ready, revealing, failed;
    uint32_t reveal_elapsed;
    uint32_t next_dot_round;
    unsigned ready_dot_rounds;
    bool dot_round_seen;
} g_intro;
static bool g_failed_create;

static float progress(uint32_t elapsed, uint32_t start, uint32_t duration)
{
    if (elapsed <= start) return 0.0f;
    if (elapsed - start >= duration) return 1.0f;
    return (float)(elapsed - start) / (float)duration;
}

static float ease(float p)
{
    if (p < 0.5f) return 4.0f * p * p * p;
    float q = -2.0f * p + 2.0f;
    return 1.0f - q * q * q / 2.0f;
}

/* Monotonic settling: fast departure, soft arrival, no overshoot or spring timer. */
static void settle_y(lv_obj_t *obj, lv_coord_t base, unsigned distance,
                     float p)
{
    float remaining = 1.0f - p;
    lv_coord_t y = base + (lv_coord_t)(distance * remaining * remaining * remaining + 0.5f);
    if (lv_obj_get_y(obj) != y) lv_obj_set_y(obj, y);
}

static lv_opa_t opacity(float value)
{
    return (lv_opa_t)(255.0f * value + 0.5f);
}

static void image_opacity(lv_obj_t *obj, lv_opa_t value)
{
    if (lv_obj_get_style_img_opa(obj, 0) != value)
        lv_obj_set_style_img_opa(obj, value, 0);
}

static void text_opacity(lv_obj_t *obj, lv_opa_t value)
{
    if (lv_obj_get_style_text_opa(obj, 0) != value)
        lv_obj_set_style_text_opa(obj, value, 0);
}

static void background_opacity(lv_obj_t *obj, lv_opa_t value)
{
    if (lv_obj_get_style_bg_opa(obj, 0) != value)
        lv_obj_set_style_bg_opa(obj, value, 0);
}

static void stop_timer(void)
{
    lv_timer_t *timer = g_intro.timer;
    g_intro.timer = NULL;
    if (timer) lv_timer_del(timer);
}

static void root_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != g_intro.root) return;
    stop_timer();
    if (g_intro.selftest_covered) ui_page_08_curr_set_covered(false);
    /* These two paths are private to this short-lived theme. Release their
     * decoded DMA cache entries even when the parent deletes us externally. */
    lv_img_cache_invalidate_src(background_src);
    lv_img_cache_invalidate_src(emblem_src);
    memset(&g_intro, 0, sizeof(g_intro));
}

static void root_drawn(lv_event_t *event)
{
    if (lv_event_get_target(event) == g_intro.root) g_intro.first_drawn = true;
}

static void finish(void)
{
    bool registered = ui_manager_get_current_page() == UI_PAGE_BOOT_ANIM;
    stop_timer();
    if (registered) ui_manager_switch(UI_PAGE_BOOT);
    else ui_page_00_boot_anim_destroy();
}

static void create_failed(void)
{
    ui_page_00_boot_anim_destroy();
    /* The manager has not committed its current page while create is on the
     * stack. Resolve a registered BOOT_ANIM fallback in the outer loop. */
    g_failed_create = true;
}

void ui_page_00_boot_anim_poll(void)
{
    if (!g_failed_create) return;
    g_failed_create = false;
    if (ui_manager_get_current_page() == UI_PAGE_BOOT_ANIM)
        ui_manager_switch(UI_PAGE_BOOT);
}

static void apply_elapsed(uint32_t elapsed)
{
    uint32_t reveal = g_intro.managed ?
        (g_intro.revealing ? g_intro.reveal_elapsed : UINT32_MAX) : INTRO_REVEAL_MS;
    float foreground = 1.0f - ease(progress(elapsed, reveal, INTRO_FADE_MS));
    float entrance = progress(elapsed, 150U, 800U);
    float icon = ease(entrance);
    float brand = icon *
                  (1.0f - ease(progress(elapsed, 2700U, 600U)));
    float welcome = ease(progress(elapsed, 3450U, 700U));

    settle_y(g_intro.icon, 82, 6U, entrance);
    settle_y(g_intro.brand, 219, 4U, entrance);
    settle_y(g_intro.welcome, 219, 4U, progress(elapsed, 3450U, 700U));

    if (g_intro.background_ready)
        image_opacity(g_intro.background, opacity(foreground));
    else background_opacity(g_intro.root, opacity(foreground));
    if (g_intro.icon_ready) image_opacity(g_intro.icon, opacity(icon * foreground));
    text_opacity(g_intro.brand, opacity(brand * foreground));
    text_opacity(g_intro.welcome, opacity(welcome * foreground));

    for (unsigned i = 0; i < 3; ++i) {
        float visible = ease(progress(elapsed, DOT_START_MS + i * 120U, 450U));
        uint32_t phase = elapsed < DOT_START_MS ? 0U :
            (elapsed - DOT_START_MS + DOT_PERIOD_MS - i * 160U) % DOT_PERIOD_MS;
        /* Quicker lift, slower landing, then rest. Both joins have zero slope. */
        float jump = phase < 660U ?
            (phase < 240U ? ease((float)phase / 240.0f) :
                           1.0f - ease((float)(phase - 240U) / 420.0f)) : 0.0f;
        lv_coord_t y = 296 - (lv_coord_t)(6.0f * jump + 0.5f);
        if (lv_obj_get_y(g_intro.dots[i]) != y) lv_obj_set_y(g_intro.dots[i], y);
        background_opacity(g_intro.dots[i],
            opacity(visible * (0.40f + 0.60f * jump) * foreground));
    }
}

void ui_page_00_boot_anim_adopt_elapsed(uint32_t elapsed_ms)
{
    if (!g_intro.root || g_intro.failed) return;
    g_intro.start_tick = lv_tick_get() - elapsed_ms;
    apply_elapsed(elapsed_ms);
}

void ui_page_00_boot_anim_set_startup_ready(bool ready)
{
    if (!g_intro.root || g_intro.failed) return;
    g_intro.managed = true;
    if (ready && !g_intro.startup_ready) {
        uint32_t elapsed = lv_tick_elaps(g_intro.start_tick);
        uint32_t rounds = elapsed <= DOT_START_MS ? 0U :
            (elapsed - DOT_START_MS + DOT_PERIOD_MS - 1U) / DOT_PERIOD_MS;
        g_intro.next_dot_round = DOT_START_MS + rounds * DOT_PERIOD_MS;
        g_intro.ready_dot_rounds = 0;
        g_intro.dot_round_seen = false;
    }
    g_intro.startup_ready = ready;
}

static void observe_ready_dots(uint32_t elapsed)
{
    if (!g_intro.startup_ready || g_intro.ready_dot_rounds >= READY_DOT_ROUNDS ||
        elapsed < g_intro.next_dot_round) return;
    uint32_t phase = elapsed - g_intro.next_dot_round;
    if (phase < 240U) g_intro.dot_round_seen = true;
    if (phase < DOT_LANDED_MS) return;
    /* A delayed timer must not count three unseen cycles as three animations. */
    if (g_intro.dot_round_seen) ++g_intro.ready_dot_rounds;
    g_intro.dot_round_seen = false;
    g_intro.next_dot_round = DOT_START_MS +
        ((elapsed - DOT_START_MS) / DOT_PERIOD_MS + 1U) * DOT_PERIOD_MS;
}

static void startup_diagnostics(lv_event_t *event)
{
    LV_UNUSED(event);
    ui_page_00_boot_anim_destroy();
    ui_manager_switch(UI_PAGE_SENSOR);
}

void ui_page_00_boot_anim_set_startup_error(bool diagnostics_available)
{
    if (!g_intro.root) return;
    if (!g_intro.failed) {
        g_intro.managed = true;
        g_intro.revealing = false;
        apply_elapsed(INTRO_MIN_REVEAL_MS);
        lv_label_set_text_static(g_intro.welcome, "STARTUP ERROR");
        lv_label_set_text_static(g_intro.brand, "RESTART DEVICE");
        lv_obj_set_style_text_font(g_intro.brand, LV_FONT_DEFAULT, 0);
        lv_obj_set_y(g_intro.brand, 305);
        text_opacity(g_intro.brand, LV_OPA_COVER);
        for (unsigned i = 0; i < 3; ++i)
            background_opacity(g_intro.dots[i], LV_OPA_TRANSP);
        g_intro.failed = true;
        stop_timer();
    }
    if (!diagnostics_available || g_intro.diagnostics) return;
    g_intro.diagnostics = lv_btn_create(g_intro.root);
    if (!g_intro.diagnostics) return;
    text_opacity(g_intro.brand, LV_OPA_TRANSP);
    lv_obj_remove_style_all(g_intro.diagnostics);
    lv_obj_set_pos(g_intro.diagnostics, 480, 288);
    lv_obj_set_size(g_intro.diagnostics, 320, 56);
    lv_obj_set_style_bg_color(g_intro.diagnostics, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_intro.diagnostics, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_intro.diagnostics, 12, 0);
    lv_obj_set_style_border_width(g_intro.diagnostics, 1, 0);
    lv_obj_set_style_border_color(g_intro.diagnostics, lv_color_hex(0xCBD5DC), 0);
    lv_obj_add_event_cb(g_intro.diagnostics, startup_diagnostics, LV_EVENT_CLICKED, NULL);
    lv_obj_t *text = lv_label_create(g_intro.diagnostics);
    if (text) {
        lv_label_set_text_static(text, "DIAGNOSTICS");
        lv_obj_set_style_text_font(text, &lv_font_instrument_sans_medium_40, 0);
        lv_obj_set_style_text_color(text, lv_color_hex(0x74818A), 0);
        lv_obj_center(text);
    }
}

static void timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    ui_page_t page = ui_manager_get_current_page();
    if (page != UI_PAGE_BOOT && page != UI_PAGE_BOOT_ANIM) {
        ui_page_00_boot_anim_destroy();
        return;
    }
    if (g_intro.failed) return;
    uint32_t elapsed = lv_tick_elaps(g_intro.start_tick);
    /* First draw skips the fully obscured self-test background. Restore it
     * in the quiet hold after entrance (950ms), well before the exit fade.
     * Its one-time decode then cannot delay the first visible intro frame. */
    bool quiet = (elapsed >= 1000U && elapsed < 2700U) ||
        (elapsed >= 4150U && elapsed < 4350U) ||
        (elapsed >= 4350U && (elapsed - 4350U) % DOT_PERIOD_MS >= 1000U);
    if (g_intro.selftest_covered && g_intro.first_drawn && quiet) {
        if (ui_page_08_curr_prepare_step()) {
            g_intro.selftest_covered = false;
            ui_page_08_curr_set_covered(false);
        }
    }
    if (g_intro.managed) observe_ready_dots(elapsed);
    if (g_intro.managed && g_intro.startup_ready && !g_intro.revealing &&
        !g_intro.selftest_covered &&
        g_intro.ready_dot_rounds >= READY_DOT_ROUNDS &&
        elapsed >= INTRO_MIN_REVEAL_MS) {
        g_intro.revealing = true;
        g_intro.reveal_elapsed = elapsed;
    }
    if ((!g_intro.managed && elapsed >= INTRO_REVEAL_MS + INTRO_FADE_MS) ||
        (g_intro.revealing && elapsed - g_intro.reveal_elapsed >= INTRO_FADE_MS)) {
        finish(); return;
    }
    apply_elapsed(elapsed);
}

static lv_obj_t *label(lv_obj_t *parent, const char *text,
                       const lv_font_t *font, int spacing)
{
    lv_obj_t *obj = lv_label_create(parent);
    if (!obj) return NULL;
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, 390, 219);
    lv_obj_set_width(obj, 500);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x74818A), 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(obj, spacing, 0);
    lv_obj_set_style_text_opa(obj, LV_OPA_TRANSP, 0);
    lv_label_set_text_static(obj, text);
    return obj;
}

void ui_page_00_boot_anim_create(lv_obj_t *parent)
{
    if (ui_page_00_boot_anim_is_active()) return;
    ui_page_00_boot_anim_destroy();
    g_failed_create = false;
    if (!parent) parent = lv_layer_top();
    g_intro.root = lv_obj_create(parent);
    if (!g_intro.root) { create_failed(); return; }
    lv_obj_remove_style_all(g_intro.root);
    lv_obj_set_size(g_intro.root, 1280, 400);
    lv_obj_set_pos(g_intro.root, 0, 0);
    lv_obj_clear_flag(g_intro.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_intro.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(g_intro.root, lv_color_hex(0xEDF3F6), 0);
    lv_obj_add_event_cb(g_intro.root, root_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(g_intro.root, root_drawn, LV_EVENT_DRAW_POST_END, NULL);
    if (ui_manager_get_current_page() == UI_PAGE_BOOT) {
        g_intro.selftest_covered = true;
        ui_page_08_curr_set_covered(true);
    }

    g_intro.background = lv_img_create(g_intro.root);
    g_intro.icon = lv_img_create(g_intro.root);
    if (!g_intro.background || !g_intro.icon) { create_failed(); return; }
    lv_img_set_src(g_intro.background, background_src);
    lv_img_set_src(g_intro.icon, emblem_src);
    lv_obj_set_pos(g_intro.icon, 592, 82);
    image_opacity(g_intro.icon, LV_OPA_TRANSP);
    /* Same predecode API used by the page manager. No per-frame file I/O,
     * rasterization, zoom variants or alpha offscreen object layers. */
    g_intro.background_ready = _lv_img_cache_open(background_src, lv_color_black(), 0) != NULL;
    g_intro.icon_ready = _lv_img_cache_open(emblem_src, lv_color_black(), 0) != NULL;
    if (!g_intro.background_ready) {
        lv_obj_add_flag(g_intro.background, LV_OBJ_FLAG_HIDDEN);
        background_opacity(g_intro.root, LV_OPA_COVER);
    }
    if (!g_intro.icon_ready) lv_obj_add_flag(g_intro.icon, LV_OBJ_FLAG_HIDDEN);

    g_intro.brand = label(g_intro.root, "UN260", &lv_font_instrument_sans_semibold_48, 1);
    g_intro.welcome = label(g_intro.root, "WELCOME", &lv_font_instrument_sans_medium_40, 5);
    if (!g_intro.brand || !g_intro.welcome) { create_failed(); return; }
    for (unsigned i = 0; i < 3; ++i) {
        g_intro.dots[i] = lv_obj_create(g_intro.root);
        if (!g_intro.dots[i]) { create_failed(); return; }
        lv_obj_remove_style_all(g_intro.dots[i]);
        lv_obj_set_size(g_intro.dots[i], 7, 7);
        lv_obj_set_pos(g_intro.dots[i], 619 + (int)i * 17, 296);
        lv_obj_set_style_radius(g_intro.dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(g_intro.dots[i], lv_color_hex(0xF85820), 0);
        background_opacity(g_intro.dots[i], LV_OPA_TRANSP);
        lv_obj_clear_flag(g_intro.dots[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    apply_elapsed(0U);
    lv_obj_update_layout(g_intro.root);
    lv_obj_move_foreground(g_intro.root);
    g_intro.start_tick = lv_tick_get();
    g_intro.timer = lv_timer_create(timer_cb, INTRO_TIMER_MS, NULL);
    if (!g_intro.timer) create_failed();
}

void ui_page_00_boot_anim_destroy(void)
{
    stop_timer();
    if (g_intro.root) lv_obj_del(g_intro.root); /* DELETE callback clears ownership. */
    memset(&g_intro, 0, sizeof(g_intro));
}

bool ui_page_00_boot_anim_is_active(void)
{
    return g_intro.root != NULL;
}

#endif /* UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C */
