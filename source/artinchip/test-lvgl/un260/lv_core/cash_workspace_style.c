#include "cash_workspace_style.h"
lv_obj_t *cw_box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{lv_obj_t *o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,radius,0);return o;}
lv_obj_t *cw_label(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *font,uint32_t color)
{lv_obj_t *o=lv_label_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_obj_set_style_text_font(o,ui_message_font(font),0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);lv_label_set_text(o,s);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);return o;}
lv_obj_t *cw_button(lv_obj_t *p,int x,int y,int w,int h,const char *s,uint32_t bg,uint32_t fg,lv_event_cb_t cb,void *data)
{
    lv_obj_t *o=lv_settings_button(p,x,y,w,h,s,false,cb,data);
    const lv_state_t states[]={0,LV_STATE_CHECKED,LV_STATE_PRESSED,LV_STATE_CHECKED|LV_STATE_PRESSED,LV_STATE_DISABLED,LV_STATE_FOCUSED};
    for(unsigned i=0;i<sizeof(states)/sizeof(*states);++i){lv_obj_set_style_border_width(o,0,states[i]);lv_obj_set_style_outline_width(o,0,states[i]);lv_obj_set_style_shadow_width(o,0,states[i]);lv_obj_set_style_bg_color(o,lv_color_hex(states[i]&LV_STATE_PRESSED?0xDDE4E7:bg),states[i]);}
    lv_obj_set_style_bg_color(o,lv_color_hex(CW_PANEL),LV_STATE_DISABLED);
    lv_obj_set_style_text_color(o,lv_color_hex(fg),0);
    lv_obj_set_style_text_color(o,lv_color_hex(CW_MUTED),LV_STATE_DISABLED);
    lv_obj_set_style_text_opa(o,LV_OPA_COVER,LV_STATE_DISABLED);
    lv_obj_set_style_radius(o,12,0);lv_obj_t *l=lv_obj_get_child(o,0);lv_obj_remove_local_style_prop(l,LV_STYLE_TEXT_COLOR,0);lv_obj_set_style_text_font(l,ui_message_font(&lv_font_instrument_sans_medium_15),0);lv_obj_set_width(l,w-16);lv_obj_set_height(l,ui_message_font(&lv_font_instrument_sans_medium_15)->line_height);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_center(l);return o;
}
lv_obj_t *cw_root(lv_obj_t *parent)
{lv_obj_t *r=cw_box(parent,0,0,1280,400,0xF4F7FB,0);lv_obj_set_style_bg_grad_color(r,lv_color_hex(0xFFFFFF),0);lv_obj_set_style_bg_grad_dir(r,LV_GRAD_DIR_HOR,0);lv_obj_set_style_bg_grad_stop(r,140,0);return r;}
