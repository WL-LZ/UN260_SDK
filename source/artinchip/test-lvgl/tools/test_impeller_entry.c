/* Reuse the real settings root, resources, pointer handling and fixture edges.
 * This focused entry test does not weaken the legacy suite's assertions. */
#define main existing_settings_suite_main
#include "tools/test_settings_view.c"
#undef main
static lv_obj_t *translated_label(lv_obj_t *parent,const char *key)
{
 lv_obj_t *probe=lv_label_create(lv_scr_act());lv_label_set_text(probe,ui_tr(key));
 lv_obj_t *label=label_find(parent,lv_label_get_text(probe));lv_obj_del(probe);assert(label);return label;
}
static lv_obj_t *row_for(lv_obj_t *label)
{while(label&&!lv_obj_has_flag(label,LV_OBJ_FLAG_CLICKABLE))label=lv_obj_get_parent(label);assert(label);return label;}
int main(void)
{
 lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);
 lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
 lv_img_decoder_t *dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);
 lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer_read;input.feedback_cb=evdev_feedback;lv_indev_drv_register(&input);device_info_init("V1.0.0");
 size_t count;const settings_node_t *items=settings_catalog(&count);assert(settings_catalog_validate(items,count));const settings_node_t *entry=settings_catalog_find(items,count,"impeller");assert(entry&&!strcmp(entry->parent,"maintenance")&&!strcmp(entry->group,"hardware")&&entry->page==UI_PAGE_IMPELLER_SETTING);
 for(size_t i=0;i<ui_lang_count();i++){
  const ui_locale_t *locale=ui_lang_at(i);assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
  ui_page_06_settings_reset_navigation();ui_page_06_settings_create(lv_scr_act());
  lv_obj_t *maintenance=row_for(translated_label(sidebar,"Maintenance"));lv_event_send(maintenance,LV_EVENT_CLICKED,NULL);assert(scope_is("maintenance"));
  lv_obj_t *label=translated_label(grid,"Impeller motor speed"),*row=row_for(label);
  lv_obj_update_layout(settings_page);lv_obj_scroll_to_view_recursive(row,LV_ANIM_OFF);lv_obj_update_layout(settings_page);check_single_line(settings_page);
  lv_area_t a,b;lv_obj_get_coords(row,&a);lv_obj_get_coords(grid,&b);if(!(a.y1>=b.y1&&a.y2<=b.y2&&a.x1>=b.x1&&a.x2<=b.x2))fprintf(stderr,"entry bounds %s: row %d,%d,%d,%d grid %d,%d,%d,%d scroll %d\n",locale->tag,a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2,lv_obj_get_scroll_y(grid));assert(a.y1>=b.y1&&a.y2<=b.y2&&a.x1>=b.x1&&a.x2<=b.x2);
  const lv_font_t *font=lv_obj_get_style_text_font(label,0);lv_point_t ink;lv_txt_get_size(&ink,lv_label_get_text(label),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(ink.x<=lv_obj_get_width(label)&&ink.y<=font->line_height);
  char name[80];snprintf(name,sizeof(name),"entry-%s",locale->tag);snapshot(name);
  requested=UI_PAGE_MAIN;unsigned before=sends;int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
  pointer_feed(x,y,LV_INDEV_STATE_PRESSED);pointer_feed(x,y,LV_INDEV_STATE_RELEASED);
  assert(requested==UI_PAGE_IMPELLER_SETTING&&sends==before);ui_page_06_settings_destroy();
 }
 puts("PASS ten-locale real settings Maintenance entry: hardware group, full row visible after scroll, title glyphs/width, actual pointer routes once to impeller page without protocol send");return 0;
}
