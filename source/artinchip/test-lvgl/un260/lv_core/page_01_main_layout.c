#include "page_01_main_layout.h"
#include <stdlib.h>
#include <string.h>
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/machine_state/machine_state.h"
#include "un260/font/main_fonts.h"
#include "lv_port_indev.h"

enum { HOLD_MS=650, MOVE_SLOP=10, ITEM_COUNT=PAGE_01_MAIN_LAYOUT_ITEM_COUNT };
static bool edit_feature_enabled=UI_MAIN_LAYOUT_EDIT_DEFAULT_ENABLED!=0;
static struct {
    lv_obj_t *root, *items[ITEM_COUNT], *overlay, *marks[4], *hint, *bar;
    lv_obj_t *actions[3];
    lv_timer_t *timer;
    lv_indev_t *indev;
    void (*apply)(const ui_main_layout_t *);
    ui_main_layout_t saved, draft;
    bool enabled, candidate, editing, down, drain, blue_visible, dragging, drag_moved;
    lv_point_t start, point;
    uint32_t tick;
    int group, source, target, action;
} editor;


static bool descendant(lv_obj_t *object, lv_obj_t *parent)
{
    for (;object;object=lv_obj_get_parent(object)) if (object==parent) return true;
    return false;
}

static bool allowed(void)
{
    fault_source_t source; uint8_t type,code;
    if (!edit_feature_enabled || !editor.enabled || !editor.root || !lv_obj_is_visible(editor.root) ||
        ui_manager_get_current_page()!=UI_PAGE_MAIN || ui_manager_is_transitioning() ||
        app_command_runtime_count_start_busy() || machine_state_aging_running() ||
        fault_popup_is_showing() || fault_popup_get_pending_fault(&source,&type,&code)) return false;
    lv_point_t point=editor.point;
    /* Never start through a keyboard, dialog, or another page. Ignore only the
       empty top/system layer itself and non-interactive touch-feedback art. */
    lv_obj_t *layers[]={lv_layer_sys(),lv_layer_top()};
    for (unsigned i=0;i<2;++i) {
        lv_obj_t *hit=lv_indev_search_obj(layers[i],&point);
        if (hit && hit!=layers[i]) return false;
    }
    lv_obj_t *hit=lv_indev_search_obj(lv_scr_act(),&point);
    if (hit && !descendant(hit,editor.root)) return false;
    return true;
}

static unsigned group_count(int group)
{ return group==0?4:group==1?3:group==2 || group==3?2:0; }
static lv_obj_t *slot_object(int group,int slot)
{
    if (group==0) return editor.items[editor.draft.left[slot]];
    if (group==1) return editor.items[4+editor.draft.right[slot]];
    if (group==3) return editor.items[9+(slot ^ editor.draft.footer_swapped)];
    return editor.items[7+(slot ^ editor.draft.mirrored)];
}
static bool contains(lv_obj_t *object,const lv_point_t *point)
{
    if (!object || !lv_obj_is_visible(object)) return false;
    lv_area_t area;lv_obj_get_coords(object,&area);
    return point->x>=area.x1 && point->x<=area.x2 && point->y>=area.y1 && point->y<=area.y2;
}
static bool hit_item(const lv_point_t *point,int *group,int *slot)
{
    /* The editor's toolbar is not part of the underlying panel target. */
    if (editor.editing && contains(editor.bar,point)) return false;
    for (int g=0;g<4;++g) for (unsigned s=0;s<group_count(g);++s)
        if (contains(slot_object(g,s),point)) { *group=g;*slot=(int)s;return true; }
    return false;
}
static int hit_action(const lv_point_t *point)
{
    for (int i=0;i<3;++i) if (contains(editor.actions[i],point)) return i;
    return -1;
}

static void hint(const char *text)
{ if (strcmp(lv_label_get_text(editor.hint),text)) lv_label_set_text(editor.hint,text); }

static void marks_refresh(void)
{
    lv_obj_update_layout(editor.root);
    unsigned count=group_count(editor.group);
    lv_area_t root;lv_obj_get_coords(editor.root,&root);
    for (unsigned i=0;i<4;++i) {
        lv_obj_t *mark=editor.marks[i];
        if (i>=count) { lv_obj_add_flag(mark,LV_OBJ_FLAG_HIDDEN);continue; }
        lv_area_t area;lv_obj_get_coords(slot_object(editor.group,i),&area);
        lv_obj_set_pos(mark,area.x1-root.x1,area.y1-root.y1);
        lv_obj_set_size(mark,lv_area_get_width(&area),lv_area_get_height(&area));
        bool selected=i==(unsigned)editor.source;
        bool target=i==(unsigned)editor.target;
        uint32_t color=selected?0x23864B:target?0x125FCB:0x6D97C6;
        lv_obj_set_style_radius(mark,editor.group==3?22:12,0);
        lv_obj_set_style_bg_color(mark,lv_color_hex(selected?0x23864B:target?0x125FCB:0xFFFFFF),0);
        lv_obj_set_style_bg_opa(mark,selected?14:target?20:24,0);
        lv_obj_set_style_border_width(mark,selected?3:target?4:2,0);
        lv_obj_set_style_border_color(mark,lv_color_hex(color),0);
        lv_obj_clear_flag(mark,LV_OBJ_FLAG_HIDDEN);
    }
    hint(editor.target>=0?"Release to swap":editor.source>=0?"Selected: drag to a blue area":"Select an area to move");
}

