/* Focused real-LVGL tutorial test; surrounding I/O uses the existing Main fixture. */
#include "lvgl/lvgl.h"
#define main main_suite_not_run
#include "test_main_view.c"
#undef main
#include "un260/lv_components/lv_content_pager.h"
#include "un260/lv_components/lv_damped_button.h"

static void guide_expect_text(lv_obj_t *label, ui_text_id_t id)
{
    lv_obj_t *expected = lv_label_create(lv_scr_act());
    lv_label_set_text(expected, ui_text_get(id));
    assert(lv_label_get_text(label)[0]);
    assert(!strcmp(lv_label_get_text(label), lv_label_get_text(expected)));
    lv_obj_del(expected);
    if (ui_lang_get() != LANGUAGE_EN)
        assert(strcmp(ui_text_get(id), ui_tr_for(LANGUAGE_EN, ui_text_msgid(id))));
}

static void guide_check_label(lv_obj_t *label, unsigned pixels, bool one_line)
{
    assert(lv_obj_check_type(label, &lv_label_class));
    const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
    if (ui_lang_get() != LANGUAGE_EN)
        assert(font->fallback == ui_locale_font_get(ui_lang_get(), pixels));
    const char *text = lv_label_get_text(label);
    uint32_t offset = 0;
    while (text[offset]) {
        uint32_t cp = _lv_txt_encoded_next(text, &offset);
        if (cp <= 32 || (cp >= 0x200b && cp <= 0x200f) ||
            (cp >= 0x202a && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069)) continue;
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, cp, 0) || glyph.is_placeholder) {
            fprintf(stderr, "guide missing glyph locale=%s U+%04X text=%s\n",
                    ui_lang_current()->tag, cp, text);
            assert(false);
        }
        assert(glyph.resolved_font);
        if (glyph.box_w && glyph.box_h &&
            !lv_font_get_glyph_bitmap(glyph.resolved_font, cp)) {
            fprintf(stderr, "guide missing bitmap locale=%s U+%04X base=%p resolved=%p box=%ux%u text=%s\n",
                    ui_lang_current()->tag, cp, (const void *)font,
                    (const void *)glyph.resolved_font, glyph.box_w, glyph.box_h, text);
            assert(false);
        }
    }
    lv_point_t extent;
    lv_txt_get_size(&extent, text, font,
        lv_obj_get_style_text_letter_space(label, 0),
        lv_obj_get_style_text_line_space(label, 0),
        one_line ? LV_COORD_MAX : lv_obj_get_width(label), LV_TEXT_FLAG_NONE);
    if (extent.x > lv_obj_get_width(label) || extent.y > lv_obj_get_height(label)) {
        fprintf(stderr, "guide overflow locale=%s text=%s extent=%dx%d box=%dx%d\n",
                ui_lang_current()->tag, text, extent.x, extent.y,
                lv_obj_get_width(label), lv_obj_get_height(label));
        assert(false);
    }
    if (one_line) assert(extent.y <= font->line_height);
}

