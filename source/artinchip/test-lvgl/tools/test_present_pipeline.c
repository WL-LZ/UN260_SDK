/* This is a fixture template. Run test_present_pipeline.py to inject the
 * current production functions and compile with the real damage planner. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aic_ui/present_damage.h"

#define WIDTH 41
#define HEIGHT 29
#define PIXELS (WIDTH * HEIGHT)
#define LV_INV_BUF_SIZE 32
#define LV_UNUSED(value) ((void)(value))
#define LV_LOG_ERROR(...) ((void)0)
#define LV_LOG_WARN(...) ((void)0)
#define MPP_PHY_ADDR 1
#define FBIOGET_VSCREENINFO 100
#define FBIOPAN_DISPLAY 101
#define AICFB_WAIT_FOR_VSYNC 102

typedef uint32_t lv_color_t;
typedef int32_t lv_coord_t;
typedef struct { lv_coord_t x1, y1, x2, y2; } lv_area_t;
typedef struct { bool paused; } lv_timer_t;
typedef struct {
    lv_color_t *buf1, *buf2, *buf_act;
    bool flushing, flushing_last;
} lv_disp_draw_buf_t;
typedef struct {
    int hor_res, ver_res;
    bool direct_mode;
    lv_disp_draw_buf_t *draw_buf;
} lv_disp_drv_t;
typedef struct {
    lv_disp_drv_t *driver;
    lv_timer_t *refr_timer;
    uint16_t inv_p;
    lv_area_t inv_areas[LV_INV_BUF_SIZE];
    uint8_t inv_area_joined[LV_INV_BUF_SIZE];
} lv_disp_t;
struct fb_var_screeninfo { unsigned xoffset, yoffset; };
enum ge_mode { GE_MODE_NORMAL, GE_MODE_CMDQ };
enum mpp_pixel_format { MPP_FMT_ARGB_8888 };
struct mpp_ge { int unused; };
struct mock_ge_buffer {
    int buf_type, fd[3], stride[3];
    unsigned phy_addr[3];
    struct { int width, height; } size;
    bool crop_en;
    struct { int x, y, width, height; } crop;
    enum mpp_pixel_format format;
};
struct ge_bitblt { struct mock_ge_buffer src_buf, dst_buf; };
typedef struct {
    uint32_t invalid_area_count;
    uint64_t invalid_pixels, mirror_pixels;
    uint32_t total_us, pan_us, vsync_us, mirror_us;
    bool full_screen;
} perf_profile_flush_sample_t;

static lv_color_t frame_storage[2][PIXELS];
static char *g_frame_buf[3] = { (char *)frame_storage[0], (char *)frame_storage[1], NULL };
static unsigned g_frame_phy[3] = { 0x1000, 0x2000, 0 };
static struct mpp_ge mock_ge;
static struct mpp_ge *g_ge = &mock_ge;
static enum ge_mode g_ge_mode = GE_MODE_NORMAL;
static int g_fb = 4, g_triple_fb;
static char *buf_next;
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static lv_timer_t refresh_timer;
static lv_disp_t display;
/* ACTUAL_PRESENT_GLOBALS */

static uint64_t clock_us;
static unsigned visible_buffer, pan_target, pan_calls, vsync_calls, get_info_calls;
static unsigned successful_vsyncs, acknowledged_vsyncs, flush_ready_calls;
static unsigned copy_calls, copy_fail_at, pan_failures, vsync_failures, get_info_failures;
static unsigned pan_interruptions, vsync_interruptions, full_invalidations;
static uint64_t copied_pixels, profiled_copied, profiled_saved;
static uint32_t profiled_fallbacks, profiled_submit_errors;
static unsigned profiled_frames, completed_fps_frames;
static bool profile_enabled = true;
static lv_color_t scene_base[PIXELS], scene_overlay[PIXELS], reference[PIXELS];
static uint8_t scene_alpha[PIXELS];
static uint32_t random_state = 0x72ab4361U;
static const lv_area_t full_area = { 0, 0, WIDTH - 1, HEIGHT - 1 };

