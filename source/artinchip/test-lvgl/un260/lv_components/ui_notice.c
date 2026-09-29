#include "ui_notice.h"
#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/font/ui_message_font.h"
#include <stdio.h>
#include <string.h>

#define NOTICE_WIDTH 750
#define NOTICE_HEIGHT 116
#define NOTICE_Y 14
#define NOTICE_OFFSCREEN_Y (-140)
#define NOTICE_ENTER_MS 240
#define NOTICE_EXIT_MS 240
#define NOTICE_SPINNER_MS 40

static uint32_t dialog_hold_count;

static struct {
    ui_notice_state_t state;
    ui_notice_item_t shown;
    lv_obj_t *object;
    lv_timer_t *timer;
    const lv_font_t *title_font;
    char title[UI_NOTICE_TITLE_CAPACITY];
    char detail[UI_NOTICE_DETAIL_CAPACITY];
    uint32_t suspended;
    uint32_t last_tick;
    uint16_t angle;
    lv_coord_t animation_y;
    bool initialized;
    bool visible;
    bool leaving;
    bool animating;
    bool held;
    bool deleting;
} notice;

static void present(bool animate);
static void service_start(void);

static void line(lv_draw_ctx_t *ctx, const lv_area_t *a,
                 int x1, int y1, int x2, int y2, int width)
{
    lv_draw_line_dsc_t d;
    lv_point_t p1 = { a->x1 + x1, a->y1 + y1 };
    lv_point_t p2 = { a->x1 + x2, a->y1 + y2 };
    lv_draw_line_dsc_init(&d);
    d.color = lv_color_white();
    d.opa = LV_OPA_COVER;
    d.width = width;
    d.round_start = d.round_end = 1;
    lv_draw_line(ctx, &d, &p1, &p2);
}

static void arc(lv_draw_ctx_t *ctx, const lv_area_t *a, unsigned start, unsigned end, unsigned opa)
{
    lv_draw_arc_dsc_t d;
    lv_point_t center = { a->x1 + 58, a->y1 + 58 };
    lv_draw_arc_dsc_init(&d);
    d.color = lv_color_white();
    d.opa = opa;
    d.width = 3;
    d.rounded = 1;
    lv_draw_arc(ctx, &d, &center, 17, start, end);
}

static void label(lv_draw_ctx_t *ctx, const lv_area_t *origin, int x, int y, int width,
                  const char *text, const lv_font_t *font, uint32_t color, lv_text_align_t align)
{
    lv_draw_label_dsc_t d;
    lv_area_t area = { origin->x1 + x, origin->y1 + y,
                       origin->x1 + x + width - 1, origin->y1 + y + font->line_height - 1 };
    lv_area_t clip;
    const lv_area_t *saved_clip = ctx->clip_area;
    if (!_lv_area_intersect(&clip, saved_clip, &area)) return;
    lv_draw_label_dsc_init(&d);
    d.font = ui_message_font(font);
    d.color = lv_color_hex(color);
    d.opa = LV_OPA_COVER;
    d.align = align;
    d.flag = LV_TEXT_FLAG_EXPAND;
    ctx->clip_area = &clip;
    lv_draw_label(ctx, &d, &area, text, NULL);
    ctx->clip_area = saved_clip;
}

