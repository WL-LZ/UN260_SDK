#include "panel_entry.h"
#include "un260/lv_components/ui_panel.h"
#include "un260/lv_system/ui_i18n.h"
#include "lv_port_indev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ENTRY_DIGIT = 100, ENTRY_CANCEL = 120, ENTRY_DELETE, ENTRY_CLEAR, ENTRY_APPLY };
typedef struct {lv_point_t point;bool armed;} outside_contact_t;
typedef struct {
    ui_panel_t panel;
    panel_entry_config_t config;
    lv_obj_t *value, *error, *pin[4], *dots[4];
    char title[128], prompt[192], field[128], unit[32], range[160], draft[16];
    uint32_t generation, revision;
    outside_contact_t outside;
} entry_context_t;
static entry_context_t entry;
typedef struct {
    lv_obj_t *shade, *error;
    panel_choice_confirm_cb_t confirm;
    panel_entry_cancel_cb_t cancel;
    void *user;
    uint32_t generation;
    outside_contact_t outside;
} choice_context_t;
static choice_context_t choice;
static void entry_keys(uint16_t trigger);
static bool outside_released(lv_event_t *event,lv_obj_t *target,outside_contact_t *contact)
{
    if(lv_event_get_target(event)!=target)return false;
    lv_event_code_t code=lv_event_get_code(event);lv_indev_t *indev=lv_indev_get_act();
    lv_point_t point={0};if(indev)lv_indev_get_point(indev,&point);
    if(code==LV_EVENT_PRESSED){contact->point=point;contact->armed=lv_port_indev_touch_count()<=1;return false;}
    int dx=point.x-contact->point.x,dy=point.y-contact->point.y;
    if(code==LV_EVENT_PRESS_LOST || code==LV_EVENT_LONG_PRESSED || dx*dx+dy*dy>100 || lv_port_indev_touch_count()>1)
        contact->armed=false;
    if(code!=LV_EVENT_RELEASED)return false;
    bool accepted=contact->armed && (!indev || (!lv_indev_get_scroll_obj(indev) && lv_indev_get_gesture_dir(indev)==LV_DIR_NONE));
    contact->armed=false;return accepted;
}
static void outside_events(lv_obj_t *object,lv_event_cb_t callback)
{
    const lv_event_code_t events[]={LV_EVENT_PRESSED,LV_EVENT_PRESSING,LV_EVENT_PRESS_LOST,LV_EVENT_LONG_PRESSED,LV_EVENT_RELEASED};
    for(unsigned i=0;i<sizeof(events)/sizeof(events[0]);i++)lv_obj_add_event_cb(object,callback,events[i],NULL);
}