static uint32_t random_u32(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static uint64_t app_clock_monotonic_us(void) { return clock_us; }
static uint32_t app_clock_uptime_ms(void) { return (uint32_t)(clock_us / 1000U); }
static uint32_t app_clock_elapsed_us32(uint64_t start, uint64_t end) { return (uint32_t)(end - start); }
static lv_disp_t *_lv_refr_get_disp_refreshing(void) { return &display; }
static lv_disp_draw_buf_t *lv_disp_get_draw_buf(lv_disp_t *disp) { return disp->driver->draw_buf; }
static lv_coord_t lv_area_get_width(const lv_area_t *a) { return a->x2 - a->x1 + 1; }
static lv_coord_t lv_area_get_height(const lv_area_t *a) { return a->y2 - a->y1 + 1; }
static enum mpp_pixel_format draw_buf_fmt(void) { return MPP_FMT_ARGB_8888; }
static int draw_buf_pitch(void) { return WIDTH * (int)sizeof(lv_color_t); }
static void lv_timer_pause(lv_timer_t *timer) { timer->paused = true; }
static void *lv_scr_act(void) { return &display; }
static void lv_obj_invalidate(void *screen)
{
    assert(screen == &display);
    full_invalidations++;
    /* A full-screen invalidation subsumes any pending smaller areas. */
    memset(display.inv_area_joined, 0, sizeof(display.inv_area_joined));
    display.inv_areas[0] = full_area;
    display.inv_p = 1;
    refresh_timer.paused = false;
}
static void cal_frame_rate(void) { completed_fps_frames++; }
static bool perf_profile_is_enabled(void) { return profile_enabled; }
static void perf_profile_report_flush(const perf_profile_flush_sample_t *sample)
{
    assert(sample->total_us >= sample->mirror_us);
    profiled_frames++;
}
static void perf_profile_report_present_reuse(uint64_t copied, uint64_t saved,
                                              uint32_t fallbacks, uint32_t errors)
{
    profiled_copied = copied;
    profiled_saved = saved;
    profiled_fallbacks = fallbacks;
    profiled_submit_errors = errors;
}
static void lv_disp_flush_ready(lv_disp_drv_t *drv)
{
    assert(drv == &disp_drv && drv->draw_buf->flushing);
    if (drv->draw_buf->flushing_last) {
        /* No ownership release until BOTH PAN and VSYNC have succeeded. */
        assert(successful_vsyncs > acknowledged_vsyncs);
        acknowledged_vsyncs = successful_vsyncs;
    }
    drv->draw_buf->flushing = false;
    flush_ready_calls++;
}

static int mock_ioctl(int fd, unsigned request, void *arg)
{
    assert(fd == g_fb);
    clock_us += 11;
    if (request == FBIOGET_VSCREENINFO) {
        get_info_calls++;
        if (get_info_failures) { get_info_failures--; errno = EIO; return -1; }
        *(struct fb_var_screeninfo *)arg = (struct fb_var_screeninfo){0, visible_buffer * HEIGHT};
    } else if (request == FBIOPAN_DISPLAY) {
        const struct fb_var_screeninfo *var = arg;
        pan_calls++;
        assert(var->xoffset == 0 && (var->yoffset == 0 || var->yoffset == HEIGHT));
        if (pan_interruptions) { pan_interruptions--; errno = EINTR; return -1; }
        if (pan_failures) { pan_failures--; errno = EIO; return -1; }
        pan_target = var->yoffset / HEIGHT;
        /* Model the hardest case: PAN changes scanout before VSYNC then fails. */
        visible_buffer = pan_target;
    } else {
        assert(request == AICFB_WAIT_FOR_VSYNC && *(int *)arg == 0);
        vsync_calls++;
        if (vsync_interruptions) { vsync_interruptions--; errno = EINTR; return -1; }
        if (vsync_failures) { vsync_failures--; errno = EIO; return -1; }
        successful_vsyncs++;
    }
    return 0;
}
#define ioctl mock_ioctl

static int mpp_ge_bitblt(struct mpp_ge *ge, struct ge_bitblt *blt)
{
    struct mock_ge_buffer *src = &blt->src_buf, *dst = &blt->dst_buf;
    unsigned source = src->phy_addr[0] == g_frame_phy[0] ? 0 : 1;
    unsigned target = dst->phy_addr[0] == g_frame_phy[0] ? 0 : 1;
    bool fail;

    assert(ge == g_ge && src->buf_type == MPP_PHY_ADDR && dst->buf_type == MPP_PHY_ADDR);
    assert(src->phy_addr[0] == g_frame_phy[source] && dst->phy_addr[0] == g_frame_phy[target]);
    assert(source != target && source == visible_buffer);
    assert((lv_color_t *)g_frame_buf[target] == disp_buf.buf_act);
    assert(!disp_buf.flushing); /* Never write either buffer during held ownership. */
    assert(src->crop_en && dst->crop_en && src->format == MPP_FMT_ARGB_8888 && dst->format == src->format);
    assert(src->stride[0] == WIDTH * 4 && dst->stride[0] == src->stride[0]);
    assert(src->size.width == WIDTH && src->size.height == HEIGHT);
    assert(dst->size.width == WIDTH && dst->size.height == HEIGHT);
    assert(src->crop.x == dst->crop.x && src->crop.y == dst->crop.y);
    assert(src->crop.width == dst->crop.width && src->crop.height == dst->crop.height);
    assert(src->crop.x >= 0 && src->crop.y >= 0 && src->crop.width > 0 && src->crop.height > 0);
    assert(src->crop.x + src->crop.width <= WIDTH && src->crop.y + src->crop.height <= HEIGHT);
    copy_calls++;
    fail = copy_fail_at != 0 && copy_calls == copy_fail_at;
    for (int y = src->crop.y; y < src->crop.y + src->crop.height; y++) {
        for (int x = src->crop.x; x < src->crop.x + src->crop.width; x++) {
            frame_storage[target][y * WIDTH + x] = frame_storage[source][y * WIDTH + x];
            copied_pixels++;
            /* A failing GE ioctl is allowed to have modified part of its target. */
            if (fail) { clock_us += 17; return -1; }
        }
    }
    clock_us += 17;
    return 0;
}
static int mpp_ge_emit(struct mpp_ge *ge) { assert(ge == g_ge); return 0; }
static int mpp_ge_sync(struct mpp_ge *ge) { assert(ge == g_ge); return 0; }

/* ACTUAL_PRESENT_FUNCTIONS */

static bool point_in(present_rect_t r, int x, int y)
{
    return x >= r.x1 && x <= r.x2 && y >= r.y1 && y <= r.y2;
}

static bool in_any(const present_rect_t *rects, size_t count, int x, int y)
{
    for (size_t i = 0; i < count; i++) if (point_in(rects[i], x, y)) return true;
    return false;
}

static present_rect_t random_rect(void)
{
    int x1 = (int)(random_u32() % WIDTH), x2 = (int)(random_u32() % WIDTH);
    int y1 = (int)(random_u32() % HEIGHT), y2 = (int)(random_u32() % HEIGHT);
    return (present_rect_t){ x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2,
                             x1 > x2 ? x1 : x2, y1 > y2 ? y1 : y2 };
}

static void test_planner(void)
{
    present_rect_t previous[32], redraw[32];
    struct {
        uint64_t before[2];
        present_rect_t out[PRESENT_DAMAGE_CAPACITY];
        uint64_t after[2];
    } guarded;
    unsigned succeeded = 0, failed = 0;

    for (unsigned trial = 0; trial < 5000; trial++) {
        size_t pcount = random_u32() % 13U, rcount = random_u32() % 13U;
        size_t capacity = trial % 5 ? PRESENT_DAMAGE_CAPACITY : random_u32() % 65U;
        size_t count = 999;
        guarded.before[0] = guarded.before[1] = guarded.after[0] = guarded.after[1] = UINT64_C(0xda72b91134eecafe);
        for (size_t i = 0; i < pcount; i++) previous[i] = random_rect();
        for (size_t i = 0; i < rcount; i++) redraw[i] = random_rect();
        bool ok = present_damage_plan(previous, pcount, redraw, rcount, guarded.out, capacity, &count);
        if (ok) {
            uint64_t sum = 0;
            succeeded++;
            assert(count <= capacity);
            for (size_t i = 0; i < count; i++) {
                present_rect_t a = guarded.out[i];
                assert(a.x1 <= a.x2 && a.y1 <= a.y2);
                sum += (uint64_t)(a.x2 - a.x1 + 1) * (uint64_t)(a.y2 - a.y1 + 1);
            }
            assert(sum == present_damage_pixels(guarded.out, count));
            for (int y = -1; y <= HEIGHT; y++) {
                for (int x = -1; x <= WIDTH; x++) {
                    bool wanted = in_any(previous, pcount, x, y) && !in_any(redraw, rcount, x, y);
                    assert(in_any(guarded.out, count, x, y) == wanted);
                }
            }
        } else failed++;
        assert(guarded.before[0] == UINT64_C(0xda72b91134eecafe) && guarded.before[1] == guarded.before[0]);
        assert(guarded.after[0] == guarded.before[0] && guarded.after[1] == guarded.before[0]);
    }
    assert(succeeded > 1000 && failed > 100);
    previous[0] = (present_rect_t){0, 0, WIDTH - 1, HEIGHT - 1};
    redraw[0] = previous[0];
    size_t count;
    assert(present_damage_plan(previous, 1, redraw, 1, guarded.out, 64, &count) && count == 0);
    redraw[0] = (present_rect_t){1, 1, WIDTH - 2, HEIGHT - 2};
    assert(present_damage_plan(previous, 1, redraw, 1, guarded.out, 4, &count) && count == 4);
    assert(!present_damage_plan(previous, 1, redraw, 1, guarded.out, 3, &count));
    assert(!present_damage_plan(previous, 1, redraw, 1, guarded.out, 0, &count));
    assert(!present_damage_plan(previous, 1, redraw, 1, guarded.out, 65, &count));
    previous[0] = (present_rect_t){0, 0, 0, 0};
    redraw[0] = (present_rect_t){1, 0, 1, 0};
    assert(present_damage_plan(previous, 1, redraw, 1, guarded.out, 1, &count) && count == 1);
    assert(present_damage_pixels(guarded.out, count) == 1);
    previous[0] = (present_rect_t){0, 0, WIDTH - 1, HEIGHT - 1};
    for (unsigned i = 0; i < 9; i++) {
        redraw[i] = (present_rect_t){2 + (int)i * 4, 0, 2 + (int)i * 4, HEIGHT - 1};
        redraw[9 + i] = (present_rect_t){0, 2 + (int)i * 3, WIDTH - 1, 2 + (int)i * 3};
    }
    assert(!present_damage_plan(previous, 1, redraw, 18, guarded.out, 64, &count));
    printf("planner: 5000 randomized pixel-set cases (%u success, %u bounded fallback) plus edges/capacity passed\n", succeeded, failed);
}

static lv_color_t compose(lv_color_t base, lv_color_t overlay, unsigned alpha)
{
    uint32_t color = 0xff000000U;
    for (unsigned shift = 0; shift < 24; shift += 8) {
        unsigned b = (base >> shift) & 255U, f = (overlay >> shift) & 255U;
        color |= ((b * (255U - alpha) + f * alpha + 127U) / 255U) << shift;
    }
    return color;
}

static void update_reference(void)
{
    for (unsigned p = 0; p < PIXELS; p++) reference[p] = compose(scene_base[p], scene_overlay[p], scene_alpha[p]);
}

static void change_scene(lv_area_t rect, unsigned generation)
{
    for (int y = rect.y1; y <= rect.y2; y++) {
        for (int x = rect.x1; x <= rect.x2; x++) {
            unsigned p = (unsigned)(y * WIDTH + x);
            scene_base[p] = 0xff000000U | ((generation * 713U + p * 193U) & 0xffffffU);
            scene_overlay[p] = 0xff000000U | ((generation * 311U + p * 971U) & 0xffffffU);
            scene_alpha[p] = (uint8_t)((generation + p) % 4U * 85U);
        }
    }
    update_reference();
}

static void load_damage(const lv_area_t *areas, unsigned count)
{
    assert(count <= LV_INV_BUF_SIZE);
    memset(display.inv_areas, 0, sizeof(display.inv_areas));
    memset(display.inv_area_joined, 0, sizeof(display.inv_area_joined));
    if (count) memcpy(display.inv_areas, areas, count * sizeof(*areas));
    display.inv_p = count;
    refresh_timer.paused = false;
}

static void assert_visible_reference(void)
{
    for (unsigned p = 0; p < PIXELS; p++) {
        if (frame_storage[visible_buffer][p] != reference[p]) {
            fprintf(stderr, "pixel mismatch buffer=%u xy=%u,%u got=%08x expected=%08x sequence=%u\n",
                    visible_buffer, p % WIDTH, p / WIDTH, frame_storage[visible_buffer][p], reference[p], g_present_sequence);
            assert(false);
        }
    }
}

/* Mirrors the verified LVGL 8.3 order: choose last_i; render_start; draw/flush
 * unjoined clips; swap buf_act only after the last flush callback returns.
 * The mocked drawing recomposes transparent content over its opaque base. */
static bool render_frame(void)
{
    int last_i = -1;
    bool flushed_last = false;
    for (int i = (int)display.inv_p - 1; i >= 0; i--) {
        if (!display.inv_area_joined[i]) { last_i = i; break; }
    }
    if (last_i < 0) return false;
    fbdev_render_start(&disp_drv);
    for (unsigned i = 0; i < display.inv_p; i++) {
        if (display.inv_area_joined[i]) continue;
        assert(!disp_buf.flushing);
        lv_area_t a = display.inv_areas[i];
        assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < WIDTH && a.y2 < HEIGHT);
        for (int y = a.y1; y <= a.y2; y++) {
            for (int x = a.x1; x <= a.x2; x++) {
                unsigned p = (unsigned)(y * WIDTH + x);
                /* Repaint base first; alpha content never blends with stale back pixels. */
                disp_buf.buf_act[p] = compose(scene_base[p], scene_overlay[p], scene_alpha[p]);
            }
        }
        disp_buf.flushing = true;
        disp_buf.flushing_last = (int)i == last_i;
        fbdev_flush(&disp_drv, &a, disp_buf.buf_act);
        if ((int)i == last_i) {
            flushed_last = true;
            disp_buf.buf_act = disp_buf.buf_act == disp_buf.buf1 ? disp_buf.buf2 : disp_buf.buf1;
        }
    }
    if (display.inv_p) {
        /* Standard LVGL cleanup is skipped when the recovery gate zeroes inv_p. */
        memset(display.inv_areas, 0, sizeof(display.inv_areas));
        memset(display.inv_area_joined, 0, sizeof(display.inv_area_joined));
        display.inv_p = 0;
    }
    if (flushed_last && !g_retry_driver) assert_visible_reference();
    return flushed_last;
}

