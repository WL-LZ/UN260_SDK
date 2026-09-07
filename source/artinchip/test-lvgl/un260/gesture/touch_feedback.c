#include "touch_feedback.h"
#include "un260/lv_system/user_cfg.h"
#include "lv_port_indev.h"
#include "un260/lv_drivers/uart_io.h"
#include "lvgl/src/draw/sw/lv_draw_sw_blend.h"
#include <string.h>
#define FEEDBACK_CONTACTS 16
#define FEEDBACK_DIAMETER 32
#define FEEDBACK_HOLD_MS 100U
#define FEEDBACK_FADE_MS 240U
#define EDGE_HINT_WIDTH 36
#define EDGE_HINT_RETURN_MS 300
/* Fixed, clipped viewport: neither side moves a widget through negative X. */
#define EDGE_HINT_VIEW_WIDTH 48
static lv_obj_t *g_edge_hint;
static lv_timer_t *g_edge_timer;
static int g_edge_side;
static bool g_edge_returning;
static int32_t g_edge_extent, g_edge_return_extent;
static uint32_t g_edge_return_tick, g_edge_last_step_tick;
static uint32_t g_edge_steps, g_edge_max_gap;
static uint32_t g_edge_elapsed;
static bool g_edge_frame_pending;
static uint32_t g_edge_draws, g_edge_last_draw_tick, g_edge_draw_gap;
/* Tiny, shared CPU masks (8640 bytes), not an image decoder/DMA allocation.
 * Both edges rasterize the SAME capsule in positive viewport coordinates. */
static lv_opa_t g_edge_body[72 * EDGE_HINT_WIDTH];
static lv_opa_t g_edge_arrow[72 * EDGE_HINT_WIDTH];
static lv_opa_t g_edge_mask[72 * EDGE_HINT_VIEW_WIDTH];
static bool g_edge_mask_ready;

static bool edge_segment(int x, int y, int ax, int ay, int bx, int by)
{
    int dx = bx-ax, dy = by-ay, px = x-ax, py = y-ay;
    int dot = px*dx+py*dy, len = dx*dx+dy*dy;
    if(dot < 0) return px*px+py*py <= 16;
    if(dot > len) { px=x-bx; py=y-by; return px*px+py*py <= 16; }
    int cross = px*dy-py*dx;
    return cross*cross <= 16*len;
}

static void edge_mask_init(void)
{
    if(g_edge_mask_ready) return;
    for(int y=0; y<72; ++y) for(int x=0; x<EDGE_HINT_WIDTH; ++x) {
        unsigned body=0, arrow=0;
        for(int sy=0; sy<4; ++sy) for(int sx=0; sx<4; ++sx) {
            int px=x*4+sx, py=y*4+sy;
            int dx=2*px+1-144;
            int dy=py<72 ? 2*py+1-144 : py>=216 ? 2*py+1-432 : 0;
            body += dx*dx+dy*dy <= 144*144;
            arrow += edge_segment(px,py,60,120,84,144) || edge_segment(px,py,84,144,60,168);
        }
        g_edge_body[y*EDGE_HINT_WIDTH+x]=(lv_opa_t)(body*LV_OPA_80/16);
        g_edge_arrow[y*EDGE_HINT_WIDTH+x]=(lv_opa_t)(arrow*255/16);
    }
    g_edge_mask_ready=true;
}

/* Only drawing geometry changes; the fixed viewport stays on the system layer. */
static void edge_hint_apply_extent(lv_obj_t *object, int32_t extent)
{
    g_edge_extent = extent;
    if(g_edge_returning) g_edge_frame_pending = true;
    lv_obj_invalidate(object);
}

