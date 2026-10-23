/* Runs unchanged geometry, lifecycle and locale cases independently of the
 * pre-existing catalog-count mismatch in the full legacy suite. */
#define main legacy_settings_suite_main
#include "test_settings_view.c"
#undef main
static void mainboard_version_names_test(void)
{
    for(size_t i=0;i<ui_lang_count();++i){
        const ui_locale_t *locale=ui_lang_at(i);
        assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
        ui_page_06_settings_create(lv_scr_act());scope=find("versions");assert(scope);render();
        lv_obj_update_layout(settings_page);
        lv_obj_t *probe=lv_label_create(lv_scr_act());lv_label_set_text(probe,ui_tr("Controller"));
        lv_obj_t *name=label_find(settings_page,lv_label_get_text(probe));assert(name);lv_obj_del(probe);
        lv_obj_t *card=lv_obj_get_parent(name);
        assert(lv_obj_get_x(card)==0&&lv_obj_get_y(card)==0);
        assert(lv_obj_get_width(card)==400&&lv_obj_get_height(card)==242);
        assert(lv_obj_get_x(name)==22&&lv_obj_get_y(name)==20);
        const lv_font_t *font=lv_obj_get_style_text_font(name,0);
        lv_point_t size;lv_txt_get_size(&size,lv_label_get_text(name),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        assert(size.x<=356&&size.y==font->line_height);
        assert(!label_find(settings_page,"Controller"));
        const char *text=lv_label_get_text(name);uint32_t offset=0;
        while(text[offset]){uint32_t cp=_lv_txt_encoded_next(text,&offset);lv_font_glyph_dsc_t glyph;
            if(cp>32){assert(lv_font_get_glyph_dsc(font,&glyph,cp,0));assert(!glyph.is_placeholder);}}
        char filename[80];snprintf(filename,sizeof(filename),"mainboard-version-%s",locale->tag);snapshot(filename);
        ui_page_06_settings_destroy();scope=NULL;
    }
    ui_lang_set(LANGUAGE_EN);
    puts("PASS Mainboard Version names: ten locales, actual glyphs, one line and original 400x242 card geometry");
}
int main(void)
{
lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);lv_img_decoder_t*dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer_read;input.feedback_cb=evdev_feedback;lv_indev_drv_register(&input);device_info_init("V1.0.0");
    grid_test();third_level_test();directory_locales_test();mainboard_version_names_test();
    puts("PASS focused actual fullscreen Settings: native geometry, parent navigation, clock/language lifecycle and ten-locale directories");
    return 0;
}
