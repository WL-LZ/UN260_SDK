#include "lv_nav_button.h"
#include "lv_damped_button.h"
#include "un260/lv_resources/ui_icons.h"
#include "un260/lv_system/user_cfg.h"
/* Lifetime-bound marker: no registry of pointers to deleted/cached pages. */
static void nav_back_marker(lv_event_t *event) { LV_UNUSED(event); }

static lv_obj_t *nav_back_find(lv_obj_t *root)
{
    if(!root || !lv_obj_is_visible(root)) return NULL;
    for(uint32_t n = lv_obj_get_child_cnt(root); n > 0; --n) {
        lv_obj_t *found = nav_back_find(lv_obj_get_child(root, (int32_t)n - 1));
        if(found) return found;
    }
    return lv_obj_get_event_user_data(root, nav_back_marker) ? root : NULL;
}

lv_nav_back_result_t lv_nav_button_request_back(void)
{
    lv_obj_t *button = nav_back_find(lv_layer_top());
    if(!button) button = nav_back_find(lv_scr_act());
    if(!button) return LV_NAV_BACK_NONE;
    if(lv_obj_has_state(button, LV_STATE_DISABLED)) return LV_NAV_BACK_BLOCKED;
    /* The callback may synchronously destroy button and its entire page. */
    lv_event_send(button, LV_EVENT_CLICKED, NULL);
    return LV_NAV_BACK_HANDLED;
}

lv_obj_t *lv_nav_button_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
    lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, void *user_data)
{
    const lv_damped_button_style_t style = {
        .normal_color=0xE9EDF0, .pressed_color=0xD6DADE,
        .disabled_color=0xF0F1F2, .text_color=0x293B44,
        .disabled_text_color=0xADB7BD, .radius=10
    };
    lv_obj_t *button = lv_damped_button_create(parent, &style, "Back",
        &lv_font_instrument_sans_medium_16);
    if(!button) return NULL;
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0xE1E6E9), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    if(h < 60 && w >= 90) ui_button_icon(button, UI_ICON("back_18"), false);
    if(cb) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
        lv_obj_add_event_cb(button, nav_back_marker, LV_EVENT_DELETE, button);
    }
    return button;
}

lv_obj_t *lv_nav_home_icon_create(lv_obj_t *parent)
{
    if(!parent) return NULL;
    lv_obj_t *icon = lv_img_create(parent);
    if(!icon) return NULL;
    lv_img_set_src(icon, LVGL_DIR "ui_icons/home_27.png");
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return icon;
}
