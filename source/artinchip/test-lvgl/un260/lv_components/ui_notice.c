#include "ui_notice.h"
#include "lvgl/lvgl.h"
#if LV_USE_ARABIC_PERSIAN_CHARS
#include "lvgl/src/misc/lv_txt_ap.h"
#endif
#include "un260/lv_system/ui_i18n.h"
#include "un260/font/ui_message_font.h"
#include <stdio.h>
#include <string.h>

/* Apple 01 reference at native 1280x400: one screen-space owner. */
#define NOTICE_WIDTH 360
#define NOTICE_HEIGHT 72
#define NOTICE_Y 12
#define NOTICE_ENTRY_Y (NOTICE_Y - 24)
#define NOTICE_EXIT_Y (NOTICE_Y - 8)
#define NOTICE_ENTER_MS 380
#define NOTICE_EXIT_MS 180
#define NOTICE_SPINNER_MS 40
#define NOTICE_TEXT_X 60
#define NOTICE_TEXT_WIDTH 248
#define NOTICE_ICON_X 32
#define NOTICE_SHADOW_EXTENT 20

static uint32_t dialog_hold_count;
static bool enabled = true;

static struct {
    ui_notice_state_t state;
    ui_notice_item_t shown;
    lv_obj_t *object;
    lv_timer_t *timer;
    const lv_font_t *title_font;
    char title[UI_NOTICE_TITLE_CAPACITY * 2];
    char detail[UI_NOTICE_DETAIL_CAPACITY * 2];
    uint32_t suspended;
    uint32_t last_tick;
    uint16_t angle;
    lv_coord_t animation_y;
    lv_coord_t title_height, detail_height, copy_y, height;
    lv_opa_t opacity, animation_opacity;
    bool initialized;
    bool visible;
    bool leaving;
    bool animating;
    bool held;
    bool deleting;
} notice;

static void present(bool animate);
static void service_start(void);

static lv_opa_t opacity(unsigned alpha)
{
    return (lv_opa_t)(alpha * notice.opacity / LV_OPA_COVER);
}

static void line(lv_draw_ctx_t *ctx, const lv_area_t *a,
                 int x1, int y1, int x2, int y2, int width, uint32_t color)
{
    lv_draw_line_dsc_t d;
    lv_point_t p1 = { a->x1 + x1, a->y1 + y1 };
    lv_point_t p2 = { a->x1 + x2, a->y1 + y2 };
    lv_draw_line_dsc_init(&d);
    d.color = lv_color_hex(color);
    d.opa = notice.opacity;
    d.width = width;
    d.round_start = d.round_end = 1;
    lv_draw_line(ctx, &d, &p1, &p2);
}

static void arc(lv_draw_ctx_t *ctx, const lv_area_t *a, unsigned start, unsigned end, unsigned alpha)
{
    lv_draw_arc_dsc_t d;
    lv_point_t center = { a->x1 + NOTICE_ICON_X, a->y1 + notice.height / 2 };
    lv_draw_arc_dsc_init(&d);
    d.color = lv_color_white();
    d.opa = opacity(alpha);
    d.width = 2;
    d.rounded = 1;
    lv_draw_arc(ctx, &d, &center, 9, start, end);
}

static void label(lv_draw_ctx_t *ctx, const lv_area_t *origin, int y, int height,
                  const char *text, const lv_font_t *font, uint32_t color)
{
    lv_draw_label_dsc_t d;
    lv_area_t area = { origin->x1 + NOTICE_TEXT_X, origin->y1 + y,
                       origin->x1 + NOTICE_TEXT_X + NOTICE_TEXT_WIDTH - 1,
                       origin->y1 + y + height - 1 };
    lv_area_t clip;
    const lv_area_t *saved_clip = ctx->clip_area;
    if (!_lv_area_intersect(&clip, saved_clip, &area)) return;
    lv_draw_label_dsc_init(&d);
    d.font = ui_message_font(font);
    d.color = lv_color_hex(color);
    d.opa = notice.opacity;
    d.line_space = 2;
    d.bidi_dir = LV_BASE_DIR_AUTO;
    d.align = ui_lang_current()->direction==UI_LANG_RTL ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT;
    ctx->clip_area = &clip;
    lv_draw_label(ctx, &d, &area, text, NULL);
    ctx->clip_area = saved_clip;
}