static void copy_text(char *out, size_t size, const char *text)
{
    if(!size)return;
    size_t length=text?strlen(text):0;if(length>=size)length=size-1;
    if(length)memcpy(out,text,length);
    out[length]='\0';
}
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object); lv_obj_set_pos(object,x,y); lv_obj_set_size(object,w,h);
    lv_obj_set_style_bg_color(object,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(object,LV_OPA_COVER,0);
    lv_obj_set_style_radius(object,radius,0);
    lv_obj_clear_flag(object,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return object;
}
static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int w,
                       unsigned size, uint32_t color, bool centered)
{
    lv_obj_t *object=lv_label_create(parent); lv_obj_remove_style_all(object);
    lv_obj_set_style_text_font(object,ui_panel_font(size,false),0);
    lv_obj_set_style_text_color(object,lv_color_hex(color),0);
    lv_obj_set_style_text_align(object,centered?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
    lv_label_set_long_mode(object,LV_LABEL_LONG_CLIP);
    lv_label_set_text(object,text?text:""); lv_obj_set_pos(object,x,y); lv_obj_set_width(object,w);
    return object;
}
static lv_obj_t *button(lv_obj_t *parent,const char *text,int x,int y,int w,int h,
                        lv_event_cb_t cb,void *user)
{
    lv_obj_t *object=box(parent,x,y,w,h,0xEEF4FA,7);
    lv_obj_add_flag(object,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_color(object,lv_color_hex(0xD4E2EF),0);
    lv_obj_set_style_border_width(object,1,0);
    lv_obj_t *caption=label(object,text,0,0,w-10,13,0x527799,true);
    lv_obj_center(caption); lv_obj_add_event_cb(object,cb,LV_EVENT_CLICKED,user);
    return object;
}

bool panel_entry_is_open(void) { return entry.panel.root && lv_obj_is_valid(entry.panel.root); }
bool panel_entry_is_open_for(const void *user) { return panel_entry_is_open() && entry.config.user==user; }
void panel_entry_close(void)
{
    entry.generation++;
    ui_panel_destroy(&entry.panel);
    memset(entry.draft,0,sizeof(entry.draft));
    memset(&entry.config,0,sizeof(entry.config));
    entry.value=entry.error=NULL;
    memset(entry.pin,0,sizeof(entry.pin)); memset(entry.dots,0,sizeof(entry.dots));
}
void panel_entry_close_for(const void *user) { if(panel_entry_is_open_for(user)) panel_entry_close(); }
void panel_entry_destroy(void) { panel_entry_close(); }
void panel_entry_set_error(const char *message)
{
    if(panel_entry_is_open() && entry.error) lv_label_set_text(entry.error,message?message:"");
}
static void cancel_entry(void)
{
    panel_entry_cancel_cb_t callback=entry.config.cancel; void *user=entry.config.user;
    panel_entry_close(); if(callback) callback(user);
}
static void entry_motion(void)
{
    ui_panel_motion_t motion={.scene=entry.config.kind==PANEL_ENTRY_PIN?UI_PANEL_SCENE_LOCK:UI_PANEL_SCENE_STACK,
        .title=entry.config.kind==PANEL_ENTRY_PIN?ui_tr("Verify PIN"):entry.field,
        .detail=entry.config.kind==PANEL_ENTRY_PIN?ui_tr("Auto verify"):ui_tr("Enter a value"),
        .pin_digits=(uint8_t)strlen(entry.draft),.revision=entry.revision};
    if(entry.config.kind!=PANEL_ENTRY_PIN && entry.config.max)
        motion.value=(uint16_t)((strtoul(entry.draft,NULL,10)*1000U)/entry.config.max);
    if(motion.value>1000)motion.value=1000;
    ui_panel_set_motion(&entry.panel,&motion);
}
static void paint_value(void)
{
    if(!panel_entry_is_open())return;
    size_t length=strlen(entry.draft);
    if(entry.config.kind==PANEL_ENTRY_PIN){
        for(unsigned i=0;i<4;i++){
            lv_obj_set_style_border_color(entry.pin[i],lv_color_hex(i==length?0x6196C9:0xD4E0EB),0);
            lv_obj_set_style_bg_color(entry.pin[i],lv_color_hex(i==length?0xFFFFFF:0xF5F8FB),0);
            if(i<length)lv_obj_clear_flag(entry.dots[i],LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(entry.dots[i],LV_OBJ_FLAG_HIDDEN);
        }
    }else if(entry.value){
        lv_label_set_text(entry.value,length?entry.draft:"0");
        lv_obj_set_style_text_color(entry.value,lv_color_hex(length?0x245E98:0xC8D5E2),0);
    }
    entry_motion();
}
static void confirm_entry(bool off)
{
    if(!panel_entry_is_open())return;
    char submitted[16]; copy_text(submitted,sizeof(submitted),entry.draft);
    if(off)snprintf(submitted,sizeof(submitted),"%lu",(unsigned long)entry.config.off_value);
    if(entry.config.kind==PANEL_ENTRY_NUMBER && !off){
        unsigned long value=strtoul(submitted,NULL,10);
        if(!submitted[0] || value<entry.config.min || value>entry.config.max){
            panel_entry_set_error(ui_tr("Check the value")); return;
        }
    }
    uint32_t generation=entry.generation;
    panel_entry_confirm_cb_t callback=entry.config.confirm; void *user=entry.config.user;
    const bool pin=entry.config.kind==PANEL_ENTRY_PIN;
    bool accepted=callback && callback(submitted,user);
    memset(submitted,0,sizeof(submitted));
    if(generation!=entry.generation || !panel_entry_is_open())return;
    if(accepted){ panel_entry_close(); return; }
    if(pin){ memset(entry.draft,0,sizeof(entry.draft)); paint_value(); }
}
static void entry_key(void *user,uint16_t action,ui_panel_event_t event)
{
    (void)user; if(event!=UI_PANEL_EVENT_CLICK || !panel_entry_is_open())return;
    if(action==ENTRY_CANCEL){cancel_entry();return;}
    entry.revision++;
    panel_entry_set_error(""); size_t length=strlen(entry.draft);
    if(action>=ENTRY_DIGIT && action<ENTRY_DIGIT+10){
        if(length>=entry.config.max_digits)return;
        entry.draft[length]=(char)('0'+action-ENTRY_DIGIT); entry.draft[length+1]='\0';
    }else if(action==ENTRY_DELETE){if(length)entry.draft[length-1]='\0';}
    else if(action==ENTRY_CLEAR)memset(entry.draft,0,sizeof(entry.draft));
    else if(action==ENTRY_APPLY){confirm_entry(false);return;}
    paint_value();
    entry_keys(action);
    if(entry.config.kind==PANEL_ENTRY_PIN && strlen(entry.draft)==4)confirm_entry(false);
}
static void off_clicked(lv_event_t *event){(void)event;confirm_entry(true);}
static void outside_entry(lv_event_t *event)
{
    if(outside_released(event,ui_panel_content(&entry.panel),&entry.outside))cancel_entry();
}
static void entry_deleted(lv_event_t *event)
{
    if(lv_event_get_code(event)!=LV_EVENT_DELETE)return;
    entry.generation++; memset(entry.draft,0,sizeof(entry.draft));
    memset(&entry.config,0,sizeof(entry.config));
    entry.value=entry.error=NULL;
}
bool panel_entry_open(lv_obj_t *parent,const panel_entry_config_t *config)
{
    if(!parent || !config || !config->confirm)return false;
    if(panel_entry_is_open() && entry.config.user!=config->user)return false;
    panel_entry_close(); entry.config=*config;
    entry.config.max_digits=config->kind==PANEL_ENTRY_PIN?4:config->max_digits;
    if(!entry.config.max_digits || entry.config.max_digits>10)entry.config.max_digits=10;
    copy_text(entry.title,sizeof(entry.title),config->title);
    copy_text(entry.prompt,sizeof(entry.prompt),config->prompt);
    copy_text(entry.field,sizeof(entry.field),config->field_label);
    copy_text(entry.unit,sizeof(entry.unit),config->unit);
    copy_text(entry.range,sizeof(entry.range),config->range);
    if(config->kind==PANEL_ENTRY_NUMBER && config->initial){
        size_t length=strlen(config->initial);
        if(length<=entry.config.max_digits && strspn(config->initial,"0123456789")==length)
            copy_text(entry.draft,sizeof(entry.draft),config->initial);
    }
    if(!ui_panel_create(&entry.panel,parent,entry_key,NULL)){panel_entry_close();return false;}
    lv_obj_add_event_cb(entry.panel.root,entry_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *content=ui_panel_content(&entry.panel);
    lv_obj_add_flag(content,LV_OBJ_FLAG_CLICKABLE);
    entry.outside.armed=false;outside_events(content,outside_entry);
    label(content,entry.title,28,16,736,20,0x243447,false);
    box(content,28,56,736,1,0xDCE4EC,0);
    box(content,28,362,736,1,0xDCE4EC,0);
    label(content,ui_tr("Cancel discards changes"),28,374,330,12,0x6A7D8F,false);
    lv_obj_t *foot=label(content,ui_tr("Use the side keypads"),410,374,354,12,0x6A7D8F,false);
    lv_obj_set_style_text_align(foot,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_t *card=box(content,28,72,736,277,0xFFFFFF,14);
    lv_obj_add_flag(card,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0xE0E7EF),0);
    label(card,entry.field,18,38,700,21,0x304E68,true);
    label(card,entry.prompt,18,73,700,12,0x6A7D8F,true);
    if(config->kind==PANEL_ENTRY_PIN){
        for(unsigned i=0;i<4;i++){
            entry.pin[i]=box(card,232+(int)i*72,125,57,62,0xF5F8FB,10);
            lv_obj_set_style_border_width(entry.pin[i],1,0);
            entry.dots[i]=box(entry.pin[i],23,26,10,10,0x345B7E,5);
        }
    }else{
        entry.value=label(card,"0",233,107,270,66,0xC8D5E2,true);
        box(card,233,197,270,1,0xB1C9DF,0);
        label(card,entry.unit,515,159,100,13,0x6A7D8F,false);
        lv_obj_t *range=label(card,entry.range,180,222,376,11,0x6A7D8F,true);
        if(config->allow_off){lv_obj_set_width(range,225);lv_obj_set_x(range,200);
            button(card,ui_tr("Batch off"),430,215,100,29,off_clicked,NULL);}
    }
    entry.error=label(card,"",18,246,700,12,0xB55231,true);
    entry_keys(0);
    paint_value(); lv_obj_move_foreground(entry.panel.root); return true;
}
static void entry_keys(uint16_t trigger)
{
    const panel_entry_config_t *config=&entry.config;
    ui_panel_key_t left[7]={0},right[7]={0};
    static const char *const digits[]={"0","1","2","3","4","5","6","7","8","9"};
    for(unsigned i=0;i<6;i++)left[i]=(ui_panel_key_t){.action=ENTRY_DIGIT+i+1,.numeric=true,.digit=i+1,.label=digits[i+1]};
    left[6]=(ui_panel_key_t){.action=ENTRY_CANCEL,.icon="x",.label=ui_tr("Cancel")};
    right[0]=(ui_panel_key_t){.action=ENTRY_DIGIT+7,.numeric=true,.digit=7,.label=digits[7]};
    right[1]=(ui_panel_key_t){.action=ENTRY_DIGIT+8,.numeric=true,.digit=8,.label=digits[8]};
    right[2]=(ui_panel_key_t){.action=ENTRY_DIGIT+9,.numeric=true,.digit=9,.label=digits[9]};
    right[3]=(ui_panel_key_t){.action=ENTRY_DELETE,.icon="delete",.label=ui_tr("Delete")};
    right[4]=(ui_panel_key_t){.action=ENTRY_DIGIT,.numeric=true,.digit=0,.label=digits[0]};
    right[5]=(ui_panel_key_t){.action=ENTRY_CLEAR,.label=ui_tr("Clear")};
    right[6]=(ui_panel_key_t){.action=ENTRY_APPLY,.icon=config->kind==PANEL_ENTRY_PIN?"shield-check":"check",
        .label=ui_tr(config->kind==PANEL_ENTRY_PIN?UI_N_("Auto verify"):UI_N_("Confirm")),.primary=true,.disabled=config->kind==PANEL_ENTRY_PIN};
    ui_panel_set_keys(&entry.panel,UI_PANEL_LAYOUT_ENTRY,left,7,right,7,entry.revision,trigger);
}

bool panel_choice_is_open(void){return choice.shade && lv_obj_is_valid(choice.shade);}
bool panel_choice_is_open_for(const void *user)
{return choice.shade && lv_obj_is_valid(choice.shade) && choice.user==user;}
void panel_choice_close(void)
{
    lv_obj_t *shade=choice.shade;choice.shade=NULL;choice.generation++;
    choice.confirm=NULL;choice.cancel=NULL;choice.user=NULL;choice.error=NULL;
    if(shade && lv_obj_is_valid(shade))lv_obj_del(shade);
}
void panel_choice_close_for(const void *user){if(panel_choice_is_open_for(user))panel_choice_close();}
void panel_choice_set_error(const char *message)
{if(choice.shade && choice.error)lv_label_set_text(choice.error,message?message:"");}
static void choice_cancel(lv_event_t *event)
{
    if(lv_event_get_user_data(event)==NULL && !outside_released(event,choice.shade,&choice.outside))return;
    panel_entry_cancel_cb_t callback=choice.cancel;void *user=choice.user;
    panel_choice_close();if(callback)callback(user);
}
static void choose(lv_event_t *event)
{
    uint32_t value=(uint32_t)(uintptr_t)lv_event_get_user_data(event);
    uint32_t generation=choice.generation;
    bool accepted=choice.confirm && choice.confirm(value,choice.user);
    if(accepted && generation==choice.generation)panel_choice_close();
}
static void choice_deleted(lv_event_t *event)
{
    if(lv_event_get_target(event)!=choice.shade)return;
    choice.shade=NULL;choice.error=NULL;choice.user=NULL;choice.confirm=NULL;choice.cancel=NULL;choice.generation++;
}
bool panel_choice_open(lv_obj_t *parent,const panel_choice_config_t *config)
{
    if(!parent || !config || !config->confirm || !config->items || !config->count)return false;
    if(choice.shade && choice.user!=config->user)return false;
    panel_choice_close();choice.user=config->user;choice.confirm=config->confirm;choice.cancel=config->cancel;
    choice.shade=box(parent,0,57,792,305,0xE7EDF4,0);
    lv_obj_set_style_bg_opa(choice.shade,227,0);lv_obj_add_flag(choice.shade,LV_OBJ_FLAG_CLICKABLE);
    choice.outside.armed=false;outside_events(choice.shade,choice_cancel);
    lv_obj_add_event_cb(choice.shade,choice_deleted,LV_EVENT_DELETE,NULL);
    size_t rows=(config->count+2)/3;int height=114+(int)(rows>3?3:rows)*55;
    lv_obj_t *card=box(choice.shade,186,(305-height)/2,420,height,0xFFFFFF,13);
    lv_obj_add_flag(card,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_border_width(card,1,0);
    lv_obj_set_style_border_color(card,lv_color_hex(0xD7E2EC),0);
    label(card,config->title,23,20,374,19,0x304E68,false);
    lv_obj_t *grid=box(card,23,59,374,height-113,0xFFFFFF,0);
    lv_obj_add_flag(grid,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(grid,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid,LV_SCROLLBAR_MODE_AUTO);
    for(size_t i=0;i<config->count;i++){
        const panel_choice_item_t *item=&config->items[i];
        lv_obj_t *option=button(grid,item->label,(int)(i%3)*128,(int)(i/3)*55,118,45,choose,(void *)(uintptr_t)item->value);
        lv_obj_set_style_bg_color(option,lv_color_hex(item->selected?0xE3EFFB:0xF2F6FA),0);
        lv_obj_set_style_border_color(option,lv_color_hex(item->selected?0x91B5D8:0xD9E3ED),0);
        if(item->disabled)lv_obj_add_state(option,LV_STATE_DISABLED);
    }
    choice.error=label(card,"",23,height-50,374,11,0xB55231,true);
    lv_obj_t *cancel=button(card,ui_tr("Cancel"),160,height-32,100,25,choice_cancel,(void *)1);
    lv_obj_set_style_bg_opa(cancel,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(cancel,0,0);
    lv_obj_move_foreground(choice.shade);return true;
}
