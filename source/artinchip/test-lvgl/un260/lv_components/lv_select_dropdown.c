#include "lv_select_dropdown.h"
#include "lv_port_indev.h"
#include "ui_scrollbar.h"
#include "lv_nav_button.h"
#include "ui_selection_palette.h"
#include "lv_settings_palette.h"

#define SELECT_ROW_HEIGHT 44

static void select_field_event(lv_event_t *e)
{
    lv_obj_t *field=lv_event_get_target(e);
    if(lv_event_get_code(e)==LV_EVENT_READY) {
        lv_obj_t *list=lv_dropdown_get_list(field);
        lv_obj_set_style_min_width(list,lv_obj_get_width(field),0);
        lv_obj_set_style_max_width(list,lv_obj_get_width(field),0);
        lv_area_t a;lv_obj_get_coords(field,&a);
        int screen_height=lv_disp_get_ver_res(lv_obj_get_disp(field));
        int available=LV_MAX(a.y1,screen_height-a.y2-1)-12;
        lv_obj_set_style_max_height(list,LV_MIN(SELECT_ROW_HEIGHT*5+14,LV_MAX(SELECT_ROW_HEIGHT,available)),0);
    } else if(lv_event_get_code(e)==LV_EVENT_DRAW_MAIN_END) {
        /* Paint inside the field: a decorative child outside its content area
         * used to create horizontal overflow and a spurious scrollbar. */
        lv_area_t a;lv_obj_get_coords(field,&a);
        bool open=lv_dropdown_is_open(field);
        lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(e);
        char caption[128];lv_dropdown_get_selected_str(field,caption,sizeof(caption));
        lv_draw_label_dsc_t text;lv_draw_label_dsc_init(&text);
        lv_obj_init_draw_label_dsc(field,LV_PART_MAIN,&text);
        text.flag=LV_TEXT_FLAG_EXPAND;
        lv_area_t text_area={a.x1+17,a.y1+(lv_area_get_height(&a)-text.font->line_height)/2,
                             a.x2-40,a.y2-1},clip;
        const lv_area_t *old_clip=ctx->clip_area;
        if(_lv_area_intersect(&clip,old_clip,&text_area)) {
            ctx->clip_area=&clip;lv_draw_label(ctx,&text,&text_area,caption,NULL);
            ctx->clip_area=old_clip;
        }
        int x=a.x2-23,y=(a.y1+a.y2)/2;
        lv_point_t p[3]={{x-5,y+(open?2:-2)},{x,y+(open?-3:3)},{x+5,y+(open?2:-2)}};
        lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);
        line.color=lv_color_hex(open?LV_SETTINGS_PRIMARY:0x627C8D);line.width=2;
        line.round_start=line.round_end=1;
        lv_draw_line(lv_event_get_draw_ctx(e),&line,&p[0],&p[1]);
        lv_draw_line(lv_event_get_draw_ctx(e),&line,&p[1],&p[2]);
    }
}

static void select_list_event(lv_event_t *e)
{
    if(lv_event_get_code(e)==LV_EVENT_CLICKED) {
        /* The common Back dispatcher closes the topmost list first. Normal
         * option clicks have already committed/closed in the native handler. */
        lv_obj_t *field=lv_event_get_user_data(e);
        if(lv_dropdown_is_open(field))lv_dropdown_close(field);
        return;
    }
    if(lv_event_get_code(e)!=LV_EVENT_DRAW_POST_END)return;
    lv_obj_t *field=lv_event_get_user_data(e),*list=lv_event_get_target(e);
    lv_obj_t *label=lv_obj_get_child(list,0);
    if(!label || !lv_dropdown_get_option_cnt(field))return;
    lv_area_t a,text,clip;lv_obj_get_coords(list,&a);lv_obj_get_coords(label,&text);
    lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(e);
    a.x1+=1;a.y1+=1;a.x2-=1;a.y2-=1;
    if(!_lv_area_intersect(&clip,ctx->clip_area,&a))return;
    const lv_area_t *old=ctx->clip_area;ctx->clip_area=&clip;
    const lv_font_t *font=lv_obj_get_style_text_font(list,LV_PART_MAIN);
    int y=text.y1+lv_dropdown_get_selected(field)*SELECT_ROW_HEIGHT+font->line_height/2;
    int x=a.x2-UI_SCROLLBAR_GUTTER-15;
    lv_point_t p[3]={{x-5,y},{x-1,y+4},{x+6,y-4}};
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);
    line.color=lv_color_hex(LV_SETTINGS_PRIMARY);line.width=2;line.round_start=line.round_end=1;
    lv_draw_line(ctx,&line,&p[0],&p[1]);lv_draw_line(ctx,&line,&p[1],&p[2]);
    ctx->clip_area=old;
}

