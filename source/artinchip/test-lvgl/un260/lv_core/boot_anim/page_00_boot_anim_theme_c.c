#include "un260/lv_core/page_00_boot_anim.h"

#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C

#include <string.h>
#include "un260/font/manrope_fonts.h"
#include "un260/lv_core/lv_page_manager.h"

/* Theme B's 7.875s hold + 320ms cubic dissolve, without its terminal layer. */
#define INTRO_REVEAL_MS 7875U
#define INTRO_FADE_MS 320U
#define INTRO_TIMER_MS 16U
#define DOT_START_MS 4350U
#define DOT_PERIOD_MS 1500U

static const char background_src[] =
    "L:/usr/local/share/lvgl_data/boot_theme_c/background.png";
static const char emblem_src[] =
    "L:/usr/local/share/lvgl_data/boot_theme_c/emblem.png";

static struct {
    lv_obj_t *root, *background, *icon, *brand, *welcome, *dots[3];
    lv_timer_t *timer;
    uint32_t start_tick;
    bool background_ready, icon_ready;
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
    /* These two paths are private to this short-lived theme. Release their
     * decoded DMA cache entries even when the parent deletes us externally. */
    lv_img_cache_invalidate_src(background_src);
    lv_img_cache_invalidate_src(emblem_src);
    memset(&g_intro, 0, sizeof(g_intro));
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
    float foreground = 1.0f - ease(progress(elapsed, INTRO_REVEAL_MS, INTRO_FADE_MS));
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

static void timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    ui_page_t page = ui_manager_get_current_page();
    if (page != UI_PAGE_BOOT && page != UI_PAGE_BOOT_ANIM) {
        ui_page_00_boot_anim_destroy();
        return;
    }
    uint32_t elapsed = lv_tick_elaps(g_intro.start_tick);
    if (elapsed >= INTRO_REVEAL_MS + INTRO_FADE_MS) { finish(); return; }
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
