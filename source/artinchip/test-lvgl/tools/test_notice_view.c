#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_components/ui_notice.c"
#include "un260/font/ui_locale_font_bank.h"
#include "message_codepoints.h"
#include "page_transition_fixture.h"

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
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xF9FAFB), 0);
    lv_obj_t *heading = lv_label_create(lv_scr_act());
    lv_label_set_text(heading, "Menu     Preferences     Receipt");
    lv_obj_set_style_text_font(heading, &lv_font_instrument_sans_semibold_22, 0);
    lv_obj_set_pos(heading, 28, 25);
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *card = lv_obj_create(lv_scr_act());
        lv_obj_set_pos(card, 28 + 312 * i, 102);
        lv_obj_set_size(card, 288, 264);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
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

static void test_top_popup_switch(void)
{
    unsigned children=lv_obj_get_child_cnt(lv_layer_top());
    assert(ui_notice_get_enabled());
    ui_notice_set_enabled(false);
    ui_notice_config_t ignored={.kind=UI_NOTICE_INFO,.key="ignored",.title="Ignored while disabled"};
    assert(!ui_notice_show(&ignored) && !notice.object && !notice.timer);
    ui_notice_post_text(UI_NOTICE_SUCCESS,"menu","Confirm","Saved");
    ui_notice_post(UI_NOTICE_ERROR,"export","Export failed",NULL);
    assert(!notice.state.has_active && !notice.state.queued_count && !ui_notice_is_visible());
    ui_notice_set_enabled(true);
    assert(!ui_notice_is_visible() && !notice.state.has_active);
    /* Switching off during entry immediately stops the animation and all queued work. */
    ui_notice_post(UI_NOTICE_PROGRESS,"running","Applying settings",NULL);
    advance(480);
    assert(notice.animating && lv_anim_get(notice.object,animation_exec));
    ui_notice_post(UI_NOTICE_PROGRESS,"queued","Another task",NULL);
    assert(notice.state.queued_count);
    ui_notice_set_enabled(false);
    assert(!ui_notice_is_visible() && !notice.state.has_active && !notice.state.queued_count);
    assert(notice.timer->paused && !lv_anim_get(notice.object,animation_exec));
    advance(6000);
    ui_notice_set_enabled(true);
    assert(!ui_notice_is_visible() && notice.timer->paused);
    /* Dialog/fault blockers retain ownership across switch changes. */
    ui_notice_dialog_acquire();
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT,true);
    ui_notice_set_enabled(false);
    ui_notice_post(UI_NOTICE_SUCCESS,"ignored","Saved",NULL);
    ui_notice_set_enabled(true);
    ui_notice_post(UI_NOTICE_PROGRESS,"blocked","Waiting for the device",NULL);
    assert(!ui_notice_is_visible() && notice.state.has_active);
    ui_notice_dialog_release();
    assert(!ui_notice_is_visible());
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT,false);
    assert(notice.pending && !ui_notice_is_visible());
    advance(820);
    lv_event_send(notice.object,LV_EVENT_PRESSED,NULL);
    assert(notice.held && notice.timer->paused);
    ui_notice_set_enabled(false);
    assert(!notice.held && !notice.animating && !notice.state.has_active);
    lv_event_send(notice.object,LV_EVENT_RELEASED,NULL);
    assert(!ui_notice_is_visible() && notice.timer->paused);
    /* Deleting/recreating the view cannot silently turn Top popup back on. */
    ui_notice_deinit();
    assert(!ui_notice_get_enabled() && lv_obj_get_child_cnt(lv_layer_top())==children);
    ui_notice_post(UI_NOTICE_SUCCESS,"menu","Saved",NULL);
    assert(!notice.object && !notice.timer && !ui_notice_is_visible());
    ui_notice_set_enabled(true);
    ui_notice_post(UI_NOTICE_SUCCESS,"fresh","A new result",NULL);
    assert(notice.pending && !ui_notice_is_visible() && !strcmp(notice.state.active.key,"fresh"));
    advance(820);
    ui_notice_deinit();
    assert(lv_obj_get_child_cnt(lv_layer_top())==children);
}