static void clear_drag(void)
{
    editor.candidate=editor.dragging=editor.drag_moved=false;
    editor.group=editor.source=editor.target=editor.action=-1;
}

static void finish(bool keep)
{
    if (!keep && editor.editing) { editor.draft=editor.saved;editor.apply(&editor.saved); }
    editor.editing=false;
    editor.candidate=false;
    editor.drain=editor.down;
    clear_drag();
    lv_obj_add_flag(editor.overlay,LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(editor.timer);
}

static void enter(void)
{
    editor.candidate=false;editor.editing=true;
    editor.dragging=true;editor.drag_moved=false;
    editor.blue_visible=lv_obj_is_visible(editor.items[7]);
    lv_obj_move_foreground(editor.overlay);
    lv_obj_clear_flag(editor.overlay,LV_OBJ_FLAG_HIDDEN);
    marks_refresh();
    /* Stationary fingers need not generate another hardware frame. Cancel the
       original pressed widget now; the raw hook drains its eventual release. */
    lv_port_indev_capture_pointer(editor.indev);
}

static void timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!allowed() || (editor.editing && editor.blue_visible!=lv_obj_is_visible(editor.items[7]))) {
        if (editor.editing) finish(false);
        else { clear_drag();lv_timer_pause(editor.timer); }
        return;
    }
    if (editor.candidate && editor.down && lv_tick_elaps(editor.tick)>=HOLD_MS) enter();
}

static void release_action(int action)
{
    if (action==0) {
        ui_main_layout_default(&editor.draft);editor.apply(&editor.draft);
        clear_drag();marks_refresh();
        hint("Default restored. Done to save.");
    } else if (action==1) finish(false);
    else if (action==2) {
        if (ui_main_layout_equal(&editor.saved,&editor.draft) || ui_state_main_layout_save(&editor.draft)) {
            editor.saved=editor.draft;finish(true);
        } else hint("Could not save. Retry or Cancel.");
    }
}

bool page_01_main_layout_pointer(lv_indev_t *indev,lv_event_code_t event,const lv_point_t *point,uint8_t count)
{
    bool was_down=editor.down;
    bool released=event==LV_EVENT_RELEASED || count==0;
    editor.down=!released;editor.indev=indev;
    if (point) editor.point=*point;
    if (editor.drain) { if (released) editor.drain=false;return true; }
    if (editor.candidate && !editor.editing && (released || count==1) && allowed() &&
        lv_tick_elaps(editor.tick)>=HOLD_MS &&
        abs(editor.point.x-editor.start.x)<=MOVE_SLOP &&
        abs(editor.point.y-editor.start.y)<=MOVE_SLOP) enter();
    if (editor.editing && (!allowed() || (!released && count!=1) ||
        editor.blue_visible!=lv_obj_is_visible(editor.items[7]))) {
        finish(false);return true;
    }
    if (editor.editing) {
        if (!was_down && !released) {
            editor.start=editor.point;editor.drag_moved=false;editor.target=-1;
            editor.action=hit_action(&editor.point);
            int group=-1,slot=-1;
            editor.dragging=editor.action<0 && hit_item(&editor.point,&group,&slot);
            if (editor.dragging) { editor.group=group;editor.source=slot; }
            marks_refresh();
        }
        if (editor.dragging &&
            (abs(editor.point.x-editor.start.x)>MOVE_SLOP ||
             abs(editor.point.y-editor.start.y)>MOVE_SLOP)) editor.drag_moved=true;
        if (released) {
            /* The final hardware report may lift outside the last hovered
               target without a separate PRESSING frame. Validate the drop. */
            if (editor.dragging && editor.drag_moved) {
                int group=-1,slot=-1;
                hit_item(&editor.point,&group,&slot);
                editor.target=group==editor.group && slot!=editor.source?slot:-1;
            }
            int action=editor.action;
            bool swapped=false;
            if (action>=0 && hit_action(&editor.point)==action &&
                abs(editor.point.x-editor.start.x)<=MOVE_SLOP &&
                abs(editor.point.y-editor.start.y)<=MOVE_SLOP) release_action(action);
            else if (editor.target>=0 && ui_main_layout_swap(&editor.draft,editor.group,editor.source,editor.target)) {
                editor.apply(&editor.draft);editor.source=editor.target;editor.target=-1;
                swapped=true;
            }
            editor.target=-1;editor.action=-1;editor.dragging=editor.drag_moved=false;
            if (editor.editing && action<0) {
                marks_refresh();
                if (swapped) hint("Position swapped. Done to save.");
            }
        } else if (editor.dragging && editor.drag_moved) {
            int group=-1,slot=-1;
            hit_item(&editor.point,&group,&slot);
            int target=group==editor.group && slot!=editor.source?slot:-1;
            if (target!=editor.target) { editor.target=target;marks_refresh(); }
        }
        return true;
    }
    if (released || count!=1 || !allowed()) {
        clear_drag();lv_timer_pause(editor.timer);return false;
    }
    if (!was_down) {
        clear_drag();editor.start=editor.point;editor.tick=lv_tick_get();
        /* Resume/rebuild must not adopt a finger already held on another
         * page or before the feature was enabled. Require a fresh contact. */
        editor.candidate=event==LV_EVENT_PRESSED && hit_item(&editor.point,&editor.group,&editor.source);
        if (editor.candidate) { lv_timer_reset(editor.timer);lv_timer_resume(editor.timer); }
    } else if (editor.candidate &&
        (abs(editor.point.x-editor.start.x)>MOVE_SLOP || abs(editor.point.y-editor.start.y)>MOVE_SLOP)) {
        clear_drag();lv_timer_pause(editor.timer);
    }
    return false;
}

