#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "lv_quick_controls.h"
#include "lv_settings.h"
#include "un260/lv_resources/ui_icons.h"
#include "un260/lv_system/ui_text.h"
#include <string.h>
static lv_obj_t *copy(lv_obj_t *p,const char *s,int x,int y,int w,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *l=lv_settings_label(p,s,x,y,font,color);lv_obj_set_width(l,w);
    lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);return l;
}
static lv_obj_t *card(lv_obj_t *p,int x,int y,int w)
{
    lv_obj_t *o=lv_settings_box(p,x,y,w,120,0xFFFFFF);
    lv_obj_set_style_radius(o,16,0);lv_obj_set_style_border_width(o,1,0);
    lv_obj_set_style_border_color(o,lv_color_hex(0xE9EFF3),0);return o;
}
void lv_quick_controls_create(lv_quick_controls_t *v,lv_obj_t *parent,int x,int y,lv_event_cb_t cb)
{
    memset(v,0,sizeof(*v));
    copy(parent,ui_text_get(UI_TEXT_QUICK_PREFERENCES),x,y,830,&lv_font_instrument_sans_semibold_12,0x586B77);
    copy(parent,ui_text_get(UI_TEXT_QUICK_VERSIONS),x+868,y,324,&lv_font_instrument_sans_semibold_12,0x586B77);
    const ui_text_id_t titles[]={UI_TEXT_QUICK_LAYOUT,UI_TEXT_QUICK_GESTURES};
    const ui_text_id_t hints[]={UI_TEXT_QUICK_LAYOUT_HINT,UI_TEXT_QUICK_GESTURE_HINT};
    const char *icons[]={LVGL_DIR "quick_icons/layout.png",LVGL_DIR "quick_icons/gesture.png"};
    for(unsigned i=0;i<2;i++){
        lv_obj_t *tile=card(parent,x+298*i,y+24,286);
        lv_obj_add_flag(tile,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(tile,lv_color_hex(0xEDF2F6),LV_STATE_PRESSED);
        lv_obj_add_event_cb(tile,cb,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        lv_obj_t *im=ui_icon_create(tile,icons[i]);lv_obj_set_pos(im,16,18);
        copy(tile,ui_text_get(titles[i]),54,16,138,&lv_font_instrument_sans_semibold_18,0x1D2B34);
        v->labels[i]=copy(tile,"",54,43,135,&lv_font_instrument_sans_medium_14,0x586B77);
        copy(tile,ui_text_get(hints[i]),16,85,254,&lv_font_instrument_sans_medium_12,0x586B77);
        v->switches[i]=lv_settings_toggle(tile,202,21,false,cb,(void *)(uintptr_t)i);
        lv_obj_set_size(v->switches[i],64,34);
        lv_obj_set_size(lv_obj_get_child(v->switches[i],0),28,28);
    }
    lv_obj_t *standby=card(parent,x+596,y+24,240);
    lv_obj_t *im=ui_icon_create(standby,LVGL_DIR "quick_icons/standby.png");lv_obj_set_pos(im,16,18);
    copy(standby,ui_text_get(UI_TEXT_QUICK_STANDBY),54,18,122,&lv_font_instrument_sans_semibold_18,0x1D2B34);
    copy(standby,ui_text_get(UI_TEXT_QUICK_WAKE_HINT),16,46,156,&lv_font_instrument_sans_medium_12,0x586B77);
    lv_obj_t *settings=lv_settings_button(standby,180,10,44,44,"",false,cb,(void *)QUICK_APPEARANCE);
    lv_damped_button_set_exact_palette(settings,lv_color_hex(0xEAF0F4),lv_color_hex(0xD7E5F6));
    lv_obj_center(ui_icon_create(settings,LVGL_DIR "quick_icons/settings.png"));
    lv_obj_t *enter=lv_settings_button(standby,16,65,208,44,ui_text_get(UI_TEXT_QUICK_ENTER),false,cb,(void *)QUICK_STANDBY);
    lv_damped_button_set_exact_palette(enter,lv_color_hex(0xEAF1FB),lv_color_hex(0xD7E5F6));
    lv_obj_set_style_text_color(lv_obj_get_child(enter,0),lv_color_hex(0x1462CC),0);
    lv_settings_box(parent,x+854,y,1,144,0xDFE7ED);
    const ui_text_id_t names[]={UI_TEXT_QUICK_CONTROLLER,UI_TEXT_QUICK_IMAGE,UI_TEXT_QUICK_UI};
    for(unsigned i=0;i<3;i++){
        copy(parent,ui_text_get(names[i]),x+868,y+31+38*i,146,&lv_font_instrument_sans_medium_14,0x586B77);
        v->versions[i]=copy(parent,"",x+1014,y+28+38*i,180,&lv_font_instrument_sans_semibold_18,0x1D2B34);
        lv_obj_set_style_text_align(v->versions[i],LV_TEXT_ALIGN_RIGHT,0);
    }
}
bool lv_quick_controls_refresh(lv_quick_controls_t *v,lv_quick_controls_state_t state)
{
    bool changed=!v->initialized,values[]={state.layout,state.gestures};
    bool previous[]={v->state.layout,v->state.gestures};
    for(unsigned i=0;i<2;i++)if(!v->initialized||values[i]!=previous[i]){
        lv_settings_toggle_set(v->switches[i],values[i],false);
        lv_label_set_text(v->labels[i],ui_text_get(values[i]?UI_TEXT_QUICK_ON:UI_TEXT_QUICK_OFF));changed=true;
    }
    for(unsigned i=0;i<3;i++){
        const char *s=state.versions[i]&&*state.versions[i]?state.versions[i]:ui_text_get(UI_TEXT_QUICK_UNAVAILABLE);
        if(strcmp(lv_label_get_text(v->versions[i]),s)){lv_label_set_text(v->versions[i],s);changed=true;}
    }
    v->state=state;v->initialized=true;return changed;
}
