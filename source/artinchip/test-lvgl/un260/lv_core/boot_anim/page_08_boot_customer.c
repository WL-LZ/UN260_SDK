#include "boot_theme_config.h"
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D && UI_BOOT_VARIANT == UI_BOOT_VARIANT_B
#include "un260/lv_core/page_08_boot.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/boot/boot_service.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
#include "boot_welcome_locale.h"
#include "boot_customer_art.h"
#include <stdlib.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_instrument_sans_medium_16);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_36);
LV_FONT_DECLARE(lv_font_open_runde_medium_48);

/* This page projects the five controller results. Its motion clock never
 * advances a self-test, supplies a result or delays an existing exit. */
#define ROW_X 786
#define ROW_Y 68
#define ROW_STEP 48
#define ROW_WIDTH 344
#define ROW_HEIGHT 46
#define HAND_WIDTH 200U
#define HAND_HEIGHT 220U
#define READY_QUIET_MS 2170U
#define READY_HOLD_MS 2350U
static const char *const names[5] = {
    UI_N_("Configuration"), UI_N_("Sensors"), UI_N_("Motor"),
    UI_N_("Electromagnet"), UI_N_("Image board")
};
enum { WAIT, CHECK, PASS, FAIL };
typedef struct {
    lv_obj_t *gray_clip, *active_clip, *gray, *active, *state;
    unsigned text_width;
} check_row_t;
typedef struct {
    lv_obj_t *image, *fallback;
    lv_img_dsc_t sampled;
    uint8_t *pixels;
    const lv_img_dsc_t *source;
    unsigned width, gesture;
    bool mirrored;
} hand_visual_t;
static struct {
    lv_obj_t *root, *background, *logo, *initialization, *system, *version, *powered;
    lv_obj_t *init_text, *system_text, *caption;
    lv_obj_t *ready_gray_clip, *ready_active_clip, *ready_gray, *ready_active;
    lv_obj_t *ready_gray_text, *ready_active_text;
    lv_img_dsc_t ready_pixels[2];
    uint8_t *ready_buffers[2];
    unsigned ready_width, ready_height;
    check_row_t rows[5];
    hand_visual_t hand;
    lv_timer_t *timer;
    uint32_t shown_at, done_at, active_at[5], language_generation;
    uint8_t phase[5], step, gesture;
    bool covered, done, failed, prepare_failed, art_acquired, ready_images;
} view;
static bool defer_create;