static void draw(lv_event_t *event)
{
    static const uint32_t colors[] = {0x20AC71,0xDF5754,0xDD982F,0x397FE1,0x74879D};
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_area_t a, tile;
    lv_draw_rect_dsc_t d;
    const char *meta = ui_text_get(notice.shown.kind == UI_NOTICE_PROGRESS ?
                                  UI_TEXT_NOTICE_ONGOING : UI_TEXT_NOTICE_NOW);
    char repeated[24];
    lv_obj_get_coords(notice.object, &a);
    lv_draw_rect_dsc_init(&d);
    d.radius = 29;
    d.bg_color = lv_color_hex(0xECEFF1);
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 1;
    d.border_color = lv_color_white();
    d.border_opa = LV_OPA_COVER;
    lv_draw_rect(ctx, &d, &a);

    tile = (lv_area_t){ a.x1 + 25, a.y1 + 25, a.x1 + 90, a.y1 + 90 };
    lv_draw_rect_dsc_init(&d);
    d.radius = 18;
    d.bg_color = lv_color_hex(colors[notice.shown.kind]);
    d.bg_opa = LV_OPA_COVER;
    lv_draw_rect(ctx, &d, &tile);

    switch (notice.shown.kind) {
    case UI_NOTICE_SUCCESS:
        line(ctx, &a, 44, 58, 54, 68, 4);
        line(ctx, &a, 54, 68, 74, 48, 4);
        break;
    case UI_NOTICE_ERROR:
        arc(ctx, &a, 0, 360, LV_OPA_COVER);
        line(ctx, &a, 51, 51, 65, 65, 3);
        line(ctx, &a, 65, 51, 51, 65, 3);
        break;
    case UI_NOTICE_WARNING:
        line(ctx, &a, 58, 41, 77, 73, 3);
        line(ctx, &a, 77, 73, 39, 73, 3);
        line(ctx, &a, 39, 73, 58, 41, 3);
        line(ctx, &a, 58, 53, 58, 61, 3);
        line(ctx, &a, 58, 67, 58, 67, 3);
        break;
    case UI_NOTICE_PROGRESS:
        arc(ctx, &a, 0, 360, 70);
        arc(ctx, &a, notice.angle, (notice.angle + 95) % 360, LV_OPA_COVER);
        break;
    case UI_NOTICE_INFO:
        arc(ctx, &a, 0, 360, LV_OPA_COVER);
        line(ctx, &a, 58, 57, 58, 67, 3);
        line(ctx, &a, 58, 49, 58, 49, 3);
        break;
    }
    label(ctx, &a, 113, notice.detail[0] ? 29 : 43, 518, notice.title,
          notice.title_font, 0x1F252C, LV_TEXT_ALIGN_LEFT);
    if (notice.detail[0])
        label(ctx, &a, 113, 64, 610, notice.detail, &lv_font_instrument_sans_medium_18,
              0x4B525B, LV_TEXT_ALIGN_LEFT);
    label(ctx, &a, 637, 28, 88, meta, &lv_font_instrument_sans_medium_14,
          0x737B84, LV_TEXT_ALIGN_RIGHT);
    if (notice.shown.repeats > 1 && notice.shown.kind != UI_NOTICE_PROGRESS) {
        snprintf(repeated, sizeof(repeated), "x%u", notice.shown.repeats);
        label(ctx, &a, 651, 48, 74, repeated, &lv_font_instrument_sans_medium_14,
              0x737B84, LV_TEXT_ALIGN_RIGHT);
    }
}

