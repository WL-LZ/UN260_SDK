#!/usr/bin/env python3
"""Test the real card-surface implementation, optionally with LVGL 8 raster QA.

Default: host C interface/ownership tests against a deliberately small API shim.
This mode does NOT verify rendered pixels.

Actual software raster test (no framebuffer/device needed):
    python3 tools/test_card_surface.py --lvgl-dir /path/to/lvgl-8.3.2

The raster mode compiles LVGL's real sources with a private 32-bit transparent
software configuration, then snapshots the card and asserts all four corner
alphas, exact edge/interior colors, focus-capsule pixels, footprint and ownership
preservation. The real production NO. font also verifies footer clearance. It
does not test the board's GE/DMA path. All test builds are in a temporary folder.
"""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPONENT = ROOT / "un260/lv_components/lv_card_surface.c"

STUB = r'''
#ifndef TEST_LVGL_H
#define TEST_LVGL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t lv_style_selector_t;
typedef uint32_t lv_color_t;
typedef struct lv_obj_t {
    int width, height, x, y, radius, bg_opa, bg_grad_dir;
    int border_width, border_opa, border_side, border_post;
    int padding, shadow_width, shadow_opa, outline_width;
    int align, align_x, align_y;
    uint32_t background, border, flags;
    void *parent, *events;
} lv_obj_t;
enum { LV_PART_MAIN = 0, LV_STATE_DEFAULT = 0, LV_OPA_COVER = 255,
       LV_OPA_TRANSP = 0, LV_GRAD_DIR_NONE = 0, LV_BORDER_SIDE_FULL = 15,
       LV_RADIUS_CIRCLE = 0x7FFF, LV_ALIGN_BOTTOM_MID = 1,
       LV_OBJ_FLAG_CLICKABLE = 1, LV_OBJ_FLAG_SCROLLABLE = 2,
       LV_OBJ_FLAG_HIDDEN = 4 };
static inline lv_color_t lv_color_hex(uint32_t color) { return color; }
static inline void lv_obj_set_size(lv_obj_t *o, int w, int h)
{ o->width = w; o->height = h; }
static inline void lv_obj_clear_flag(lv_obj_t *o, uint32_t flags)
{ o->flags &= ~flags; }
static inline void lv_obj_add_flag(lv_obj_t *o, uint32_t flags)
{ o->flags |= flags; }
static inline void lv_obj_align(lv_obj_t *o, int align, int x, int y)
{ o->align = align; o->align_x = x; o->align_y = y; }
#define SETTER(name, field) \
static inline void lv_obj_set_style_##name(lv_obj_t *o, int v, lv_style_selector_t s) \
{ (void)s; o->field = v; }
SETTER(radius, radius)
SETTER(bg_color, background)
SETTER(bg_opa, bg_opa)
SETTER(bg_grad_dir, bg_grad_dir)
SETTER(border_width, border_width)
SETTER(border_color, border)
SETTER(border_opa, border_opa)
SETTER(border_side, border_side)
SETTER(border_post, border_post)
SETTER(pad_all, padding)
SETTER(shadow_width, shadow_width)
SETTER(shadow_opa, shadow_opa)
SETTER(outline_width, outline_width)
#undef SETTER
#endif
'''

