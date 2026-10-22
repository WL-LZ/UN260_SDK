#include "ui_panel.h"
#include "un260/font/ui_message_font.h"
#include "un260/font/panel/panel_fonts.h"
#include "un260/font/panel/panel_key_fonts.h"
#include "un260/font/ui_font_fallback.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_resources/panel/panel_assets.h"
#include "lv_port_indev.h"
#include <math.h>
#include <string.h>

#define PANEL_FRAME_MS 16

typedef struct panel_state panel_state_t;
typedef struct {
    panel_state_t *owner;
    lv_obj_t *button, *body, *glyph, *hand, *label, *sub, *led;
    ui_panel_key_t spec;
    char icon[40];
    lv_point_t pressed_at;
    bool cancel, long_fired, initialized, visible, pressed, tracking;
    unsigned icon_px;
    uint32_t started, duration;
    int16_t old_angle;
    uint8_t old_led;
    bool animate_glyph, animate_led, animate_hand;
} panel_key_t;

struct panel_state {
    ui_panel_t *public_panel;
    lv_obj_t *left, *right, *motion_box, *title, *detail, *copy;
    panel_key_t keys[2][UI_PANEL_MAX_KEYS];
    ui_panel_action_cb_t callback;
    void *user;
    lv_timer_t *timer;
    ui_panel_layout_t layout;
    uint32_t key_revision;
    bool enabled, visible;
};

static bool active(const panel_state_t *s) { return s->enabled && s->visible; }
static float clampf(float n,float lo,float hi){return n<lo?lo:n>hi?hi:n;}
static float smooth(float x){x=clampf(x,0,1);return 1-powf(1-x,3);}
static uint8_t alpha(float value){return (uint8_t)(255*clampf(value,0,1)+.5f);}
static int16_t angle_norm(int angle){angle%=3600;return angle<0?angle+3600:angle;}
static void show(lv_obj_t *obj,bool yes){if(yes)lv_obj_clear_flag(obj,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);}
static void text_if_changed(lv_obj_t *label,const char *text){if(!text)text="";if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);}
static void plain(lv_obj_t *obj){lv_obj_remove_style_all(obj);lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(obj,0,0);}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color){lv_obj_t *o=lv_obj_create(parent);plain(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);return o;}

const lv_font_t *ui_panel_font(unsigned px,bool semibold){return ui_message_font_sized(panel_base_font(px,semibold),px);}

/* Only side keys use this true-bold subset. Locale bank ownership and central
 * page typography remain unchanged; unlisted data falls through to that bank. */
static const lv_font_t *ui_panel_key_font(unsigned px)
{
    const lv_font_t *base=panel_base_font(px,true);
    const lv_font_t *regular=ui_message_font_sized(base,px);
    const lv_font_t *bold=panel_key_bold_font(ui_lang_get(),px);
    if(!bold)return regular;
    static struct {const lv_font_t *bold,*regular;lv_font_t tail,font;} cache[PANEL_KEY_FONT_COUNT];
    for(unsigned i=0;i<PANEL_KEY_FONT_COUNT;i++){
        if(cache[i].bold&&cache[i].bold!=bold)continue;
        if(!cache[i].bold||cache[i].regular!=regular){
            cache[i].tail=*bold;cache[i].tail.fallback=regular;
            ui_font_compose_fallback(&cache[i].font,base,&cache[i].tail);
            int above=LV_MAX(cache[i].font.line_height-cache[i].font.base_line,regular->line_height-regular->base_line);
            int below=LV_MAX(cache[i].font.base_line,regular->base_line);
            cache[i].font.line_height=above+below;cache[i].font.base_line=below;
            cache[i].bold=bold;cache[i].regular=regular;
        }
        return &cache[i].font;
    }
    return regular;
}


lv_obj_t *ui_panel_icon_create(lv_obj_t *parent,const char *name,unsigned px,lv_color_t color){
    lv_obj_t *image=lv_img_create(parent);plain(image);
    const lv_img_dsc_t *asset=panel_asset_icon(name,px);
    if(asset){lv_img_set_src(image,asset);lv_img_set_zoom(image,(uint16_t)(256*px/asset->header.w));}
    else lv_obj_set_size(image,px,px);
    lv_obj_set_style_img_recolor(image,color,0);lv_obj_set_style_img_recolor_opa(image,LV_OPA_COVER,0);
    lv_obj_clear_flag(image,LV_OBJ_FLAG_CLICKABLE);return image;
}