static void edge_hint_draw(lv_event_t *event)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_area_t bounds, clip;
    lv_obj_get_coords(lv_event_get_target(event), &bounds);
    const lv_area_t *old_clip = ctx->clip_area;
    if(!_lv_area_intersect(&clip, old_clip, &bounds)) return;
    ctx->clip_area = &clip;
    edge_mask_init();
    for(int part=0; part<2 && g_edge_extent>0; ++part) {
        const lv_opa_t *source=part ? g_edge_arrow : g_edge_body;
        for(int y=0; y<72; ++y) for(int x=0; x<EDGE_HINT_VIEW_WIDTH; ++x) {
            int inward=g_edge_side>0 ? x : EDGE_HINT_VIEW_WIDTH-1-x;
            int sx=inward-g_edge_extent+EDGE_HINT_WIDTH;
            g_edge_mask[y*EDGE_HINT_VIEW_WIDTH+x]=sx>=0 && sx<EDGE_HINT_WIDTH ?
                source[y*EDGE_HINT_WIDTH+sx] : 0;
        }
        lv_draw_sw_blend_dsc_t blend={0};
        blend.blend_area=&bounds;
        blend.mask_area=&bounds;
        blend.mask_buf=g_edge_mask;
        blend.mask_res=LV_DRAW_MASK_RES_CHANGED;
        blend.color=part ? lv_color_white() : lv_color_hex(0x30464F);
        blend.opa=LV_OPA_COVER;
        lv_draw_sw_blend(ctx,&blend);
    }
    ctx->clip_area = old_clip;
    if(g_edge_returning && g_edge_frame_pending) {
        uint32_t gap=lv_tick_elaps(g_edge_last_draw_tick);
        if(gap>g_edge_draw_gap) g_edge_draw_gap=gap;
        g_edge_last_draw_tick=lv_tick_get();
        ++g_edge_draws;
        g_edge_frame_pending=false;
    }
}

static void edge_hint_return_step(lv_timer_t *timer)
{
    uint32_t gap = lv_tick_elaps(g_edge_last_step_tick);
    if(gap > g_edge_max_gap) g_edge_max_gap = gap;
    g_edge_last_step_tick = lv_tick_get();
    /* Do not advance through several positions before even one was drawn. */
    if(g_edge_frame_pending) return;
    ++g_edge_steps;
    /* A synchronous page commit must not consume the entire animation while
     * no frames can be drawn. Nominal duration is 300ms; stalls extend it. */
    if(g_edge_elapsed < EDGE_HINT_RETURN_MS) {
    g_edge_elapsed += gap > 32 ? 32 : gap;
    if(g_edge_elapsed > EDGE_HINT_RETURN_MS) g_edge_elapsed = EDGE_HINT_RETURN_MS;
    int32_t t = (int32_t)(g_edge_elapsed * 1024 / EDGE_HINT_RETURN_MS);
    int32_t eased = (int32_t)((int64_t)t * t * (3072 - 2 * t) / (1024 * 1024));
    edge_hint_apply_extent(g_edge_hint, g_edge_return_extent * (1024 - eased) / 1024);
    return;
    }
    lv_obj_add_flag(g_edge_hint, LV_OBJ_FLAG_HIDDEN);
    g_edge_returning = false;
    lv_timer_pause(timer);
    uart_debug_printf("EDGE_RETURN mode=MASK_V3 side=%s from=%ld ms=%lu steps=%lu gap=%lu draws=%lu draw_gap=%lu\n",
                      g_edge_side > 0 ? "L" : "R", (long)g_edge_return_extent,
                      (unsigned long)lv_tick_elaps(g_edge_return_tick),
                      (unsigned long)g_edge_steps, (unsigned long)g_edge_max_gap,
                      (unsigned long)g_edge_draws, (unsigned long)g_edge_draw_gap);
}