UNIT_TEST = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_components/lv_card_surface.h"
int main(void)
{
    int parent = 1, events = 2;
    lv_obj_t card = {0}, before;
    lv_card_surface_style_t normal = {200, 265, 14, 0xF7F8FA, 0xDEDFE1, 1};
    lv_card_surface_style_t focus = {200, 265, 14, 0xFFFFFF, 0xBFC7CF, 1};
    card.x = 42; card.y = 57; card.flags = 0xA55A;
    card.parent = &parent; card.events = &events;
    lv_card_surface_apply(&card, &normal);
    assert(card.width == 200 && card.height == 265 && card.radius == 14);
    assert(card.background == 0xF7F8FA && card.border == 0xDEDFE1);
    assert(card.bg_opa == 255 && card.bg_grad_dir == LV_GRAD_DIR_NONE);
    assert(card.border_width == 1 && card.border_opa == 255);
    assert(card.border_side == LV_BORDER_SIDE_FULL && card.border_post == 0);
    assert(card.padding == 0 && card.shadow_width == 0);
    assert(card.shadow_opa == 0 && card.outline_width == 0);
    assert(card.x == 42 && card.y == 57 && card.flags == 0xA55A);
    assert(card.parent == &parent && card.events == &events);
    before = card;
    lv_card_surface_apply(&card, &normal);
    assert(memcmp(&before, &card, sizeof(card)) == 0);
    lv_card_surface_apply(&card, &focus);
    assert(card.background == 0xFFFFFF && card.border == 0xBFC7CF);
    assert(card.width == 200 && card.height == 265 && card.radius == 14);
    before = card;
    lv_card_surface_apply(NULL, &normal);
    lv_card_surface_apply(&card, NULL);
    normal.width = 0;
    lv_card_surface_apply(&card, &normal);
    assert(memcmp(&before, &card, sizeof(card)) == 0);
    normal.width = 20; normal.height = 10; normal.radius = 99;
    normal.border_width = 255;
    lv_card_surface_apply(&card, &normal);
    assert(card.radius == 5 && card.border_width == 5);
    normal.radius = -1;
    lv_card_surface_apply(&card, &normal);
    assert(card.radius == 0);

    lv_obj_t mark = {0};
    mark.parent = &card; mark.events = &events;
    mark.flags = 0x80 | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE;
    lv_card_surface_focus_mark_apply(&mark, false);
    assert(mark.width == 28 && mark.height == 3 && mark.radius == LV_RADIUS_CIRCLE);
    assert(mark.background == 0x4D5965 && mark.bg_opa == LV_OPA_COVER);
    assert(mark.bg_grad_dir == LV_GRAD_DIR_NONE && mark.border_width == 0);
    assert(mark.padding == 0 && mark.shadow_width == 0);
    assert(mark.shadow_opa == LV_OPA_TRANSP && mark.outline_width == 0);
    assert(mark.align == LV_ALIGN_BOTTOM_MID && mark.align_x == 0);
    assert(mark.align_y == -11); /* Content edge: outer 1px border is extra. */
    assert(mark.flags == (0x80 | LV_OBJ_FLAG_HIDDEN));
    assert(mark.parent == &card && mark.events == &events);
    before = mark;
    lv_card_surface_focus_mark_apply(&mark, false);
    assert(memcmp(&before, &mark, sizeof(mark)) == 0);
    lv_card_surface_focus_mark_apply(&mark, true);
    assert(mark.flags == 0x80 && mark.parent == &card && mark.events == &events);
    before = mark;
    lv_card_surface_focus_mark_apply(&mark, true);
    assert(memcmp(&before, &mark, sizeof(mark)) == 0);
    lv_card_surface_focus_mark_apply(NULL, false);
    lv_card_surface_focus_mark_apply(NULL, true);
    lv_card_surface_focus_mark_apply(&mark, false);
    assert(mark.flags == (0x80 | LV_OBJ_FLAG_HIDDEN));
    puts("PASS card_surface INTERFACE: geometry, states, bounds, ownership, capsule flags, idempotence (not pixel QA)");
    return 0;
}
'''

RASTER_TEST = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/lv_components/lv_card_surface.h"
#include "lvgl/src/extra/others/snapshot/lv_snapshot.h"
LV_FONT_DECLARE(lv_font_instrument_sans_medium_14);
static int clicks;
static int mark_events;
static void click_cb(lv_event_t *event) { (void)event; ++clicks; }
static void mark_cb(lv_event_t *event) { (void)event; ++mark_events; }
static void flush_cb(lv_disp_drv_t *driver, const lv_area_t *area,
                     lv_color_t *pixels)
{ (void)area; (void)pixels; lv_disp_flush_ready(driver); }

static uint32_t pixel(const lv_img_dsc_t *image, int x, int y)
{
    uint32_t color;
    assert(x >= 0 && y >= 0 && x < image->header.w && y < image->header.h);
    memcpy(&color, image->data + ((size_t)y * image->header.w + x) * 4U, 4);
    return color;
}

static void save_preview(const lv_img_dsc_t *image, const char *name)
{
    const char *directory = getenv("CARD_SURFACE_RASTER_OUTPUT");
    char path[1024];
    if (directory == NULL || directory[0] == '\0') return;
    assert(snprintf(path, sizeof(path), "%s/%s.bmp", directory, name) > 0);
    FILE *output = fopen(path, "wb");
    assert(output != NULL);
    const uint32_t row_bytes = (image->header.w * 3U + 3U) & ~3U;
    const uint32_t file_bytes = 54U + row_bytes * image->header.h;
    uint8_t header[54] = {0};
    header[0] = 'B'; header[1] = 'M';
    memcpy(header + 2, &file_bytes, 4);
    header[10] = 54; header[14] = 40;
    uint32_t width = image->header.w, height = image->header.h;
    memcpy(header + 18, &width, 4); memcpy(header + 22, &height, 4);
    header[26] = 1; header[28] = 24;
    assert(fwrite(header, 1, sizeof(header), output) == sizeof(header));
    uint8_t *row = calloc(1, row_bytes);
    assert(row != NULL);
    for (int y = image->header.h - 1; y >= 0; --y) {
        for (int x = 0; x < image->header.w; ++x) {
            const uint32_t p = pixel(image, x, y), alpha = p >> 24;
            const uint32_t checker = ((x / 12 + y / 12) & 1) ? 214U : 232U;
            for (unsigned channel = 0; channel < 3; ++channel)
                row[x * 3 + channel] = ((((p >> (channel * 8)) & 255U) * alpha +
                                          checker * (255U - alpha) + 127U) / 255U);
        }
        assert(fwrite(row, 1, row_bytes, output) == row_bytes);
    }
    free(row); fclose(output);
}

static void assert_body_pixel(const lv_img_dsc_t *image, int x, int y,
                               uint32_t background)
{
    const uint32_t p = pixel(image, x, y);
    /* LVGL 8 snapshot's generic masked fill_set_px calculates
     * (opa * mask) >> 8: even 255 * 255 becomes 254. Rounded top/bottom rows
     * take this path while the unmasked center is 255. Preserve exact RGB
     * and accept only this documented one-alpha-unit quantization, not a
     * wider color tolerance or a thicker border. */
    assert((p & 0xFFFFFFU) == background);
    assert((p >> 24) >= 254U);
}

static void verify_surface(lv_obj_t *card, lv_obj_t *mark,
                           const lv_card_surface_style_t *style, bool focused)
{
    lv_img_dsc_t image;
    lv_card_surface_apply(card, style);
    lv_card_surface_focus_mark_apply(mark, focused);
    lv_obj_update_layout(card);
    lv_area_t card_area, mark_area;
    lv_obj_get_coords(card, &card_area);
    lv_obj_get_coords(mark, &mark_area);
    assert(lv_obj_get_width(mark) == 28 && lv_obj_get_height(mark) == 3);
    assert(mark_area.x1 - card_area.x1 == 86 && mark_area.x2 - card_area.x1 == 113);
    assert(mark_area.y1 - card_area.y1 == 250 && mark_area.y2 - card_area.y1 == 252);
    assert(_lv_obj_get_ext_draw_size(card) == 0 && _lv_obj_get_ext_draw_size(mark) == 0);
    assert(lv_obj_has_flag(mark, LV_OBJ_FLAG_HIDDEN) == !focused);
    assert(!lv_obj_has_flag(mark, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    const uint32_t bytes = lv_snapshot_buf_size_needed(card, LV_IMG_CF_TRUE_COLOR_ALPHA);
    assert(bytes == 200U * 265U * 4U);
    uint8_t *buffer = malloc(bytes);
    assert(buffer != NULL);
    /* A dirty input buffer ensures the snapshot actually clears its corners. */
    memset(buffer, 0x91, bytes);
    assert(lv_snapshot_take_to_buf(card, LV_IMG_CF_TRUE_COLOR_ALPHA, &image,
                                   buffer, bytes) == LV_RES_OK);
    assert(image.header.w == 200 && image.header.h == 265);
    save_preview(&image, style->background == 0xFFFFFF ? "focus" : "normal");
    printf("RASTER body=%06X border=%06X style_width=%d radius=%d center=%08X\n",
           style->background, style->border,
           lv_obj_get_style_border_width(card, LV_PART_MAIN),
           lv_obj_get_style_radius(card, LV_PART_MAIN), pixel(&image, 100, 100));
    for (int d = 0; d < 8; ++d)
        printf("RASTER depth=%d top=%08X left=%08X bottom=%08X right=%08X\n", d,
               pixel(&image, 100, d), pixel(&image, d, 100),
               pixel(&image, 100, 264-d), pixel(&image, 199-d, 100));
    fflush(stdout);
    assert((pixel(&image, 0, 0) >> 24) == 0);
    assert((pixel(&image, 199, 0) >> 24) == 0);
    assert((pixel(&image, 0, 264) >> 24) == 0);
    assert((pixel(&image, 199, 264) >> 24) == 0);
    assert(pixel(&image, 100, 100) == (0xFF000000U | style->background));
    assert_body_pixel(&image, 100, 1, style->background);
    assert_body_pixel(&image, 1, 100, style->background);
    assert_body_pixel(&image, 198, 100, style->background);
    assert_body_pixel(&image, 100, 263, style->background);
    assert(pixel(&image, 100, 0) == (0xFF000000U | style->border));
    assert(pixel(&image, 0, 100) == (0xFF000000U | style->border));
    assert(pixel(&image, 199, 100) == (0xFF000000U | style->border));
    assert(pixel(&image, 100, 264) == (0xFF000000U | style->border));
    /* The cue is wholly inside the card. Its three central rows are opaque
     * graphite; rounded caps may be antialiased, but never extend the box. */
    for (int y = 246; y <= 258; ++y) for (int x = 80; x <= 119; ++x) {
        if (!focused || x < 86 || x > 113 || y < 250 || y > 252)
            assert_body_pixel(&image, x, y, style->background);
    }
    if (focused) {
        for (int y = 250; y <= 252; ++y) for (int x = 88; x <= 111; ++x)
            assert_body_pixel(&image, x, y, 0x4D5965);
        assert(pixel(&image, 86, 250) != pixel(&image, 100, 250));
        assert(pixel(&image, 113, 250) != pixel(&image, 100, 250));
        assert(pixel(&image, 86, 252) != pixel(&image, 100, 252));
        assert(pixel(&image, 113, 252) != pixel(&image, 100, 252));
    }
    /* Rounded edge pixels must be neutral for normal cards, never black dirt.
     * Semi-transparent anti-aliasing pixels can have any alpha, but their RGB
     * stays between the border and background colors, within rounding noise. */
    if (style->background == 0xF7F8FA) {
        for (int y = 0; y < 265; ++y) for (int x = 0; x < 200; ++x) {
            uint32_t p = pixel(&image, x, y);
            if ((p >> 24) >= 16) {
                assert(((p >> 16) & 255U) >= 220U);
                assert(((p >> 8) & 255U) >= 222U);
                assert((p & 255U) >= 223U);
            }
        }
    }
    free(buffer);
}

int main(void)
{
    static lv_color_t framebuffer[240 * 300];
    lv_disp_draw_buf_t draw_buffer;
    lv_disp_drv_t driver;
    lv_card_surface_style_t normal = {200, 265, 14, 0xF7F8FA, 0xDEDFE1, 1};
    lv_card_surface_style_t focus = {200, 265, 14, 0xFFFFFF, 0xBFC7CF, 1};
    lv_init();
    lv_disp_draw_buf_init(&draw_buffer, framebuffer, NULL, 240 * 300);
    lv_disp_drv_init(&driver);
    driver.hor_res = 240; driver.ver_res = 300;
    driver.draw_buf = &draw_buffer; driver.flush_cb = flush_cb;
    assert(lv_disp_drv_register(&driver) != NULL);
    lv_obj_t *parent = lv_scr_act();
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, 17, 23);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *mark = lv_obj_create(card);
    lv_obj_remove_style_all(mark);
    lv_obj_add_flag(mark, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(mark, mark_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_card_surface_apply(card, &focus);
    lv_card_surface_focus_mark_apply(mark, true);
    /* Measure the real footer text at the production location, then remove
     * only this test fixture so the preview remains a reusable plain skin. */
    lv_obj_t *number = lv_label_create(card);
    lv_label_set_text(number, "NO.34");
    lv_obj_set_style_text_font(number, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_pos(number, 21, 224);
    lv_obj_update_layout(card);
    lv_area_t number_area, mark_area, card_area;
    lv_obj_get_coords(number, &number_area);
    lv_obj_get_coords(mark, &mark_area);
    lv_obj_get_coords(card, &card_area);
    assert(lv_obj_get_height(number) == 15);
    assert(number_area.y2 - card_area.y1 == 239);
    assert(mark_area.y1 - number_area.y2 - 1 >= 8);
    printf("RASTER footer_bottom=%d mark_top=%d clear_rows=%d\n",
           number_area.y2 - card_area.y1, mark_area.y1 - card_area.y1,
           mark_area.y1 - number_area.y2 - 1);
    lv_obj_del(number);
    verify_surface(card, mark, &normal, false);
    verify_surface(card, mark, &focus, true);
    verify_surface(card, mark, &normal, false);
    assert(lv_obj_get_x(card) == 17 && lv_obj_get_y(card) == 23);
    assert(lv_obj_get_parent(card) == parent);
    assert(lv_obj_has_flag(card, LV_OBJ_FLAG_CLICKABLE));
    assert(!lv_obj_has_flag(card, LV_OBJ_FLAG_SCROLLABLE));
    assert(lv_obj_get_child_cnt(card) == 1 && lv_obj_get_parent(mark) == card);
    lv_event_send(mark, LV_EVENT_VALUE_CHANGED, NULL);
    assert(mark_events == 1);
    lv_event_send(card, LV_EVENT_CLICKED, NULL);
    assert(clicks == 1);
    lv_obj_del(card);
    puts("PASS card_surface LVGL8 SOFTWARE PIXELS: four transparent corners, exact 1px silver border, 28x3 focus-only capsule, footer clearance, zero expansion, ownership (not GE/DMA QA)");
    return 0;
}
'''

