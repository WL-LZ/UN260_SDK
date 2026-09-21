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
    lv_obj_t *o=lv_settings_box(p,x,y,w,h,primary?0x1462CC:LV_SETTINGS_CONTROL_SURFACE);
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
    for(lv_obj_t *ancestor=p;ancestor;ancestor=lv_obj_get_parent(ancestor)){
        if(lv_obj_has_flag(ancestor,LV_OBJ_FLAG_SCROLLABLE)&&lv_obj_has_flag(ancestor,LV_OBJ_FLAG_USER_4)){
            lv_port_indev_set_drag_obj(o,true);break;
        }
    }
    return o;
}
lv_obj_t *lv_settings_segment_base(lv_obj_t *p,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_settings_box(p,x,y,w,h,LV_SETTINGS_CONTROL_SURFACE);
    lv_obj_set_style_radius(o,12,0);return o;
}
lv_obj_t *lv_settings_segment(lv_obj_t *p,unsigned index,unsigned count,const char *text,lv_event_cb_t cb,void *data)
{
    if(!count||index>=count)return NULL;
    int width=(lv_obj_get_style_width(p,0)-8)/(int)count;
    int height=lv_obj_get_style_height(p,0)-8;
    lv_obj_t *o=lv_settings_button(p,4+index*width,4,width,height,text,false,cb,data);
    lv_obj_set_style_radius(o,8,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),0);
    lv_obj_set_style_bg_color(o,lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xFFFFFF),LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xFFFFFF),LV_STATE_CHECKED|LV_STATE_DISABLED);
    lv_obj_set_style_border_width(o,0,LV_STATE_CHECKED);
    lv_obj_set_style_text_color(o,lv_color_hex(0x1D2B34),LV_STATE_CHECKED);
    lv_obj_set_style_text_color(o,lv_color_hex(0x1D2B34),LV_STATE_CHECKED|LV_STATE_DISABLED);
    lv_obj_t *label=lv_obj_get_child(o,0);
    lv_obj_set_width(label,width-12);lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(label);
    return o;
}
lv_obj_t *lv_settings_back(lv_obj_t *p,int x,int y,int w,int h,lv_event_cb_t cb,void *data)
{
    lv_obj_t *o=lv_nav_button_create(p,x,y,w,h,cb,data);
    lv_damped_button_set_exact_palette(o,lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),lv_color_hex(0xE2E9EE));
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
    lv_settings_header_t header=*cfg;header.subtitle=NULL;
    f.back=lv_settings_header(f.root,24,10,1232,&header);
    lv_obj_t *heading=lv_obj_get_parent(f.back);
    for(uint32_t i=0;i<lv_obj_get_child_cnt(heading);i++){
        lv_obj_t *child=lv_obj_get_child(heading,i);
        if(lv_obj_check_type(child,&lv_label_class))lv_obj_set_width(child,540);
    }
    f.body=lv_settings_panel(f.root,24,84,1232,300);
    /* Keep footer as the action owner for existing page lifecycles (PIN overlay).
     * Flex ignores historical absolute button positions and reflows hidden actions. */
    f.footer=lv_settings_box(f.root,640,15,494,46,0);lv_obj_set_style_bg_opa(f.footer,0,0);
    lv_obj_add_flag(f.footer,LV_OBJ_FLAG_USER_3);
    lv_obj_set_flex_flow(f.footer,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(f.footer,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(f.footer,10,0);
    lv_settings_box(f.root,1147,23,1,30,0xDCE3E7);
    f.message=lv_settings_label(f.root,cfg->subtitle?cfg->subtitle:"",24,65,&lv_font_instrument_sans_medium_12,0x586B78);
    lv_obj_set_width(f.message,1232);lv_label_set_long_mode(f.message,LV_LABEL_LONG_DOT);
    return f;
}
lv_obj_t *lv_settings_actions(lv_obj_t *root)
{
    for(uint32_t i=0;i<lv_obj_get_child_cnt(root);i++){
        lv_obj_t *child=lv_obj_get_child(root,i);
        if(lv_obj_has_flag(child,LV_OBJ_FLAG_USER_3))return child;
    }
    return NULL;
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
    int width=cfg->grouped?lv_obj_get_content_width(grid):(lv_obj_get_content_width(grid)-16)/2;
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
    if(cfg->grouped){
        lv_obj_set_style_radius(o,0,0);
        lv_obj_set_style_border_side(o,LV_BORDER_SIDE_BOTTOM,0);
        lv_obj_set_style_border_color(o,lv_color_hex(0xEEF1F3),0);
        lv_settings_group_refresh(grid);
    }
    return o;
}

lv_obj_t *lv_settings_list(lv_obj_t *p,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_settings_grid(p,x,y,w,h);
    lv_obj_set_flex_flow(o,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(o,16,0);
    return o;
}
void lv_settings_group_refresh(lv_obj_t *group)
{
    lv_obj_t *last=NULL;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(group);i++){
        lv_obj_t *row=lv_obj_get_child(group,i);
        if(lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN))continue;
        if(last&&lv_obj_get_style_border_width(last,0)!=1)lv_obj_set_style_border_width(last,1,0);
        last=row;
    }
    if(last&&lv_obj_get_style_border_width(last,0)!=0)lv_obj_set_style_border_width(last,0,0);
    if(last)lv_obj_clear_flag(group,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(group,LV_OBJ_FLAG_HIDDEN);
}
static void group_layout(lv_event_t *event)
{
    lv_settings_group_refresh(lv_event_get_target(event));
}
lv_obj_t *lv_settings_group(lv_obj_t *list)
{
    lv_obj_update_layout(list);
    lv_obj_t *o=lv_settings_box(list,0,0,lv_obj_get_content_width(list),LV_SIZE_CONTENT,0xFFFFFF);
    lv_obj_set_style_radius(o,14,0);lv_obj_set_style_clip_corner(o,true,0);
    lv_obj_set_flex_flow(o,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_pad_row(o,0,0);
    lv_obj_add_event_cb(o,group_layout,LV_EVENT_LAYOUT_CHANGED,NULL);
    return o;
}
lv_obj_t *lv_settings_control_row(lv_obj_t *p,int x,int y,int w,int control_height,bool separator)
{
    lv_obj_t *o=lv_settings_box(p,x,y,w,control_height+17,0);
    lv_obj_set_style_bg_opa(o,0,0);
    if(separator){lv_obj_set_style_border_width(o,1,0);
        lv_obj_set_style_border_side(o,LV_BORDER_SIDE_TOP,0);
        lv_obj_set_style_border_color(o,lv_color_hex(0xEEF1F3),0);}
    return o;
}