static void test_delayed_notice(void)
{
    ui_notice_post(UI_NOTICE_PROGRESS,"delay","Working",NULL);
    assert(notice.pending && !ui_notice_is_visible() && !notice.animating);
    advance(399);assert(!ui_notice_is_visible());
    ui_notice_post(UI_NOTICE_SUCCESS,"delay","Saved",NULL);
    advance(1);assert(notice.visible && notice.animating && notice.shown.kind==UI_NOTICE_SUCCESS);
    advance(400);assert(notice.state.active.remaining_ms>1700);
    ui_notice_clear("delay");assert(!notice.pending && notice.timer->paused);
    ui_notice_post(UI_NOTICE_PROGRESS,"cancel","Working",NULL);
    advance(200);ui_notice_clear("cancel");advance(1000);
    assert(!notice.visible && notice.timer->paused && !notice.state.has_active);
    ui_notice_deinit();
    puts("PASS automatic notice delay: exact 400ms, progress/result coalescing, full lifetime, clear cancellation");
}

/* The manager normally swaps retained roots on one screen; transient pages
 * also delete their roots. Full-screen replacement verifies the top-layer owner. */
typedef enum { NAV_CACHED, NAV_RECREATE, NAV_SCREEN } navigation_kind_t;
static lv_obj_t *navigation_screen, *navigation_roots[2];
static unsigned navigation_current;

static void navigation_create_roots(void)
{
    navigation_roots[0] = lv_obj_create(navigation_screen);
    navigation_roots[1] = lv_obj_create(navigation_screen);
    navigation_current = 0;
    lv_obj_add_flag(navigation_roots[1], LV_OBJ_FLAG_HIDDEN);
}