static float smooth(float p)
{
    if (p <= 0) return 0;
    if (p >= 1) return 1;
    return p * p * (3 - 2 * p);
}
static float span(uint32_t age, uint32_t start, uint32_t duration)
{
    return age <= start ? 0 : smooth((float)(age - start) / duration);
}
static void visible(lv_obj_t *obj, bool show)
{
    if (!obj) return;
    bool hidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    if (show && hidden) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else if (!show && !hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
static void text_opacity(lv_obj_t *obj, lv_opa_t opacity)
{
    if (obj && lv_obj_get_style_text_opa(obj, 0) != opacity)
        lv_obj_set_style_text_opa(obj, opacity, 0);
}
static void image_opacity(lv_obj_t *obj, lv_opa_t opacity)
{
    if (obj && lv_obj_get_style_img_opa(obj, 0) != opacity)
        lv_obj_set_style_img_opa(obj, opacity, 0);
}
static void text_color(lv_obj_t *obj, uint32_t color)
{
    if (obj && lv_obj_get_style_text_color(obj, 0).full != lv_color_hex(color).full)
        lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}
static void text(lv_obj_t *obj, const char *value)
{
    if (obj && strcmp(lv_label_get_text(obj), value)) lv_label_set_text(obj, value);
}
static lv_obj_t *plain(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    if (!obj) return NULL;
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
        LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    return obj;
}
static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w,
    const char *value, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    if (!obj) return NULL;
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    lv_obj_set_style_text_font(obj, ui_message_font(font), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    lv_label_set_text(obj, value);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}
static const lv_img_dsc_t *art(unsigned id)
{
    return view.art_acquired ? boot_customer_art_image(id) : NULL;
}
static lv_obj_t *picture(lv_obj_t *parent, unsigned id, int x, int y)
{
    const lv_img_dsc_t *source = art(id);
    if (!source) return NULL;
    lv_obj_t *obj = lv_img_create(parent);
    if (!obj) return NULL;
    lv_obj_remove_style_all(obj);
    lv_img_set_src(obj, source);
    lv_obj_set_pos(obj, x, y);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *placed_picture(lv_obj_t *parent, unsigned id, int fallback_x, int fallback_y)
{
    lv_coord_t x = fallback_x, y = fallback_y;
    if (view.art_acquired) boot_customer_art_position(id, &x, &y);
    return picture(parent, id, x, y);
}
static void stop(void)
{
    lv_timer_t *timer = view.timer;
    view.timer = NULL;
    if (timer) lv_timer_del(timer);
}
static void deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != view.root) return;
    stop();
    lv_img_cache_invalidate_src(&view.hand.sampled);
    for (unsigned i = 0; i < 2; ++i) {
        lv_img_cache_invalidate_src(&view.ready_pixels[i]);
        free(view.ready_buffers[i]);
    }
    free(view.hand.pixels);
    view.hand.pixels = NULL;
    if (view.art_acquired) boot_customer_art_release();
    memset(&view, 0, sizeof(view));
}
static void set_clip(lv_obj_t *clip, lv_obj_t *obj, int x, int y,
    int width, int height, int image_x, int image_y)
{
    visible(clip, width > 0 && height > 0);
    if (!clip || !obj || width <= 0 || height <= 0) return;
    if (lv_obj_get_x(clip) != x || lv_obj_get_y(clip) != y) lv_obj_set_pos(clip, x, y);
    if (lv_obj_get_width(clip) != width || lv_obj_get_height(clip) != height)
        lv_obj_set_size(clip, width, height);
    if (lv_obj_get_x(obj) != image_x - x || lv_obj_get_y(obj) != image_y - y)
        lv_obj_set_pos(obj, image_x - x, image_y - y);
}

/* Page-owned fixed-height hand transform. Source alpha is straight BGRA;
 * integer area sampling retains fingers at the 1-pixel flip edge. The extra
 * transparent column avoids LVGL 8's one-column image tiling boundary. */
static void hand_render(unsigned gesture, unsigned width, bool mirrored)
{
    hand_visual_t *hand = &view.hand;
    const lv_img_dsc_t *source = art(BOOT_CUSTOMER_ART_HAND_1 + gesture - 1);
    bool usable = source && source->header.cf == LV_IMG_CF_TRUE_COLOR_ALPHA &&
        source->header.w == HAND_WIDTH && source->header.h == HAND_HEIGHT && hand->pixels;
    visible(hand->image, usable);
    visible(hand->fallback, !usable);
    if (!usable) {
        if (hand->gesture != gesture) lv_label_set_text_fmt(hand->fallback, "%u", gesture);
        hand->gesture = gesture;
        return;
    }
    if (hand->source == source && hand->width == width && hand->mirrored == mirrored) return;
    unsigned stride = width < 2 ? 2 : width;
    for (unsigned y = 0; y < HAND_HEIGHT; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            unsigned sample_x = mirrored ? width - 1 - x : x;
            unsigned left = sample_x * HAND_WIDTH, right = (sample_x + 1) * HAND_WIDTH;
            unsigned alpha = 0, color[3] = {0, 0, 0};
            unsigned first = left / width, last = (right - 1) / width;
            for (unsigned k = first; k <= last; ++k) {
                unsigned begin = left > k * width ? left : k * width;
                unsigned end = right < (k + 1) * width ? right : (k + 1) * width;
                const uint8_t *pixel = source->data + ((size_t)y * HAND_WIDTH + k) * 4;
                unsigned weight = pixel[3] * (end - begin);
                alpha += weight;
                for (unsigned c = 0; c < 3; ++c) color[c] += pixel[c] * weight;
            }
            uint8_t *pixel = hand->pixels + ((size_t)y * stride + x) * 4;
            pixel[3] = (uint8_t)((alpha + HAND_WIDTH / 2) / HAND_WIDTH);
            for (unsigned c = 0; c < 3; ++c)
                pixel[c] = alpha ? (uint8_t)((color[c] + alpha / 2) / alpha) : 0;
        }
        if (stride != width) memset(hand->pixels + ((size_t)y * stride + width) * 4, 0, 4);
    }
    lv_img_cache_invalidate_src(&hand->sampled);
    hand->sampled.header.w = stride;
    hand->sampled.data_size = stride * HAND_HEIGHT * 4;
    lv_img_set_src(hand->image, &hand->sampled);
    static const int top[5] = {64, 61, 65, 62, 64};
    lv_coord_t source_x = 540, source_y = top[gesture - 1];
    boot_customer_art_position(BOOT_CUSTOMER_ART_HAND_1 + gesture - 1, &source_x, &source_y);
    lv_obj_set_pos(hand->image, source_x + HAND_WIDTH / 2 - (int)width / 2, source_y);
    hand->source = source;
    hand->width = width;
    hand->mirrored = mirrored;
    hand->gesture = gesture;
}
static void hand_motion(unsigned gesture, uint32_t age, bool moving)
{
    unsigned width = HAND_WIDTH;
    bool mirrored = false;
    if (moving) {
        unsigned phase = age % 1800U, part = phase % 450U;
        float p = (float)part / 450;
        if ((phase / 450U) % 2U == 0) p = 1 - p;
        width = 1U + (unsigned)(199 * smooth(p) + .5f);
        mirrored = phase >= 450U && phase < 1350U;
    }
    hand_render(gesture, width, mirrored);
}
static const char *state_text(unsigned state)
{
    return state == FAIL ? ui_tr("Not passed") : state == PASS ? ui_tr("Ready") :
        state == CHECK ? ui_tr("Checking") : ui_tr("Waiting");
}
static void refresh_language(void)
{
    uint32_t generation = ui_lang_generation();
    if (generation == view.language_generation) return;
    view.language_generation = generation;
    lv_obj_t *titles[] = {view.init_text, view.system_text, view.ready_gray_text, view.ready_active_text};
    const char *values[] = {ui_tr("Initialization"), ui_tr("System"), ui_tr("Ready"), ui_tr("Ready")};
    for (unsigned i = 0; i < 4; ++i) if (titles[i]) {
        lv_obj_set_style_text_font(titles[i], ui_message_font(&lv_font_open_runde_medium_48), 0);
        text(titles[i], values[i]);
    }
    for (unsigned i = 0; i < 5; ++i) {
        check_row_t *row = &view.rows[i];
        const lv_font_t *font = ui_message_font(&lv_font_instrument_sans_medium_36);
        lv_obj_set_style_text_font(row->gray, font, 0);
        lv_obj_set_style_text_font(row->active, font, 0);
        text(row->gray, ui_tr(names[i]));
        text(row->active, ui_tr(names[i]));
        lv_point_t extent;
        lv_txt_get_size(&extent, ui_tr(names[i]), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        row->text_width = extent.x > ROW_WIDTH ? ROW_WIDTH : (unsigned)extent.x;
        int center_y = ROW_Y + i * ROW_STEP + (ROW_HEIGHT - font->line_height) / 2;
        lv_obj_set_y(row->gray, center_y - ROW_Y - i * ROW_STEP);
        lv_obj_set_y(row->active, center_y - ROW_Y - i * ROW_STEP);
        const lv_font_t *small = ui_message_font(&lv_font_instrument_sans_medium_16);
        lv_obj_set_style_text_font(row->state, small, 0);
        lv_obj_set_y(row->state, ROW_Y + i * ROW_STEP + (ROW_HEIGHT - small->line_height) / 2);
        text(row->state, state_text(view.phase[i]));
    }
    if (view.caption) lv_obj_set_style_text_font(view.caption, ui_message_font(&lv_font_instrument_sans_medium_16), 0);
}
static void row_render(unsigned index, unsigned state, uint32_t age, bool show)
{
    check_row_t *row = &view.rows[index];
    visible(row->state, show);
    if (!show) { visible(row->gray_clip, false); visible(row->active_clip, false); return; }
    unsigned cut = state == CHECK ? (unsigned)(row->text_width * span(age % 1800U, 0, 1400U)) : 0;
    const lv_font_t *font = lv_obj_get_style_text_font(row->gray, 0);
    int y = ROW_Y + index * ROW_STEP;
    int text_y = y + (ROW_HEIGHT - font->line_height) / 2;
    set_clip(row->gray_clip, row->gray, ROW_X + cut, y, ROW_WIDTH - cut, ROW_HEIGHT, ROW_X, text_y);
    set_clip(row->active_clip, row->active, ROW_X, y, cut, ROW_HEIGHT, ROW_X, text_y);
    text_color(row->gray, state == FAIL ? 0xB53622 : 0x363632);
    text_opacity(row->gray, state == WAIT ? 120 : 255);
    text_color(row->state, state == FAIL ? 0xB53622 : state == PASS ? 0x397C21 : state == CHECK ? 0xE86E31 : 0x898B86);
}
/* Ready uses the catalog's large A8 title. Two local buffers are allocated
 * once; the final source size is never enlarged. Avoid per-zoom GE variants. */
static void ready_sample(unsigned width, unsigned height)
{
    if (view.ready_width == width && view.ready_height == height) return;
    const lv_img_dsc_t *source = art(BOOT_CUSTOMER_ART_READY_GRAY);
    const lv_img_dsc_t *orange = art(BOOT_CUSTOMER_ART_READY_ORANGE);
    unsigned sw = source->header.w, sh = source->header.h;
    for (unsigned y = 0; y < height; ++y) {
        int sy = (int)((2 * y + 1) * sh * 256 / (2 * height)) - 128;
        if (sy < 0) sy = 0;
        if (sy > (int)(sh - 1) * 256) sy = (sh - 1) * 256;
        unsigned y0 = sy / 256, y1 = y0 + 1 < sh ? y0 + 1 : y0, fy = sy % 256;
        for (unsigned x = 0; x < width; ++x) {
            int sx = (int)((2 * x + 1) * sw * 256 / (2 * width)) - 128;
            if (sx < 0) sx = 0;
            if (sx > (int)(sw - 1) * 256) sx = (sw - 1) * 256;
            unsigned x0 = sx / 256, x1 = x0 + 1 < sw ? x0 + 1 : x0, fx = sx % 256;
            unsigned alpha = source->data[(y0 * sw + x0) * 4 + 3] * (256 - fx) * (256 - fy) +
                source->data[(y0 * sw + x1) * 4 + 3] * fx * (256 - fy) +
                source->data[(y1 * sw + x0) * 4 + 3] * (256 - fx) * fy +
                source->data[(y1 * sw + x1) * 4 + 3] * fx * fy;
            size_t at = ((size_t)y * width + x) * 4;
            for (unsigned c = 0; c < 3; ++c) {
                view.ready_buffers[0][at + c] = source->data[c];
                view.ready_buffers[1][at + c] = orange->data[c];
            }
            view.ready_buffers[0][at + 3] = view.ready_buffers[1][at + 3] = (alpha + 32768U) >> 16;
        }
    }
    for (unsigned i = 0; i < 2; ++i) {
        lv_img_cache_invalidate_src(&view.ready_pixels[i]);
        view.ready_pixels[i].header.w = width;
        view.ready_pixels[i].header.h = height;
        view.ready_pixels[i].data_size = width * height * 4;
    }
    lv_img_set_src(view.ready_gray, &view.ready_pixels[0]);
    lv_img_set_src(view.ready_active, &view.ready_pixels[1]);
    view.ready_width = width; view.ready_height = height;
}
static void ready_render(uint32_t age, bool show)
{
    visible(view.system, show);
    visible(view.system_text, show && !view.system);
    visible(view.initialization, !show);
    visible(view.init_text, !show && !view.initialization);
    if (!show) {
        visible(view.ready_gray_clip, false); visible(view.ready_active_clip, false);
        return;
    }
    float grow = span(age, 350, 1100);
    float color = span(age, 650, 1200);
    lv_opa_t opacity = (lv_opa_t)(255 * span(age, 0, 180));
    int width, height, x, y, cut;
    if (view.ready_images) {
        const lv_img_dsc_t *source = art(BOOT_CUSTOMER_ART_READY_GRAY);
        unsigned scale = 171U + (unsigned)(85 * grow + .5f);
        width = source->header.w * scale / 256;
        height = source->header.h * scale / 256;
        ready_sample(width, height);
        lv_coord_t source_x = 786, source_y = 181;
        boot_customer_art_position(BOOT_CUSTOMER_ART_READY_GRAY, &source_x, &source_y);
        x = source_x + source->header.w / 2 - width / 2;
        y = source_y - 8 + (int)(8 * grow + .5f);
        cut = (int)(width * color + .5f);
        image_opacity(view.ready_gray, opacity); image_opacity(view.ready_active, opacity);
        set_clip(view.ready_gray_clip, view.ready_gray, x + cut, y, width - cut, height, x, y);
        set_clip(view.ready_active_clip, view.ready_active, x, y, cut, height, x, y);
    } else {
        width = 360; height = 76; x = 800; y = 173 + (int)(8 * grow + .5f);
        cut = (int)(width * color + .5f);
        text_opacity(view.ready_gray_text, opacity); text_opacity(view.ready_active_text, opacity);
        set_clip(view.ready_gray_clip, view.ready_gray_text, x + cut, y, width - cut, height, x, y);
        set_clip(view.ready_active_clip, view.ready_active_text, x, y, cut, height, x, y);
    }
}
static void apply(uint32_t now)
{
    if (!view.root || view.covered || view.step || view.prepare_failed) return;
    refresh_language();
    boot_snapshot_t snapshot;
    boot_service_snapshot(&snapshot);
    bool failed = snapshot.stage == BOOT_STAGE_FAIL;
    unsigned passed = 0;
    for (unsigned i = 0; i < 5; ++i) {
        unsigned state = snapshot.items[i].received ? (snapshot.items[i].result == 1 ? PASS : FAIL) :
            snapshot.connected && snapshot.requested_count == i + 1 ? CHECK : WAIT;
        if (i == 0 && !snapshot.items[i].received) state = CHECK;
        if (snapshot.stage == BOOT_STAGE_FAIL && state == CHECK) state = FAIL;
        if (state == FAIL) failed = true;
        if (state == PASS) ++passed;
        if (view.phase[i] != state) {
            view.phase[i] = state; view.active_at[i] = now;
            text(view.rows[i].state, state_text(state));
        }
    }
    bool ready = snapshot.stage == BOOT_STAGE_DONE && passed == 5 && !failed;
    if (ready && !view.done) view.done_at = now;
    view.done = ready;
    view.failed = failed;
    unsigned index = snapshot.requested_count ? snapshot.requested_count - 1 : 0;
    if (index > 4) index = 4;
    view.gesture = ready ? 1 : 5 - index;
    hand_motion(view.gesture, now - view.active_at[index], !ready && !failed && view.phase[index] == CHECK);
    for (unsigned i = 0; i < 5; ++i) row_render(i, view.phase[i], now - view.active_at[i], !ready);
    text(view.caption, failed ? ui_tr("Check the reported fault before continuing.") : "");
    visible(view.caption, failed);
    visible(view.powered, !failed);
    ready_render(ready ? now - view.done_at : 0, ready);
}
static void tick(lv_timer_t *timer)
{
    (void)timer;
    if (ui_manager_get_current_page() != UI_PAGE_BOOT) { stop(); return; }
    apply(lv_tick_get());
}
static bool make_row(unsigned index)
{
    check_row_t *row = &view.rows[index];
    int y = ROW_Y + index * ROW_STEP;
    row->gray_clip = plain(view.root, ROW_X, y, ROW_WIDTH, ROW_HEIGHT);
    row->active_clip = plain(view.root, ROW_X, y, ROW_WIDTH, ROW_HEIGHT);
    if (!row->gray_clip || !row->active_clip) return false;
    row->gray = label(row->gray_clip, 0, 0, ROW_WIDTH, ui_tr(names[index]), &lv_font_instrument_sans_medium_36, 0x363632);
    row->active = label(row->active_clip, 0, 0, ROW_WIDTH, ui_tr(names[index]), &lv_font_instrument_sans_medium_36, 0xE86E31);
    row->state = label(view.root, 1144, y, 122, ui_tr("Waiting"), &lv_font_instrument_sans_medium_16, 0x898B86);
    if (row->state) lv_obj_set_style_text_align(row->state, LV_TEXT_ALIGN_RIGHT, 0);
    view.phase[index] = 255;
    return row->gray && row->active && row->state;
}
static bool make_scene(void)
{
    unsigned language = boot_welcome_initial_index(ui_lang_get());
    view.art_acquired = boot_customer_art_acquire(boot_welcome_tags[language]);
    view.background = placed_picture(view.root, BOOT_CUSTOMER_ART_BG, 0, 0);
    view.logo = placed_picture(view.root, BOOT_CUSTOMER_ART_LOGO_SMALL, 214, 39);
    view.version = placed_picture(view.root, BOOT_CUSTOMER_ART_VERSION, 1130, 16);
    view.powered = placed_picture(view.root, BOOT_CUSTOMER_ART_POWERED, 980, 358);
    view.init_text = label(view.root, 112, 171, 400, ui_tr("Initialization"), &lv_font_open_runde_medium_48, 0x353734);
    view.system_text = label(view.root, 182, 171, 330, ui_tr("System"), &lv_font_open_runde_medium_48, 0x397C21);
    view.initialization = placed_picture(view.root, BOOT_CUSTOMER_ART_INIT, 112, 172);
    view.system = placed_picture(view.root, BOOT_CUSTOMER_ART_SYSTEM, 182, 173);
    view.hand.fallback = label(view.root, 540, 130, 200, "5", &lv_font_open_runde_medium_48, 0x627587);
    if (view.hand.fallback) lv_obj_set_style_text_align(view.hand.fallback, LV_TEXT_ALIGN_CENTER, 0);
    view.hand.image = lv_img_create(view.root);
    if (view.hand.image) {
        lv_obj_remove_style_all(view.hand.image);
        lv_obj_clear_flag(view.hand.image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        view.hand.pixels = malloc(HAND_WIDTH * HAND_HEIGHT * 4U);
        view.hand.sampled.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
        view.hand.sampled.header.h = HAND_HEIGHT;
        view.hand.sampled.data = view.hand.pixels;
    }
    return view.init_text && view.system_text && view.hand.fallback;
}
static bool make_ready(void)
{
    view.ready_gray_clip = plain(view.root, 760, 148, 460, 160);
    view.ready_active_clip = plain(view.root, 760, 148, 460, 160);
    if (!view.ready_gray_clip || !view.ready_active_clip) return false;
    view.ready_gray_text = label(view.ready_gray_clip, 0, 0, 360, ui_tr("Ready"), &lv_font_open_runde_medium_48, 0x363632);
    view.ready_active_text = label(view.ready_active_clip, 0, 0, 360, ui_tr("Ready"), &lv_font_open_runde_medium_48, 0xE86E31);
    view.ready_gray = picture(view.ready_gray_clip, BOOT_CUSTOMER_ART_READY_GRAY, 0, 0);
    view.ready_active = picture(view.ready_active_clip, BOOT_CUSTOMER_ART_READY_ORANGE, 0, 0);
    view.ready_images = view.ready_gray && view.ready_active;
    if (view.ready_images) {
        const lv_img_dsc_t *source = art(BOOT_CUSTOMER_ART_READY_GRAY);
        for (unsigned i = 0; i < 2; ++i) {
            view.ready_buffers[i] = malloc(source->data_size);
            view.ready_pixels[i].header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            view.ready_pixels[i].data = view.ready_buffers[i];
        }
        view.ready_images = view.ready_buffers[0] && view.ready_buffers[1];
    }
    visible(view.ready_gray, view.ready_images); visible(view.ready_active, view.ready_images);
    visible(view.ready_gray_text, !view.ready_images); visible(view.ready_active_text, !view.ready_images);
    view.caption = label(view.root, 786, 328, 450, "", &lv_font_instrument_sans_medium_16, 0xB53622);
    visible(view.caption, false);
    ready_render(0, false);
    return view.ready_gray_text && view.ready_active_text && view.caption;
}
static bool preparation_failed(void)
{
    view.prepare_failed = true; view.step = 0; stop(); return false;
}
bool ui_page_08_curr_prepare_failed(void) { return view.prepare_failed; }
bool ui_page_08_curr_prepare_step(void)
{
    if (view.prepare_failed) return false;
    if (!view.root || !view.step) return true;
    unsigned step = view.step++;
    if (step == 1) {
        if (!make_scene()) return preparation_failed();
    } else if (step <= 6) {
        if (!make_row(step - 2)) return preparation_failed();
    } else if (step == 7) {
        if (!make_ready()) return preparation_failed();
        view.timer = lv_timer_create(tick, LV_DISP_DEF_REFR_PERIOD, NULL);
        if (view.timer) lv_timer_pause(view.timer);
    } else {
        refresh_language(); lv_obj_update_layout(view.root);
        view.step = 0; view.shown_at = lv_tick_get();
        if (!view.covered) {
            apply(view.shown_at);
            if (view.timer) lv_timer_resume(view.timer);
        }
        return true;
    }
    return false;
}
void ui_page_08_curr_defer_next_create(void) { defer_create = true; }
void ui_page_08_curr_create(lv_obj_t *parent)
{
    if (view.root) return;
    bool deferred = defer_create; defer_create = false;
    memset(&view, 0, sizeof(view));
    if (!parent) parent = lv_scr_act();
    view.root = plain(parent, 0, 0, 1280, 400);
    if (!view.root) { view.prepare_failed = true; return; }
    lv_obj_set_style_bg_color(view.root, lv_color_hex(0xF2F5F1), 0);
    lv_obj_set_style_bg_opa(view.root, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(view.root, deleted, LV_EVENT_DELETE, NULL);
    view.language_generation = UINT32_MAX; view.step = 1;
    if (!deferred) while (!ui_page_08_curr_prepare_step() && !view.prepare_failed) { }
}
void ui_page_08_curr_set_covered(bool covered)
{
    if (!view.root) return;
    view.covered = covered;
    if (covered) {
        if (view.timer) lv_timer_pause(view.timer);
        visible(view.root, false);
        return;
    }
    while (!ui_page_08_curr_prepare_step() && !view.prepare_failed) { }
    if (view.prepare_failed) return;
    visible(view.root, true);
    view.shown_at = lv_tick_get();
    /* Covered time is not presentation time; completed hardware remains done. */
    if (view.done) view.done_at = view.shown_at;
    for (unsigned i = 0; i < 5; ++i) view.active_at[i] = view.shown_at;
    apply(view.shown_at);
    if (view.timer) lv_timer_resume(view.timer);
}
void ui_page_08_curr_destroy(void)
{
    defer_create = false; stop();
    if (view.root) lv_obj_del(view.root);
    memset(&view, 0, sizeof(view));
}
bool ui_page_08_curr_visual_is_quiet(void)
{
    apply(lv_tick_get());
    return !view.root || (!view.covered && view.done && !view.failed && lv_tick_elaps(view.done_at) >= READY_QUIET_MS);
}
bool ui_page_08_curr_ready_hold_complete(void)
{
    apply(lv_tick_get());
    return !view.root || (!view.covered && view.done && !view.failed && lv_tick_elaps(view.done_at) >= READY_HOLD_MS);
}
/* The already prepared Main/Pure page replaces this scene directly. No extra
 * overlay, input hold, image decoding or additional exit timer is introduced. */
void ui_page_08_curr_start_handoff(void) { }
/* Legacy notifications refresh the authoritative snapshot, never caller state. */
void boot_progress_set(uint8_t percent) { (void)percent; apply(lv_tick_get()); }
void boot_progress_reset(void) { apply(lv_tick_get()); }
void boot_selftest_list_reset(void) { apply(lv_tick_get()); }
void boot_selftest_list_sync_step(uint8_t step) { (void)step; apply(lv_tick_get()); }
void boot_selftest_list_set_result(uint8_t index, uint8_t result)
{ (void)index; (void)result; apply(lv_tick_get()); }
void boot_selftest_list_finish(void) { apply(lv_tick_get()); }
#endif
