#ifndef TEST_LOCALE_VIEW_CHECKS_H
#define TEST_LOCALE_VIEW_CHECKS_H
#include <stdlib.h>
static unsigned locale_view_overflows;
static void check_locale_view_result(void){assert(locale_view_overflows==0);}
static void check_single_line(lv_obj_t *o){
 static bool registered;
 if(!registered){assert(atexit(check_locale_view_result)==0);registered=true;}
 if(!lv_obj_is_visible(o))return;
 if(lv_obj_check_type(o,&lv_label_class)){
  lv_obj_t *p=lv_obj_get_parent(o);
  if(lv_obj_has_flag(p,LV_OBJ_FLAG_CLICKABLE)&&lv_obj_get_child_cnt(p)==2&&
     lv_obj_check_type(lv_obj_get_child(p,1),&lv_img_class)){
   lv_area_t a,b;lv_obj_get_coords(o,&a);lv_obj_get_coords(p,&b);
   if(a.x1<b.x1||a.x2>b.x2){fprintf(stderr,"Icon button clipped: %s\n",lv_label_get_text(o));++locale_view_overflows;}
  }
  const lv_font_t *font=lv_obj_get_style_text_font(o,0);
  const char *text=lv_label_get_text(o);uint32_t offset=0;
  while(text[offset]){uint32_t cp=_lv_txt_encoded_next(text,&offset);lv_font_glyph_dsc_t glyph;
   if(cp<32 || (cp>=0x200b&&cp<=0x200f) || (cp>=0x202a&&cp<=0x202e) || (cp>=0x2066&&cp<=0x2069))continue;
   bool exists=lv_font_get_glyph_dsc(font,&glyph,cp,0);
   if(!exists||glyph.is_placeholder)fprintf(stderr,"Missing visible glyph U+%04X: %s\n",cp,text);
   assert(exists&&!glyph.is_placeholder);
  }
 }
 if(lv_obj_check_type(o,&lv_label_class)&&lv_obj_get_child_cnt(lv_obj_get_parent(o))==1&&lv_obj_has_flag(lv_obj_get_parent(o),LV_OBJ_FLAG_CLICKABLE)){
  lv_label_long_mode_t mode=lv_label_get_long_mode(o);
  lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP); /* Measure the full text, not LVGL's inserted dots. */
  lv_obj_update_layout(o);
  const lv_font_t *font=lv_obj_get_style_text_font(o,0);lv_point_t size;lv_txt_get_size(&size,lv_label_get_text(o),font,lv_obj_get_style_text_letter_space(o,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  if(size.x>lv_obj_get_width(o))fprintf(stderr,"Button text too wide: %s (%d > %d)\n",lv_label_get_text(o),size.x,lv_obj_get_width(o));
  if(size.x>lv_obj_get_width(o))++locale_view_overflows;
  assert(lv_obj_get_height(o)<=font->line_height);
  lv_label_set_long_mode(o,mode);
 }
 for(unsigned i=0;i<lv_obj_get_child_cnt(o);++i)check_single_line(lv_obj_get_child(o,i));
}
#endif