CONF = '''#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_COLOR_SCREEN_TRANSP 1
#define LV_MEM_SIZE (1024U * 1024U)
#define LV_USE_LOG 0
#define LV_USE_SNAPSHOT 1
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#endif
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", type=Path, help="Full LVGL 8.3.2 source tree; enables actual software pixel QA")
    parser.add_argument("--output-dir", type=Path, help="Keep actual LVGL raster BMPs over a transparency checkerboard")
    args = parser.parse_args()
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        raise SystemExit("Host C compiler required (set CC); no tests were run")
    with tempfile.TemporaryDirectory(prefix="un260-card-surface-") as directory:
        temporary = Path(directory)
        include = temporary / "include"
        (include / "lvgl").mkdir(parents=True)
        executable = temporary / ("test.exe" if os.name == "nt" else "test")
        source = temporary / "test.c"
        compiler_command = [compiler] if Path(compiler).is_file() else shlex.split(compiler)
        command = compiler_command + ["-std=c99", "-O1", "-g", "-Wall", "-Wextra",
                    "-I" + str(include), "-I" + str(ROOT)]
        if args.lvgl_dir:
            lvgl = args.lvgl_dir.resolve()
            required = [lvgl / "lvgl.h", lvgl / "src/extra/others/snapshot/lv_snapshot.c"]
            if any(not path.is_file() for path in required):
                raise SystemExit("--lvgl-dir must contain the full LVGL 8 source tree; raster QA was not run")
            (include / "lvgl/lvgl.h").write_text('#include "' + (lvgl / "lvgl.h").as_posix() + '"\n')
            snapshot_alias = include / "lvgl/src/extra/others/snapshot"
            snapshot_alias.mkdir(parents=True)
            (snapshot_alias / "lv_snapshot.h").write_text('#include "' +
                (lvgl / "src/extra/others/snapshot/lv_snapshot.h").as_posix() + '"\n')
            configuration = temporary / "lv_conf.h"
            configuration.write_text(CONF)
            source.write_text(RASTER_TEST)
            # LVGL 8 stringifies LV_CONF_PATH internally; do not quote it twice.
            command += ["-DLV_CONF_PATH=" + configuration.as_posix(), "-I" + str(lvgl)]
            command += [str(path) for path in sorted((lvgl / "src").rglob("*.c"))]
            number_font = ROOT / "un260/font/lv_font_instrument_sans_medium_14.c"
            if not number_font.is_file():
                raise SystemExit("Production Instrument Sans 14 font source is required for footer-clearance raster QA")
            command += [str(number_font)]
        else:
            (include / "lvgl/lvgl.h").write_text(STUB)
            source.write_text(UNIT_TEST)
            command += ["-Werror"]
        command += [str(source), str(COMPONENT), "-lm", "-o", str(executable)]
        if os.name != "nt":
            command += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            environment["CARD_SURFACE_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        subprocess.run([str(executable)], env=environment, check=True)


if __name__ == "__main__":
    main()