void touch_feedback_edge_hint(int side, int distance, int y)
{
    if(!side) {
        /* Idle samples keep asking to hide: never restart the release timer. */
        if(g_edge_hint && !g_edge_returning &&
           !lv_obj_has_flag(g_edge_hint, LV_OBJ_FLAG_HIDDEN)) {
            g_edge_returning = true;
            g_edge_return_extent = g_edge_extent;
            g_edge_return_tick = g_edge_last_step_tick = lv_tick_get();
            g_edge_steps = g_edge_max_gap = g_edge_elapsed = 0;
            g_edge_draws = g_edge_draw_gap = 0;
            g_edge_last_draw_tick = lv_tick_get();
            g_edge_frame_pending = false;
            if(g_edge_timer) {
                lv_timer_reset(g_edge_timer);
                lv_timer_resume(g_edge_timer);
            } else {
                lv_obj_add_flag(g_edge_hint, LV_OBJ_FLAG_HIDDEN);
                g_edge_returning = false;
            }
        }
        return;
    }
    if(!g_edge_hint) {
        g_edge_hint = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(g_edge_hint);
        lv_obj_clear_flag(g_edge_hint, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(g_edge_hint, EDGE_HINT_VIEW_WIDTH, 72);
        lv_obj_add_flag(g_edge_hint, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_add_event_cb(g_edge_hint, edge_hint_draw, LV_EVENT_DRAW_MAIN, NULL);
        g_edge_timer = lv_timer_create(edge_hint_return_step, 20, NULL);
        if(g_edge_timer) lv_timer_pause(g_edge_timer);
    }
    /* A new drag owns the hint immediately; stale completion must not hide it. */
    if(g_edge_returning) {
        if(g_edge_timer) lv_timer_pause(g_edge_timer);
        g_edge_returning = false;
        g_edge_frame_pending = false;
    }
    g_edge_side = side;
    lv_obj_set_x(g_edge_hint, side > 0 ? 0 : lv_disp_get_hor_res(NULL) - EDGE_HINT_VIEW_WIDTH);
    int offset = distance / 5;
    if(offset < 0) offset = 0;
    if(offset > 24) offset = 24;
    if(y < 40) y = 40;
    if(y > 360) y = 360;
    edge_hint_apply_extent(g_edge_hint, 24 + offset);
    lv_obj_set_y(g_edge_hint, y - 36);
    lv_obj_clear_flag(g_edge_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_edge_hint);
}
bool touch_feedback_edge_hint_is_returning(void)
{
    return g_edge_returning;
}

typedef struct {
    lv_obj_t *ring;
    uint32_t released, fade_tick, fade_elapsed, fade_steps, fade_max_gap;
    bool down, fading;
} feedback_contact_t;
static feedback_contact_t g_contacts[FEEDBACK_CONTACTS];
static lv_timer_t *g_timer;
static bool g_multitouch_suppressed;
/* The ring has no children: fade its primitives, not a composed object layer.
 * Global OPA would allocate an intermediate layer in LVGL's renderer. */
static void feedback_set_opacity(lv_obj_t *ring, lv_opa_t opa)
{
    lv_obj_set_style_bg_opa(ring, opa, 0);
    lv_obj_set_style_border_opa(ring, opa, 0);
}
static void hide(void)
{
    for(int i = 0; i < FEEDBACK_CONTACTS; ++i) {
        if(g_contacts[i].ring) lv_obj_add_flag(g_contacts[i].ring, LV_OBJ_FLAG_HIDDEN);
        g_contacts[i].down = g_contacts[i].fading = false;
    }
    if(g_timer) lv_timer_pause(g_timer);
}
static void fade(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    bool active = false;
    for(int i = 0; i < FEEDBACK_CONTACTS; ++i) {
        feedback_contact_t *c = &g_contacts[i];
        if(!c->fading) continue;
        uint32_t elapsed = lv_tick_elaps(c->released);
        if(elapsed < FEEDBACK_HOLD_MS) { active = true; continue; }
        uint32_t gap = lv_tick_elaps(c->fade_tick);
        c->fade_tick = lv_tick_get();
        if(gap > c->fade_max_gap) c->fade_max_gap = gap;
        if(gap) ++c->fade_steps;
        /* A long synchronous page commit must not skip all intermediate alpha
         * values. Stalls extend wall time, not the per-frame opacity step. */
        c->fade_elapsed += gap > 32 ? 32 : gap;
        if(c->fade_elapsed >= FEEDBACK_FADE_MS) {
            feedback_set_opacity(c->ring, LV_OPA_TRANSP);
            lv_obj_add_flag(c->ring, LV_OBJ_FLAG_HIDDEN);
            c->fading = false;
            uart_debug_printf("TOUCH_FADE mode=PRIMITIVE hold=100 fade=240 actual=%lu steps=%lu gap=%lu\n",
                (unsigned long)elapsed, (unsigned long)c->fade_steps,
                (unsigned long)c->fade_max_gap);
        }
        else {
            feedback_set_opacity(c->ring, (lv_opa_t)(255 * (FEEDBACK_FADE_MS - c->fade_elapsed) / FEEDBACK_FADE_MS));
            active = true;
        }
    }
    if(!active) lv_timer_pause(g_timer);
}
void touch_feedback_init(void) { user_cfg_touch_feedback_load(); }
bool touch_feedback_enabled(void) { return user_cfg_touch_feedback_enabled(); }
bool touch_feedback_set_enabled(bool enabled)
{
    if(!user_cfg_touch_feedback_save(enabled)) return false;
    if(!enabled) hide();
    return true;
}
void touch_feedback_sample(const lv_point_t *point, uint8_t count)
{
    if(count >= 2) {
        g_multitouch_suppressed = true;
        hide();
        return;
    }
    if(g_multitouch_suppressed) {
        if(count == 0) g_multitouch_suppressed = false;
        return;
    }
    if(!touch_feedback_enabled() || !point) return;
    lv_point_t points[FEEDBACK_CONTACTS]; int32_t ids[FEEDBACK_CONTACTS];
    uint8_t n = count ? lv_port_indev_touch_points(points, ids, FEEDBACK_CONTACTS) : 0;
    if(count && !n) { n = 1; points[0] = *point; }
    bool fading = false;
    for(int i = 0; i < FEEDBACK_CONTACTS; ++i) {
        feedback_contact_t *c = &g_contacts[i];
        if(i >= n) {
            if(c->down) {
                c->down = false; c->fading = true;
                c->released = lv_tick_get();
                c->fade_tick = c->released + FEEDBACK_HOLD_MS;
                c->fade_elapsed = c->fade_steps = c->fade_max_gap = 0;
            }
            fading |= c->fading;
            continue;
        }
        lv_obj_t *g_ring = c->ring;
        if(!g_ring) {
        c->ring = g_ring = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(g_ring);
        lv_obj_clear_flag(g_ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(g_ring, FEEDBACK_DIAMETER, FEEDBACK_DIAMETER);
        lv_obj_set_style_radius(g_ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(g_ring, lv_color_hex(0xE5E5E5), 0);
        lv_obj_set_style_bg_opa(g_ring, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(g_ring, lv_color_hex(0xBCCCDA), 0);
        lv_obj_set_style_border_width(g_ring, 2, 0);
        lv_obj_set_style_border_opa(g_ring, LV_OPA_COVER, 0);
        if(!g_timer) { g_timer = lv_timer_create(fade, 20, NULL); lv_timer_pause(g_timer); }
        }
        if(!c->down) {
        feedback_set_opacity(g_ring, LV_OPA_COVER);
        lv_obj_clear_flag(g_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_ring);
        }
        c->down = true; c->fading = false;
        lv_obj_set_pos(g_ring, points[i].x - FEEDBACK_DIAMETER / 2,
                       points[i].y - FEEDBACK_DIAMETER / 2);
    }
    if(g_timer) { if(fading) lv_timer_resume(g_timer); else lv_timer_pause(g_timer); }
}