static void key_frame(panel_key_t *k,uint32_t now){
    if(!k->visible)return;
    float t=k->duration?(float)(now-k->started)/k->duration:1;
    bool anim=t<1&&active(k->owner);float eased=smooth(t);
    int dx=0,dy=0,angle=0;float opacity=1;
    if(anim&&k->animate_glyph){
        float tail=1-eased;
        if(k->spec.numeric){dy=(int)roundf(tail);opacity=1-.32f*tail;}
        else switch(k->spec.motion){
        case UI_PANEL_KEY_TURN:angle=(int)(-70*tail);opacity=1-.17f*tail;
            break;
        case UI_PANEL_KEY_FEED:dy=(int)roundf(-2*tail);
            break;
        case UI_PANEL_KEY_SCAN:dx=(int)roundf(-2*tail);
            break;
        case UI_PANEL_KEY_SWAP:dy=(int)roundf(2*tail);opacity=1-.65f*tail;
            break;
        default:dy=(int)roundf(tail);opacity=1-.3f*tail;
            break;
        }
    }
    lv_obj_t *target=k->spec.numeric?k->label:k->glyph;
    lv_obj_set_style_translate_x(target,dx,0);lv_obj_set_style_translate_y(target,dy,0);
    if(!k->spec.numeric)lv_img_set_angle(k->glyph,angle_norm(angle));
    float press_brightness=k->pressed?.55f:1.f;
    // Text opacity stays opaque; held keys use a darker color without a layer.
    if(k->spec.numeric)lv_obj_set_style_text_opa(target,LV_OPA_COVER,0);
    else lv_obj_set_style_img_opa(target,alpha(opacity*(k->spec.disabled?.34f:1)*press_brightness),0);
    if(!k->spec.numeric){
        lv_obj_set_style_translate_x(k->hand,dx,0);lv_obj_set_style_translate_y(k->hand,dy,0);
        lv_obj_set_style_img_opa(k->hand,alpha(opacity*(k->spec.disabled?.34f:1)*press_brightness),0);
    }
    int a=k->spec.angle_tenths;
    if(anim&&k->animate_hand)a=k->old_angle+(int)((a-k->old_angle)*eased);
    lv_img_set_angle(k->hand,angle_norm(a));
    float led=k->spec.lit?.9f:.16f;
    if(anim&&k->animate_led)led=(k->old_led/255.f)*(1-eased)+led*eased;
    lv_obj_set_style_bg_opa(k->led,alpha(led*(k->spec.disabled?.34f:1)*press_brightness),0);
    if(!anim)k->duration=0;
}

/* Held feedback is immediate and independent of animation preferences.
 * Only primitive styles change; the opaque black key never needs a layer. */
static void key_set_pressed(panel_key_t *k,bool pressed){
    pressed=pressed&&k->visible&&k->owner->visible&&!k->spec.disabled;
    if(k->pressed==pressed)return;
    k->pressed=pressed;
    lv_color_t text=lv_color_hex(pressed?0x8c8c8c:0xffffff);
    lv_obj_set_style_border_color(k->button,text,0);
    lv_obj_set_style_text_color(k->label,text,0);lv_obj_set_style_text_color(k->sub,text,0);
    key_frame(k,lv_tick_get());
}
static void key_cancel_press(panel_key_t *k){
    if(k->tracking)k->cancel=true;
    k->tracking=false;key_set_pressed(k,false);
}
static void cancel_presses(panel_state_t *s){
    for(unsigned w=0;w<2;w++)for(unsigned n=0;n<UI_PANEL_MAX_KEYS;n++)key_cancel_press(&s->keys[w][n]);
}