static void reset_pipeline(void)
{
    memset(frame_storage, 0xa5, sizeof(frame_storage));
    memset(&disp_buf, 0, sizeof(disp_buf));
    memset(&display, 0, sizeof(display));
    disp_buf.buf1 = frame_storage[1];
    disp_buf.buf2 = frame_storage[0];
    disp_buf.buf_act = disp_buf.buf1;
    disp_drv = (lv_disp_drv_t){WIDTH, HEIGHT, true, &disp_buf};
    display.driver = &disp_drv;
    display.refr_timer = &refresh_timer;
    g_damage_reuse = true;
    g_previous_buffer = NULL;
    g_previous_count = 0;
    g_prepare_us = 0;
    g_prepare_pixels = g_saved_pixels = 0;
    g_retry_driver = NULL;
    g_retry_buffer = NULL;
    g_retry_tick = g_present_sequence = g_present_errors = g_copy_fallbacks = 0;
    g_pan_var = (struct fb_var_screeninfo){0};
    g_pan_var_valid = 1;
    g_live_vscreeninfo = 0;
    clock_us = 1000000;
    visible_buffer = pan_target = pan_calls = vsync_calls = get_info_calls = 0;
    successful_vsyncs = acknowledged_vsyncs = flush_ready_calls = 0;
    copy_calls = copy_fail_at = pan_failures = vsync_failures = get_info_failures = 0;
    pan_interruptions = vsync_interruptions = full_invalidations = 0;
    copied_pixels = profiled_copied = profiled_saved = 0;
    profiled_fallbacks = profiled_submit_errors = 0;
    profiled_frames = completed_fps_frames = 0;
    profile_enabled = true;
    change_scene(full_area, 1);
    load_damage(&full_area, 1);
    assert(render_frame());
    assert(g_present_sequence == 1 && visible_buffer == 1 && !disp_buf.flushing);
}

