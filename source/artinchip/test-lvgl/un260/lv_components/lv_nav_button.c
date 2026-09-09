#include "lv_nav_button.h"
#include "lv_damped_button.h"
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
        .normal_color=0xF6F7F8, .pressed_color=0xE2E3E4,
        .disabled_color=0xF0F1F2, .text_color=0x697C85,
        .disabled_text_color=0xADB7BD, .radius=3
    };
    lv_obj_t *button = lv_damped_button_create(parent, &style, "ESC",
        &lv_font_instrument_sans_medium_16);
    if(!button) return NULL;
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0xE1E6E9), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    if(cb) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
        lv_obj_add_event_cb(button, nav_back_marker, LV_EVENT_DELETE, button);
    }
    return button;
}

static void nav_home_icon_draw(lv_event_t *event)
{
    static const uint8_t lines[][4] = {
        {2,12,13,2}, {13,2,24,12}, {4,10,4,24},
        {4,24,22,24}, {22,24,22,10}
    };
    lv_obj_t *icon = lv_event_get_target(event);
    lv_area_t area;
    lv_obj_get_coords(icon, &area);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    lv_obj_init_draw_line_dsc(icon, LV_PART_MAIN, &dsc);
    for(unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
        lv_point_t from = {area.x1 + lines[i][0], area.y1 + lines[i][1]};
        lv_point_t to = {area.x1 + lines[i][2], area.y1 + lines[i][3]};
        lv_draw_line(ctx, &dsc, &from, &to);
    }
}

lv_obj_t *lv_nav_home_icon_create(lv_obj_t *parent)
{
    if(!parent) return NULL;
    lv_obj_t *icon = lv_obj_create(parent);
    if(!icon) return NULL;
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, 27, 27);
    lv_obj_set_style_line_color(icon, lv_color_hex(0x657F90), 0);
    lv_obj_set_style_line_width(icon, 2, 0);
    lv_obj_set_style_line_rounded(icon, true, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    if(!lv_obj_add_event_cb(icon, nav_home_icon_draw, LV_EVENT_DRAW_MAIN, NULL)) {
        lv_obj_del(icon);
        return NULL;
    }
    return icon;
}