static void guide_locale_case(const ui_locale_t *locale, unsigned baseline_children,
                              unsigned baseline_timers, unsigned baseline_animations)
{
    assert(ui_lang_prepare_resources(locale->id));
    ui_lang_set(locale->id);
    gesture_guide_show();
    assert(gesture_guide_is_open());
    tick(260);
    lv_obj_t *overlay = lv_obj_get_child(lv_layer_top(), -1);
    lv_obj_t *panel = lv_obj_get_child(overlay, 0);
    assert(lv_obj_get_child_cnt(lv_layer_top()) == baseline_children + 1);
    assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_SCROLLABLE));
    assert(!lv_obj_has_flag(panel, LV_OBJ_FLAG_SCROLLABLE));
    assert(lv_obj_has_flag(overlay, LV_OBJ_FLAG_CLICKABLE));
    lv_obj_t *heading = lv_obj_get_child(panel, 0);
    lv_obj_t *pager = lv_obj_get_child(panel, 1);
    lv_obj_t *button = lv_obj_get_child(panel, 2);
    guide_expect_text(heading, UI_TEXT_GESTURE_GUIDE_TITLE);
    guide_check_label(heading, 20, true);
    lv_obj_t *button_label = lv_damped_button_get_label(button);
    guide_expect_text(button_label, UI_TEXT_GESTURE_GOT_IT);
    guide_check_label(button_label, 14, true);
    assert(lv_obj_get_width(button) == 130 && lv_obj_get_height(button) == 40);
    assert(lv_obj_get_width(button_label) <= 130 && lv_obj_get_height(button_label) <= 40);
    assert(gesture_service_definition_count() == 3);
    for (unsigned i = 0; i < gesture_service_definition_count(); ++i) {
        const gesture_definition_t *definition = gesture_service_definition(i);
        lv_content_pager_set_active(pager, i, false);
        render();
        lv_obj_t *page = lv_content_pager_get_page(pager, i);
        lv_obj_t *title = lv_obj_get_child(page, 1);
        lv_obj_t *body = lv_obj_get_child(page, 2);
        guide_expect_text(title, definition->title_text);
        guide_expect_text(body, definition->body_text);
        guide_check_label(title, 18, true);
        guide_check_label(body, 14, false);
        assert(lv_obj_get_style_base_dir(body, 0) == LV_BASE_DIR_AUTO);
        char file[96];
        snprintf(file, sizeof(file), "gesture-guide-%s-%u", locale->tag, i);
        write_bmp(file);
    }
    /* Keep the normal Got it button path and its exit animation. */
    lv_event_send(button, LV_EVENT_CLICKED, NULL);
    assert(gesture_guide_is_open());
    tick(220);
    assert(!gesture_guide_is_open());
    assert(lv_obj_get_child_cnt(lv_layer_top()) == baseline_children);
    assert(timers() == baseline_timers);
    assert(lv_anim_count_running() == baseline_animations);
    printf("PASS gesture-guide locale=%s: 3 translated actions, actual glyphs/bitmaps, bounds and animated close\n", locale->tag);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    lv_init();
    static lv_color_t pixels[1280 * 40];
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels, NULL, 1280 * 40);
    static lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = 1280; display.ver_res = 400;
    display.draw_buf = &buffer; display.flush_cb = flush;
    assert(lv_disp_drv_register(&display));
    ui_scrollbar_init(lv_disp_get_default());
    lv_img_decoder_t *decoder = lv_img_decoder_create();
    lv_img_decoder_set_info_cb(decoder, host_image_info);
    lv_img_decoder_set_open_cb(decoder, host_image_open);
    lv_img_decoder_set_close_cb(decoder, host_image_close);
    static lv_indev_drv_t driver;
    lv_indev_drv_init(&driver); driver.type = LV_INDEV_TYPE_POINTER;
    driver.read_cb = pointer_read; driver.feedback_cb = pointer_feedback;
    lv_indev_t *indev = lv_indev_drv_register(&driver);
    assert(indev); g_pointer_indev = indev;
    ui_lang_set_resource_preparer(ui_locale_font_prepare);
    ui_message_font_set_provider(ui_locale_font_compose);
    unsigned children = lv_obj_get_child_cnt(lv_layer_top());
    unsigned timer_count = timers(), animations = lv_anim_count_running();
    for (size_t i = 0; i < ui_lang_count(); ++i)
        guide_locale_case(ui_lang_at(i), children, timer_count, animations);
    /* Reopening while the old close animation is pending cannot delete the new tree. */
    for (unsigned i = 0; i < 12; ++i) {
        gesture_guide_show(); tick(40); gesture_guide_close(true);
        gesture_guide_show(); tick(220); assert(gesture_guide_is_open());
        gesture_guide_close(false); tick(200);
        assert(!gesture_guide_is_open());
        assert(lv_obj_get_child_cnt(lv_layer_top()) == children);
        assert(timers() == timer_count && lv_anim_count_running() == animations);
    }
    ui_lang_set(LANGUAGE_EN);
    gesture_guide_close(false);
    lv_indev_delete(indev); lv_img_decoder_delete(decoder);
    lv_deinit(); host_external_assets_release();
    puts("PASS gesture-guide real LVGL ten languages and repeated modal/animation lifetime; no board I/O");
    return 0;
}