static void draw(lv_event_t *event)
{
    static const uint32_t colors[] = {0x34C759,0xFF453A,0xFF9F0A,0x007AFF,0x8E8E93};
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_area_t a, tile;
    lv_draw_rect_dsc_t d;
    int cy = notice.height / 2;
    lv_obj_get_coords(notice.object, &a);
    lv_draw_rect_dsc_init(&d);
    d.radius = 23;
    /* An opaque frost tint avoids text bleeding through without framebuffer
     * capture/blur or an offscreen opacity layer on the embedded renderer. */
    d.bg_color = lv_color_hex(0xF5F7FA);
    d.bg_opa = notice.opacity;
    d.border_width = 1;
    d.border_color = lv_color_white();
    d.border_opa = opacity(220);
    d.shadow_color = lv_color_hex(0x20243A);
    d.shadow_width = 16;
    d.shadow_ofs_y = 4;
    d.shadow_opa = opacity(22);
    lv_draw_rect(ctx, &d, &a);

    tile = (lv_area_t){ a.x1 + 16, a.y1 + cy - 16, a.x1 + 47, a.y1 + cy + 15 };
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = lv_color_hex(colors[notice.shown.kind]);
    d.bg_opa = notice.opacity;
    lv_draw_rect(ctx, &d, &tile);

    switch (notice.shown.kind) {
    case UI_NOTICE_SUCCESS:
        line(ctx, &a, 25, cy, 30, cy + 5, 2, 0xFFFFFF);
        line(ctx, &a, 30, cy + 5, 39, cy - 5, 2, 0xFFFFFF);
        break;
    case UI_NOTICE_ERROR:
        line(ctx, &a, 27, cy - 5, 37, cy + 5, 2, 0xFFFFFF);
        line(ctx, &a, 37, cy - 5, 27, cy + 5, 2, 0xFFFFFF);
        break;
    case UI_NOTICE_WARNING:
        line(ctx, &a, 32, cy - 8, 32, cy + 2, 2, 0xFFFFFF);
        line(ctx, &a, 32, cy + 7, 32, cy + 7, 2, 0xFFFFFF);
        break;
    case UI_NOTICE_PROGRESS:
        arc(ctx, &a, 0, 360, 70);
        arc(ctx, &a, notice.angle, (notice.angle + 95) % 360, LV_OPA_COVER);
        break;
    case UI_NOTICE_INFO:
        line(ctx, &a, 32, cy - 2, 32, cy + 7, 2, 0xFFFFFF);
        line(ctx, &a, 32, cy - 7, 32, cy - 7, 2, 0xFFFFFF);
        break;
    }
    label(ctx, &a, notice.copy_y, notice.title_height, notice.title,
          notice.title_font, 0x1D1D1F);
    if (notice.detail[0])
        label(ctx, &a, notice.copy_y + notice.title_height + 2, notice.detail_height,
              notice.detail, &lv_font_instrument_sans_medium_12, 0x73737C);
    line(ctx, &a, 327, cy - 3, 333, cy + 3, 1, 0x92929A);
    line(ctx, &a, 333, cy - 3, 327, cy + 3, 1, 0x92929A);
}

/* Bound external/localized strings at UTF-8 boundaries. Keep detail intact
 * within the existing message capacity; decorative titles get two lines. */
static void copy_text(char *output, size_t capacity, const char *input)
{
    size_t length = strlen(input);
    if (length >= capacity) length = capacity - 1;
    while (length && ((unsigned char)input[length] & 0xC0) == 0x80) --length;
    memcpy(output, input, length);
    output[length] = '\0';
    for (size_t i = 0; i < length; ++i)
        if (output[i] == '\n' || output[i] == '\r') output[i] = ' ';
}

static void measure_title(lv_point_t *size)
{
    size_t length = strlen(notice.title);
    for (;;) {
        lv_txt_get_size(size, notice.title, notice.title_font, 0, 2, NOTICE_TEXT_WIDTH, 0);
        if (size->y <= 2 * notice.title_font->line_height + 2 || !length) return;
        do { --length; } while (length && ((unsigned char)notice.title[length] & 0xC0) == 0x80);
        if (length + 3 < sizeof(notice.title)) memcpy(notice.title + length, "...", 4);
        else notice.title[length] = '\0';
    }
}

