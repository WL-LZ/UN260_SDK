#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_34_standby.h"
#include "un260/lv_resources/ui_icons.h"
#include "un260/lv_resources/ui_page_background.h"
#include "un260/gesture/gesture_service.h"
#include "settings_detail_ui.h"
#include "lv_page_manager.h"
#include "un260/storage/standby_store.h"
#include "un260/app_service/app_standby_runtime.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/font/scaled_font.h"
#include "lv_port_indev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
LV_FONT_DECLARE(lv_font_standby_112);
typedef struct {lv_obj_t *root,*group,*time,*date,*greeting,*image,*dial,*date_group,*dial_group;int photo,minute;bool small;uint32_t dial_ink;scaled_font_t clock_font,detail_font;uint8_t clock_pixels[32768],detail_pixels[2048];} scene_t;
static lv_obj_t *page,*body,*note,*hex_label,*gallery,*edit_hint;
static lv_timer_t *settings_timer,*clock_timer;
static standby_config_t draft;
static scene_t preview,full;
static int tab; /* home, timeout, date, appearance, position, text colour */
static int selected;
static struct {bool active;int item;lv_point_t origin[3];} moving;
static bool redraw,saving;
static uint32_t notice_tick;
static bool notice_visible;
static const uint32_t colors[]={0x14232D,0xEDF1EC,0x30243C};
static const char*names[]={"Silver","Celadon","Champagne"};
static void render(void);
static void scene_layout(scene_t*s,const standby_layout_t*p);
static void editor_apply(void);
static standby_layout_t* layout(void){return &draft.layout[draft.mode][draft.active[draft.mode]];}
static lv_obj_t* box(lv_obj_t*p,int x,int y,int w,int h,uint32_t c){lv_obj_t*o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,lv_color_hex(c),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);return o;}
static lv_obj_t* label(lv_obj_t*p,const char*t,int x,int y,const lv_font_t*f,uint32_t c){lv_obj_t*o=lv_label_create(p);lv_label_set_text(o,t);lv_obj_set_pos(o,x,y);lv_obj_set_style_text_font(o,f,0);lv_obj_set_style_text_color(o,lv_color_hex(c),0);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;}
static uint32_t ink(uint32_t c){return (((c>>16)&255)*299+((c>>8)&255)*587+(c&255)*114)>145000?0x304957:0xFFFFFF;}
static lv_point_t dial_point(int cx,int cy,float radius,float degrees){float a=degrees*0.01745329252f;return (lv_point_t){cx+(int)roundf(sinf(a)*radius),cy-(int)roundf(cosf(a)*radius)};}
static void dial_draw(lv_event_t*e){
 scene_t*s=lv_event_get_user_data(e);lv_area_t a;lv_obj_get_coords(s->dial,&a);int cx=(a.x1+a.x2)/2,cy=(a.y1+a.y2)/2;float scale=lv_area_get_width(&a)/280.0f;
 lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=lv_color_hex(s->dial_ink);d.round_start=1;d.round_end=1;
 for(int i=0;i<60;i++){bool major=i%5==0;d.width=major?LV_MAX(1,(int)(2*scale)):1;d.opa=major?190:48;lv_point_t p=dial_point(cx,cy,125*scale,i*6),q=dial_point(cx,cy,(major?114:121)*scale,i*6);lv_draw_line(lv_event_get_draw_ctx(e),&d,&p,&q);}
 machine_time_value_t t;machine_time_get(&t);if(!machine_time_is_valid(&t)||t.year<2024)return;
 const float angles[]={(t.hour%12)*30+t.minute*.5f,t.minute*6.0f};const int lengths[]={68,98};
 for(int i=0;i<2;i++){d.width=LV_MAX(1,(int)((i?3:5)*scale));d.opa=255;d.color=lv_color_hex(s->dial_ink);lv_point_t p=dial_point(cx,cy,-10*scale,angles[i]),q=dial_point(cx,cy,lengths[i]*scale,angles[i]);lv_draw_line(lv_event_get_draw_ctx(e),&d,&p,&q);}
 lv_draw_rect_dsc_t hub;lv_draw_rect_dsc_init(&hub);hub.bg_color=lv_color_hex(s->dial_ink);hub.radius=LV_RADIUS_CIRCLE;int r=LV_MAX(2,(int)(4*scale));lv_area_t h={cx-r,cy-r,cx+r,cy+r};lv_draw_rect(lv_event_get_draw_ctx(e),&hub,&h);
}
static uint32_t text_ink(const standby_config_t*c,const standby_layout_t*p){
 if(!c->mode||!p->auto_text)return p->text_color;
 float rgb[3];for(int i=0;i<3;i++){float v=((p->color>>(16-8*i))&255)/255.0f;rgb[i]=v<=.04045f?v/12.92f:powf((v+.055f)/1.055f,2.4f);}
 float l=.2126f*rgb[0]+.7152f*rgb[1]+.0722f*rgb[2];return l>.179f?0x000000:0xFFFFFF;
}
static lv_obj_t*frame(scene_t*s,int item){return item==0?s->group:item==1?s->date_group:s->dial_group;}
static uint16_t*position(standby_layout_t*p,int item){return item==0?&p->x:item==1?&p->date_x:&p->dial_x;}
static void scene_colors(scene_t*s,const standby_config_t*c){const standby_layout_t*p=&c->layout[c->mode][c->active[c->mode]];uint32_t color=text_ink(c,p);lv_obj_set_style_text_color(s->time,lv_color_hex(color),0);lv_obj_set_style_text_color(s->date,lv_color_hex(color),0);lv_obj_set_style_text_color(s->greeting,lv_color_hex(color),0);s->dial_ink=color;if(s->dial)lv_obj_invalidate(s->dial);}

