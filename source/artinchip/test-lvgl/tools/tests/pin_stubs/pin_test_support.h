#ifndef PIN_TEST_SUPPORT_H
#define PIN_TEST_SUPPORT_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LV_UNUSED(x) ((void)(x))
#ifndef LVGL_DIR
#define LVGL_DIR "L:/usr/local/share/lvgl_data/"
#endif
#define LV_OPA_COVER 255
#define LV_OPA_TRANSP 0
#define LV_OPA_10 25
#define LV_OPA_30 76
typedef uint8_t lv_opa_t;
#define LV_RADIUS_CIRCLE 32767
#define LV_OBJ_FLAG_CLICKABLE 1
#define LV_OBJ_FLAG_SCROLLABLE 2
#define LV_OBJ_FLAG_HIDDEN 4
#define LV_STATE_PRESSED 1
#define LV_STATE_DISABLED 2
#define LV_SYMBOL_BACKSPACE "BACKSPACE"
#define LV_SYMBOL_EYE_OPEN "EYE_OPEN"
#define LV_SYMBOL_EYE_CLOSE "EYE_CLOSE"
#define LV_TEXT_ALIGN_CENTER 1
#define USER_PASSWORD_MAX_LEN 4
#define lv_snprintf snprintf
enum {LV_EVENT_CLICKED, LV_EVENT_DELETE};
enum {UI_PAGE_MAIN, UI_PAGE_SETTING, UI_PAGE_PASSWORD_CHANGE};
enum {UI_TEXT_PASSWORD_LOGIN_TITLE, UI_TEXT_SETTINGS_PASSWORD};
typedef int lv_coord_t;
typedef uint32_t lv_color_t;
typedef int lv_font_t;
typedef struct lv_obj_t lv_obj_t;
typedef struct lv_event_t {
    int code;
    lv_obj_t *target;
    void *user_data;
} lv_event_t;
typedef void (*lv_event_cb_t)(lv_event_t *);
struct lv_obj_t {
    lv_obj_t *parent;
    lv_obj_t *children[64];
    unsigned child_count;
    int flags, state, x, y, w, h, opacity;
    uint32_t color, pressed_color;
    char text[160];
    lv_event_cb_t click_cb, delete_cb;
    void *click_data, *delete_data;
};
typedef struct lv_timer_t {
    void (*callback)(struct lv_timer_t *);
    void *user_data;
    bool paused;
} lv_timer_t;

static lv_obj_t *live_objects[1024];
static unsigned live_count, timers_created, timers_deleted;
static int switched_page = -1;
static unsigned pop_count, save_count;
static bool save_success = true;
static bool saved_visibility, visibility_save_success = true;
static unsigned visibility_save_count;
static char saved_password[5] = "1111";
static char toast_text[160];
static const lv_font_t lv_font_instrument_sans_medium_12 = 12;
static const lv_font_t lv_font_instrument_sans_medium_14 = 14;
static const lv_font_t lv_font_instrument_sans_medium_16 = 16;
static const lv_font_t lv_font_instrument_sans_medium_18 = 18;
static const lv_font_t lv_font_instrument_sans_medium_24 = 24;
static const lv_font_t lv_font_instrument_sans_medium_28 = 28;
static const lv_font_t lv_font_instrument_sans_semibold_28 = 28;
static const lv_font_t lv_font_instrument_sans_semibold_24 = 24;
static const lv_font_t lv_font_instrument_sans_bold_24 = 24;
static const lv_font_t lv_font_montserrat_20 = 20;