static void prepare_copy(void)
{
    lv_point_t measured;
    char detail[UI_NOTICE_DETAIL_CAPACITY];
    const char *title = notice.shown.localized ? ui_tr(notice.shown.title) : notice.shown.title;
    const char *body = notice.shown.localized ? ui_tr(notice.shown.detail) : notice.shown.detail;
    if (notice.shown.has_message) { ui_message_render(&notice.shown.message, detail, sizeof(detail)); body = detail; }
    notice.title_font = ui_message_font(&lv_font_instrument_sans_semibold_16);
    copy_text(notice.title, UI_NOTICE_TITLE_CAPACITY, title);
    copy_text(notice.detail, UI_NOTICE_DETAIL_CAPACITY, body);
    if (notice.shown.repeats > 1 && notice.shown.kind != UI_NOTICE_PROGRESS) {
        size_t length = strlen(notice.title);
        snprintf(notice.title + length, UI_NOTICE_TITLE_CAPACITY - length, " (x%u)", notice.shown.repeats);
    }
#if LV_USE_ARABIC_PERSIAN_CHARS
    /* Direct draw bypasses lv_label_set_text's shaping. Prepare once per notice,
     * with room for UTF-8 presentation forms; never allocate in the draw path. */
    char shaped[LV_MAX(UI_NOTICE_TITLE_CAPACITY,UI_NOTICE_DETAIL_CAPACITY) * 2];
    _lv_txt_ap_proc(notice.title,shaped);strcpy(notice.title,shaped);
    _lv_txt_ap_proc(notice.detail,shaped);strcpy(notice.detail,shaped);
#endif
    measure_title(&measured);
    notice.title_height = measured.y;
    notice.detail_height = 0;
    if (notice.detail[0]) {
        lv_txt_get_size(&measured, notice.detail, ui_message_font(&lv_font_instrument_sans_medium_12),
                        0, 2, NOTICE_TEXT_WIDTH, 0);
        notice.detail_height = measured.y;
    }
    int copy_height = notice.title_height + (notice.detail_height ? notice.detail_height + 2 : 0);
    notice.height = LV_MAX(NOTICE_HEIGHT, copy_height + 24);
    notice.copy_y = (notice.height - copy_height) / 2;
    lv_obj_set_size(notice.object, NOTICE_WIDTH, notice.height);
    lv_obj_set_x(notice.object, (lv_disp_get_hor_res(NULL) - NOTICE_WIDTH) / 2);
}

static void service_stop(void)
{
    if (notice.timer) lv_timer_pause(notice.timer);
}

static void animation_exec(void *object, int32_t value)
{
    int start = notice.animation_y;
    int end = notice.leaving ? NOTICE_EXIT_Y : NOTICE_Y;
    int alpha = notice.leaving ? LV_OPA_TRANSP : LV_OPA_COVER;
    notice.opacity = notice.animation_opacity + (alpha - notice.animation_opacity) * value / 1000;
    lv_obj_set_y(object, start + (end - start) * value / 1000);
    lv_obj_invalidate(object);
}

static void animation_ready(lv_anim_t *animation)
{
    LV_UNUSED(animation);
    notice.animating = false;
    if (notice.leaving) {
        notice.visible = false;
        notice.leaving = false;
        lv_obj_add_flag(notice.object, LV_OBJ_FLAG_HIDDEN);
        present(true);
    } else {
        /* Resolve the final layout before exposing the settled position. */
        notice.opacity = LV_OPA_COVER;
        lv_obj_set_y(notice.object, NOTICE_Y);
        lv_obj_update_layout(notice.object);
        service_start();
    }
}

static void animate(bool leaving)
{
    lv_anim_t a;
    service_stop();
    lv_anim_del(notice.object, animation_exec);
    notice.leaving = leaving;
    lv_obj_update_layout(notice.object);
    notice.animation_y = lv_obj_get_y(notice.object);
    notice.animation_opacity = notice.opacity;
    notice.animating = true;
    lv_anim_init(&a);
    lv_anim_set_var(&a, notice.object);
    lv_anim_set_exec_cb(&a, animation_exec);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_time(&a, leaving ? NOTICE_EXIT_MS : NOTICE_ENTER_MS);
    lv_anim_set_path_cb(&a, leaving ? lv_anim_path_ease_in : lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, animation_ready);
    if (!lv_anim_start(&a)) {
        animation_exec(notice.object, 1000);
        animation_ready(NULL);
    }
}

