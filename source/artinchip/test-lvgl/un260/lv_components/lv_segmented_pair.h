#ifndef LV_SEGMENTED_PAIR_H
#define LV_SEGMENTED_PAIR_H
#include "lv_damped_button.h"

/* Same surface tokens as Main's ABC tray: flush segments beneath one stroke.
 * The caller owns the selection and callbacks; this is presentation only. */
static inline void lv_segmented_pair_select(lv_obj_t *const buttons[2],unsigned selected)
{
    for(unsigned i=0;i<2;++i) {
        bool active=i==selected;
        if(active)lv_obj_add_state(buttons[i],LV_STATE_CHECKED);
        else lv_obj_clear_state(buttons[i],LV_STATE_CHECKED);
        lv_damped_button_set_palette(buttons[i],lv_color_hex(active?0xFFFFFF:0xE7EDF0),lv_color_hex(0xD9E5ED));
        lv_obj_set_style_text_color(lv_damped_button_get_label(buttons[i]),lv_color_hex(active?0x233B49:0x526E80),0);
    }
}
static inline lv_obj_t *lv_segmented_pair_create(lv_obj_t *parent,int x,int y,int w,int h,
    bool vertical,const char *first,const char *second,const lv_font_t *font,
    lv_event_cb_t callback,void *context,lv_obj_t *buttons[2])
{
    lv_obj_t *tray=lv_obj_create(parent);if(!tray)return NULL;
    lv_obj_remove_style_all(tray);lv_obj_set_pos(tray,x,y);lv_obj_set_size(tray,w,h);
    lv_obj_set_style_bg_color(tray,lv_color_hex(0xE7EDF0),0);
    lv_obj_set_style_bg_opa(tray,LV_OPA_COVER,0);lv_obj_set_style_radius(tray,12,0);
    lv_obj_clear_flag(tray,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    const char *titles[]={first,second};
    const lv_damped_button_style_t style={0xE7EDF0,0xD9E5ED,0xE7EDF0,0x233B49,0x7A8D9B,12};
    for(unsigned i=0;i<2;++i) {
        buttons[i]=lv_damped_button_create(tray,&style,titles[i],font);
        if(!buttons[i] || !lv_damped_button_get_label(buttons[i]))goto failed;
        int begin=(vertical?h:w)*(int)i/2, end=(vertical?h:w)*(int)(i+1)/2;
        lv_obj_set_pos(buttons[i],vertical?0:begin,vertical?begin:0);
        lv_obj_set_size(buttons[i],vertical?w:end-begin,vertical?end-begin:h);
        lv_obj_set_style_border_width(buttons[i],0,0);
        if(!lv_obj_add_event_cb(buttons[i],callback,LV_EVENT_CLICKED,context))goto failed;
    }
    lv_obj_t *frame=lv_obj_create(tray);if(!frame)goto failed;
    lv_obj_remove_style_all(frame);lv_obj_set_size(frame,w,h);
    lv_obj_set_style_radius(frame,12,0);lv_obj_set_style_border_width(frame,3,0);
    lv_obj_set_style_border_color(frame,lv_color_hex(0xECF0F3),0);
    lv_obj_set_style_border_opa(frame,LV_OPA_COVER,0);
    lv_obj_clear_flag(frame,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_segmented_pair_select(buttons,0);return tray;
failed:
    lv_obj_del(tray);buttons[0]=buttons[1]=NULL;return NULL;
}
#endif