static inline bool lv_obj_is_valid(const lv_obj_t *obj)
{
    for (unsigned i = 0; i < live_count; ++i) if (live_objects[i] == obj) return true;
    return false;
}
static inline lv_obj_t *lv_obj_create(lv_obj_t *parent)
{
    lv_obj_t *obj = calloc(1, sizeof(*obj));
    assert(obj && live_count < 1024);
    obj->parent = parent;
    live_objects[live_count++] = obj;
    if (parent) {
        assert(parent->child_count < 64);
        parent->children[parent->child_count++] = obj;
    }
    return obj;
}
static inline lv_obj_t *lv_label_create(lv_obj_t *parent) {return lv_obj_create(parent);}
static inline lv_obj_t *lv_img_create(lv_obj_t *parent) {lv_obj_t *o=lv_obj_create(parent);o->w=o->h=24;return o;}
static inline void lv_img_set_src(lv_obj_t *obj, const char *src) {snprintf(obj->text,sizeof(obj->text),"%s",src);}
static inline void lv_obj_center(lv_obj_t *obj) {assert(obj->parent);obj->x=(obj->parent->w-obj->w)/2;obj->y=(obj->parent->h-obj->h)/2;}
static inline void lv_obj_remove_style_all(lv_obj_t *obj) {assert(lv_obj_is_valid(obj));}
static inline void lv_obj_set_pos(lv_obj_t *obj, int x, int y) {obj->x=x; obj->y=y;}
static inline void lv_obj_set_size(lv_obj_t *obj, int w, int h) {obj->w=w; obj->h=h;}
static inline void lv_obj_set_width(lv_obj_t *obj, int w) {obj->w=w;}
static inline void lv_obj_set_x(lv_obj_t *obj, int x) {obj->x=x;}
static inline uint32_t lv_color_hex(uint32_t color) {return color;}
static inline void lv_obj_set_style_bg_color(lv_obj_t *obj, uint32_t c, int s) {LV_UNUSED(s); obj->color=c;}
static inline void lv_obj_set_style_bg_opa(lv_obj_t *obj, int o, int s) {LV_UNUSED(s); obj->opacity=o;}
static inline void lv_obj_set_style_opa(lv_obj_t *obj, int o, int s) {LV_UNUSED(s); obj->opacity=o;}
typedef struct {void *var;void (*exec)(void *,int32_t);int end;} lv_anim_t;
static inline void lv_anim_init(lv_anim_t *a){memset(a,0,sizeof(*a));}
static inline void lv_anim_set_var(lv_anim_t *a,void *v){a->var=v;}
static inline void lv_anim_set_exec_cb(lv_anim_t *a,void (*cb)(void *,int32_t)){a->exec=cb;}
static inline void lv_anim_set_values(lv_anim_t *a,int from,int to){LV_UNUSED(from);a->end=to;}
static inline void lv_anim_set_time(lv_anim_t *a,unsigned ms){LV_UNUSED(a);assert(ms==100);}
static inline void lv_anim_start(lv_anim_t *a){a->exec(a->var,a->end);}
static inline void lv_anim_del(void *v,void (*cb)(void *,int32_t)){LV_UNUSED(v);LV_UNUSED(cb);}
static inline lv_obj_t *lv_layer_top(void){return NULL;}
#define PIN_STYLE_STUB(name, type) static inline void name(lv_obj_t *o, type v, int s) {assert(lv_obj_is_valid(o)); LV_UNUSED(v); LV_UNUSED(s);}
PIN_STYLE_STUB(lv_obj_set_style_radius, int)
PIN_STYLE_STUB(lv_obj_set_style_border_width, int)
PIN_STYLE_STUB(lv_obj_set_style_border_color, uint32_t)
PIN_STYLE_STUB(lv_obj_set_style_text_font, const lv_font_t *)
PIN_STYLE_STUB(lv_obj_set_style_text_color, uint32_t)
PIN_STYLE_STUB(lv_obj_set_style_text_letter_space, int)
PIN_STYLE_STUB(lv_obj_set_style_text_align, int)
PIN_STYLE_STUB(lv_obj_set_style_shadow_width, int)
PIN_STYLE_STUB(lv_obj_set_style_shadow_opa, int)
PIN_STYLE_STUB(lv_obj_set_style_translate_y, int)
static inline void lv_obj_clear_flag(lv_obj_t *obj, int flags) {obj->flags &= ~flags;}
static inline void lv_obj_add_flag(lv_obj_t *obj, int flags) {obj->flags |= flags;}
static inline void lv_obj_add_state(lv_obj_t *obj, int state) {obj->state |= state;}
static inline void lv_obj_clear_state(lv_obj_t *obj, int state) {obj->state &= ~state;}
static inline bool lv_obj_has_flag(const lv_obj_t *obj, int flags) {return (obj->flags & flags) != 0;}
static inline bool lv_obj_is_visible(const lv_obj_t *obj)
{
    while (obj) {if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return false; obj=obj->parent;}
    return true;
}
static inline void lv_obj_move_foreground(lv_obj_t *obj) {assert(lv_obj_is_valid(obj));}
static inline void lv_obj_update_layout(lv_obj_t *obj) {assert(lv_obj_is_valid(obj));}
static inline void lv_label_set_text(lv_obj_t *obj, const char *text) {snprintf(obj->text, sizeof(obj->text), "%s", text);}
static inline void lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, int code, void *data)
{
    if (code == LV_EVENT_CLICKED) {obj->click_cb=cb; obj->click_data=data;}
    else {assert(code == LV_EVENT_DELETE); obj->delete_cb=cb; obj->delete_data=data;}
}
static inline int lv_event_get_code(lv_event_t *e) {return e->code;}
static inline lv_obj_t *lv_event_get_target(lv_event_t *e) {return e->target;}
static inline void *lv_event_get_user_data(lv_event_t *e) {return e->user_data;}
static inline void click(lv_obj_t *obj)
{
    assert(lv_obj_is_valid(obj) && lv_obj_is_visible(obj));
    lv_event_t event = {LV_EVENT_CLICKED, obj, obj->click_data};
    assert(obj->click_cb);
    obj->click_cb(&event);
}
static inline void lv_obj_del(lv_obj_t *obj)
{
    assert(lv_obj_is_valid(obj));
    if (obj->delete_cb) {
        lv_event_t event = {LV_EVENT_DELETE, obj, obj->delete_data};
        obj->delete_cb(&event);
    }
    while (obj->child_count) lv_obj_del(obj->children[obj->child_count - 1]);
    if (obj->parent) {
        lv_obj_t *parent = obj->parent;
        for (unsigned i=0; i < parent->child_count; ++i) if (parent->children[i] == obj) {
            parent->children[i] = parent->children[--parent->child_count]; break;
        }
    }
    for (unsigned i=0; i<live_count; ++i) if (live_objects[i] == obj) {
        live_objects[i]=live_objects[--live_count]; break;
    }
    free(obj);
}
static inline lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *), int period, void *data)
{
    assert(period == 500);
    lv_timer_t *timer = calloc(1, sizeof(*timer)); assert(timer);
    timer->callback=cb; timer->user_data=data; ++timers_created; return timer;
}
static inline void lv_timer_del(lv_timer_t *timer) {assert(timer); ++timers_deleted; free(timer);}
static inline void lv_timer_pause(lv_timer_t *timer) {timer->paused=true;}
static inline void lv_timer_resume(lv_timer_t *timer) {timer->paused=false;}
static inline void lv_timer_reset(lv_timer_t *timer) {assert(timer);}
static inline void ui_manager_switch(int page) {switched_page=page;}
static inline void ui_manager_pop_page(void) {++pop_count;}
static inline void ui_manager_clear_stack(void) {}
static inline bool ui_manager_suspend_to_home(void){ui_manager_switch(UI_PAGE_MAIN);return true;}
static inline void settings_detail_action_block(lv_obj_t *obj,const char *reason){LV_UNUSED(reason);lv_obj_clear_state(obj,LV_STATE_DISABLED);}
static inline const char *ui_text_get(int id) {return id == UI_TEXT_PASSWORD_LOGIN_TITLE ? "SECURE ACCESS" : "Change password";}
static inline const char *user_cfg_password_get(void) {return saved_password;}
static inline bool user_cfg_password_visibility_enabled(void) {return saved_visibility;}
static inline bool user_cfg_password_visibility_save(bool visible)
{
    ++visibility_save_count;
    if (visibility_save_success) saved_visibility = visible;
    return visibility_save_success;
}
static inline bool user_cfg_password_save(const char *value)
{
    ++save_count;
    if (save_success) snprintf(saved_password, sizeof(saved_password), "%s", value);
    return save_success;
}
typedef struct {
    int w,h,auto_hide_ms;
    const char *text;
    bool show_loader,align_center;
    const lv_font_t *text_font;
    uint32_t loader_color;
} lv_print_toast_config_t;
static inline lv_print_toast_config_t lv_print_toast_get_default_config(void) {return (lv_print_toast_config_t){0};}
static inline void lv_print_toast_show_with_config(const lv_print_toast_config_t *cfg) {snprintf(toast_text,sizeof(toast_text),"%s",cfg->text);}
static inline lv_obj_t *settings_detail_create_page(lv_obj_t *parent, const char *title, lv_event_cb_t back, lv_obj_t **content)
{
    LV_UNUSED(title);
    lv_obj_t *page=lv_obj_create(parent);
    lv_obj_t *header=lv_obj_create(page);
    lv_obj_add_event_cb(header,back,LV_EVENT_CLICKED,NULL);
    *content=lv_obj_create(page);
    return page;
}
static inline lv_obj_t *settings_detail_create_card(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *card=lv_obj_create(parent); lv_obj_set_pos(card,x,y); lv_obj_set_size(card,w,h); return card;
}
static inline lv_obj_t *settings_detail_create_label(lv_obj_t *parent,const char *text,const lv_font_t *font,uint32_t color,int x,int y)
{
    LV_UNUSED(font); LV_UNUSED(color);
    lv_obj_t *label=lv_obj_create(parent); lv_label_set_text(label,text); lv_obj_set_pos(label,x,y); return label;
}
static inline lv_obj_t *settings_detail_create_button(lv_obj_t *parent,int x,int y,int w,int h,const char *text,uint32_t color,lv_event_cb_t cb,void *data)
{
    lv_obj_t *button=settings_detail_create_card(parent,x,y,w,h);
    LV_UNUSED(color); lv_label_set_text(button,text); lv_obj_add_event_cb(button,cb,LV_EVENT_CLICKED,data); return button;
}

