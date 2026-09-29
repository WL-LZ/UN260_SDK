#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_components/ui_notice.c"
#include "message_codepoints.h"

static lv_color_t pixels[1280 * 400], buffer[1280 * 40];
static unsigned language;
const char *ui_text_get(ui_text_id_t id)
{
    if (language == 1) return id == UI_TEXT_NOTICE_ONGOING ? "进行中" : "现在";
    if (language == 2) return id == UI_TEXT_NOTICE_ONGOING ? "진행 중" : "지금";
    return id == UI_TEXT_NOTICE_ONGOING ? "Ongoing" : "Now";
}

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *color)
{
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x) pixels[y * 1280 + x] = *color++;
    lv_disp_flush_ready(driver);
}

static void advance(unsigned ms)
{
    while (ms) {
        unsigned step = ms > 10 ? 10 : ms;
        lv_tick_inc(step);
        lv_timer_handler();
        ms -= step;
    }
}

static void capture(const char *name)
{
    char path[512];
    const char *out = getenv("NOTICE_OUTPUT");
    if (!out) out = "/tmp/un260-notice-renders";
    snprintf(path, sizeof(path), "%s/%s.bgra", out, name);
    lv_refr_now(NULL);
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(pixels, sizeof(pixels), 1, file) == 1);
    fclose(file);
}

static void backdrop(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xF9FAFB), 0);
    lv_obj_t *heading = lv_label_create(lv_scr_act());
    lv_label_set_text(heading, "Menu     Preferences     Receipt");
    lv_obj_set_style_text_font(heading, &lv_font_instrument_sans_semibold_22, 0);
    lv_obj_set_pos(heading, 28, 25);
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *card = lv_obj_create(lv_scr_act());
        lv_obj_set_pos(card, 28 + 312 * i, 102);
        lv_obj_set_size(card, 288, 264);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xE8EDF0), 0);
        lv_obj_set_style_border_width(card, 0, 0);
    }
}

static void font_coverage(void)
{
    const lv_font_t *fonts[] = {
        &lv_font_instrument_sans_medium_12, &lv_font_instrument_sans_medium_14,
        &lv_font_instrument_sans_medium_16, &lv_font_instrument_sans_medium_18,
        &lv_font_instrument_sans_medium_24, &lv_font_instrument_sans_semibold_20,
        &lv_font_instrument_sans_semibold_22, &lv_font_instrument_sans_semibold_28
    };
    for (unsigned i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i) {
        lv_font_glyph_dsc_t old, current;
        const lv_font_t *wrapped = ui_message_font(fonts[i]);
        assert(wrapped != fonts[i] && wrapped->fallback && !fonts[i]->fallback);
        assert(wrapped->line_height == fonts[i]->line_height && wrapped->base_line == fonts[i]->base_line);
        assert(lv_font_get_glyph_dsc(fonts[i], &old, 'A', 0));
        assert(lv_font_get_glyph_dsc(wrapped, &current, 'A', 0));
        assert(old.adv_w == current.adv_w && old.box_w == current.box_w && old.ofs_y == current.ofs_y);
        for (unsigned j = 0; j < message_coverage[i].count; ++j)
            assert(lv_font_get_glyph_dsc(wrapped, &current, message_coverage[i].points[j], 0));
    }
    puts("message_fonts: PASS (per-role CJK coverage at eight sizes, English metrics unchanged)");
}