static void test_frame_sequences(void)
{
    reset_pipeline();
    for (unsigned frame = 0; frame < 1800; frame++) {
        lv_area_t areas[8];
        unsigned count = 1;
        switch ((frame / 9U) % 6U) {
        case 0: areas[0] = (lv_area_t){4, 3, 15, 11}; break;
        case 1: {
            int x = (int)(frame % (WIDTH - 9)), y = (int)(frame % (HEIGHT - 6));
            int old_x = x ? x - 1 : WIDTH - 10, old_y = y ? y - 1 : HEIGHT - 7;
            areas[0] = (lv_area_t){old_x, old_y, old_x + 8, old_y + 5};
            areas[1] = (lv_area_t){x, y, x + 8, y + 5};
            count = 2;
            break;
        }
        case 2: areas[0] = frame % 2 ? (lv_area_t){0, 0, 4, 3} : (lv_area_t){WIDTH - 5, HEIGHT - 4, WIDTH - 1, HEIGHT - 1}; break;
        case 3:
            count = 7;
            for (unsigned i = 0; i < count; i++) {
                present_rect_t r = random_rect();
                areas[i] = (lv_area_t){r.x1, r.y1, r.x2, r.y2};
            }
            break;
        case 4: areas[0] = full_area; break;
        default:
            areas[0] = (lv_area_t){0, 0, WIDTH - 1, 0};
            areas[1] = (lv_area_t){WIDTH - 1, 0, WIDTH - 1, HEIGHT - 1};
            areas[2] = (lv_area_t){0, HEIGHT - 1, 0, HEIGHT - 1};
            count = 3;
            break;
        }
        for (unsigned i = 0; i < count; i++) change_scene(areas[i], frame + 2);
        load_damage(areas, count);
        if (frame % 11U == 0 && count < LV_INV_BUF_SIZE) {
            display.inv_areas[count] = areas[0];
            display.inv_area_joined[count] = 1;
            display.inv_p++;
        }
        profile_enabled = frame % 2U != 0; /* Pixel correctness is debug/profile independent. */
        assert(render_frame());
        assert(g_previous_buffer == frame_storage[visible_buffer]);
        assert(disp_buf.buf_act != g_previous_buffer);
        assert(g_present_sequence == frame + 2);
    }
    assert(successful_vsyncs == g_present_sequence && !disp_buf.flushing);
    unsigned sequence = g_present_sequence, previous_copies = copy_calls;
    size_t previous_damage_count = g_previous_count;
    load_damage(NULL, 0);
    assert(!render_frame() && g_present_sequence == sequence && copy_calls == previous_copies);
    assert(g_previous_count == previous_damage_count); /* idle must retain deferred debt */
    puts("display: 1800 same/moving/disjoint/multi/full/edge dirty frames with alpha composition passed");
}

