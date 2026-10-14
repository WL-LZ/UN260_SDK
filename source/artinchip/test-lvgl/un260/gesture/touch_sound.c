#include "touch_sound.h"
static touch_sound_request_t request_sound;
static lv_obj_t *target;
static bool raw_contact;
static bool active, pressed, sounded, cancelled, changed;

static bool interactive(lv_obj_t *obj)
{
    if(lv_obj_has_class(obj, &lv_btn_class) ||
       lv_obj_has_class(obj, &lv_btnmatrix_class) ||
       lv_obj_has_class(obj, &lv_dropdown_class) ||
       lv_obj_has_class(obj, &lv_dropdownlist_class) ||
       lv_obj_has_class(obj, &lv_switch_class) ||
       lv_obj_has_class(obj, &lv_checkbox_class) ||
       lv_obj_has_class(obj, &lv_slider_class) ||
       lv_obj_has_class(obj, &lv_roller_class) ||
       lv_obj_has_class(obj, &lv_textarea_class)) return true;
    for(lv_obj_t *p = obj; p; p = lv_obj_has_flag(p, LV_OBJ_FLAG_EVENT_BUBBLE) ? lv_obj_get_parent(p) : NULL)
        if(lv_obj_has_event_cb(p, LV_EVENT_CLICKED) || lv_obj_has_event_cb(p, LV_EVENT_SHORT_CLICKED) ||
           lv_obj_has_event_cb(p, LV_EVENT_VALUE_CHANGED) || lv_obj_has_event_cb(p, LV_EVENT_LONG_PRESSED)) return true;
    return false;
}
static void request_once(void)
{
    if(sounded || !request_sound) return;
    sounded = true;
    request_sound();
}
void touch_sound_contact(bool down)
{
    raw_contact = down;
    if(down) {
        sounded = active = pressed = cancelled = changed = false;
        target = NULL;
    }
}
static void sound(void)
{
    if(!target || !lv_obj_is_visible(target) || lv_obj_has_state(target, LV_STATE_DISABLED)) return;
    if(target && lv_obj_has_class(target, &lv_btnmatrix_class)) {
        uint16_t id = lv_btnmatrix_get_selected_btn(target);
        if(id == LV_BTNMATRIX_BTN_NONE ||
           lv_btnmatrix_has_btn_ctrl(target, id, LV_BTNMATRIX_CTRL_DISABLED) ||
           lv_btnmatrix_has_btn_ctrl(target, id, LV_BTNMATRIX_CTRL_HIDDEN)) return;
    }
    request_once();
}
void touch_sound_init(touch_sound_request_t request)
{
    request_sound = request;
    target = NULL;
    raw_contact = active = pressed = false;
}
void touch_sound_feedback(lv_indev_t *indev, lv_event_code_t code)
{
    if(!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) return;
    if(code == LV_EVENT_READY && raw_contact) { request_once(); return; }
    lv_obj_t *obj = lv_indev_get_obj_act();
    if(code == LV_EVENT_PRESSED) {
        /* Feedback also runs on bubbling/nested events. One physical contact
         * has one target; only dereference it while LVGL reports it active. */
        if(pressed && target == obj) return;
        target = obj;
        active = pressed = true;
        if(!raw_contact) sounded = false;
        changed = false;
        cancelled = !obj || lv_obj_has_state(obj, LV_STATE_DISABLED) || !interactive(obj);
        return;
    }
    if(code == LV_EVENT_RELEASED) pressed = false;
    if(code == LV_EVENT_PRESS_LOST || code == LV_EVENT_CANCEL || code == LV_EVENT_SCROLL_BEGIN) {
        cancelled = true;
        if(code == LV_EVENT_PRESS_LOST) { active = pressed = false; target = NULL; }
        return;
    }
    if(code == LV_EVENT_READY && active && obj == target) {
        if(!lv_obj_has_state(obj, LV_STATE_DISABLED)) sound();
        return;
    }
    if(!active || obj != target || cancelled) return;
    if(code == LV_EVENT_VALUE_CHANGED) {
        changed = true;
        if(!pressed && lv_obj_has_class(obj, &lv_roller_class)) sound();
    } else if(code == LV_EVENT_LONG_PRESSED && lv_obj_has_event_cb(obj, LV_EVENT_LONG_PRESSED)) {
        sound();
    } else if(code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED) {
        if(!lv_obj_has_state(obj, LV_STATE_DISABLED)) sound();
        if(code == LV_EVENT_CLICKED) { active = pressed = false; target = NULL; }
    } else if(code == LV_EVENT_RELEASED && changed) {
        sound();
    }
}