typedef struct { const char *title, *subtitle, *icon; lv_event_cb_t back; void *user_data; } lv_settings_header_t;
typedef struct { lv_obj_t *root, *body, *footer, *back, *message; } lv_settings_frame_t;
static inline lv_obj_t *lv_settings_label(lv_obj_t *parent,const char *text,int x,int y,const lv_font_t *font,uint32_t color)
{ return settings_detail_create_label(parent,text,font,color,x,y); }
static inline lv_obj_t *lv_settings_box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color)
{lv_obj_t *o=settings_detail_create_card(p,x,y,w,h);o->color=color;return o;}
static inline lv_obj_t *lv_settings_button(lv_obj_t *parent,int x,int y,int w,int h,const char *text,bool primary,lv_event_cb_t cb,void *data)
{ LV_UNUSED(primary); return settings_detail_create_button(parent,x,y,w,h,text,0,cb,data); }
static inline lv_settings_frame_t lv_settings_frame_create(lv_obj_t *parent,const lv_settings_header_t *header)
{
    lv_settings_frame_t frame={0};
    frame.root=lv_obj_create(parent);
    frame.back=lv_settings_button(frame.root,1162,21,94,46,"Back",false,header->back,header->user_data);
    frame.body=settings_detail_create_card(frame.root,24,84,1232,242);
    frame.footer=settings_detail_create_card(frame.root,24,338,1232,46);
    frame.message=lv_settings_label(frame.footer,"",0,14,&lv_font_instrument_sans_medium_14,0);
    return frame;
}
typedef enum { GESTURE_ACTION_EXIT_PAGE, GESTURE_ACTION_HOME, GESTURE_ACTION_RETURN } gesture_action_t;
static bool (*pin_gesture_policy)(gesture_action_t);
static inline void gesture_service_set_page_policy(uint32_t owner,bool (*drag)(void),bool (*handler)(gesture_action_t))
{ LV_UNUSED(owner); LV_UNUSED(drag); pin_gesture_policy=handler; }
static inline void gesture_service_clear_page_policy(uint32_t owner) {LV_UNUSED(owner);pin_gesture_policy=NULL;}
typedef enum {SETTINGS_DIALOG_INFO,SETTINGS_DIALOG_SUCCESS,SETTINGS_DIALOG_WARNING,SETTINGS_DIALOG_DESTRUCTIVE} settings_detail_dialog_kind_t;
typedef void (*settings_detail_dialog_cb_t)(void *);
static settings_detail_dialog_cb_t pin_discard;
static bool pin_overlay;
static inline bool settings_detail_overlay_is_open(void) {return pin_overlay;}
static inline void settings_detail_dialog_hide(void) {pin_overlay=false;pin_discard=NULL;}
static inline bool settings_detail_dialog_show_ex(settings_detail_dialog_kind_t kind,const char *title,const char *body,
    const char *ok,const char *cancel,settings_detail_dialog_cb_t confirm,settings_detail_dialog_cb_t reject,void *data)
{ LV_UNUSED(kind);LV_UNUSED(title);LV_UNUSED(body);LV_UNUSED(ok);LV_UNUSED(cancel);LV_UNUSED(reject);LV_UNUSED(data);pin_overlay=true;pin_discard=confirm;return true; }

#endif