static void test_copy_fallback(void)
{
    reset_pipeline();
    lv_area_t areas[] = {{1, 1, 3, 3}, {10, 10, 14, 14}, {20, 20, 25, 25}, {1, 1, 3, 3}};
    for (unsigned i = 0; i < 3; i++) change_scene(areas[i], 90 + i);
    load_damage(areas, 4);
    display.inv_area_joined[3] = 1; /* last_i must remain 2, not inv_p - 1 or 0. */
    copy_fail_at = copy_calls + 1;
    assert(render_frame());
    assert(profiled_fallbacks == 1 && g_previous_count == 1);
    assert(memcmp(&g_previous_damage[0], &full_area, sizeof(full_area)) == 0);
    assert(g_present_sequence == 2 && !disp_buf.flushing);

    copy_fail_at = 0;
    lv_area_t stripes[9];
    for (unsigned i = 0; i < 9; i++) {
        stripes[i] = (lv_area_t){2 + (int)i * 4, 0, 2 + (int)i * 4, HEIGHT - 1};
        change_scene(stripes[i], 111 + i);
    }
    load_damage(stripes, 9);
    assert(render_frame());
    assert(profiled_copied == PIXELS && profiled_saved == 0); /* fragmentation fallback */

    /* A later copy may fail after earlier pieces already changed the target. */
    reset_pipeline();
    lv_area_t center = {9, 7, 27, 19};
    change_scene(center, 143);
    load_damage(&center, 1);
    copy_fail_at = copy_calls + 2;
    assert(render_frame());
    assert(copy_calls == 2 && profiled_fallbacks == 1 && g_previous_count == 1);

    /* Drive the planner's actual 64-piece failure through production rendering,
     * not just the "too many GE calls" heuristic fallback. */
    reset_pipeline();
    lv_area_t grid_cuts[18];
    for (unsigned i = 0; i < 9; i++) {
        grid_cuts[i] = (lv_area_t){2 + (int)i * 4, 0, 2 + (int)i * 4, HEIGHT - 1};
        grid_cuts[9 + i] = (lv_area_t){0, 2 + (int)i * 3, WIDTH - 1, 2 + (int)i * 3};
    }
    for (unsigned i = 0; i < 18; i++) change_scene(grid_cuts[i], 177 + i);
    load_damage(grid_cuts, 18);
    assert(render_frame());
    assert(profiled_copied == PIXELS && profiled_saved == 0 && copy_calls == 1);
    puts("display: partial GE failure -> full redraw retains last_i; complex-plan copy fallback passed");
}