static void timer_event(lv_timer_t *timer)
{
    uint32_t now = lv_tick_get(), elapsed = now - notice.last_tick;
    LV_UNUSED(timer);
    notice.last_tick = now;
    if (!enabled || !notice.visible || notice.suspended || notice.held || notice.animating) return;
    if (ui_notice_state_elapse(&notice.state, elapsed)) {
        ui_notice_dismiss(NULL);
        return;
    }
    if (notice.shown.kind == UI_NOTICE_PROGRESS) {
        lv_area_t icon;
        notice.angle = (notice.angle + 13) % 360;
        lv_obj_get_coords(notice.object, &icon);
        icon.x1 += 16; icon.y1 += notice.height / 2 - 16;
        icon.x2 = icon.x1 + 31; icon.y2 = icon.y1 + 31;
        lv_obj_invalidate_area(notice.object, &icon);
    }
}

static void service_start(void)
{
    uint32_t period;
    if (!enabled || !notice.timer || !notice.visible || notice.suspended || notice.animating || notice.held) return;
    period = notice.shown.kind == UI_NOTICE_PROGRESS ? NOTICE_SPINNER_MS : notice.state.active.remaining_ms;
    if (!period) period = 1;
    notice.last_tick = lv_tick_get();
    lv_timer_set_period(notice.timer, period);
    lv_timer_reset(notice.timer);
    lv_timer_resume(notice.timer);
}

static void consume_visible_time(void)
{
    if (notice.visible && !notice.suspended && !notice.animating && !notice.held)
        ui_notice_state_elapse(&notice.state, lv_tick_get() - notice.last_tick);
    notice.last_tick = lv_tick_get();
}

static void view_deleted(void)
{
    if (notice.timer) { lv_timer_del(notice.timer); notice.timer = NULL; }
    if (notice.object) lv_anim_del(notice.object, animation_exec);
    notice.object = NULL;
    notice.visible = false;
    notice.animating = false;
    notice.leaving = false;
    notice.held = false;
    if (!notice.deleting) ui_notice_state_init(&notice.state);
}

static void event(lv_event_t *e)
{
    switch (lv_event_get_code(e)) {
    case LV_EVENT_DRAW_MAIN: draw(e); break;
    case LV_EVENT_REFR_EXT_DRAW_SIZE: {
        lv_coord_t *size = lv_event_get_param(e);
        *size = LV_MAX(*size, NOTICE_SHADOW_EXTENT);
        break;
    }
    case LV_EVENT_PRESSED:
        consume_visible_time(); notice.held = true; service_stop(); break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        notice.held = false; service_start(); break;
    case LV_EVENT_CLICKED: ui_notice_dismiss(NULL); break;
    case LV_EVENT_DELETE: view_deleted(); break;
    default: break;
    }
}

