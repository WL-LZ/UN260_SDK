#include "page_training.h"
#include "lv_page_manager.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_components/lv_settings.h"
#include "un260/font/manrope_fonts.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
#define TRAINING_INK 0x20313B
#define TRAINING_MUTED 0x627583
#define TRAINING_BLUE 0x1462CC
#define TRAINING_PANEL 0xF2F5F8
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_37);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_17);
#include "un260/lv_components/lv_damped_button.h"
#include <stdio.h>
#include <string.h>

/* Illustrations own no machine state, storage or command interfaces. */
typedef struct { const char *title, *body; } lesson_t;
static lv_obj_t *training_box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{lv_obj_t *o=lv_settings_box(p,x,y,w,h,color);lv_obj_set_style_radius(o,radius,0);return o;}
static lv_obj_t *training_label(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *font,uint32_t color)
{lv_obj_t *o=lv_settings_label(p,s,x,y,ui_message_font(font),color);lv_obj_set_width(o,w);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);return o;}
static lv_obj_t *training_button(lv_obj_t *p,int x,int y,int w,int h,const char *s,bool primary,lv_event_cb_t cb,void *data)
{lv_obj_t *o=lv_settings_button(p,x,y,w,h,s,primary,cb,data);return o;}
static const lesson_t lessons[] = {
    {UI_N_("Mixed counting"), UI_N_("Different denominations are counted together. Check the denomination totals after counting.")},
    {UI_N_("Single denomination"), UI_N_("The first note sets the denomination. Other denominations go to the reject pocket.")},
    {UI_N_("ADD counting"), UI_N_("ADD combines consecutive counts. Recounting the same notes can count them twice.")},
    {UI_N_("Batch counting"), UI_N_("Stop at the preset quantity. Remove the completed stack before continuing.")},
    {UI_N_("Review rejects"), UI_N_("A rejected note is not always counterfeit. Read the reason and check the note before retrying.")},
    {UI_N_("Compare counts"), UI_N_("Compare the same bundle again. Review differences before choosing a result; verification is not a new receipt.")},
};
#define LESSON_COUNT (sizeof(lessons)/sizeof(lessons[0]))
static struct {
    lv_obj_t *root,*stage,*title,*body,*step,*note,*note_text,*values[3],*captions[3];
    lv_obj_t *previous,*next,*pause,*progress,*dots[LESSON_COUNT];
    lv_timer_t *timer;
    unsigned lesson,phase;
    uint32_t elapsed,last_tick;
    bool playing;
} training;
static unsigned saved_lesson;
static lv_obj_t *training_caption(lv_obj_t *p,int x,int y,int w,const char *s)
{
    return training_label(p,x,y,w,s,&lv_font_instrument_sans_medium_17,TRAINING_MUTED);
}
static void training_phase_render(void)
{
    unsigned p=training.phase;
    const char *names[3]={UI_N_("Accepted"),UI_N_("Reject"),UI_N_("Total")};
    char values[3][32]={{0}};
    unsigned accepted=0,rejected=0,total=0;
    const char *note="100";
    if(training.lesson==0){static const unsigned totals[]={0,20,70,170};static const char *notes[]={"20","50","100","100"};accepted=p;total=totals[p];note=notes[p];}
    else if(training.lesson==1){accepted=p==0?0:p==1?1:2;rejected=p==3;total=accepted*100;note=p==2?"50":"100";}
    else if(training.lesson==2){accepted=2+p;total=accepted*100;note="100";}
    else if(training.lesson==3){accepted=p;total=accepted*100;names[1]=UI_N_("Batch");rejected=3;}
    else if(training.lesson==4){accepted=p>1?2:p;rejected=p==3;total=accepted*100;note=p==2?"?":"100";}
    else {names[0]=UI_N_("Original");names[1]=UI_N_("Current");names[2]=UI_N_("Difference");accepted=170;rejected=p==0?0:p==1?20:120;total=170-rejected;note=p==0?"20":"100";}
    snprintf(values[0],sizeof(values[0]),"%u",accepted);
    snprintf(values[1],sizeof(values[1]),"%u",rejected);
    snprintf(values[2],sizeof(values[2]),training.lesson==5?"-%u":"%u",total);
    for(unsigned i=0;i<3;i++){
        lv_label_set_text(training.captions[i],ui_tr(names[i]));
        lv_label_set_text(training.values[i],values[i]);
    }
    lv_label_set_text(training.note_text,note);lv_obj_center(training.note_text);
    char step[32];snprintf(step,sizeof(step),"%u / %u",training.lesson+1,(unsigned)LESSON_COUNT);lv_label_set_text(training.step,step);
}
static void training_animate(void)
{
    /* Each page plays once and holds the result. Only the note moves per training_tick. */
    unsigned phase=training.elapsed/1600U;if(phase>3)phase=3;
    if(phase!=training.phase){training.phase=phase;training_phase_render();}
    unsigned travel=training.elapsed%1600U;
    int x=80+(int)(travel*450U/1600U),y=93;
    bool reject=(training.lesson==1||training.lesson==4)&&phase==2;
    if(reject&&travel>800)y+=(int)((travel-800)*60/800);
    lv_obj_set_pos(training.note,x,y);
    if(phase==3||(training.lesson==5&&phase>=2))lv_obj_add_flag(training.note,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(training.note,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(training.progress,LV_MAX(1,(int)(training.elapsed*738U/4800U)));
    if(training.elapsed>=4800){training.playing=false;if(training.timer)lv_timer_pause(training.timer);lv_damped_button_set_text(training.pause,ui_tr("Replay"));}
}
static void training_tick(lv_timer_t *timer)
{
    (void)timer;
    if(!training.root||!lv_obj_is_visible(training.root)||!training.playing)return;
    uint32_t now=lv_tick_get(),delta=now-training.last_tick;training.last_tick=now;
    training.elapsed=LV_MIN(4800U,training.elapsed+LV_MIN(delta,100U));training_animate();
}
static void training_select_lesson(unsigned lesson)
{
    if(lesson>=LESSON_COUNT)return;
    training.lesson=saved_lesson=lesson;training.elapsed=0;training.phase=~0U;
    training.last_tick=lv_tick_get();training.playing=training.timer!=NULL;
    lv_label_set_text(training.title,ui_tr(lessons[lesson].title));
    lv_label_set_text(training.body,ui_tr(lessons[lesson].body));
    lv_obj_scroll_to_y(lv_obj_get_parent(training.body),0,LV_ANIM_OFF);
    for(unsigned i=0;i<LESSON_COUNT;i++)lv_obj_set_style_bg_color(training.dots[i],lv_color_hex(i==lesson?TRAINING_BLUE:0xD7E0E8),0);
    lv_damped_button_set_enabled(training.previous,lesson>0);
    lv_damped_button_set_enabled(training.next,lesson+1<LESSON_COUNT);
    lv_damped_button_set_enabled(training.pause,training.timer!=NULL);
    lv_damped_button_set_text(training.pause,ui_tr("Pause"));
    if(training.timer)lv_timer_resume(training.timer);
    else training.elapsed=4800; /* Readable final illustration without animation. */
    training_animate();
}
static void training_action(lv_event_t *e)
{
    unsigned id=(uintptr_t)lv_event_get_user_data(e);
    if(id==0){ui_manager_pop_page();return;}
    if(id==1&&training.lesson)training_select_lesson(training.lesson-1);
    if(id==2&&training.lesson+1<LESSON_COUNT)training_select_lesson(training.lesson+1);
    if(id==3&&training.timer){
        if(training.elapsed>=4800){training_select_lesson(training.lesson);return;}
        training.playing=!training.playing;training.last_tick=lv_tick_get();
        if(training.playing)lv_timer_resume(training.timer);else lv_timer_pause(training.timer);
        lv_damped_button_set_text(training.pause,ui_tr(training.playing?"Pause":"Continue"));
    }
}
static void training_deleted(lv_event_t *e)
{
    (void)e;if(training.timer)lv_timer_del(training.timer);
    saved_lesson=training.lesson;memset(&training,0,sizeof(training));
}
void ui_training_create(lv_obj_t *parent)
{
    if(training.root)return;
    lv_settings_header_t header={.title=ui_tr("Getting started"),.subtitle=ui_tr("Demonstration only. Do not feed notes."),.back=training_action};
    lv_settings_frame_t frame=lv_settings_frame_create(parent?parent:lv_scr_act(),&header);
    training.root=frame.root;lv_obj_add_event_cb(training.root,training_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_style_bg_opa(frame.body,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(frame.body,0,0);
    training.stage=training_box(frame.body,0,0,786,300,TRAINING_PANEL,18);
    training_caption(training.stage,24,15,730,ui_tr("Demo"));
    /* Feed tray, transport and two pockets: native shapes, no bitmap decoding. */
    training_box(training.stage,44,86,136,76,0xDCE7F0,12);
    training_box(training.stage,230,70,236,116,0xD5E0E9,20);
    training_box(training.stage,252,84,192,13,0x8DA2B3,6);
    training_box(training.stage,528,72,184,69,0xDFEEE8,12);
    training_box(training.stage,528,155,184,38,0xF2E4D8,10);
    training_caption(training.stage,44,60,170,ui_tr("Input"));
    training_caption(training.stage,528,46,234,ui_tr("Accepted"));
    training.note=training_box(training.stage,80,93,91,48,0xFFFFFF,7);
    training.note_text=training_label(training.note,0,0,LV_SIZE_CONTENT,"100",&lv_font_instrument_sans_semibold_24,TRAINING_BLUE);lv_obj_center(training.note_text);
    for(unsigned i=0;i<3;i++){
        training.captions[i]=training_caption(training.stage,26+i*251,211,231,"");
        training.values[i]=training_label(training.stage,26+i*251,235,231,"",&lv_font_instrument_sans_semibold_37,TRAINING_INK);
    }
    training_box(training.stage,24,289,738,3,0xD7E0E8,1);
    training.progress=training_box(training.stage,24,289,1,3,TRAINING_BLUE,1);
    training.title=training_label(frame.body,810,12,398,"",&lv_font_instrument_sans_semibold_24,TRAINING_INK);
    lv_obj_t *reading=training_box(frame.body,810,62,398,139,0xFFFFFF,0);
    lv_obj_add_flag(reading,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(reading,LV_DIR_VER);
    training.body=training_label(reading,0,0,376,"",&lv_font_instrument_sans_medium_17,TRAINING_MUTED);
    lv_label_set_long_mode(training.body,LV_LABEL_LONG_WRAP);
    training.step=training_caption(frame.body,810,211,80,"");
    for(unsigned i=0;i<LESSON_COUNT;i++)training.dots[i]=training_box(frame.body,901+i*20,219,9,9,0xD7E0E8,5);
    training.previous=training_button(frame.body,810,252,120,44,ui_tr("Previous"),false,training_action,(void*)1);
    training.pause=training_button(frame.body,942,252,126,44,ui_tr("Pause"),false,training_action,(void*)3);
    training.next=training_button(frame.body,1080,252,128,44,ui_tr("Next"),true,training_action,(void*)2);
    training.timer=lv_timer_create(training_tick,40,NULL);training_select_lesson(saved_lesson);
}
void ui_training_suspend(void)
{
    if(!training.root)return;
    if(training.timer)lv_timer_pause(training.timer);
    lv_obj_add_flag(training.root,LV_OBJ_FLAG_HIDDEN);
}
bool ui_training_resume(void)
{
    if(!training.root)return false;
    lv_obj_clear_flag(training.root,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(training.root);
    training.last_tick=lv_tick_get();if(training.playing&&training.timer)lv_timer_resume(training.timer);return true;
}
void ui_training_destroy(void){if(training.root)lv_obj_del(training.root);}