static void test_submit_recovery(unsigned failure_kind)
{
    reset_pipeline();
    lv_area_t damage = {2, 2, 18, 17};
    change_scene(damage, 221);
    load_damage(&damage, 1);
    if (failure_kind == 0) pan_failures = 2;
    else if (failure_kind == 1) vsync_failures = 2;
    else { g_pan_var_valid = 0; get_info_failures = 2; }
    unsigned previous_sequence = g_present_sequence;
    lv_color_t *previous_front = g_previous_buffer;
    assert(render_frame());
    assert(g_retry_driver == &disp_drv && disp_buf.flushing);
    assert(g_previous_buffer == previous_front && g_present_sequence == previous_sequence);
    assert(g_retry_buffer != disp_buf.buf_act);

    /* Model events after the failure in the SAME handler, including a joined
     * invalidation set and a direct lv_refr_now() call. No GE copy may occur. */
    lv_area_t suppressed[] = {{0, 0, 25, 25}, {0, 0, 2, 2}, {27, 4, 35, 16}};
    change_scene(suppressed[0], 222);
    change_scene(suppressed[2], 223);
    load_damage(suppressed, 3);
    display.inv_area_joined[1] = 1;
    unsigned copies_before = copy_calls;
    size_t previous_count = g_previous_count;
    present_rect_t previous_damage[LV_INV_BUF_SIZE];
    memcpy(previous_damage, g_previous_damage, sizeof(previous_damage));
    assert(!render_frame());
    assert(display.inv_p == 0 && refresh_timer.paused && copy_calls == copies_before);
    for (unsigned i = 0; i < LV_INV_BUF_SIZE; i++) {
        lv_area_t zero = {0};
        assert(!display.inv_area_joined[i] && memcmp(&display.inv_areas[i], &zero, sizeof(zero)) == 0);
    }
    assert(g_previous_count == previous_count && memcmp(previous_damage, g_previous_damage, sizeof(previous_damage)) == 0);
    unsigned submits_before = pan_calls + get_info_calls;
    assert(!lv_port_disp_poll());
    assert(pan_calls + get_info_calls == submits_before && disp_buf.flushing);
    clock_us += 50000;
    assert(!lv_port_disp_poll() && disp_buf.flushing); /* another transient failure */
    assert(g_present_sequence == previous_sequence && g_previous_buffer == previous_front);
    clock_us += 50000;
    assert(lv_port_disp_poll());
    assert(!g_retry_driver && !disp_buf.flushing && full_invalidations == 1);
    assert(g_present_sequence == previous_sequence + 1 && display.inv_p == 1);
    assert(!refresh_timer.paused && memcmp(&display.inv_areas[0], &full_area, sizeof(full_area)) == 0);
    assert(render_frame()); /* recovers ALL model changes suppressed during hold */
    assert_visible_reference();
    assert(lv_port_disp_poll());
}

static void test_eintr(void)
{
    reset_pipeline();
    change_scene(full_area, 334);
    load_damage(&full_area, 1);
    pan_interruptions = vsync_interruptions = 2;
    assert(render_frame());
    assert(!g_retry_driver && !disp_buf.flushing);
    assert(g_present_sequence == 2 && pan_calls == 4 && vsync_calls == 4);
    assert(get_info_calls == 0); /* cached vinfo path */
    g_live_vscreeninfo = 1;
    load_damage(&full_area, 1);
    assert(render_frame() && get_info_calls == 1);
    puts("display: PAN/VSYNC/GET_INFO retry ownership, same-handler suppression, recovery full redraw, EINTR passed");
}

int main(void)
{
    test_planner();
    test_frame_sequences();
    test_copy_fallback();
    test_submit_recovery(0);
    test_submit_recovery(1);
    test_submit_recovery(2);
    test_eintr();
    puts("present pipeline tests passed");
    return 0;
}
