#include "ui_detail_reveal.h"
#include "lv_loading_orbit.h"
#include <string.h>
bool ui_detail_reveal_init(ui_detail_reveal_t *s,lv_obj_t *content)
{
    if(!s||!content)return false;memset(s,0,sizeof(*s));s->content=content;
    lv_obj_update_layout(content);
    s->cover=lv_obj_create(lv_obj_get_parent(content));if(!s->cover)return false;
    lv_obj_remove_style_all(s->cover);lv_obj_set_pos(s->cover,lv_obj_get_x(content),lv_obj_get_y(content));
    lv_obj_set_size(s->cover,lv_obj_get_width(content),lv_obj_get_height(content));
    lv_obj_set_style_bg_color(s->cover,lv_color_hex(0xFFFFFF),0);lv_obj_set_style_bg_opa(s->cover,LV_OPA_COVER,0);
    lv_obj_clear_flag(s->cover,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(s->cover,LV_OBJ_FLAG_HIDDEN);
    return true;
}
void ui_detail_reveal_cancel(ui_detail_reveal_t *s)
{
    if(!s||!s->content)return;
    lv_obj_set_style_opa(s->content,LV_OPA_COVER,0);
    if(s->orbit){lv_obj_del(s->orbit);s->orbit=NULL;}
    if(s->cover)lv_obj_add_flag(s->cover,LV_OBJ_FLAG_HIDDEN);s->active=false;
}
void ui_detail_reveal_begin(ui_detail_reveal_t *s,bool refresh)
{
    ui_detail_reveal_cancel(s);if(!s||!s->cover)return;
    s->active=true;s->forced=refresh;s->started=lv_tick_get();s->spinning=0;
    lv_obj_set_style_opa(s->content,0,0);lv_obj_clear_flag(s->cover,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(s->cover);
}
void ui_detail_reveal_update(ui_detail_reveal_t *s,bool ready)
{
    if(!s||!s->active)return;uint32_t now=lv_tick_get();
    if(!s->orbit&&(s->forced||!ready)){
        s->orbit=lv_loading_orbit_create_sized(s->cover,38);
        if(s->orbit){lv_obj_center(s->orbit);s->spinning=now;}
    }
    if(!ready||(s->orbit&&now-s->spinning<900U))return;
    if(s->orbit){lv_obj_del(s->orbit);s->orbit=NULL;}
    lv_obj_add_flag(s->cover,LV_OBJ_FLAG_HIDDEN);s->active=false;
    lv_obj_set_style_opa(s->content,LV_OPA_COVER,0);
}