static bool running_animations(panel_state_t *s){
    if(!active(s))return false;
    for(unsigned w=0;w<2;w++)for(unsigned n=0;n<UI_PANEL_MAX_KEYS;n++)if(s->keys[w][n].duration)return true;
    return false;
}
static void timer_tick(lv_timer_t *timer){
    panel_state_t *s=timer->user_data;uint32_t now=lv_tick_get();
    for(unsigned w=0;w<2;w++)for(unsigned n=0;n<UI_PANEL_MAX_KEYS;n++)if(s->keys[w][n].duration)key_frame(&s->keys[w][n],now);
    if(!running_animations(s))lv_timer_pause(timer);
}
static void sync_timer(panel_state_t *s){if(running_animations(s))lv_timer_resume(s->timer);else lv_timer_pause(s->timer);}
static void settle(panel_state_t *s){
    for(unsigned w=0;w<2;w++)for(unsigned n=0;n<UI_PANEL_MAX_KEYS;n++){s->keys[w][n].duration=0;key_frame(&s->keys[w][n],lv_tick_get());}
    sync_timer(s);
}

static void key_event(lv_event_t *event){
    panel_key_t *k=lv_event_get_user_data(event);lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_DELETE)return;
    if(k->spec.disabled||!k->visible||!k->owner->visible){key_cancel_press(k);return;}
    lv_indev_t *indev=lv_indev_get_act();lv_point_t point={0,0};if(indev)lv_indev_get_point(indev,&point);
    uint8_t contacts=lv_port_indev_touch_count();lv_area_t bounds;lv_obj_get_coords(k->button,&bounds);
    bool inside=!indev||(point.x>=bounds.x1&&point.x<=bounds.x2&&point.y>=bounds.y1&&point.y<=bounds.y2);
    if(code==LV_EVENT_PRESSED){
        k->pressed_at=point;k->cancel=contacts>1||!inside;k->long_fired=false;
        k->tracking=contacts==1&&!k->cancel;
    }
    if(code==LV_EVENT_PRESSING||code==LV_EVENT_RELEASED||code==LV_EVENT_LONG_PRESSED){
        int dx=point.x-k->pressed_at.x,dy=point.y-k->pressed_at.y;
        if(dx*dx+dy*dy>100||contacts>1||!inside||
           ((code==LV_EVENT_PRESSING||code==LV_EVENT_LONG_PRESSED)&&contacts==0))k->cancel=true;
        if(k->cancel)k->tracking=false;
    }
    if(code==LV_EVENT_PRESS_LOST||code==LV_EVENT_CANCEL||code==LV_EVENT_SCROLL_BEGIN){k->cancel=true;k->tracking=false;}
    if(code==LV_EVENT_RELEASED||code==LV_EVENT_CLICKED)k->tracking=false;
    key_set_pressed(k,k->tracking&&!k->cancel);
    ui_panel_event_t action_event;
    if(code==LV_EVENT_LONG_PRESSED&&k->spec.long_press&&!k->long_fired&&!k->cancel){k->long_fired=true;action_event=UI_PANEL_EVENT_LONG_PRESS;}
    else if(code==LV_EVENT_CLICKED&&!k->cancel&&!k->long_fired)action_event=UI_PANEL_EVENT_CLICK;
    else return;
    // Global touch_sound owns the one effective-touch response; no local sound.
    ui_panel_action_cb_t callback=k->owner->callback;void *user=k->owner->user;uint16_t action=k->spec.action;
    if(callback&&action)callback(user,action,action_event);
}

static void root_deleted(lv_event_t *event){
    panel_state_t *s=lv_event_get_user_data(event);
    if(lv_event_get_code(event)!=LV_EVENT_DELETE)return;
    if(s->timer)lv_timer_del(s->timer);
    for(unsigned w=0;w<2;w++)for(unsigned n=0;n<UI_PANEL_MAX_KEYS;n++)if(s->keys[w][n].button)while(lv_obj_remove_event_cb(s->keys[w][n].button,key_event)){}
    s->public_panel->root=NULL;s->public_panel->content=NULL;s->public_panel->private_state=NULL;
    lv_mem_free(s);
}