void ui_notice_init(void)
{
    if (!enabled) return;
    if (dialog_hold_count) notice.suspended |= UI_NOTICE_SUSPEND_DIALOG;
    if (!notice.initialized) {
        ui_notice_state_init(&notice.state);
        notice.initialized = true;
    }
    if (notice.object) return;
    notice.object = lv_obj_create(lv_layer_top());
    if (!notice.object) return;
    lv_obj_remove_style_all(notice.object);
    lv_obj_set_size(notice.object, NOTICE_WIDTH, NOTICE_HEIGHT);
    lv_obj_set_pos(notice.object, (lv_disp_get_hor_res(NULL) - NOTICE_WIDTH) / 2, NOTICE_Y);
    lv_obj_clear_flag(notice.object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(notice.object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(notice.object, event, LV_EVENT_ALL, NULL);
    lv_obj_refresh_ext_draw_size(notice.object);
    notice.timer = lv_timer_create(timer_event, NOTICE_SPINNER_MS, NULL);
    service_stop();
}

static void present(bool with_animation)
{
    if (!enabled || !notice.object || notice.suspended) return;
    if (!notice.state.has_active) {
        service_stop();
        lv_anim_del(notice.object, animation_exec);
        lv_obj_add_flag(notice.object, LV_OBJ_FLAG_HIDDEN);
        notice.visible = notice.animating = notice.leaving = false;
        return;
    }
    bool continue_motion = notice.visible && notice.leaving;
    notice.shown = notice.state.active;
    prepare_copy();
    /* A result may arrive before the first entry frame. Updating its content
     * must not complete or restart the existing presentation timeline. */
    if (!with_animation && notice.visible && notice.animating && !notice.leaving) {
        lv_obj_invalidate(notice.object);
        return;
    }
    lv_anim_del(notice.object, animation_exec);
    notice.animating = notice.leaving = notice.held = false;
    notice.visible = true;
    if (!with_animation || !continue_motion) {
        notice.opacity = with_animation ? LV_OPA_TRANSP : LV_OPA_COVER;
        lv_obj_set_y(notice.object, with_animation ? NOTICE_ENTRY_Y : NOTICE_Y);
    }
    lv_obj_clear_flag(notice.object, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(notice.object);
    if (with_animation) animate(false);
    else { lv_obj_invalidate(notice.object); service_start(); }
}

bool ui_notice_show(const ui_notice_config_t *config)
{
    bool accepted, was_visible;
    uint32_t revision;
    if (!enabled) return false;
    ui_notice_init();
    consume_visible_time();
    revision = notice.state.has_active ? notice.state.active.revision : 0;
    was_visible = notice.visible && !notice.leaving;
    accepted = ui_notice_state_post(&notice.state, config);
    if (accepted && notice.state.has_active && notice.state.active.revision != revision)
        present(!was_visible);
    else if (accepted && !notice.visible) present(true);
    else service_start();
    return accepted;
}

void ui_notice_post(ui_notice_kind_t kind, const char *key, const char *title, const char *detail)
{
    ui_notice_config_t config = { .kind=kind, .key=key, .title=title, .detail=detail };
    (void)ui_notice_show(&config);
}
void ui_notice_post_text(ui_notice_kind_t kind,const char *key,const char *title,const char *detail)
{
    ui_notice_config_t config={.kind=kind,.key=key,.title=title,.detail=detail,.localized=true};
    (void)ui_notice_show(&config);
}
void ui_notice_post_message(ui_notice_kind_t kind,const char *key,const char *title,const ui_message_t *message)
{
    ui_notice_config_t config={.kind=kind,.key=key,.title=title,.localized=true,.message=message};
    (void)ui_notice_show(&config);
}
void ui_notice_language_changed(void)
{
    if(!notice.initialized||!notice.object)return;
    /* Repaint the existing snapshot, including an exit already in flight.
     * Do not post/merge, restart animation, or touch the queue/visible lifetime. */
    prepare_copy();lv_obj_invalidate(notice.object);
}

void ui_notice_dismiss(const char *key)
{
    bool current;
    if (!notice.initialized) return;
    current = notice.state.has_active && (!key || strcmp(key, notice.state.active.key) == 0);
    if (notice.leaving && !key) return;
    if (!ui_notice_state_remove(&notice.state, key, true) || !current) return;
    if (notice.visible && !notice.suspended) animate(true);
    else present(false);
}

void ui_notice_clear(const char *key)
{
    if (!notice.initialized) return;
    uint32_t revision = notice.state.has_active ? notice.state.active.revision : 0;
    bool was_visible = notice.visible && !notice.leaving;
    if (ui_notice_state_remove(&notice.state, key, false) &&
        (!notice.state.has_active || notice.state.active.revision != revision))
        present(!was_visible);
}

void ui_notice_set_suspended(uint32_t reason, bool suspended)
{
    uint32_t previous = notice.suspended;
    if (!reason) return;
    consume_visible_time();
    if (suspended) notice.suspended |= reason;
    else notice.suspended &= ~reason;
    if (previous == notice.suspended) return;
    if (notice.suspended) {
        service_stop();
        if (notice.object) {
            lv_anim_del(notice.object, animation_exec);
            lv_obj_add_flag(notice.object, LV_OBJ_FLAG_HIDDEN);
        }
        notice.visible = notice.animating = notice.leaving = notice.held = false;
    } else present(true);
}

bool ui_notice_is_visible(void)
{
    return enabled && notice.visible && !notice.suspended;
}

void ui_notice_set_enabled(bool value)
{
    if (enabled == value) return;
    enabled = value;
    if (enabled) return;
    service_stop();
    if (notice.object) {
        lv_anim_del(notice.object, animation_exec);
        lv_obj_add_flag(notice.object, LV_OBJ_FLAG_HIDDEN);
    }
    ui_notice_state_init(&notice.state);
    notice.visible = notice.animating = notice.leaving = notice.held = false;
}

bool ui_notice_get_enabled(void) { return enabled; }

void ui_notice_dialog_acquire(void)
{
    if (dialog_hold_count++ == 0)
        ui_notice_set_suspended(UI_NOTICE_SUSPEND_DIALOG, true);
}

void ui_notice_dialog_release(void)
{
    if (dialog_hold_count && --dialog_hold_count == 0)
        ui_notice_set_suspended(UI_NOTICE_SUSPEND_DIALOG, false);
}

void ui_notice_deinit(void)
{
    notice.deleting = true;
    if (notice.object) lv_obj_del(notice.object);
    else if (notice.timer) lv_timer_del(notice.timer);
    memset(&notice, 0, sizeof(notice));
}
