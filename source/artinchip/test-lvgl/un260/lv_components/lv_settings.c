#include "lv_settings.h"
#include "lv_nav_button.h"
#include "lv_damped_button.h"
#include "lv_port_indev.h"
#include "un260/lv_resources/ui_page_background.h"
#include "un260/lv_system/user_cfg.h"
#include <stdio.h>
#include <string.h>

lv_obj_t *lv_settings_box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color)
{
    lv_obj_t *o=lv_obj_create(p);
    lv_obj_remove_style_all(o); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return o;
}
lv_obj_t *lv_settings_label(lv_obj_t *p,const char *s,int x,int y,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(p); lv_label_set_text(o,s?s:"");
    lv_obj_set_pos(o,x,y); lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
lv_obj_t *lv_settings_icon(lv_obj_t *p,const char *name,int x,int y)
{
    /* One alpha icon serves both colors: avoid duplicate decoded pixels and files. */
    size_t length=strlen(name);
    bool active=length>7&&!strcmp(name+length-7,"-active");
    char path[160]; snprintf(path,sizeof(path),LVGL_DIR "settings_icons/%.*s.png",
                            (int)(active?length-7:length),name);
    lv_obj_t *o=lv_img_create(p);lv_img_set_src(o,path);lv_obj_set_pos(o,x,y);
    if(active){lv_obj_set_style_img_recolor(o,lv_color_hex(0x1462CC),0);
        lv_obj_set_style_img_recolor_opa(o,LV_OPA_COVER,0);}
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;
}
lv_obj_t *lv_settings_button(lv_obj_t *p,int x,int y,int w,int h,const char *s,
                            bool primary,lv_event_cb_t cb,void *data)
{
    lv_obj_t *o=lv_settings_box(p,x,y,w,h,primary?0x1462CC:0xF1F4F5);
    lv_obj_set_style_radius(o,11,0);lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(o,lv_color_hex(primary?0x1056B5:0xE2E9EE),LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xE7EDF1),LV_STATE_DISABLED);
    lv_obj_set_style_text_color(o,lv_color_hex(primary?0xFFFFFF:0x1D2B34),0);
    lv_obj_set_style_text_color(o,lv_color_hex(0x627580),LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xEDF4FF),LV_STATE_CHECKED);
    lv_obj_set_style_text_color(o,lv_color_hex(0x1462CC),LV_STATE_CHECKED);
    lv_obj_set_style_border_width(o,1,LV_STATE_CHECKED);
    lv_obj_set_style_border_color(o,lv_color_hex(0x8CACDA),LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xE2E9EE),LV_STATE_CHECKED|LV_STATE_PRESSED);
    lv_obj_t *l=lv_label_create(o);lv_label_set_text(l,s);
    lv_obj_set_style_text_font(l,&lv_font_instrument_sans_medium_16,0);lv_obj_center(l);
    if(cb)lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,data);
    /* Buttons inside a scroll region keep the initiating finger. Standalone
     * actions still cancel when the finger leaves their bounds. */
    if(lv_obj_has_flag(p,LV_OBJ_FLAG_SCROLLABLE)&&lv_obj_has_flag(p,LV_OBJ_FLAG_USER_4))
        lv_port_indev_set_drag_obj(o,true);
    return o;
}
lv_obj_t *lv_settings_back(lv_obj_t *p,int x,int y,int w,int h,lv_event_cb_t cb,void *data)
{
    lv_obj_t *o=lv_nav_button_create(p,x,y,w,h,cb,data);
    lv_damped_button_set_exact_palette(o,lv_color_hex(0xF8FAFB),lv_color_hex(0xE2E9EE));
    return o;
}
lv_obj_t *lv_settings_panel(lv_obj_t *p,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_settings_box(p,x,y,w,h,0xFFFFFF);
    lv_obj_set_style_radius(o,16,0);lv_obj_set_style_border_width(o,1,0);
    lv_obj_set_style_border_color(o,lv_color_hex(0xE3E9ED),0);
    return o;
}
lv_obj_t *lv_settings_header(lv_obj_t *p,int x,int y,int w,const lv_settings_header_t *cfg)
{
    lv_obj_t *h=lv_settings_box(p,x,y,w,56,0);lv_obj_set_style_bg_opa(h,0,0);
    int tx=0;
    if(cfg->icon){lv_obj_t *b=lv_settings_box(h,0,6,44,44,0xE8EFF9);lv_obj_set_style_radius(b,12,0);
        lv_settings_icon(b,cfg->icon,10,10);tx=60;}
    lv_obj_t *title=lv_settings_label(h,cfg->title,tx,cfg->subtitle?1:12,&lv_font_instrument_sans_semibold_28,0x1D2B34);
    lv_obj_set_width(title,w-tx-122);lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);
    if(cfg->subtitle){lv_obj_t *s=lv_settings_label(h,cfg->subtitle,tx,36,&lv_font_instrument_sans_medium_14,0x586B78);
        lv_obj_set_width(s,w-tx-122);lv_label_set_long_mode(s,LV_LABEL_LONG_DOT);}
    /* Reuse the navigation marker, so hardware/gesture Back invokes the same guard. */
    return lv_settings_back(h,w-94,5,94,46,cfg->back,cfg->user_data);
}
lv_settings_frame_t lv_settings_frame_create(lv_obj_t *parent,const lv_settings_header_t *cfg)
{
    lv_settings_frame_t f={0};f.root=lv_settings_box(parent,0,0,1280,400,0xF1F4F5);
    ui_page_background_apply(f.root,UI_BACKGROUND_SETTINGS);
    f.back=lv_settings_header(f.root,24,16,1232,cfg);
    f.body=lv_settings_panel(f.root,24,84,1232,242);
    f.footer=lv_settings_box(f.root,24,338,1232,46,0);lv_obj_set_style_bg_opa(f.footer,0,0);
    f.message=lv_settings_label(f.footer,"",0,14,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_obj_set_width(f.message,860);lv_label_set_long_mode(f.message,LV_LABEL_LONG_DOT);
    return f;
}
lv_obj_t *lv_settings_grid(lv_obj_t *p,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_settings_box(p,x,y,w,h,0);lv_obj_set_style_bg_opa(o,0,0);
    lv_obj_add_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_port_indev_set_drag_obj(o,true);
    lv_obj_set_scroll_dir(o,LV_DIR_VER);lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(o,5,LV_PART_SCROLLBAR);lv_obj_set_style_radius(o,3,LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(o,lv_color_hex(0x9EACB8),LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(o,LV_OPA_70,LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_all(o,3,0);lv_obj_set_style_pad_right(o,12,0);
    lv_obj_set_flex_flow(o,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(o,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(o,12,0);lv_obj_set_style_pad_column(o,16,0);return o;
}
static void item_fit(lv_obj_t *o)
{
    lv_coord_t content_height=0;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);i++){
        lv_coord_t h=lv_obj_get_height(lv_obj_get_child(o,i));
        if(h>content_height)content_height=h;
    }
    lv_coord_t height=LV_MAX(80,content_height+34);
    if(lv_obj_get_height(o)!=height)lv_obj_set_height(o,height);
}
static void item_content_resized(lv_event_t *event)
{
    item_fit(lv_event_get_user_data(event));
}
lv_obj_t *lv_settings_item(lv_obj_t *grid,const lv_settings_item_t *cfg)
{
    if(cfg->value_label)*cfg->value_label=NULL;
    lv_obj_update_layout(grid);
    int width=(lv_obj_get_content_width(grid)-16)/2;
    lv_obj_t *o=lv_settings_box(grid,0,0,width,80,0xFFFFFF);
    lv_obj_set_style_pad_all(o,16,0);
    lv_obj_set_style_radius(o,14,0);lv_obj_set_style_border_width(o,1,0);
    lv_obj_set_style_border_color(o,lv_color_hex(0xE3E9ED),0);
    lv_obj_set_flex_flow(o,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(o,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(o,12,0);
    if(cfg->activate){lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(o,cfg->activate,LV_EVENT_CLICKED,cfg->user_data);}
    lv_port_indev_set_drag_obj(o,true);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xE2E9EE),LV_STATE_PRESSED);
    lv_obj_set_style_opa(o,LV_OPA_50,LV_STATE_DISABLED);
    if(cfg->disabled)lv_obj_add_state(o,LV_STATE_DISABLED);
    if(cfg->icon)lv_settings_icon(o,cfg->icon,0,0);
    lv_obj_t *copy=lv_settings_box(o,0,0,1,LV_SIZE_CONTENT,0);lv_obj_set_style_bg_opa(copy,0,0);
    lv_obj_set_flex_grow(copy,1);lv_obj_set_flex_flow(copy,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(copy,5,0);
    lv_obj_t *title=lv_settings_label(copy,cfg->title,0,0,&lv_font_instrument_sans_semibold_18,0x1D2B34);
    lv_obj_set_width(title,lv_pct(100));lv_label_set_long_mode(title,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(title,3,0);
    if(cfg->hint&&cfg->hint[0]){lv_obj_t *hint=lv_settings_label(copy,cfg->hint,0,0,&lv_font_instrument_sans_medium_14,0x586B78);
        lv_obj_set_style_text_line_space(hint,2,0);
        lv_obj_set_width(hint,lv_pct(100));lv_label_set_long_mode(hint,LV_LABEL_LONG_WRAP);}
    if(cfg->value){lv_obj_t *value=lv_settings_label(o,cfg->value,0,0,&lv_font_instrument_sans_medium_16,0x586B78);
        lv_obj_set_style_max_width(value,width/3,0);lv_label_set_long_mode(value,LV_LABEL_LONG_WRAP);
        if(cfg->value_label)*cfg->value_label=value;}
    if(cfg->activate)lv_settings_icon(o,"ChevronRight",0,0);
    /* LVGL 8.3 min-height is applied after flex cross alignment. Measure the
     * wrapped content, then set the actual height so single-line and two-line
     * cards both align to their final vertical centre. */
    lv_obj_update_layout(o);
    item_fit(o);
    lv_obj_update_layout(o);
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);i++)
        lv_obj_add_event_cb(lv_obj_get_child(o,i),item_content_resized,LV_EVENT_SIZE_CHANGED,o);
    return o;
}