lv_obj_t *lv_select_dropdown_create(lv_obj_t *parent,lv_coord_t x,lv_coord_t y,
                                    lv_coord_t width,lv_coord_t height)
{
    lv_obj_t *o=lv_dropdown_create(parent);
    if(!o)return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,height);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),0);
    lv_obj_set_style_bg_color(o,lv_color_hex(UI_SELECTION_SURFACE),LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(o,lv_color_hex(LV_SETTINGS_CONTROL_PRESSED),LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(o,lv_color_hex(LV_SETTINGS_CONTROL_PRESSED),LV_STATE_CHECKED|LV_STATE_PRESSED);
    lv_obj_set_style_border_width(o,1,0);
    lv_obj_set_style_border_color(o,lv_color_hex(0xD4DBE1),0);
    lv_obj_set_style_border_color(o,lv_color_hex(LV_SETTINGS_CHOICE_BORDER),LV_STATE_CHECKED);
    lv_obj_set_style_radius(o,12,0);
    lv_obj_set_style_text_font(o,&lv_font_instrument_sans_medium_18,0);
    lv_obj_set_style_text_color(o,lv_color_hex(LV_SETTINGS_ACTION_TEXT),0);
    lv_obj_set_style_text_color(o,lv_color_hex(LV_SETTINGS_PRIMARY),LV_STATE_CHECKED);
    lv_obj_set_style_pad_left(o,16,0);lv_obj_set_style_pad_right(o,40,0);
    lv_obj_set_style_pad_top(o,(height-lv_font_instrument_sans_medium_18.line_height)/2-1,0);
    /* Native selection and list remain intact; render the field's single-line
     * value/chevron ourselves to reserve their separate, non-scrollable areas. */
    lv_dropdown_set_symbol(o,NULL);
    lv_dropdown_set_text(o,"");
    lv_obj_t *list=lv_dropdown_get_list(o);
    if(!list)goto failed;
    lv_obj_remove_style_all(list);
    const int space=SELECT_ROW_HEIGHT-lv_font_instrument_sans_medium_18.line_height;
    lv_obj_set_style_text_font(list,&lv_font_instrument_sans_medium_18,0);
    lv_obj_set_style_text_font(list,&lv_font_instrument_sans_medium_18,LV_PART_SELECTED);
    lv_obj_set_style_text_line_space(list,space,0);
    lv_obj_set_style_text_line_space(list,space,LV_PART_SELECTED);
    lv_obj_set_style_text_color(list,lv_color_hex(0x20303A),0);
    lv_obj_set_style_text_color(list,lv_color_hex(LV_SETTINGS_PRIMARY),LV_PART_SELECTED);
    lv_obj_set_style_text_align(list,LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_style_bg_color(list,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_bg_opa(list,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(list,lv_color_hex(UI_SELECTION_SURFACE),LV_PART_SELECTED);
    lv_obj_set_style_bg_color(list,lv_color_hex(LV_SETTINGS_CONTROL_PRESSED),LV_PART_SELECTED|LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(list,LV_OPA_COVER,LV_PART_SELECTED);
    lv_obj_set_style_border_color(list,lv_color_hex(0xD5DFE6),0);
    lv_obj_set_style_border_width(list,1,0);
    lv_obj_set_style_radius(list,12,0);
    lv_obj_set_style_clip_corner(list,true,0);
    lv_obj_set_style_pad_left(list,16,0);
    lv_obj_set_style_pad_right(list,UI_SCROLLBAR_GUTTER+28,0);
    lv_obj_set_style_pad_ver(list,space/2+6,0);
    lv_obj_set_style_max_height(list,SELECT_ROW_HEIGHT*5+14,0);
    lv_obj_set_style_shadow_width(list,12,0);
    lv_obj_set_style_shadow_opa(list,LV_OPA_10,0);
    lv_obj_set_style_shadow_color(list,lv_color_hex(0x20303A),0);
    lv_obj_set_style_shadow_ofs_y(list,4,0);
    lv_obj_set_scroll_dir(list,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(list,LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_port_indev_set_drag_obj(list,true);
    if(!lv_obj_add_event_cb(o,select_field_event,LV_EVENT_ALL,NULL) ||
       !lv_obj_add_event_cb(list,select_list_event,LV_EVENT_DRAW_POST_END,o) ||
       !lv_obj_add_event_cb(list,select_list_event,LV_EVENT_CLICKED,o))goto failed;
    lv_nav_button_mark_back(list);
    return o;
failed:
    lv_obj_del(o);return NULL;
}