static void make_key(panel_state_t *s,panel_key_t *k,lv_obj_t *wing){
    k->owner=s;k->button=lv_btn_create(wing);plain(k->button);
    lv_obj_set_style_bg_color(k->button,lv_color_hex(0),LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(k->button,LV_OPA_COVER,LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(k->button,lv_color_hex(0xffffff),LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(k->button,1,LV_PART_MAIN|LV_STATE_DEFAULT);lv_obj_set_style_radius(k->button,14,0);
    lv_obj_set_style_shadow_width(k->button,0,0);lv_obj_set_style_pad_all(k->button,8,0);
    const lv_event_code_t events[]={LV_EVENT_PRESSED,LV_EVENT_PRESSING,LV_EVENT_RELEASED,LV_EVENT_PRESS_LOST,LV_EVENT_LONG_PRESSED,LV_EVENT_CLICKED,LV_EVENT_CANCEL,LV_EVENT_SCROLL_BEGIN};
    for(unsigned i=0;i<sizeof(events)/sizeof(events[0]);i++)lv_obj_add_event_cb(k->button,key_event,events[i],k);
    k->body=lv_obj_create(k->button);plain(k->body);lv_obj_set_size(k->body,LV_SIZE_CONTENT,LV_SIZE_CONTENT);
    lv_obj_clear_flag(k->body,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_flex_flow(k->body,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(k->body,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    k->glyph=ui_panel_icon_create(k->body,"plus",27,lv_color_hex(0xc5c5c5));
    k->label=lv_label_create(k->body);plain(k->label);lv_obj_set_style_text_color(k->label,lv_color_hex(0xffffff),0);
    k->sub=lv_label_create(k->body);plain(k->sub);lv_obj_set_style_text_color(k->sub,lv_color_hex(0xffffff),0);
    k->hand=ui_panel_icon_create(k->button,"gauge-hand",27,lv_color_hex(0xc5c5c5));
    k->led=box(k->button,0,0,4,4,0xe5e5e5);lv_obj_clear_flag(k->led,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_radius(k->led,LV_RADIUS_CIRCLE,0);
    show(k->button,false);show(k->hand,false);
}

bool ui_panel_create(ui_panel_t *panel,lv_obj_t *parent,ui_panel_action_cb_t callback,void *user){
    if(!panel||!parent||panel->root||panel->private_state)return false;
    panel_state_t *s=lv_mem_alloc(sizeof(*s));if(!s)return false;memset(s,0,sizeof(*s));
    s->public_panel=panel;s->callback=callback;s->user=user;s->enabled=s->visible=true;panel->private_state=s;
    panel->root=box(parent,0,0,1280,400,0);lv_obj_add_event_cb(panel->root,root_deleted,LV_EVENT_DELETE,s);
    s->left=box(panel->root,0,0,244,400,0);s->right=box(panel->root,1036,0,244,400,0);
    panel->content=box(panel->root,244,0,792,400,0xf3f5f8);
    box(s->left,243,0,1,400,0x242424);box(s->right,0,0,1,400,0x242424);
    for(unsigned n=0;n<UI_PANEL_MAX_KEYS;n++){make_key(s,&s->keys[0][n],s->left);make_key(s,&s->keys[1][n],s->right);}
    s->motion_box=lv_obj_create(s->right);plain(s->motion_box);lv_obj_set_pos(s->motion_box,20,20);lv_obj_set_size(s->motion_box,203,72);
    s->copy=lv_obj_create(s->motion_box);plain(s->copy);lv_obj_set_size(s->copy,203,LV_SIZE_CONTENT);lv_obj_set_pos(s->copy,0,0);lv_obj_set_flex_flow(s->copy,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(s->copy,3,0);
    s->title=lv_label_create(s->copy);plain(s->title);lv_obj_set_width(s->title,203);lv_label_set_long_mode(s->title,LV_LABEL_LONG_DOT);lv_obj_set_style_text_font(s->title,ui_panel_font(14,false),0);lv_obj_set_style_text_color(s->title,lv_color_hex(0xe3e5e6),0);
    s->detail=lv_label_create(s->copy);plain(s->detail);lv_obj_set_width(s->detail,203);lv_obj_set_style_max_height(s->detail,34,0);lv_obj_set_style_text_font(s->detail,ui_panel_font(12,false),0);lv_obj_set_style_text_color(s->detail,lv_color_hex(0x929ba1),0);lv_obj_set_style_text_line_space(s->detail,3,0);
    s->timer=lv_timer_create(timer_tick,PANEL_FRAME_MS,s);lv_timer_pause(s->timer);return true;
}

lv_obj_t *ui_panel_content(const ui_panel_t *panel){return panel?panel->content:NULL;}

static void update_key(panel_state_t *s,panel_key_t *k,const ui_panel_key_t *spec,unsigned wing,unsigned index,size_t count,uint32_t revision,uint16_t trigger){
    if(!spec){key_cancel_press(k);show(k->button,false);k->visible=false;k->duration=0;return;}
    const char *icon=spec->icon?spec->icon:"";
    bool icon_changed=strcmp(k->icon,icon)!=0;
    bool semantic_changed=k->spec.value!=spec->value||icon_changed||k->spec.action!=spec->action;
    bool lamp_changed=k->spec.lit!=spec->lit;
    bool hand_changed=k->spec.angle_tenths!=spec->angle_tenths;
    bool revised=s->key_revision!=revision;
    bool touched=trigger&&spec->action==trigger&&revised;
    bool entry=s->layout==UI_PANEL_LAYOUT_ENTRY;
    bool wide=index+1==count&&(wing||entry);
    bool animate=k->initialized&&active(s)&&!spec->disabled;
    int16_t previous_angle=k->spec.angle_tenths;
    uint8_t previous_led=alpha(k->spec.lit?.9f:.16f);
    bool function_changed=icon_changed||k->spec.action!=spec->action;
    if(function_changed||spec->disabled||k->spec.numeric!=spec->numeric||k->spec.digit!=spec->digit)key_cancel_press(k);
    k->spec=*spec;k->spec.icon=NULL;k->spec.label=NULL;k->spec.sub=NULL;
    strncpy(k->icon,icon,sizeof(k->icon)-1);k->icon[sizeof(k->icon)-1]=0;
    k->visible=true;show(k->button,true);
    int width=wide?203:96,height=wing?(entry?60:84):(entry?81:112);
    int x=20+(index%2)*108,y=(wing?104:20)+(index/2)*(height+12);if(wide)x=20;
    lv_obj_set_pos(k->button,x,y);lv_obj_set_size(k->button,width,height);
    lv_obj_set_style_pad_all(k->button,wing&&entry?6:8,0);
    unsigned px=wing?(entry&&!wide?20:24):27;k->icon_px=px;
    unsigned font=spec->numeric?35:wide?(wing&&entry?17:18):wing?(entry?15:17):18;
    lv_obj_set_style_text_font(k->label,ui_panel_key_font(font),0);lv_obj_set_style_text_font(k->sub,ui_panel_key_font(14),0);
    lv_label_set_long_mode(k->label,LV_LABEL_LONG_CLIP);char digit[2]={(char)('0'+spec->digit%10),0};
    text_if_changed(k->label,spec->numeric&&!spec->label?digit:spec->label);text_if_changed(k->sub,spec->sub);
    show(k->glyph,*icon&&!spec->numeric);show(k->label,true);show(k->sub,spec->sub&&*spec->sub&&!spec->numeric);
    lv_obj_set_flex_flow(k->body,wide?LV_FLEX_FLOW_ROW:LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_column(k->body,12,0);lv_obj_set_style_pad_row(k->body,spec->numeric?0:wing?(entry?4:6):7,0);
    bool gauge=!strcmp(icon,"gauge");
    const char *icon_name=gauge?"gauge-shell":!strcmp(icon,"volume-2")&&!spec->lit?"volume-2-muted":!strcmp(icon,"scan-line")&&!spec->lit?"scan-line-off":icon;
    const lv_img_dsc_t *asset=panel_asset_icon(icon_name,px);
    if(asset)lv_img_set_src(k->glyph,asset);
    lv_img_set_pivot(k->glyph,px/2,px/2);
    lv_color_t color=lv_color_hex(spec->lit?0xeeeeee:spec->primary||spec->danger?0xdddddd:0xc5c5c5);
    lv_obj_set_style_img_recolor(k->glyph,color,0);lv_obj_set_style_img_recolor(k->hand,color,0);
    show(k->hand,gauge&&!spec->numeric);
    if(gauge){lv_img_set_src(k->hand,panel_asset_icon("gauge-hand",px));lv_img_set_pivot(k->hand,px/2,14*px/24);}
    if(spec->disabled)lv_obj_add_state(k->button,LV_STATE_DISABLED);else lv_obj_clear_state(k->button,LV_STATE_DISABLED);
    lv_obj_update_layout(k->body);lv_obj_center(k->body);
    lv_obj_update_layout(k->button);
    if(gauge){lv_area_t ga,ba;lv_obj_get_coords(k->glyph,&ga);lv_obj_get_content_coords(k->button,&ba);lv_obj_set_pos(k->hand,ga.x1-ba.x1,ga.y1-ba.y1);}
    lv_obj_set_pos(k->led,width-15-(wing&&entry?6:8),10-(wing&&entry?6:8));
    // Keyboard appearance is immediate. Only the pressed digit gets feedback.
    bool keyboard=s->layout==UI_PANEL_LAYOUT_ENTRY;
    bool feedback_glyph=animate&&(keyboard?touched:!hand_changed&&(touched||semantic_changed||lamp_changed));
    bool feedback_led=animate&&(lamp_changed||touched);
    bool feedback_hand=animate&&!keyboard&&hand_changed;
    if(feedback_glyph||feedback_led||feedback_hand){
        k->animate_glyph=feedback_glyph;k->animate_led=feedback_led;k->animate_hand=feedback_hand;
        k->old_angle=previous_angle;k->old_led=previous_led;
        if(function_changed&&!keyboard)k->spec.motion=UI_PANEL_KEY_SWAP;
        k->started=lv_tick_get();k->duration=spec->numeric?180:feedback_hand?320:k->spec.motion==UI_PANEL_KEY_TURN?300:240;
        if(touched&&!lamp_changed)k->old_led=alpha(spec->lit?.62f:.76f);
    }else if(!animate)k->duration=0;
    k->initialized=true;key_frame(k,lv_tick_get());
}

void ui_panel_set_keys(ui_panel_t *panel,ui_panel_layout_t layout,const ui_panel_key_t *left,size_t left_count,const ui_panel_key_t *right,size_t right_count,uint32_t revision,uint16_t trigger_action){
    if(!panel||!panel->private_state)return;
    panel_state_t *s=panel->private_state;
    if(s->layout!=layout)cancel_presses(s);
    s->layout=layout;
    if(left_count>UI_PANEL_MAX_KEYS)left_count=UI_PANEL_MAX_KEYS;
    if(right_count>UI_PANEL_MAX_KEYS)right_count=UI_PANEL_MAX_KEYS;
    for(unsigned i=0;i<UI_PANEL_MAX_KEYS;i++){update_key(s,&s->keys[0][i],left&&i<left_count?&left[i]:NULL,0,i,left_count,revision,trigger_action);update_key(s,&s->keys[1][i],right&&i<right_count?&right[i]:NULL,1,i,right_count,revision,trigger_action);}
    s->key_revision=revision;sync_timer(s);
}

void ui_panel_set_motion(ui_panel_t *panel,const ui_panel_motion_t *motion){
    if(!panel||!panel->private_state||!motion)return;
    panel_state_t *s=panel->private_state;
    /* Keep existing page callers; this slot now contains static status text only. */
    text_if_changed(s->title,motion->title);text_if_changed(s->detail,motion->detail);
    lv_obj_set_style_text_font(s->title,ui_panel_font(14,false),0);lv_obj_set_style_text_font(s->detail,ui_panel_font(12,false),0);
    bool warning=motion->tone!=UI_PANEL_TONE_SUCCESS&&(motion->tone>=UI_PANEL_TONE_WARNING||motion->scene==UI_PANEL_SCENE_JAM);
    lv_obj_set_style_text_color(s->title,lv_color_hex(warning?0xdebb96:0xe3e5e6),0);
    lv_obj_update_layout(s->copy);lv_obj_set_y(s->copy,(72-lv_obj_get_height(s->copy))/2);
}

void ui_panel_set_animation_enabled(ui_panel_t *panel,bool enabled){if(!panel||!panel->private_state)return;panel_state_t *s=panel->private_state;if(s->enabled==enabled)return;s->enabled=enabled;settle(s);}
void ui_panel_set_visible(ui_panel_t *panel,bool visible){if(!panel||!panel->private_state)return;panel_state_t *s=panel->private_state;if(!visible)cancel_presses(s);s->visible=visible;show(panel->root,visible);settle(s);}
void ui_panel_destroy(ui_panel_t *panel){if(panel&&panel->root)lv_obj_del(panel->root);}