static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color)
{
    lv_obj_t *obj=lv_obj_create(parent);lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj,x,y);lv_obj_set_size(obj,w,h);
    lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_set_style_radius(obj,12,0);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *label(lv_obj_t *parent,const char *text,uint32_t color)
{
    lv_obj_t *obj=lv_label_create(parent);lv_label_set_text(obj,text);
    lv_obj_set_style_text_font(obj,&lv_font_instrument_sans_medium_16,0);
    lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    return obj;
}

void page_01_main_layout_attach(lv_obj_t *root,lv_obj_t *const items[ITEM_COUNT],void (*apply)(const ui_main_layout_t *))
{
    page_01_main_layout_detach();
    editor.root=root;editor.apply=apply;memcpy(editor.items,items,sizeof(editor.items));
    ui_state_main_layout_get(&editor.saved);ui_main_layout_normalize(&editor.saved);
    editor.draft=editor.saved;apply(&editor.saved);
    editor.overlay=box(root,0,0,1280,400,0);
    lv_obj_set_style_bg_opa(editor.overlay,LV_OPA_TRANSP,0);
    lv_obj_add_flag(editor.overlay,LV_OBJ_FLAG_HIDDEN|LV_OBJ_FLAG_CLICKABLE);
    for (unsigned i=0;i<4;++i) editor.marks[i]=box(editor.overlay,0,0,1,1,0xFFFFFF);
    /* Keep every side action and both footer targets reachable in all groups.
     * The central panels remain selectable below this fixed, compact toolbar. */
    editor.bar=box(editor.overlay,252,8,776,52,0xFFFFFF);
    lv_obj_set_style_border_width(editor.bar,2,0);
    lv_obj_set_style_border_color(editor.bar,lv_color_hex(0xD5E0E7),0);
    editor.hint=label(editor.bar,"Layout edit",0x1D2B34);lv_obj_set_pos(editor.hint,16,16);
    lv_obj_set_width(editor.hint,366);lv_label_set_long_mode(editor.hint,LV_LABEL_LONG_CLIP);
    const char *titles[]={"Reset","Cancel","Done"};
    for (unsigned i=0;i<3;++i) {
        editor.actions[i]=box(editor.bar,392+124*i,4,116,44,i==2?0x125FCB:0xF1F4F5);
        lv_obj_t *text=label(editor.actions[i],titles[i],i==2?0xFFFFFF:0x1D2B34);lv_obj_center(text);
    }
    editor.timer=lv_timer_create(timer_cb,25,NULL);lv_timer_pause(editor.timer);
    clear_drag();page_01_main_layout_resume();
}
void page_01_main_layout_set_enabled(bool enabled)
{
    if (edit_feature_enabled==enabled) return;
    edit_feature_enabled=enabled;
    if (!enabled && editor.root) {
        bool capture=editor.down && (editor.editing || editor.candidate || editor.drain);
        if (capture) lv_port_indev_capture_pointer(editor.indev);
        finish(false);
        editor.drain=capture;
    }
}
bool page_01_main_layout_is_enabled(void)
{ return edit_feature_enabled; }
bool page_01_main_layout_is_editing(void)
{ return editor.editing || editor.drain; }
void page_01_main_layout_suspend(void)
{
    if (!editor.root) return;
    finish(false);editor.enabled=false;
}
void page_01_main_layout_resume(void)
{
    if (!editor.root) return;
    editor.enabled=true;editor.down=false;editor.drain=false;
}
void page_01_main_layout_detach(void)
{
    if (!editor.root) return;
    page_01_main_layout_suspend();
    lv_timer_del(editor.timer);lv_obj_del(editor.overlay);
    memset(&editor,0,sizeof(editor));
}