static void navigate(navigation_kind_t kind)
{
    ui_notice_state_t state;
    ui_notice_item_t shown;
    memcpy(&state, &notice.state, sizeof(state));
    memcpy(&shown, &notice.shown, sizeof(shown));
    lv_obj_t *object = notice.object;
    lv_timer_t *timer = notice.timer;
    lv_anim_t *motion = lv_anim_get(object, animation_exec);
    int32_t animation_time = motion ? motion->act_time : 0;
    uint32_t last_tick = notice.last_tick, last_run = timer->last_run, period = timer->period;
    bool paused = timer->paused, pending = notice.pending, visible = notice.visible;
    bool animating = notice.animating, leaving = notice.leaving, held = notice.held;
    lv_opa_t alpha = notice.opacity;
    lv_obj_update_layout(object);
    lv_coord_t y = lv_obj_get_y(object);

    assert(ui_manager_transition_begin());
    if (kind == NAV_SCREEN) {
        lv_obj_t *old_screen = navigation_screen;
        navigation_screen = lv_obj_create(NULL);
        navigation_create_roots();
        lv_scr_load(navigation_screen);
        lv_obj_del(old_screen);
    } else {
        unsigned previous = navigation_current;
        navigation_current ^= 1;
        lv_obj_add_flag(navigation_roots[previous], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(navigation_roots[navigation_current], LV_OBJ_FLAG_HIDDEN);
        if (kind == NAV_RECREATE) {
            lv_obj_del(navigation_roots[previous]);
            navigation_roots[previous] = lv_obj_create(navigation_screen);
            lv_obj_add_flag(navigation_roots[previous], LV_OBJ_FLAG_HIDDEN);
        }
    }
    ui_manager_transition_finish();
    lv_obj_update_layout(object);

    assert(notice.object == object && lv_obj_get_parent(object) == lv_layer_top());
    assert(notice.timer == timer && timer->paused == paused);
    assert(timer->last_run == last_run && timer->period == period && notice.last_tick == last_tick);
    assert(notice.pending == pending && notice.visible == visible);
    assert(notice.animating == animating && notice.leaving == leaving && notice.held == held);
    assert(notice.opacity == alpha && lv_obj_get_y(object) == y);
    assert(lv_anim_get(object, animation_exec) == motion);
    assert(!motion || motion->act_time == animation_time);
    assert(!memcmp(&notice.state, &state, sizeof(state)));
    assert(!memcmp(&notice.shown, &shown, sizeof(shown)));
}

static void advance_to(uint32_t tick)
{
    int32_t remaining = (int32_t)(tick - lv_tick_get());
    assert(remaining >= 0);
    advance((unsigned)remaining);
}

static void test_navigation_lifecycle(void)
{
    lv_obj_t *original_screen = lv_scr_act();
    navigation_screen = lv_obj_create(NULL);
    navigation_create_roots();
    lv_scr_load(navigation_screen);

    ui_notice_post(UI_NOTICE_SUCCESS, "navigation", "Setting saved", NULL);
    assert(notice.pending && !notice.visible);
    advance(100); navigate(NAV_CACHED);
    advance(100); navigate(NAV_RECREATE);
    advance(199); navigate(NAV_SCREEN);
    assert(notice.pending && !notice.visible);
    advance(1);
    assert(notice.visible && notice.animating && !notice.pending);
    advance(100); navigate(NAV_CACHED);
    advance(80); navigate(NAV_RECREATE);
    advance(100); navigate(NAV_SCREEN);
    advance(140);
    assert(notice.visible && !notice.animating && notice.opacity == LV_OPA_COVER);

    uint32_t deadline = notice.last_tick + notice.state.active.remaining_ms;
    for (unsigned i = 0; i < 12; ++i) {
        navigate((navigation_kind_t)(i % 3));
        advance(80);
        assert(notice.visible && !notice.leaving);
    }
    advance_to(deadline - 10);
    navigate(NAV_CACHED);
    advance(10);
    assert(notice.visible && notice.leaving && !notice.state.has_active);
    navigate(NAV_SCREEN);
    advance(60); navigate(NAV_RECREATE);
    advance(60); navigate(NAV_CACHED);
    advance(100);
    assert(!notice.visible && !notice.pending && !notice.animating && notice.timer->paused);
    for (unsigned i = 0; i < 6; ++i) {
        navigate((navigation_kind_t)(i % 3));
        advance(100);
        assert(!notice.visible && !notice.state.has_active);
    }

    /* Promotion during exit must preserve the old visual until it leaves,
     * followed by exactly one delayed presentation of the queued item. */
    ui_notice_config_t first = {
        .kind=UI_NOTICE_SUCCESS, .key="first", .title="First result", .duration_ms=400
    };
    ui_notice_config_t second = {
        .kind=UI_NOTICE_INFO, .key="second", .title="Second result", .duration_ms=600
    };
    assert(ui_notice_show(&first) && ui_notice_show(&second));
    assert(notice.state.queued_count == 1 && !strcmp(notice.state.active.key, "first"));
    navigate(NAV_CACHED);
    advance(480); navigate(NAV_SCREEN);
    advance(340); navigate(NAV_RECREATE);
    assert(!notice.animating && !strcmp(notice.shown.key, "first"));
    ui_notice_dismiss("first");
    assert(notice.leaving && notice.state.has_active && !notice.state.queued_count);
    assert(!strcmp(notice.shown.key, "first") && !strcmp(notice.state.active.key, "second"));
    navigate(NAV_CACHED);
    advance(80); navigate(NAV_SCREEN);
    advance(140);
    assert(notice.pending && !notice.visible && !notice.animating);
    deadline = notice.timer->last_run + NOTICE_DELAY_MS;
    navigate(NAV_RECREATE);
    advance_to(deadline - 1);
    navigate(NAV_CACHED);
    assert(!notice.visible);
    advance(1);
    assert(notice.visible && notice.animating && !strcmp(notice.shown.key, "second"));
    advance(120); navigate(NAV_SCREEN);
    advance(300);
    assert(!notice.animating);
    deadline = notice.last_tick + notice.state.active.remaining_ms;
    advance(200); navigate(NAV_RECREATE);
    advance_to(deadline);
    assert(notice.leaving && !notice.state.has_active);
    navigate(NAV_CACHED);
    advance(220);
    assert(!notice.visible && !notice.pending && notice.timer->paused);

    /* Crossing a page boundary must not suppress a new terminal service result. */
    ui_notice_post(UI_NOTICE_PROGRESS, "operation", "Applying settings", NULL);
    advance(820);
    navigate(NAV_SCREEN);
    ui_notice_post(UI_NOTICE_SUCCESS, "operation", "Setting saved", NULL);
    assert(notice.visible && !notice.animating && notice.shown.kind == UI_NOTICE_SUCCESS);
    navigate(NAV_RECREATE);
    deadline = notice.last_tick + notice.state.active.remaining_ms;
    advance_to(deadline);
    navigate(NAV_CACHED);
    advance(220);
    assert(!notice.visible && !notice.state.has_active && notice.timer->paused);

    ui_notice_deinit();
    lv_scr_load(original_screen);
    lv_obj_del(navigation_screen);
    navigation_screen = NULL;
    assert(!g_page_switch_committing && !g_page_input_unlock_timer);
    puts("PASS navigation lifetime: production transition completion, cached/transient/screen roots, pending/entry/dwell/empty exit/queued promotion, no replay or timer extension, new terminal result");
}

static void test_interrupted_notice(void)
{
    const unsigned times[] = {0,20,80,200,360};
    for (unsigned i=0;i<sizeof(times)/sizeof(times[0]);++i) {
        ui_notice_post(UI_NOTICE_PROGRESS,"fast","Receipt settings","Applying settings...");
        advance(400);advance(times[i]);lv_obj_update_layout(notice.object);
        lv_coord_t y=lv_obj_get_y(notice.object);lv_opa_t alpha=notice.opacity;
        lv_anim_t *motion=lv_anim_get(notice.object,animation_exec);
        assert(motion && notice.animating);
        ui_notice_post(UI_NOTICE_SUCCESS,"fast","Receipt settings","Setting saved.");
        lv_obj_update_layout(notice.object);
        assert(notice.animating && lv_anim_get(notice.object,animation_exec)==motion);
        assert(lv_obj_get_y(notice.object)==y && notice.opacity==alpha);
        advance(420-times[i]);
        assert(!notice.animating && notice.opacity==LV_OPA_COVER && lv_obj_get_y(notice.object)==NOTICE_Y);
        assert(notice.state.active.remaining_ms>1600); /* Starts after entry. */
        ui_notice_clear("fast");
    }
    ui_notice_post(UI_NOTICE_PROGRESS,"active","Working",NULL);advance(480);
    ui_notice_post(UI_NOTICE_INFO,"queued","Queued",NULL);
    lv_anim_t *motion=lv_anim_get(notice.object,animation_exec);lv_opa_t alpha=notice.opacity;
    ui_notice_clear("queued");
    assert(notice.animating && lv_anim_get(notice.object,animation_exec)==motion && notice.opacity==alpha);
    advance(820);ui_notice_dismiss(NULL);advance(80);lv_obj_update_layout(notice.object);
    lv_coord_t y=lv_obj_get_y(notice.object);alpha=notice.opacity;
    ui_notice_post(UI_NOTICE_SUCCESS,"active","Done",NULL);lv_obj_update_layout(notice.object);
    assert(notice.animating && !notice.leaving && notice.opacity==alpha && lv_obj_get_y(notice.object)==y);
    advance(820);ui_notice_clear(NULL);
    ui_notice_dialog_acquire();ui_notice_dialog_acquire();
    ui_notice_post(UI_NOTICE_PROGRESS,"blocked","Working",NULL);
    ui_notice_post(UI_NOTICE_SUCCESS,"blocked","Saved",NULL);
    ui_notice_dialog_release();assert(!notice.visible);
    ui_notice_dialog_release();assert(notice.pending && !notice.visible);
    advance(480);assert(notice.opacity>0 && notice.opacity<LV_OPA_COVER);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT,true);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT,false);
    assert(notice.pending && !notice.visible);advance(820);
    assert(notice.opacity==LV_OPA_COVER && lv_obj_get_y(notice.object)==NOTICE_Y);
    ui_notice_deinit();
    puts("PASS notice motion: 0/20/80/200/360ms ACK, queued removal, exit reversal, nested dialog/fault resume, full visible lifetime");
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
    test_delayed_notice();
    test_navigation_lifecycle();
    test_top_popup_switch();
    test_interrupted_notice();
    assert(NOTICE_ENTER_MS == 380 && NOTICE_EXIT_MS == 180);
    backdrop();
    unsigned children = lv_obj_get_child_cnt(lv_layer_top());
    ui_notice_post(UI_NOTICE_SUCCESS, "receipt", "Receipt settings", "Saved");
    advance(820);
    assert(ui_notice_is_visible());
    assert(lv_obj_get_x(notice.object) == 460 && lv_obj_get_y(notice.object) == 12);
    assert(lv_obj_get_child_cnt(lv_layer_top()) == children + 1);
    assert(lv_obj_get_child_cnt(notice.object) == 0);
    capture("success");
    assert(lv_obj_get_width(notice.object) == 360 && lv_obj_get_height(notice.object) == 72);
    assert(notice.opacity == LV_OPA_COVER);
    assert(lv_color_to32(pixels[70*1280+780]) == lv_color_to32(lv_color_hex(0xF5F7FA)));
    lv_event_send(notice.object, LV_EVENT_CLICKED, NULL);
    advance(100);
    assert(ui_notice_is_visible() && notice.leaving);
    assert(lv_obj_get_y(notice.object) <= 12 && notice.opacity < LV_OPA_COVER);
    capture("dismiss-animation");
    advance(200);
    assert(!ui_notice_is_visible() && notice.timer->paused);
    assert(!lv_anim_get(notice.object, animation_exec));

    ui_notice_post(UI_NOTICE_PROGRESS, "export", "Exporting records", "Keep the USB drive connected.");
    advance(820);
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
    assert(!ui_notice_is_visible() && notice.pending && !notice.timer->paused);
    advance(820);
    ui_notice_dismiss(NULL); advance(260);
    ui_notice_post(UI_NOTICE_PROGRESS, "export", "Exporting records", "Keep the USB drive connected.");
    assert(!ui_notice_is_visible());
    ui_notice_post(UI_NOTICE_ERROR, "export", "Export failed", "Check the USB drive, then try again.");
    advance(820);
    assert(ui_notice_is_visible() && notice.shown.kind == UI_NOTICE_ERROR);
    capture("error");
    ui_notice_clear("export");

    /* A task may finish while its old card is still sliding away. */
    ui_notice_post(UI_NOTICE_PROGRESS, "race", "Applying", "Waiting for reply");
    advance(820);
    ui_notice_dismiss(NULL); advance(80);
    ui_notice_post(UI_NOTICE_SUCCESS, "race", "Saved", NULL);
    advance(820);
    assert(ui_notice_is_visible() && notice.shown.kind == UI_NOTICE_SUCCESS && !notice.leaving);
    ui_notice_clear("race");

    /* Tapping during entry must continue from the current position. */
    ui_notice_post(UI_NOTICE_INFO, "early", "Information", NULL);
    advance(500);
    lv_obj_update_layout(notice.object);
    lv_coord_t entry_y = lv_obj_get_y(notice.object);
    lv_opa_t entry_opacity = notice.opacity;
    ui_notice_dismiss(NULL);
    assert(lv_obj_get_y(notice.object) == entry_y && notice.opacity == entry_opacity);
    advance(260);
    assert(!ui_notice_is_visible());

    ui_notice_config_t short_notice = { .kind=UI_NOTICE_INFO, .key="short", .title="Timer pause", .detail="Shown time only", .duration_ms=1500 };
    ui_notice_show(&short_notice); advance(820); advance(400);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_STANDBY, true);
    uint32_t remaining = notice.state.active.remaining_ms;
    advance(10000);
    assert(notice.state.active.remaining_ms == remaining);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_STANDBY, false);
    assert(notice.pending && !notice.visible);
    advance(820);
    advance(remaining - 100);
    assert(ui_notice_is_visible() && !notice.leaving);
    advance(400);
    assert(!ui_notice_is_visible());

    ui_notice_post(UI_NOTICE_PROGRESS, "long", "Applying settings", "Waiting for the device");
    advance(820);
    ui_notice_post(UI_NOTICE_WARNING, "check", "No setting result", "The last confirmed value is retained.");
    assert(notice.shown.kind == UI_NOTICE_WARNING);
    capture("warning");
    ui_notice_dismiss(NULL); advance(1060);
    assert(notice.shown.kind == UI_NOTICE_PROGRESS);
    lv_obj_del(notice.object);
    assert(!notice.object && !notice.timer && !notice.state.has_active);
    ui_notice_post(UI_NOTICE_INFO, "new", "Verification cancelled", "Existing results are unchanged.");
    advance(820); capture("info");
    ui_notice_deinit();
    assert(lv_obj_get_child_cnt(lv_layer_top()) == children);
    ui_notice_clear("new");
    ui_notice_post_text(UI_NOTICE_PROGRESS,"locale","Confirm","Saved");advance(820);
    uint32_t locale_revision=notice.state.active.revision,locale_remaining=notice.state.active.remaining_ms;
    uint16_t locale_repeats=notice.state.active.repeats;
    lv_obj_t *same_object=notice.object;
    ui_lang_set(LANGUAGE_CN);ui_notice_language_changed();
    assert(!strcmp(notice.title,ui_tr("Confirm")));
    assert(notice.object==same_object&&notice.state.active.revision==locale_revision&&notice.state.active.remaining_ms==locale_remaining&&notice.state.active.repeats==locale_repeats);
    ui_notice_dismiss("locale");ui_lang_set(LANGUAGE_EN);ui_notice_language_changed();advance(300);
    ui_notice_post_text(UI_NOTICE_PROGRESS,"locale","Confirm","Saved");assert(!ui_notice_is_visible());
    /* Render all full localized catalog strings at the selected compact width. */
    ui_notice_clear(NULL);
    ui_notice_post(UI_NOTICE_WARNING, "wrap", "Update result unknown",
        "Keep power connected. The update may still be running. Check the machine before starting another test.");
    advance(820);
    assert(notice.detail_height > ui_message_font(&lv_font_instrument_sans_medium_12)->line_height);
    assert(lv_obj_get_x(notice.object) == 460 && lv_obj_get_y(notice.object) == 12);
    assert(notice.copy_y >= 12 && notice.copy_y + notice.title_height + 2 + notice.detail_height <= notice.height - 12);
    capture("long-warning");
    ui_notice_clear("wrap");
    ui_notice_post(UI_NOTICE_PROGRESS, "replace", "Start", "Waiting for controller...");
    advance(820);
    lv_obj_t *shared = notice.object;
    ui_notice_post(UI_NOTICE_SUCCESS, "replace", "Start", "Start mode confirmed");
    assert(notice.object == shared && !notice.animating && notice.opacity == LV_OPA_COVER);
    assert(lv_obj_get_x(notice.object) == 460 && lv_obj_get_y(notice.object) == 12);
    capture("start-confirmed");
    ui_notice_clear("replace");
    char huge[UI_NOTICE_DETAIL_CAPACITY];
    memset(huge, 'W', sizeof(huge)-1); huge[sizeof(huge)-1] = 0;
    ui_notice_post(UI_NOTICE_WARNING,"capacity",huge,huge); advance(820);
    assert(notice.title_height <= 2 * notice.title_font->line_height + 2);
    assert(notice.height + NOTICE_Y + NOTICE_SHADOW_EXTENT < 400);
    assert(!strcmp(notice.detail, huge));
    capture("capacity-wrap"); ui_notice_clear("capacity");
    /* Wide backdrop reveals shadow damage/trails when a large card contracts. */
    ui_notice_post(UI_NOTICE_WARNING,"contract","Notice","Long explanation with enough words to require several lines and a taller card. The existing result has not been changed.");
    advance(820); int taller = notice.height;
    ui_notice_post(UI_NOTICE_SUCCESS,"contract","Saved",NULL);
    assert(notice.height <= taller && notice.height == NOTICE_HEIGHT);
    capture("contracted"); ui_notice_clear("contract");
    language = 1;
    ui_notice_post(UI_NOTICE_SUCCESS, "receipt", "打印设置", "已保存");
    advance(820); capture("success-cn"); ui_notice_clear("receipt");
    language = 2;
    ui_notice_post(UI_NOTICE_SUCCESS, "receipt", "인쇄 설정", "설정이 저장되었습니다.");
    advance(820); capture("success-kr"); ui_notice_deinit();
    ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);
    for(size_t i=1;i<ui_lang_count();++i){
        const ui_locale_t *locale=ui_lang_at(i);assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
        ui_notice_post_text(UI_NOTICE_SUCCESS,"locale-shape","Language changed","Settings saved");advance(820);
        lv_obj_t *probe=lv_label_create(lv_scr_act());lv_label_set_text(probe,ui_tr("Language changed"));
        assert(!strcmp(notice.title,lv_label_get_text(probe)));lv_obj_del(probe);
        const char *texts[]={notice.title,notice.detail};
        const lv_font_t *faces[]={notice.title_font,ui_message_font(&lv_font_instrument_sans_medium_12)};
        for(unsigned j=0;j<2;++j){uint32_t offset=0;while(texts[j][offset]){
            uint32_t cp=_lv_txt_encoded_next(texts[j],&offset);lv_font_glyph_dsc_t glyph;
            if(cp>32)assert(lv_font_get_glyph_dsc(faces[j],&glyph,cp,0)&&!glyph.is_placeholder);
        }}
        char file[64];snprintf(file,sizeof(file),"notice-locale-%s",locale->tag);capture(file);ui_notice_deinit();
    }
    ui_lang_set(LANGUAGE_EN);
    for (unsigned i = 0; i < 40; ++i) {
        ui_notice_post(UI_NOTICE_PROGRESS, "cycle", "Working", NULL);
        advance(40);
        ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT, true);
        ui_notice_deinit();
    }
    assert(lv_obj_get_child_cnt(lv_layer_top()) == children);
    assert(lv_anim_count_running() == 0);
    puts("ui_notice_view: PASS (compact geometry/wrapping/fade/update, Top popup off during entry/press, queue clearing, no replay, independent blockers, deletion/recreation, actual LVGL draw and lifecycle)");
    return 0;
}
