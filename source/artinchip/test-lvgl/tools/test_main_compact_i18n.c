/* Real Main widgets: all compact state captions, glyphs, widths and resume. */
#include "lvgl/lvgl.h"
#define main existing_main_regression
#include "test_main_view.c"
#undef main
static unsigned compact_overflows;

static lv_obj_t *mode_button_label(void)
{
    lv_obj_t *label = page_01_main_find_obj("mode_label");
    assert(label); return label;
}

static void check_caption(lv_obj_t *label, const char *expected)
{
    if (!label || !lv_obj_is_visible(label))
        fprintf(stderr, "COMPACT_HIDDEN %s: %s\n", ui_lang_current()->tag, expected);
    assert(label && lv_obj_is_visible(label));
    /* LVGL shapes Arabic on set_text, so compare the same shaped bytes. */
    lv_obj_t *reference = lv_label_create(lv_scr_act());
    lv_label_set_text(reference, expected);
    assert(!strcmp(lv_label_get_text(label), lv_label_get_text(reference)));
    lv_obj_del(reference);
    const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
    lv_point_t size;
    lv_txt_get_size(&size, lv_label_get_text(label), font,
                    lv_obj_get_style_text_letter_space(label, 0), 0,
                    LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x > lv_obj_get_content_width(label))
        fprintf(stderr, "COMPACT_OVERFLOW %s: %s (%d > %d)\n",
                ui_lang_current()->tag, expected, size.x, lv_obj_get_content_width(label));
    if (size.x > lv_obj_get_content_width(label)) ++compact_overflows;
    assert(size.y <= font->line_height);
    check_single_line(label);
}

static lv_obj_t *find_visible_caption(lv_obj_t *root, const char *shaped)
{
    if (!lv_obj_is_visible(root)) return NULL;
    if (lv_obj_check_type(root, &lv_label_class) &&
        !strcmp(lv_label_get_text(root), shaped)) return root;
    for (unsigned i = 0; i < lv_obj_get_child_cnt(root); ++i) {
        lv_obj_t *found = find_visible_caption(lv_obj_get_child(root, i), shaped);
        if (found) return found;
    }
    return NULL;
}