int main(void)
{
    static lv_disp_draw_buf_t draw_buffer;
    static lv_disp_drv_t driver;
    lv_init();
    font_coverage();
    lv_disp_draw_buf_init(&draw_buffer, buffer, NULL, 1280 * 40);
    lv_disp_drv_init(&driver);
    driver.hor_res = 1280; driver.ver_res = 400;
    driver.draw_buf = &draw_buffer; driver.flush_cb = flush;
    assert(lv_disp_drv_register(&driver));
    assert(NOTICE_ENTER_MS == NOTICE_EXIT_MS && NOTICE_EXIT_MS == 240);
    backdrop();
    unsigned children = lv_obj_get_child_cnt(lv_layer_top());
    ui_notice_post(UI_NOTICE_SUCCESS, "receipt", "Receipt settings", "Saved");
    advance(420);
    assert(ui_notice_is_visible());
    assert(lv_obj_get_x(notice.object) == 265 && lv_obj_get_y(notice.object) == 14);
    assert(lv_obj_get_child_cnt(lv_layer_top()) == children + 1);
    assert(lv_obj_get_child_cnt(notice.object) == 0);
    capture("success");
    assert(lv_color_to32(pixels[35*1280+850])==lv_color_to32(lv_color_hex(0xECEFF1)));
    assert(lv_color_to32(pixels[113*1280+850])==lv_color_to32(lv_color_hex(0xECEFF1)));
    assert(lv_color_to32(pixels[14*1280+850])==lv_color_to32(lv_color_white()));
    lv_event_send(notice.object, LV_EVENT_CLICKED, NULL);
    advance(100);
    assert(ui_notice_is_visible() && notice.leaving);
    assert(lv_obj_get_y(notice.object) < 14);
    capture("dismiss-animation");
    int sample_y=lv_obj_get_y(notice.object)+99;
    if(sample_y>=0)assert(lv_color_to32(pixels[sample_y*1280+850])==lv_color_to32(lv_color_hex(0xECEFF1)));
    advance(200);
    assert(!ui_notice_is_visible() && notice.timer->paused);
    assert(!lv_anim_get(notice.object, animation_exec));

    ui_notice_post(UI_NOTICE_PROGRESS, "export", "Exporting records", "Keep the USB drive connected.");
    advance(420);
    capture("progress");
    ui_notice_dialog_acquire();
    ui_notice_dialog_acquire();
    assert(!ui_notice_is_visible() && notice.timer->paused);
    ui_notice_dialog_release();
    assert(!ui_notice_is_visible());
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT, true);
    ui_notice_dialog_release();
    ui_notice_dialog_release(); /* Unmatched cleanup is harmless. */
    assert(!ui_notice_is_visible());
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_MODAL, true);
    assert(!ui_notice_is_visible() && notice.timer->paused);
    assert(!lv_anim_get(notice.object, animation_exec));
    advance(6000);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT, false);
    assert(!ui_notice_is_visible());
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_MODAL, false);
    assert(ui_notice_is_visible() && !notice.timer->paused);
    ui_notice_dismiss(NULL); advance(260);
    ui_notice_post(UI_NOTICE_PROGRESS, "export", "Exporting records", "Keep the USB drive connected.");
    assert(!ui_notice_is_visible());
    ui_notice_post(UI_NOTICE_ERROR, "export", "Export failed", "Check the USB drive, then try again.");
    advance(420);
    assert(ui_notice_is_visible() && notice.shown.kind == UI_NOTICE_ERROR);
    capture("error");
    ui_notice_clear("export");

    /* A task may finish while its old card is still sliding away. */
    ui_notice_post(UI_NOTICE_PROGRESS, "race", "Applying", "Waiting for reply");
    advance(420);
    ui_notice_dismiss(NULL); advance(80);
    ui_notice_post(UI_NOTICE_SUCCESS, "race", "Saved", NULL);
    advance(420);
    assert(ui_notice_is_visible() && notice.shown.kind == UI_NOTICE_SUCCESS && !notice.leaving);
    ui_notice_clear("race");

    /* Tapping during entry must continue from the current position. */
    ui_notice_post(UI_NOTICE_INFO, "early", "Information", NULL);
    advance(100);
    lv_coord_t entry_y = lv_obj_get_y(notice.object);
    ui_notice_dismiss(NULL);
    assert(lv_obj_get_y(notice.object) == entry_y);
    advance(260);
    assert(!ui_notice_is_visible());

    ui_notice_config_t short_notice = { .kind=UI_NOTICE_INFO, .key="short", .title="Timer pause", .detail="Shown time only", .duration_ms=1500 };
    ui_notice_show(&short_notice); advance(420); advance(400);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_STANDBY, true);
    uint32_t remaining = notice.state.active.remaining_ms;
    advance(10000);
    assert(notice.state.active.remaining_ms == remaining);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_STANDBY, false);
    advance(remaining - 100);
    assert(ui_notice_is_visible() && !notice.leaving);
    advance(400);
    assert(!ui_notice_is_visible());

    ui_notice_post(UI_NOTICE_PROGRESS, "long", "Applying settings", "Waiting for the device");
    advance(420);
    ui_notice_post(UI_NOTICE_WARNING, "check", "No setting result", "The last confirmed value is retained.");
    assert(notice.shown.kind == UI_NOTICE_WARNING);
    capture("warning");
    ui_notice_dismiss(NULL); advance(660);
    assert(notice.shown.kind == UI_NOTICE_PROGRESS);
    lv_obj_del(notice.object);
    assert(!notice.object && !notice.timer && !notice.state.has_active);
    ui_notice_post(UI_NOTICE_INFO, "new", "Verification cancelled", "Existing results are unchanged.");
    advance(420); capture("info");
    ui_notice_deinit();
    assert(lv_obj_get_child_cnt(lv_layer_top()) == children);
    ui_notice_clear("new");
    ui_notice_post_text(UI_NOTICE_PROGRESS,"locale","Confirm","Saved");advance(420);
    uint32_t locale_revision=notice.state.active.revision,locale_remaining=notice.state.active.remaining_ms;
    uint16_t locale_repeats=notice.state.active.repeats;
    lv_obj_t *same_object=notice.object;
    ui_lang_set(LANGUAGE_CN);ui_notice_language_changed();
    assert(!strcmp(notice.title,ui_tr("Confirm")));
    assert(notice.object==same_object&&notice.state.active.revision==locale_revision&&notice.state.active.remaining_ms==locale_remaining&&notice.state.active.repeats==locale_repeats);
    ui_notice_dismiss("locale");ui_lang_set(LANGUAGE_EN);ui_notice_language_changed();advance(300);
    ui_notice_post_text(UI_NOTICE_PROGRESS,"locale","Confirm","Saved");assert(!ui_notice_is_visible());
    language = 1;
    ui_notice_post(UI_NOTICE_SUCCESS, "receipt", "打印设置", "已保存");
    advance(420); capture("success-cn"); ui_notice_clear("receipt");
    language = 2;
    ui_notice_post(UI_NOTICE_SUCCESS, "receipt", "인쇄 설정", "설정이 저장되었습니다.");
    advance(420); capture("success-kr"); ui_notice_deinit();
    for (unsigned i = 0; i < 40; ++i) {
        ui_notice_post(UI_NOTICE_PROGRESS, "cycle", "Working", NULL);
        advance(40);
        ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT, true);
        ui_notice_deinit();
    }
    assert(lv_obj_get_child_cnt(lv_layer_top()) == children);
    assert(lv_anim_count_running() == 0);
    puts("ui_notice_view: PASS (actual LVGL geometry, draw, dismissal, pause, task results, deletion/recreation)");
    return 0;
}
