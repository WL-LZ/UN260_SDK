#include "un260/font/manrope_fonts.h"
#include "smart_island_activity.h"
#include "smart_island_internal.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/font/ui_message_font.h"
#include <string.h>
static lv_obj_t *panel,*heading,*copy,*bars[4],*mark;
static lv_timer_t *timer;
static si_activity_t kind;
static unsigned step;
static bool visible;
static void animate(lv_timer_t *t)
{
 (void)t;static const uint8_t h[]={7,11,17,23,17,11};
 for(unsigned i=0;i<4;i++){unsigned height=h[(step+i)%6];lv_obj_set_height(bars[i],height);lv_obj_set_y(bars[i],(44-height)/2);}
 step++;
}
static void deleted(lv_event_t *e)
{
 (void)e;if(timer)lv_timer_del(timer);timer=NULL;panel=heading=copy=mark=NULL;memset(bars,0,sizeof(bars));visible=false;
}
void smart_island_activity_destroy(void){if(panel)lv_obj_del(panel);kind=SI_ACTIVITY_NONE;}
bool smart_island_activity_slot_available(void)
{
 return g_si_ctx.lifecycle.created&&!g_si_ctx.lifecycle.suspended&&
   !g_si_ctx.lifecycle.count_session_active&&!g_si_ctx.lifecycle.result_transition_pending&&
   g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&g_si_ctx.view.visual==SMART_ISLAND_VISUAL_COMPACT&&!g_si_ctx.view.anim_running;
}
void smart_island_activity_sync(void)
{
 bool show=kind!=SI_ACTIVITY_NONE&&smart_island_activity_slot_available();
 if(!panel)return;
 if(show!=visible){visible=show;if(show){lv_obj_clear_flag(panel,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(panel);}else lv_obj_add_flag(panel,LV_OBJ_FLAG_HIDDEN);}
 if(timer){if(show&&kind==SI_ACTIVITY_WORKING)lv_timer_resume(timer);else lv_timer_pause(timer);}
}
bool smart_island_activity_set(si_activity_t next,const char *title,const char *subtitle,lv_event_cb_t click)
{
 kind=next;
 if(!panel&&kind!=SI_ACTIVITY_NONE&&g_si_ctx.objects.root){
   panel=lv_settings_box(g_si_ctx.objects.root,0,0,lv_pct(100),44,0x111111);
   lv_obj_set_style_radius(panel,22,0);lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);
   lv_obj_add_event_cb(panel,deleted,LV_EVENT_DELETE,NULL);if(click)lv_obj_add_event_cb(panel,click,LV_EVENT_CLICKED,NULL);
   for(unsigned i=0;i<4;i++){bars[i]=lv_settings_box(panel,14+i*5,16,3,12,0xA6C5DF);lv_obj_set_style_radius(bars[i],2,0);}
   mark=lv_settings_icon(panel,"Check",13,10);lv_obj_set_style_img_recolor(mark,lv_color_white(),0);lv_obj_set_style_img_recolor_opa(mark,LV_OPA_COVER,0);
   heading=lv_settings_label(panel,"",42,6,&lv_font_instrument_sans_medium_14,0xFFFFFF);
   copy=lv_settings_label(panel,"",42,24,&lv_font_instrument_sans_medium_12,0xE2EAF1);
   lv_obj_set_width(heading,207);lv_obj_set_width(copy,207);lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);lv_label_set_long_mode(copy,LV_LABEL_LONG_DOT);
   timer=lv_timer_create(animate,120,NULL);lv_timer_pause(timer);
 }
 if(panel){
   uint32_t color=kind==SI_ACTIVITY_SUCCESS?0x258566:kind==SI_ACTIVITY_UNCERTAIN?0x956A2D:kind==SI_ACTIVITY_ERROR?0xA63C49:0x111111;
   lv_obj_set_style_bg_color(panel,lv_color_hex(color),0);
   if(strcmp(lv_label_get_text(heading),title?title:""))lv_label_set_text(heading,title?title:"");
   if(strcmp(lv_label_get_text(copy),subtitle?subtitle:""))lv_label_set_text(copy,subtitle?subtitle:"");
   for(unsigned i=0;i<4;i++){if(kind==SI_ACTIVITY_WORKING)lv_obj_clear_flag(bars[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(bars[i],LV_OBJ_FLAG_HIDDEN);}
   if(kind==SI_ACTIVITY_SUCCESS)lv_obj_clear_flag(mark,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(mark,LV_OBJ_FLAG_HIDDEN);
 }
 smart_island_activity_sync();return visible;
}