static void test_compact_captions(void)
{
    ui_lang_set_resource_preparer(ui_locale_font_prepare);
    ui_message_font_set_provider(ui_locale_font_compose);
    const unsigned modes[] = { MODE_MDC, MODE_SDC, MODE_CNT };
    const unsigned mode_keys[] = { UI_TEXT_PAGE01_BOTTOM_MODE_MDC,
                                  UI_TEXT_PAGE01_BOTTOM_MODE_SDC,
                                  UI_TEXT_PAGE01_BOTTOM_MODE_CNT };
    const char *levels[] = { "Low", "Medium", "High" };
    for (size_t l = 0; l < ui_lang_count(); ++l) {
        const ui_locale_t *locale = ui_lang_at(l);
        assert(ui_lang_prepare_resources(locale->id)); ui_lang_set(locale->id);
        currency_state_reset(); fixture(true); saved_tab=0;
        assert(currency_state_confirm_active_code("CNY"));
        ui_main_create(lv_scr_act()); assert(page_01_main_resume()); tick(300);
        assert(!strcmp(currency_state_display_code(CURRENCY_AUTO_CODE), "AUTO"));
        assert(!strcmp(currency_state_display_code(CURRENCY_MULTI_CODE), "MULTI"));
        assert(!strcmp(currency_state_display_code("EUR"), "EUR"));
        if (locale->id == LANGUAGE_EN) {
            assert(!strcmp(ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_MDC), "MDC"));
            assert(!strcmp(ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_SDC), "SDC"));
            assert(!strcmp(ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_CNT), "CNT"));
        }
        if (locale->id == LANGUAGE_CN) {
            assert(!strcmp(ui_text_get(UI_TEXT_PAGE01_BOTTOM_WORK_AUTO), "自动"));
            assert(!strcmp(ui_tr("Auto"), "自动"));
        }
        assert(!strcmp(currency_state_display_code("USD"), "USD"));
        assert(!strcmp(currency_state_display_code("CNY"), "CNY"));
        for (unsigned c = 0; c < 2; ++c) {
            assert(c ? currency_state_confirm_multi_selection() : currency_state_confirm_auto_selection());
            ui_refresh_main_page(); tick(120);
            const char *expected = c ? "MULTI" : "AUTO";
            if (c) {
                /* Multi owns its card; the single-currency summary is hidden. */
                lv_obj_t *reference = lv_label_create(lv_scr_act());
                lv_label_set_text(reference, expected);
                assert(!strcmp(lv_label_get_text(s_currency_code), lv_label_get_text(reference)));
                lv_obj_t *actual = find_visible_caption(s_multi_card, lv_label_get_text(reference));
                lv_obj_del(reference);
                check_caption(actual, expected);
            } else check_caption(s_currency_code, expected);
        }
        assert(currency_state_leave_special_selection());
        assert(currency_state_confirm_active_code("CNY")); ui_refresh_main_page(); tick(120);
        for (unsigned m = 0; m < 3; ++m) {
            machine_state_confirm_mode(modes[m]); page_01_mode_switch_refre(); tick(220);
            check_caption(mode_button_label(), ui_text_get(mode_keys[m]));
            check_caption(s_bottom_a_label_mode, ui_text_get(mode_keys[m]));
        }
        for (unsigned add = 0; add < 2; ++add) {
            machine_state_confirm_add(add); page_01_add_refre(); tick(220);
            check_caption(s_bottom_a_label_add, ui_text_get(add ?
                          UI_TEXT_PAGE01_BOTTOM_ADD_ON : UI_TEXT_PAGE01_BOTTOM_ADD_OFF));
        }
        for (unsigned fo = 0; fo < 4; ++fo) {
            machine_state_confirm_fo_mode(fo); page_01_face_refre(); tick(220);
            check_caption(s_bottom_a_label_fo, page_01_bottom_fo_text_get());
        }
        for (unsigned work = 0; work < 2; ++work) {
            machine_state_confirm_work_mode(work); page_01_work_refre(); tick(220);
            check_caption(s_bottom_a_label_work, page_01_bottom_work_text_get());
        }
        page_01_detail_section_set(2, true); tick(220);
        for (unsigned speed = 0; speed < 3; ++speed) {
            machine_state_confirm_speed(speed); page_01_speed_refre(); tick(220);
            check_caption(s_bottom_c_label_speed, page_01_bottom_speed_text_get());
        }
        machine_state_confirm_batch(false, 100); page_01_batch_refre(); tick(220);
        check_caption(s_bottom_c_label_batch, ui_text_get(UI_TEXT_PAGE01_BOTTOM_BATCH_OFF));
        const unsigned quantities[] = {1, 100, 255};
        for (unsigned b = 0; b < sizeof(quantities)/sizeof(quantities[0]); ++b) {
            machine_state_confirm_batch(true, quantities[b]); page_01_batch_refre(); tick(220);
            char expected[160];
            snprintf(expected, sizeof(expected), ui_text_get(UI_TEXT_PAGE01_BOTTOM_BATCH_VALUE_FMT),
                     quantities[b]);
            check_caption(s_bottom_c_label_batch, expected);
        }
        for (unsigned level = 0; level < 3; ++level) {
            machine_state_confirm_cfd_mode(level); page_01_cfd_refre(); tick(80);
            char expected[160];
            snprintf(expected, sizeof(expected), ui_text_get(UI_TEXT_PAGE01_BOTTOM_CFD_FMT),
                     ui_trc("Detection level", levels[level]));
            check_caption(s_bottom_c_label_cfd, expected);
            assert(machine_state_cfd_mode() == level);
        }
        machine_state_snapshot_t before = {0}, after = {0};
        machine_state_get_snapshot(&before);
        const int pcs = counting_data_current()->total_pcs;
        const float amount = counting_data_current()->total_amount;
        page_01_main_suspend(); page_01_update_language_texts(); tick(60);
        assert(page_01_main_resume()); tick(160);
        machine_state_get_snapshot(&after);
        assert(!memcmp(&before, &after, sizeof(before)));
        assert(counting_data_current()->total_pcs == pcs);
        assert(counting_data_current()->total_amount == amount);
        check_caption(mode_button_label(), ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_CNT));
        check_single_line(lv_scr_act());
        char file[80]; snprintf(file, sizeof(file), "professional-main-%s-cfd", locale->tag);
        write_bmp(file);
        page_01_detail_section_set(0, true); tick(220);
        snprintf(file, sizeof(file), "professional-main-%s-mode", locale->tag); write_bmp(file);
        ui_main_destroy();
        printf("CHECK compact native captions, all states and hidden resume: %s\n", locale->tag);
    }
}

int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0); assert(sizeof(lv_coord_t)==2); lv_init();
    static lv_color_t pixels[1280*40]; static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*40);
    static lv_disp_drv_t display; lv_disp_drv_init(&display);
    display.hor_res=1280; display.ver_res=400; display.draw_buf=&buffer; display.flush_cb=flush;
    assert(lv_disp_drv_register(&display)); ui_scrollbar_init(lv_disp_get_default());
    lv_img_decoder_t *decoder=lv_img_decoder_create(); assert(decoder);
    lv_img_decoder_set_info_cb(decoder,host_image_info);
    lv_img_decoder_set_open_cb(decoder,host_image_open);
    lv_img_decoder_set_close_cb(decoder,host_image_close);
    static lv_indev_drv_t driver; lv_indev_drv_init(&driver); driver.type=LV_INDEV_TYPE_POINTER;
    driver.read_cb=pointer_read; driver.feedback_cb=pointer_feedback;
    lv_indev_t *indev=lv_indev_drv_register(&driver); assert(indev); g_pointer_indev=indev;
    test_compact_captions();
    assert(compact_overflows == 0);
    counting_data_clear_serials(counting_data_mutable()); counting_data_clear_errors(counting_data_mutable());
    lv_indev_delete(indev); lv_img_decoder_delete(decoder); lv_deinit(); host_external_assets_release();
    return 0;
}