static void date_format(char*out,size_t n,const standby_layout_t*p,const machine_time_value_t*t){
 const char*months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
 const char*days[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
 char buf[80]="",v[32];bool numeric=p->date_style==2;
 if(p->date_bits&2){if(numeric)snprintf(v,sizeof(v),"%02u",t->month);else snprintf(v,sizeof(v),"%s",months[t->month-1]);strcat(buf,v);}
 if(p->date_bits&4){snprintf(v,sizeof(v),"%s%02u",buf[0]?(numeric?" / ":" "):"",t->day);strcat(buf,v);}
 if(p->date_bits&1){snprintf(v,sizeof(v),"%s%u",buf[0]?(numeric?" / ":" "):"",t->year);strcat(buf,v);}
 if(p->date_bits&8){struct tm tm={0};tm.tm_year=t->year-1900;tm.tm_mon=t->month-1;tm.tm_mday=t->day;tm.tm_hour=12;tm.tm_isdst=-1;mktime(&tm);snprintf(v,sizeof(v),"%s%s",buf[0]?"  |  ":"",days[tm.tm_wday]);strcat(buf,v);}
 snprintf(out,n,"%s",buf);
}
static void scene_update(scene_t*s,const standby_config_t*c,bool force){
 if(!s->root)return;
 const standby_layout_t*p=&c->layout[c->mode][c->active[c->mode]];
 machine_time_value_t t;machine_time_get(&t);bool valid=machine_time_is_valid(&t)&&t.year>=2024;
 int stamp=valid?((t.year-2000)*535680+t.month*44640+t.day*1440+t.hour*60+t.minute):-1;if(!force&&stamp==s->minute)return;s->minute=stamp;
 if(s->dial)lv_obj_invalidate(s->dial);
 unsigned photo=p->scheduled?(valid?t.hour/8:1):p->photo;if(standby_photo_is_imported(photo)&&!standby_photo_exists(photo-3))photo=1;
 if(!c->mode&&s->photo!=(int)photo){const char*path=standby_photo_path(photo);if(standby_photo_is_imported(photo))lv_img_cache_invalidate_src(path);lv_img_set_src(s->image,path);lv_img_set_pivot(s->image,0,0);s->photo=photo;}
 char timebuf[32],datebuf[96];if(valid){snprintf(timebuf,sizeof(timebuf),"%02u:%02u",p->hour12?(t.hour%12?t.hour%12:12):t.hour,t.minute);date_format(datebuf,sizeof(datebuf),p,&t);}else{snprintf(timebuf,sizeof(timebuf),"--:--");snprintf(datebuf,sizeof(datebuf),"Set device date and time");}
 lv_label_set_text(s->time,timebuf);lv_label_set_text(s->date,datebuf);
 const char*g=t.hour<12?"Good morning.":t.hour<18?"Good afternoon.":"Good evening.";char greeting[48];snprintf(greeting,sizeof(greeting),"%s%s%s",p->greeting?g:"",p->greeting&&p->hour12?"  ":"",p->hour12?(t.hour<12?"AM":"PM"):"");lv_label_set_text(s->greeting,valid?greeting:"UN260");scene_layout(s,p);
}
static void scene_layout(scene_t*s,const standby_layout_t*p){
 unsigned z=standby_layout_scale(p)*(s->small?4:10)/10;int k=s->small?4:10,pad=20*k/10;
 const lv_font_t*clock=&lv_font_standby_112;
 const lv_font_t*detail=&lv_font_instrument_sans_medium_18;
 scaled_font_init(&s->clock_font,clock,z,s->clock_pixels,sizeof(s->clock_pixels));scaled_font_init(&s->detail_font,detail,z,s->detail_pixels,sizeof(s->detail_pixels));
 lv_obj_set_style_text_font(s->time,z==100?clock:&s->clock_font.font,0);lv_obj_set_style_text_font(s->greeting,z==100?detail:&s->detail_font.font,0);lv_obj_set_style_text_font(s->date,z==100?detail:&s->detail_font.font,0);
 lv_obj_refresh_self_size(s->time);lv_obj_refresh_self_size(s->date);lv_obj_refresh_self_size(s->greeting);lv_obj_update_layout(s->root);
 bool greeting=p->greeting||p->hour12;int gap=greeting?lv_obj_get_height(s->greeting)+6*k/10:0;
 lv_obj_set_pos(s->greeting,pad,pad);lv_obj_set_pos(s->time,pad,pad+gap);lv_obj_set_pos(s->date,pad,pad);
 lv_obj_set_size(s->group,LV_MAX(lv_obj_get_width(s->time),greeting?lv_obj_get_width(s->greeting):0)+2*pad,lv_obj_get_height(s->time)+gap+2*pad);
 lv_obj_set_size(s->date_group,lv_obj_get_width(s->date)+2*pad,lv_obj_get_height(s->date)+2*pad);
 if(p->date_bits)lv_obj_clear_flag(s->date_group,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(s->date_group,LV_OBJ_FLAG_HIDDEN);
 for(int i=0;i<3;i++){lv_obj_t*f=frame(s,i);if(!f)continue;const uint16_t*xy=i==0?&p->x:i==1?&p->date_x:&p->dial_x;int x=LV_CLAMP(0,xy[0]*k/10,1280*k/10-lv_obj_get_width(f)),y=LV_CLAMP(40*k/10,xy[1]*k/10,334*k/10-lv_obj_get_height(f));lv_obj_set_pos(f,x,y);}
}
static void editor_apply(void){
 standby_layout_t*p=layout();scene_layout(&preview,p);lv_obj_update_layout(preview.root);
 for(int i=0;i<3;i++){lv_obj_t*f=frame(&preview,i);if(!f)continue;uint16_t*xy=position(p,i);xy[0]=lv_obj_get_x(f);xy[1]=lv_obj_get_y(f);}
}
static lv_area_t frame_area(int i){lv_obj_t*f=frame(&preview,i);return (lv_area_t){lv_obj_get_x(f),lv_obj_get_y(f),lv_obj_get_x(f)+lv_obj_get_width(f)-1,lv_obj_get_y(f)+lv_obj_get_height(f)-1};}
static bool overlap_half(lv_area_t a,lv_area_t b){lv_area_t cross;if(!_lv_area_intersect(&cross,&a,&b))return false;return (uint32_t)lv_area_get_size(&cross)*2>LV_MIN(lv_area_get_size(&a),lv_area_get_size(&b));}
static void move_begin(int item){moving.active=true;moving.item=item;selected=item;if(edit_hint)lv_label_set_text(edit_hint,"Drag a frame. Release over another to swap.");for(int i=0;i<3;i++){uint16_t*xy=position(layout(),i);moving.origin[i]=(lv_point_t){xy[0],xy[1]};}}
static void move_finish(void){
 if(!moving.active)return;
 int a=moving.item;moving.active=false;lv_area_t area=frame_area(a);
 for(int b=0;b<3;b++){lv_obj_t*f=frame(&preview,b);if(b==a||!f||lv_obj_has_flag(f,LV_OBJ_FLAG_HIDDEN)||!overlap_half(area,frame_area(b)))continue;
  standby_layout_t saved=*layout();uint16_t*pa=position(layout(),a),*pb=position(layout(),b);lv_obj_t*fa=frame(&preview,a);
  int aw=lv_obj_get_width(fa),ah=lv_obj_get_height(fa),bw=lv_obj_get_width(f),bh=lv_obj_get_height(f);
  pa[0]=LV_MAX(0,moving.origin[b].x+(bw-aw)/2);pa[1]=LV_MAX(40,moving.origin[b].y+(bh-ah)/2);
  pb[0]=LV_MAX(0,moving.origin[a].x+(aw-bw)/2);pb[1]=LV_MAX(40,moving.origin[a].y+(ah-bh)/2);editor_apply();
  bool invalid=false;for(int i=0;i<3;i++)for(int j=i+1;j<3;j++){lv_obj_t*fi=frame(&preview,i),*fj=frame(&preview,j);if(fi&&fj&&!lv_obj_has_flag(fi,LV_OBJ_FLAG_HIDDEN)&&!lv_obj_has_flag(fj,LV_OBJ_FLAG_HIDDEN)&&overlap_half(frame_area(i),frame_area(j)))invalid=true;}
  if(invalid){*layout()=saved;pa=position(layout(),a);pa[0]=moving.origin[a].x;pa[1]=moving.origin[a].y;editor_apply();if(edit_hint)lv_label_set_text(edit_hint,"Not enough room to swap. Move the other frame first.");}break;
 }
}
static void drag_cb(lv_event_t*e){
 int item=(int)(intptr_t)lv_event_get_user_data(e);lv_event_code_t code=lv_event_get_code(e);
 if(code==LV_EVENT_PRESSED){move_begin(item);editor_apply();return;}
 if(code==LV_EVENT_RELEASED){move_finish();return;}
 if(code==LV_EVENT_PRESS_LOST){moving.active=false;return;}
 if(code!=LV_EVENT_PRESSING)return;
 lv_indev_t*i=lv_indev_get_act();if(!i)return;lv_point_t v;lv_indev_get_vect(i,&v);uint16_t*xy=position(layout(),item);xy[0]=LV_MAX(0,xy[0]+v.x);xy[1]=LV_MAX(40,xy[1]+v.y);editor_apply();
}
static void drag_outline(lv_event_t*e){lv_area_t a;lv_obj_get_coords(lv_event_get_target(e),&a);lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=lv_color_hex(0x287BE4);d.width=1;d.dash_width=6;d.dash_gap=4;lv_point_t p[5]={{a.x1,a.y1},{a.x2,a.y1},{a.x2,a.y2},{a.x1,a.y2},{a.x1,a.y1}};for(int i=0;i<4;i++)lv_draw_line(lv_event_get_draw_ctx(e),&d,&p[i],&p[i+1]);}
static void scene_create(scene_t*s,lv_obj_t*parent,int x,int y,bool small,bool edit,const standby_config_t*c){
 memset(s,0,sizeof(*s));s->small=small;s->photo=-1;s->minute=-2;
 const standby_layout_t*p=&c->layout[c->mode][c->active[c->mode]];uint32_t text=text_ink(c,p);
 int w=small?512:1280,h=small?160:400;s->root=box(parent,x,y,w,h,c->mode?p->color:0xEEF2F4);
 if(!c->mode){s->image=lv_img_create(s->root);lv_img_set_pivot(s->image,0,0);lv_obj_set_pos(s->image,0,0);if(small)lv_img_set_zoom(s->image,104);
 lv_obj_t*shade=box(s->root,0,0,w,h,ink(text)==0x304957?0x001421:0xFFFFFF);lv_obj_set_style_bg_opa(shade,standby_photo_is_imported(p->photo)?(ink(text)==0x304957?80:65):0,0);lv_obj_clear_flag(shade,LV_OBJ_FLAG_CLICKABLE);
 }else{
  uint32_t accent=text==0xFFFFFF?0xCDFB84:0x233B39;int k=small?4:10;
  lv_obj_t*rail=box(s->root,34*k/10,62*k/10,1212*k/10,1,text);lv_obj_set_style_bg_opa(rail,35,0);lv_obj_clear_flag(rail,LV_OBJ_FLAG_CLICKABLE);if(edit)lv_obj_add_flag(rail,LV_OBJ_FLAG_HIDDEN);
  s->dial_group=box(s->root,p->dial_x*k/10,p->dial_y*k/10,256*k/10,256*k/10,0);lv_obj_set_style_bg_opa(s->dial_group,0,0);s->dial=box(s->dial_group,8*k/10,8*k/10,240*k/10,240*k/10,p->color);s->dial_ink=text;lv_obj_set_style_bg_opa(s->dial,0,0);lv_obj_clear_flag(s->dial,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(s->dial,dial_draw,LV_EVENT_DRAW_MAIN,s);
  lv_obj_t*pill=box(s->root,34*k/10,340*k/10,154*k/10,28*k/10,accent);lv_obj_clear_flag(pill,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_radius(pill,14*k/10,0);lv_obj_t*status=label(pill,"READY TO COUNT",0,0,small?&lv_font_instrument_sans_medium_10:&lv_font_instrument_sans_medium_12,ink(accent));lv_obj_center(status);if(small)lv_obj_add_flag(status,LV_OBJ_FLAG_HIDDEN);if(edit)lv_obj_add_flag(pill,LV_OBJ_FLAG_HIDDEN);
  if(!edit)label(s->root,"PRECISION. AT REST.",small?82:207,small?139:347,small?&lv_font_instrument_sans_medium_10:&lv_font_instrument_sans_medium_12,text);
 }
 if(!edit)label(s->root,"UN260",small?13:34,small?10:25,small?&lv_font_instrument_sans_medium_10:&lv_font_instrument_sans_medium_16,text);
 s->group=box(s->root,p->x*(small?4:10)/10,p->y*(small?4:10)/10,small?210:526,small?80:200,0);lv_obj_set_style_bg_opa(s->group,0,0);
 s->date_group=box(s->root,0,0,100,60,0);lv_obj_set_style_bg_opa(s->date_group,0,0);
 if(edit){for(int i=0;i<3;i++){lv_obj_t*f=frame(s,i);if(!f)continue;lv_port_indev_set_drag_obj(f,true);lv_obj_add_event_cb(f,drag_cb,LV_EVENT_ALL,(void*)(intptr_t)i);lv_obj_add_event_cb(f,drag_outline,LV_EVENT_DRAW_MAIN,NULL);}edit_hint=label(s->root,"Drag a frame. Release over another to swap.",32,16,&lv_font_instrument_sans_medium_14,0x287BE4);}
 s->greeting=label(s->group,"",small?6:16,small?2:4,small?&lv_font_instrument_sans_medium_10:&lv_font_instrument_sans_medium_18,text);if(!p->greeting&&!p->hour12)lv_obj_add_flag(s->greeting,LV_OBJ_FLAG_HIDDEN);
 s->time=label(s->group,"",small?4:10,small?12:25,small?&lv_font_instrument_sans_medium_48:&lv_font_standby_112,text);
 s->date=label(s->date_group,"",small?6:16,small?65:163,small?&lv_font_instrument_sans_medium_10:&lv_font_instrument_sans_medium_18,text);lv_obj_set_width(s->date,LV_SIZE_CONTENT);if(p->date_style==1)lv_obj_set_style_text_letter_space(s->date,small?0:1,0);
 scene_update(s,c,true);
}
static void action(lv_event_t*e);
/* Own every state: the shared settings button intentionally has a different theme. */
static lv_obj_t* control(lv_obj_t*p,int x,int y,int w,int h,int id,uint32_t bg,uint32_t border){
 lv_obj_t*b=box(p,x,y,w,h,bg);lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_radius(b,12,0);
 lv_obj_set_style_border_width(b,border?1:0,0);lv_obj_set_style_border_color(b,lv_color_hex(border),0);
 lv_obj_set_style_bg_color(b,lv_color_mix(lv_color_hex(0x7096B3),lv_color_hex(bg),28),LV_STATE_PRESSED);
 lv_obj_add_event_cb(b,action,LV_EVENT_CLICKED,(void*)(intptr_t)id);return b;
}
static lv_obj_t*button(lv_obj_t*p,int x,int y,int w,const char*t,int id,bool active){bool neutral=!active&&(id==1||id==64);lv_obj_t*b=control(p,x,y,w,44,id,active?0x176FE8:neutral?0xE9EDF0:0xF0F5F9,0);char clean[96];snprintf(clean,sizeof(clean),"%s",t);char*arrow=strchr(clean,'>');bool has_arrow=arrow!=NULL||clean[0]=='<';if(arrow){while(arrow>clean&&arrow[-1]==' ')arrow--;*arrow=0;}const char*text=clean[0]=='<'?clean+1:clean;while(*text==' ')text++;lv_obj_t*l=label(b,text,0,0,&lv_font_instrument_sans_medium_14,active?0xFFFFFF:neutral?0x293B44:0x4682B8);lv_obj_align(l,LV_ALIGN_CENTER,has_arrow?(id==1?4:-7):0,0);if(has_arrow){lv_obj_t*im=ui_icon_create(b,id==1?UI_ICON("back_18"):(id==3||id==14)?UI_ICON("expand_18"):UI_ICON("chevron_18"));if(im){lv_obj_align(im,id==1?LV_ALIGN_LEFT_MID:LV_ALIGN_RIGHT_MID,id==1?6:-6,0);lv_obj_set_style_img_recolor(im,lv_color_hex(active?0xFFFFFF:0x4682B8),0);lv_obj_set_style_img_recolor_opa(im,LV_OPA_COVER,0);}}return b;}
static void link_button(lv_obj_t*p,int x,int y,int w,const char*t,int id){lv_obj_t*b=button(p,x,y,w,t,id,false);lv_obj_set_style_bg_opa(b,0,0);lv_obj_set_style_bg_opa(b,255,LV_STATE_PRESSED);lv_obj_set_style_text_font(lv_obj_get_child(b,0),&lv_font_instrument_sans_medium_12,0);}
static lv_obj_t* choice(lv_obj_t*p,int x,int y,int w,int h,const char*t,int id,bool selected,uint32_t bg){lv_obj_t*b=control(p,x,y,w,h,id,bg,selected?0x3B87E7:0xDCE4E9);lv_obj_t*l=label(b,t,0,0,&lv_font_instrument_sans_medium_14,0x293B44);lv_obj_center(l);return b;}
static void toggle(lv_obj_t*p,int x,int y,int w,const char*t,int id,bool on){lv_obj_t*b=control(p,x,y,w,48,id,0xFFFFFF,0xE4EBEF);label(b,t,15,15,&lv_font_instrument_sans_medium_14,0x293B44);lv_obj_t*s=box(b,w-59,11,43,25,on?0x287CE5:0xDCE4E9);lv_obj_clear_flag(s,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_radius(s,13,0);lv_obj_t*k=box(s,on?20:2,2,21,21,0xFFFFFF);lv_obj_clear_flag(k,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_radius(k,11,0);}
static void queue_save(void){if(standby_store_save(&draft)){saving=true;redraw=true;}else if(note){lv_label_set_text(note,"Could not start saving. Please try again.");lv_obj_clear_flag(note,LV_OBJ_FLAG_HIDDEN);}}
static void discard(void*u){(void)u;draft=*standby_config();tab=0;redraw=true;}
static bool owns_single_drag(void){return tab==4||standby_store_busy()||settings_detail_overlay_is_open();}
static void discard_to_home(void*u){(void)u;ui_manager_clear_stack();ui_manager_switch(UI_PAGE_MAIN);}
static bool handle_gesture(gesture_action_t action){
 if(standby_store_busy()||settings_detail_overlay_is_open())return true;
 if(action==GESTURE_ACTION_EXIT_PAGE)return ui_page_34_standby_request_back();
 if(action==GESTURE_ACTION_HOME&&memcmp(&draft,standby_config(),sizeof(draft))){
  settings_detail_dialog_show("Discard changes?","Return Home without saving your layout?","Discard","Keep editing",discard_to_home,NULL,NULL);return true;
 }
 return false;
}
bool ui_page_34_standby_request_back(void){if(!page)return false;if(standby_store_busy()||settings_detail_overlay_is_open())return true;if(tab){if(memcmp(&draft,standby_config(),sizeof(draft)))settings_detail_dialog_show("Discard changes?","Your saved layout will not change.","Discard","Keep editing",discard,NULL,NULL);else{tab=0;redraw=true;}}else ui_manager_pop_page();return true;}
static void reset_mode(void*u){(void)u;standby_config_t d;standby_defaults(&d);memcpy(draft.layout[draft.mode],d.layout[draft.mode],sizeof(d.layout[0]));draft.active[draft.mode]=0;queue_save();}
static void delete_photo(void*u){(void)u;unsigned photo=layout()->photo;if(standby_store_delete(photo)){for(unsigned m=0;m<2;m++)for(unsigned i=0;i<3;i++)if(draft.layout[m][i].photo==photo){draft.layout[m][i].photo=1;draft.layout[m][i].scheduled=1;}redraw=true;}}
static void timeout_input(const char*s,void*u){(void)u;char*end;long v=strtol(s,&end,10);if(!*s||*end||v<1||v>1440){settings_detail_dialog_show("Invalid duration","Enter 1 to 1440 minutes. Choose Never separately.","OK",NULL,NULL,NULL,NULL);return;}draft.minutes=v;redraw=true;}
static void hex_input(const char*s,void*u){(void)u;if(*s=='#')s++;bool digits=true;for(const char*t=s;*t;t++)if(!isxdigit((unsigned char)*t))digits=false;char*end;unsigned long v=strtoul(s,&end,16);if(!digits||strlen(s)!=6||*end||v>0xFFFFFF){settings_detail_dialog_show("Invalid colour","Enter exactly six hexadecimal digits, e.g. EAF0F3.","OK",NULL,NULL,NULL,NULL);return;}if(tab==5){if(draft.mode&&layout()->auto_text)return;layout()->text_color=v;}else layout()->color=v;redraw=true;}
static void action(lv_event_t*e){int id=(int)(intptr_t)lv_event_get_user_data(e);if(standby_store_busy())return;
 if(id==1)ui_page_34_standby_request_back();
 else if(id==2)queue_save();
 else if(id==3){ui_manager_push_page(UI_PAGE_STANDBY);}
 else if(id>=10&&id<=14){tab=id-10;if(tab==4)selected=0;redraw=true;}
 else if(id==20||id==21){draft.mode=id-20;queue_save();}
 else if(id>=30&&id<=32){draft.active[draft.mode]=id-30;queue_save();}
 else if(id>=40&&id<=45){const int values[]={1,5,10,30,0,-1};if(id==45)settings_detail_keyboard_show("Custom duration (1-1440 min)","",4,SETTINGS_DETAIL_KEYBOARD_NUM,timeout_input,NULL);else draft.minutes=values[id-40];redraw=true;}
 else if(id>=50&&id<54){layout()->date_bits^=1<<(id-50);redraw=true;}
 else if(id>=54&&id<=56){layout()->date_style=id-54;redraw=true;}
 else if(id==57){layout()->hour12^=1;redraw=true;}
 else if(id==58){layout()->greeting^=1;redraw=true;}
 else if(id>=60&&id<=62){layout()->color=colors[id-60];redraw=true;}
 else if(id==63){if(tab==5&&draft.mode&&layout()->auto_text)return;char hex[8];snprintf(hex,sizeof(hex),"%06X",tab==5?layout()->text_color:layout()->color);settings_detail_keyboard_show("HEX colour",hex,7,SETTINGS_DETAIL_KEYBOARD_TEXT,hex_input,NULL);}
 else if(id==64){if(standby_store_import())redraw=true;}
 else if(id==66){tab=5;redraw=true;}
 else if(id==94){layout()->auto_text^=1;redraw=true;}
 else if(id==67){settings_detail_dialog_show_ex(SETTINGS_DIALOG_DESTRUCTIVE,"Delete imported photo?","Layouts using this photo return to daily rotation.","Delete","Cancel",delete_photo,NULL,NULL);}
 else if(id==68||id==69){layout()->scheduled=id==68;redraw=true;}
 else if(id>=70&&id<70+STANDBY_PHOTO_COUNT){layout()->photo=id-70;layout()->scheduled=0;redraw=true;}
 else if(id==80){settings_detail_dialog_show_ex(SETTINGS_DIALOG_DESTRUCTIVE,"Restore this mode?","Reset all three layouts. Keep photos, timeout and the other mode.","Restore","Cancel",reset_mode,NULL,NULL);}
 else if(id>=90&&id<=92){lv_obj_t*f=frame(&preview,selected);uint16_t*xy=position(layout(),selected);xy[0]=id==90?20:id==91?(1280-lv_obj_get_width(f))/2:1260-lv_obj_get_width(f);editor_apply();}
}
static void color_changed(lv_event_t*e){
 if(tab==5&&draft.mode&&layout()->auto_text)return;
 int shift=(int)(intptr_t)lv_event_get_user_data(e);uint32_t*c=tab==5?&layout()->text_color:&layout()->color;*c=(*c&~(255U<<shift))|((uint32_t)lv_slider_get_value(lv_event_get_target(e))<<shift);
 if(tab!=5)lv_obj_set_style_bg_color(preview.root,lv_color_hex(layout()->color),0);
 scene_colors(&preview,&draft);if(hex_label)lv_label_set_text_fmt(hex_label,"#%06X",*c);
}
static void rgb_controls(lv_obj_t*parent,int top,uint32_t value,bool locked){
 for(int i=0;i<3;i++){label(parent,(const char*[]){"R","G","B"}[i],5,top-9+i*48,&lv_font_instrument_sans_medium_18,0x52758A);lv_obj_t*s=lv_slider_create(parent);lv_obj_set_pos(s,55,top+i*48);lv_obj_set_size(s,588,10);lv_obj_set_style_bg_color(s,lv_color_hex(0xDAE4EC),LV_PART_MAIN);lv_obj_set_style_bg_opa(s,255,LV_PART_MAIN);lv_obj_set_style_radius(s,5,LV_PART_MAIN);lv_obj_set_style_bg_color(s,lv_color_hex(locked?0xA8B6C2:0x287BE4),LV_PART_INDICATOR);lv_obj_set_style_bg_opa(s,255,LV_PART_INDICATOR);lv_obj_set_style_bg_color(s,lv_color_hex(0xFFFFFF),LV_PART_KNOB);lv_obj_set_style_bg_opa(s,255,LV_PART_KNOB);lv_obj_set_style_radius(s,LV_RADIUS_CIRCLE,LV_PART_KNOB);lv_obj_set_style_pad_all(s,9,LV_PART_KNOB);lv_obj_set_style_border_width(s,2,LV_PART_KNOB);lv_obj_set_style_border_color(s,lv_color_hex(locked?0xA8B6C2:0x287BE4),LV_PART_KNOB);lv_obj_set_ext_click_area(s,16);lv_slider_set_range(s,0,255);lv_slider_set_value(s,(value>>(16-i*8))&255,LV_ANIM_OFF);lv_obj_add_event_cb(s,color_changed,LV_EVENT_VALUE_CHANGED,(void*)(intptr_t)(16-i*8));if(locked)lv_obj_add_state(s,LV_STATE_DISABLED);}
}
static void render(void){lv_obj_clean(page);memset(&preview,0,sizeof(preview));moving.active=false;hex_label=NULL;note=NULL;gallery=NULL;edit_hint=NULL;notice_visible=false;
 if(tab==4){if((selected==2&&!draft.mode)||(selected==1&&!layout()->date_bits))selected=0;scene_create(&preview,page,0,0,false,true,&draft);button(page,390,345,100,"Left",90,false);button(page,500,345,100,"Centre",91,false);button(page,610,345,100,"Right",92,false);button(page,870,345,120,"Cancel",1,false);button(page,1000,345,190,"Save layout",2,true);editor_apply();return;}
 lv_obj_t*icon=lv_img_create(page);lv_img_set_src(icon,"L:/usr/local/share/lvgl_data/standby/un260-mark.png");lv_obj_set_pos(icon,30,24);lv_obj_clear_flag(icon,LV_OBJ_FLAG_CLICKABLE);
 const char*titles[]={"Standby","Start after","Clock & date",draft.mode?"Colour palette":"Photo collection","Edit position","Text colour"};label(page,titles[tab],65,24,&lv_font_instrument_sans_medium_24,0x293B44);if(tab){char crumb[64];snprintf(crumb,sizeof(crumb),"%s / Layout %u",draft.mode?"Typographic":"Photo",draft.active[draft.mode]+1);label(page,crumb,305,31,&lv_font_instrument_sans_medium_12,0x8395A1);}
 if(tab)button(page,1006,16,112,"Cancel",1,false);else label(page,"Saved on this device",986,32,&lv_font_instrument_sans_medium_12,0x8395A1);
 button(page,1130,16,120,tab?"Save":"Preview >",tab?2:3,true);
 scene_create(&preview,page,30,76,true,false,&draft);lv_obj_set_size(preview.root,520,163);lv_obj_set_style_radius(preview.root,16,0);lv_obj_set_style_clip_corner(preview.root,true,0);
 lv_obj_t*tag=box(preview.root,411,130,97,22,0xFFFFFF);lv_obj_clear_flag(tag,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_radius(tag,7,0);label(tag,"LIVE PREVIEW",8,5,&lv_font_instrument_sans_medium_10,0x5C7480);
 note=label(page,standby_store_busy()?"Working... Please wait.":"",315,364,&lv_font_instrument_sans_medium_12,0x52758A);lv_obj_set_width(note,650);lv_obj_set_style_bg_color(note,lv_color_hex(0xFFFFFF),0);lv_obj_set_style_bg_opa(note,255,0);lv_obj_set_style_pad_all(note,7,0);lv_obj_set_style_radius(note,8,0);lv_obj_set_style_text_align(note,LV_TEXT_ALIGN_CENTER,0);if(!standby_store_busy())lv_obj_add_flag(note,LV_OBJ_FLAG_HIDDEN);
 body=box(page,574,76,676,296,0xF6F8FA);lv_obj_set_style_bg_opa(body,LV_OPA_TRANSP,0);
 standby_layout_t*p=layout();
 if(!tab){
  lv_obj_t*seg=box(body,0,0,676,48,0xE7ECF0);lv_obj_set_style_radius(seg,13,0);
  for(int i=0;i<2;i++){lv_obj_t*b=choice(seg,4+i*336,4,332,40,i?"Aa  Typographic":"Photo",20+i,draft.mode==i,draft.mode==i?0xFFFFFF:0xE7ECF0);lv_obj_set_style_border_width(b,0,0);if(!i)label(b,LV_SYMBOL_IMAGE,119,13,LV_FONT_DEFAULT,0x7595AA);}
  lv_obj_t*rows=box(body,0,83,676,198,0xFFFFFF);lv_obj_set_style_radius(rows,16,0);lv_obj_set_style_border_width(rows,1,0);lv_obj_set_style_border_color(rows,lv_color_hex(0xE6ECF0),0);
  const char*rt[]={"Start after","Clock & date",draft.mode?"Colour palette":"Photo collection"};const char*rs[]={"When the device is not in use","Date details, format and greeting",draft.mode?"Three colours. No background images.":"Daily rotation and your own photos."};
  char duration[32];snprintf(duration,sizeof(duration),"%u minutes  >",draft.minutes);if(!draft.minutes)snprintf(duration,sizeof(duration),"Never  >");
  for(int i=0;i<3;i++){if(i){lv_obj_t*d=box(rows,18,i*66,640,1,0xEEF1F6);lv_obj_clear_flag(d,LV_OBJ_FLAG_CLICKABLE);}label(rows,rt[i],18,17+i*66,&lv_font_instrument_sans_medium_14,0x293B44);label(rows,rs[i],18,39+i*66,&lv_font_instrument_sans_medium_10,0x667F90);button(rows,528,11+i*66,130,i==0?duration:i==1?"Customize  >":"Choose  >",11+i,false);}
  const char*presets[]={"Signature","Balance","Offset"};for(int i=0;i<3;i++){bool selected=draft.active[draft.mode]==i;lv_obj_t*b=control(page,30+i*177,253,166,69,30+i,selected?0xF5FAFF:0xFFFFFF,selected?0x3B87E7:0xDCE4E9);label(b,presets[i],14,18,&lv_font_instrument_sans_medium_14,0x293B44);label(b,i?"Personal layout":"Default layout",14,41,&lv_font_instrument_sans_medium_10,0x8395A1);if(selected)label(b,LV_SYMBOL_OK,140,12,LV_FONT_DEFAULT,0x227AE1);}
  label(page,"3 layouts - saved separately for each mode",30,340,&lv_font_instrument_sans_medium_10,0x8395A1);link_button(page,428,327,122,"Edit position >",14);link_button(page,574,357,208,"Restore this mode to defaults",80);link_button(page,30,355,72,"< Back",1);
 }else if(tab==1){const char*opts[]={"1","5","10","30","Never","Custom"};const int values[]={1,5,10,30,0,-1};bool custom=draft.minutes!=0&&draft.minutes!=1&&draft.minutes!=5&&draft.minutes!=10&&draft.minutes!=30;for(int i=0;i<6;i++){bool selected=i==5?custom:draft.minutes==values[i];lv_obj_t*b=control(body,(i%3)*228,(i/3)*88,218,78,40+i,selected?0xF2F8FF:0xFFFFFF,selected?0x3786E4:0xE0E8EE);label(b,opts[i],16,12,&lv_font_instrument_sans_medium_24,selected?0x2B79CF:0x293B44);label(b,i<4?"minutes":i==4?"Stay awake":"1-1440 minutes",16,48,&lv_font_instrument_sans_medium_12,0x8395A1);}label(body,"Applies to both modes. Counting and active tasks prevent standby.",0,192,&lv_font_instrument_sans_medium_12,0x667F90);char s[60];snprintf(s,sizeof(s),"Selected: %u minutes",draft.minutes);label(body,draft.minutes?s:"Selected: Never",0,224,&lv_font_instrument_sans_medium_16,0x287BE4);
 }else if(tab==2){const char*fields[]={"Year","Month","Day","Weekday"};for(int i=0;i<4;i++)toggle(body,(i%2)*343,(i/2)*58,333,fields[i],50+i,p->date_bits&(1<<i));label(body,"DATE STYLE",0,124,&lv_font_instrument_sans_medium_10,0x8597A3);lv_obj_t*seg=box(body,0,142,676,44,0xE8EDF1);lv_obj_set_style_radius(seg,11,0);const char*styles[]={"Editorial","Spaced","Numeric"};for(int i=0;i<3;i++){lv_obj_t*b=choice(seg,3+i*224,3,222,38,styles[i],54+i,false,p->date_style==i?0xFFFFFF:0xE8EDF1);lv_obj_set_style_border_width(b,0,0);}toggle(body,0,202,333,"12-hour time",57,p->hour12);toggle(body,343,202,333,"Greeting",58,p->greeting);link_button(page,30,255,200,"Edit clock position >",14);button(page,30,310,200,"Text colour",66,false);
 }else if(tab==5){
  bool locked=draft.mode&&p->auto_text;uint32_t color=locked?text_ink(&draft,p):p->text_color;
  if(draft.mode)toggle(body,0,0,676,"Automatic black / white",94,p->auto_text);else label(body,"CUSTOM TEXT COLOUR",0,12,&lv_font_instrument_sans_medium_14,0x293B44);
  lv_obj_t*enter=button(body,0,64,198,"Enter HEX",63,false);if(locked){lv_obj_add_state(enter,LV_STATE_DISABLED);lv_obj_set_style_text_color(lv_obj_get_child(enter,0),lv_color_hex(0xA8B6C2),0);}
  char h[10];snprintf(h,sizeof(h),"#%06X",color);hex_label=label(body,h,228,74,&lv_font_instrument_sans_medium_22,0x405F72);
  label(body,locked?"Turn off Automatic to use your saved custom colour.":"Time, date and greeting share this colour in this layout.",0,119,&lv_font_instrument_sans_medium_12,0x667F90);
  rgb_controls(body,156,color,locked);
  label(page,"Your custom colour is kept when Automatic is enabled.",30,262,&lv_font_instrument_sans_medium_12,0x8395A1);
 }else if(draft.mode){label(body,"COLOUR STUDIES",0,0,&lv_font_instrument_sans_medium_10,0x8597A3);const char*palettes[]={"Midnight","Chalk","Plum"};for(int i=0;i<3;i++){lv_obj_t*b=choice(body,i*228,24,218,48,palettes[i],60+i,p->color==colors[i],colors[i]);lv_obj_set_style_text_color(lv_obj_get_child(b,0),lv_color_hex(ink(colors[i])),0);}button(body,0,84,198,"Enter HEX",63,false);char h[10];snprintf(h,sizeof(h),"#%06X",p->color);hex_label=label(body,h,228,94,&lv_font_instrument_sans_medium_22,0x405F72);
  rgb_controls(body,132,p->color,false);button(page,30,278,220,"Text colour",66,false);
 }else{

  lv_obj_t*seg=box(body,0,0,662,44,0xE7ECF0);lv_obj_set_style_radius(seg,12,0);for(int i=0;i<2;i++){lv_obj_t*b=choice(seg,4+i*329,4,325,36,i?"Single photo":"Follow the day",68+i,false,p->scheduled==!i?0xFFFFFF:0xE7ECF0);lv_obj_set_style_border_width(b,0,0);}
  lv_obj_t*schedule=box(body,0,58,662,51,0xFFFFFF);lv_obj_set_style_radius(schedule,12,0);const char*periods[]={"00:00 - 08:00","08:00 - 16:00","16:00 - 24:00"};for(int i=0;i<3;i++){label(schedule,names[i],12+i*220,8,&lv_font_instrument_sans_medium_12,0x293B44);label(schedule,periods[i],12+i*220,29,&lv_font_instrument_sans_medium_10,0x667F90);}
  label(body,"CHOOSE A PHOTO",0,121,&lv_font_instrument_sans_medium_10,0x8395A1);
gallery=box(body,0,140,676,156,0xF6F8FA);lv_obj_set_style_bg_opa(gallery,LV_OPA_TRANSP,0);lv_obj_add_flag(gallery,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_ELASTIC);lv_obj_clear_flag(gallery,LV_OBJ_FLAG_SCROLL_CHAIN_VER);lv_obj_set_scroll_dir(gallery,LV_DIR_VER);lv_obj_set_scrollbar_mode(gallery,LV_SCROLLBAR_MODE_AUTO);lv_obj_set_style_width(gallery,4,LV_PART_SCROLLBAR);lv_obj_set_style_pad_right(gallery,1,LV_PART_SCROLLBAR);lv_obj_set_style_radius(gallery,2,LV_PART_SCROLLBAR);lv_obj_set_style_bg_opa(gallery,170,LV_PART_SCROLLBAR);lv_obj_set_style_bg_color(gallery,lv_color_hex(0x8395A1),LV_PART_SCROLLBAR);int count=0;
  for(int order=0;order<STANDBY_PHOTO_COUNT;order++){int i=order<3?order:order==3?STANDBY_PHOTO_MIST:order-1;if(standby_photo_is_imported(i)&&!standby_photo_exists(i-3))continue;char text[32];if(i<3)snprintf(text,sizeof(text),"%s",names[i]);else if(i==STANDBY_PHOTO_MIST)snprintf(text,sizeof(text),"Mist");else snprintf(text,sizeof(text),"USB photo %d",i-2);lv_obj_t*b=control(gallery,(count%3)*224,(count/3)*101,214,91,70+i,0xE6ECEE,!p->scheduled&&p->photo==i?0x3485E6:0xDCE4E9);count++;lv_obj_set_style_border_post(b,true,0);lv_obj_set_style_clip_corner(b,true,0);lv_obj_t*im=lv_img_create(b);lv_img_set_src(im,standby_photo_path(i));lv_img_set_pivot(im,0,0);lv_img_set_zoom(im,44);lv_obj_clear_flag(im,LV_OBJ_FLAG_CLICKABLE);lv_obj_t*caption=box(b,1,62,212,28,0xFFFFFF);lv_obj_clear_flag(caption,LV_OBJ_FLAG_CLICKABLE);label(caption,text,9,7,&lv_font_instrument_sans_medium_12,0x293B44);if(!p->scheduled&&p->photo==i)label(caption,LV_SYMBOL_OK,190,6,LV_FONT_DEFAULT,0x227AE1);}
  button(page,30,284,190,"Import from USB",64,false);button(page,232,284,318,"Text colour",66,false);if(standby_photo_is_imported(p->photo))link_button(page,30,346,190,"Delete selected photo",67);
  label(page,"Changes apply only to this layout. Other layouts stay unchanged.",30,253,&lv_font_instrument_sans_medium_12,0x8395A1);label(page,"USB root: un260_delay_01.png ... 99.png\nPNG up to 5 MB. Max 6 photos.",232,338,&lv_font_instrument_sans_medium_12,0x667F90);
 }
 if(note)lv_obj_move_foreground(note);
}
static void settings_tick(lv_timer_t*t){(void)t;char msg[160];if(standby_store_poll(msg,sizeof(msg))){if(saving){bool ok=!memcmp(&draft,standby_config(),sizeof(draft));saving=false;if(ok)tab=0;}render();redraw=false;if(note){lv_label_set_text(note,msg);lv_obj_clear_flag(note,LV_OBJ_FLAG_HIDDEN);notice_tick=lv_tick_get();notice_visible=true;}return;}if(redraw){render();redraw=false;}if(notice_visible&&lv_tick_elaps(notice_tick)>=2500){notice_visible=false;if(note)lv_obj_add_flag(note,LV_OBJ_FLAG_HIDDEN);}if(!(tab==4&&moving.active))scene_update(&preview,&draft,false);}
void ui_page_34_standby_create(lv_obj_t*parent){if(page)return;draft=*standby_config();tab=0;selected=0;saving=false;gesture_service_set_page_policy(UI_PAGE_STANDBY_SETTING,owns_single_drag,handle_gesture);page=box(parent,0,0,1280,400,0xF6F8FA);ui_page_background_apply(page,UI_BACKGROUND_SETTINGS);render();settings_timer=lv_timer_create(settings_tick,100,NULL);}
void ui_page_34_standby_destroy(void){gesture_service_clear_page_policy(UI_PAGE_STANDBY_SETTING);settings_detail_keyboard_hide();settings_detail_dialog_hide();if(settings_timer)lv_timer_del(settings_timer);settings_timer=NULL;if(page)lv_obj_del(page);page=NULL;body=NULL;note=NULL;hex_label=NULL;gallery=NULL;edit_hint=NULL;moving.active=false;notice_visible=false;memset(&preview,0,sizeof(preview));}
/* A black cover animates without a full-screen translucent object layer.
 * The page owns cover and animation; entry and exit timings are independent. */
#define STANDBY_FADE_IN_MS 300
#define STANDBY_FADE_OUT_MS 150
static lv_obj_t *fade_cover;
static bool fade_exiting,fade_finished;
static void fade_exec(void *obj,int32_t value){lv_obj_set_style_bg_opa(obj,(lv_opa_t)value,0);}
static void fade_ready(lv_anim_t*a){(void)a;if(fade_exiting)fade_finished=true;}
static void fade_start(int from,int to){
 lv_anim_del(fade_cover,fade_exec);
 lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,fade_cover);
 lv_anim_set_values(&a,from,to);lv_anim_set_time(&a,fade_exiting?STANDBY_FADE_OUT_MS:STANDBY_FADE_IN_MS);
 lv_anim_set_exec_cb(&a,fade_exec);lv_anim_set_path_cb(&a,lv_anim_path_linear);
 lv_anim_set_ready_cb(&a,fade_ready);
 if(!lv_anim_start(&a)){fade_exec(fade_cover,to);if(fade_exiting)fade_finished=true;}
}
bool ui_page_35_standby_fade_out(void){
 if(!fade_cover)return true;
 if(!fade_exiting){fade_exiting=true;fade_finished=false;
  fade_start(lv_obj_get_style_bg_opa(fade_cover,0),LV_OPA_COVER);
 }
 return fade_finished;
}
static void clock_tick(lv_timer_t*t){(void)t;if(!fade_exiting)scene_update(&full,standby_config(),false);}
void ui_page_35_standby_create(lv_obj_t*parent){
 app_standby_runtime_enter();scene_create(&full,parent,0,0,false,false,standby_config());
 fade_exiting=false;fade_finished=false;
 fade_cover=box(full.root,0,0,1280,400,0x000000);
 lv_obj_clear_flag(fade_cover,LV_OBJ_FLAG_CLICKABLE);
 fade_start(LV_OPA_COVER,LV_OPA_TRANSP);
 clock_timer=lv_timer_create(clock_tick,1000,NULL);
}
void ui_page_35_standby_destroy(void){
 if(clock_timer)lv_timer_del(clock_timer);
 clock_timer=NULL;
 if(fade_cover)lv_anim_del(fade_cover,fade_exec);
 fade_cover=NULL;fade_exiting=false;fade_finished=false;
 if(full.root)lv_obj_del(full.root);
 memset(&full,0,sizeof(full));
}