static void fit_text(char *out, size_t size, const char *text, const lv_font_t *font, int width)
{
    size_t count = strlen(text), i;
    lv_point_t measured;
    font = ui_message_font(font);
    if (count >= size) count = size - 1;
    while (count && ((unsigned char)text[count] & 0xc0) == 0x80) --count;
    memcpy(out, text, count);
    out[count] = '\0';
    for (i = 0; i < count; ++i) if (out[i] == '\n' || out[i] == '\r') out[i] = ' ';
    lv_txt_get_size(&measured, out, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
    if (measured.x <= width) return;
    while (count) {
        do { --count; } while (count && ((unsigned char)out[count] & 0xc0) == 0x80);
        if (count + 3 >= size) continue;
        memcpy(out + count, "...", 4);
        lv_txt_get_size(&measured, out, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
        if (measured.x <= width) return;
    }
    out[0] = '\0';
}

static void prepare_copy(void)
{
    lv_point_t measured;
    char detail[UI_NOTICE_DETAIL_CAPACITY];
    const char *title=notice.shown.localized?ui_tr(notice.shown.title):notice.shown.title;
    const char *body=notice.shown.localized?ui_tr(notice.shown.detail):notice.shown.detail;
    if(notice.shown.has_message){ui_message_render(&notice.shown.message,detail,sizeof(detail));body=detail;}
    notice.title_font = ui_message_font(&lv_font_instrument_sans_semibold_22);
    lv_txt_get_size(&measured, title, notice.title_font, 0, 0,
                    LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
    if (measured.x > 518) notice.title_font = ui_message_font(&lv_font_instrument_sans_semibold_20);
    fit_text(notice.title, sizeof(notice.title), title, notice.title_font, 518);
    fit_text(notice.detail, sizeof(notice.detail), body,
             &lv_font_instrument_sans_medium_18, 610);
}

static void service_stop(void)
{
    if (notice.timer) lv_timer_pause(notice.timer);
}

static void animation_exec(void *object, int32_t value)
{
    int start = notice.animation_y;
    int end = notice.leaving ? NOTICE_OFFSCREEN_Y : NOTICE_Y;
    lv_obj_set_y(object, start + (end - start) * value / 1000);
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
    } else service_start();
}

static void animate(bool leaving)
{
    lv_anim_t a;
    service_stop();
    lv_anim_del(notice.object, animation_exec);
    notice.leaving = leaving;
    notice.animation_y = leaving ? lv_obj_get_y(notice.object) : NOTICE_OFFSCREEN_Y;
    notice.animating = true;
    lv_anim_init(&a);
    lv_anim_set_var(&a, notice.object);
    lv_anim_set_exec_cb(&a, animation_exec);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_time(&a, leaving ? NOTICE_EXIT_MS : NOTICE_ENTER_MS);
    lv_anim_set_path_cb(&a, leaving ? lv_anim_path_ease_in : lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, animation_ready);
    lv_anim_start(&a);
}

static void timer_event(lv_timer_t *timer)
{
    uint32_t now = lv_tick_get(), elapsed = now - notice.last_tick;
    LV_UNUSED(timer);
    notice.last_tick = now;
    if (!notice.visible || notice.suspended || notice.held || notice.animating) return;
    if (ui_notice_state_elapse(&notice.state, elapsed)) {
        ui_notice_dismiss(NULL);
        return;
    }
    if (notice.shown.kind == UI_NOTICE_PROGRESS) {
        lv_area_t icon;
        notice.angle = (notice.angle + 13) % 360;
        lv_obj_get_coords(notice.object, &icon);
        icon.x1 += 25; icon.y1 += 25;
        icon.x2 = icon.x1 + 65; icon.y2 = icon.y1 + 65;
        lv_obj_invalidate_area(notice.object, &icon);
    }
}

static void service_start(void)
{
    uint32_t period;
    if (!notice.timer || !notice.visible || notice.suspended || notice.animating || notice.held) return;
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
    notice.timer = lv_timer_create(timer_event, NOTICE_SPINNER_MS, NULL);
    service_stop();
}

static void present(bool with_animation)
{
    if (!notice.object || notice.suspended) return;
    if (!notice.state.has_active) {
        service_stop();
        lv_anim_del(notice.object, animation_exec);
        lv_obj_add_flag(notice.object, LV_OBJ_FLAG_HIDDEN);
        notice.visible = notice.animating = notice.leaving = false;
        return;
    }
    notice.shown = notice.state.active;
    prepare_copy();
    lv_anim_del(notice.object, animation_exec);
    notice.animating = notice.leaving = notice.held = false;
    notice.visible = true;
    lv_obj_set_y(notice.object, with_animation ? NOTICE_OFFSCREEN_Y : NOTICE_Y);
    lv_obj_clear_flag(notice.object, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(notice.object);
    if (with_animation) animate(false);
    else { lv_obj_invalidate(notice.object); service_start(); }
}

bool ui_notice_show(const ui_notice_config_t *config)
{
    bool accepted, was_visible;
    uint32_t revision;
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
    if (ui_notice_state_remove(&notice.state, key, false)) present(false);
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
    } else present(false);
}

bool ui_notice_is_visible(void)
{
    return notice.visible && !notice.suspended;
}

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
