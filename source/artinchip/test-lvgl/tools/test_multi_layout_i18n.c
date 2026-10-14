/* Actual localized widgets: currency ink alignment and island clipping. */
#include "lvgl/lvgl.h"
#define main existing_main_regression
#include "test_main_view.c"
#undef main
static unsigned failures;
static int ink_center(lv_obj_t *label)
{
 const lv_font_t *font=lv_obj_get_style_text_font(label,0);
 const char *text=lv_label_get_text(label);uint32_t pos=0;int top=1000,bottom=-1000;
 while(text[pos]){uint32_t cp=_lv_txt_encoded_next(text,&pos);lv_font_glyph_dsc_t g;
  if(!lv_font_get_glyph_dsc(font,&g,cp,_lv_txt_encoded_next(text+pos,NULL))||!g.box_h)continue;
  int y=font->line_height-font->base_line-g.box_h-g.ofs_y;
  if(y<top)top=y;if(y+g.box_h>bottom)bottom=y+g.box_h;
 }
 return 2*lv_obj_get_y(label)+top+bottom;
}
static void fits(lv_obj_t *label)
{
 lv_point_t size;lv_txt_get_size(&size,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),0,0,LV_COORD_MAX,0);
 if(size.x>lv_obj_get_content_width(label)||size.y>lv_obj_get_content_height(label)){
  fprintf(stderr,"CLIP %s %s: %d x %d in %d x %d\n",ui_lang_current()->tag,lv_label_get_text(label),size.x,size.y,lv_obj_get_content_width(label),lv_obj_get_content_height(label));failures++;
 }
}
static void island_fits(void)
{
 lv_obj_update_layout(lv_scr_act());int right=0;
 for(unsigned i=0;i<4;i++){
  lv_obj_t *label=g_si_ctx.objects.counting_multi[i];fits(label);
  if(!*lv_label_get_text(label))continue;
  int x=lv_obj_get_x(label),y=lv_obj_get_y(label);
  if(x<right||x+lv_obj_get_width(label)>SMART_ISLAND_COUNT_W-8||y<0||y+lv_obj_get_height(label)>SMART_ISLAND_COUNT_H){fprintf(stderr,"ISLAND_BOUNDS %s %s\n",ui_lang_current()->tag,lv_label_get_text(label));failures++;}
  right=x+lv_obj_get_width(label);
 }
}
static void alignment_matrix(void)
{
 ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);
 for(size_t l=0;l<ui_lang_count();l++){
  const ui_locale_t *locale=ui_lang_at(l);assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
  currency_state_reset();fixture(true);saved_tab=0;assert(currency_state_confirm_multi_selection());
  ui_main_create(lv_scr_act());assert(page_01_main_resume());tick(300);
  counting_data_mark_multi_result(counting_data_mutable());counting_multi_begin(false);
  smart_island_notify_count_start();tick(400);island_fits();
  for(size_t c=0;c<sizeof(all_currency_codes)/sizeof(all_currency_codes[0]);c++){
   counting_multi_reset();counting_multi_begin(false);
   uint8_t f[16]={0xfd,0xdf,16,14,0,0,0,0,0,0,1,0,1,0,1,0};memcpy(f+4,all_currency_codes[c],3);
   assert(counting_multi_info(f,16));smart_island_update_counting(1,0);island_fits();
   uint8_t end[13]={0xfd,0xdf,13,14,0,0,0,0,0,0,0,2};assert(counting_multi_info(end,13));
   for(unsigned expanded=0;expanded<2;expanded++){
    ui_multi_detail_t *v=ui_multi_detail_create(lv_scr_act(),expanded,NULL,NULL);assert(v);ui_multi_detail_visible(v,true);lv_obj_update_layout(lv_scr_act());
    lv_obj_t *row=lv_obj_get_child(ui_multi_detail_scroll(v),0),*symbol=lv_obj_get_child(row,4),*amount=lv_obj_get_child(row,2);
    fits(symbol);int delta=abs(ink_center(symbol)-ink_center(amount));
    if(delta>2){fprintf(stderr,"ALIGN %s %s expanded=%u delta/2=%d\n",locale->tag,all_currency_codes[c],expanded,delta);failures++;}
    if(locale->id==LANGUAGE_KR && !strcmp(all_currency_codes[c],"USD") && !expanded)write_bmp("multi-korean-usd-alignment");
    ui_multi_detail_select(v,0,0);
    if(!expanded){
     uint8_t denom[16]={0xfd,0xdf,16,11};counting_multi_denom(denom,16,lv_tick_get());
     memcpy(denom+4,"       1  1",11);counting_multi_denom(denom,16,lv_tick_get());
     memset(denom+4,255,11);counting_multi_denom(denom,16,lv_tick_get());
    }
    ui_multi_detail_refresh(v);lv_obj_update_layout(lv_scr_act());
    assert(counting_multi_current()->currencies[0].denom_count==1);
    row=lv_obj_get_child(ui_multi_detail_scroll(v),0);symbol=lv_obj_get_child(row,4);amount=lv_obj_get_child(row,0);
    assert(!strcmp(lv_label_get_text(amount),"1"));fits(symbol);
    assert(abs(ink_center(symbol)-ink_center(amount))<=2);
    lv_obj_t *root=ui_multi_detail_object(v);bool header_found=false;
    for(unsigned i=1;i<lv_obj_get_child_cnt(root);i++){
     lv_obj_t *item=lv_obj_get_child(root,i);
     if(!lv_obj_check_type(item,&lv_label_class)||lv_obj_get_style_text_font(item,0)!=lv_obj_get_style_text_font(symbol,0))continue;
     fits(item);assert(abs(ink_center(item)-ink_center(lv_obj_get_child(root,i-1)))<=2);header_found=true;
    }
    assert(header_found);
    ui_multi_detail_visible(v,false);ui_multi_detail_visible(v,true);lv_obj_update_layout(lv_scr_act());
    assert(abs(ink_center(symbol)-ink_center(amount))<=2);
    ui_multi_detail_destroy(v);
   }
   counting_multi_begin(false);memset(f+7,255,6);assert(counting_multi_info(f,16));smart_island_update_counting(65535,0);island_fits();
  }
  counting_multi_begin(true);smart_island_update_counting(0,0);island_fits();
  assert(lv_obj_is_visible(g_si_ctx.objects.counting_gate));
  if(locale->id==LANGUAGE_KR)write_bmp("multi-korean-island-empty");
  ui_main_destroy();counting_multi_reset();printf("CHECK multi layout: %s, all currencies, compact/expanded, empty/live/max/reset\n",locale->tag);
 }
 assert(failures==0);
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
    alignment_matrix();
    counting_data_clear_serials(counting_data_mutable()); counting_data_clear_errors(counting_data_mutable());
    lv_indev_delete(indev); lv_img_decoder_delete(decoder); lv_deinit(); host_external_assets_release();
    return 0;
}
