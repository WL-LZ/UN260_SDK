#include "lv_loading_orbit.h"

#define LOADING_ORBIT_SIZE_DEFAULT      38
#define LOADING_ORBIT_RING_RATIO_NUM    24
#define LOADING_ORBIT_RING_RATIO_DEN    38
#define LOADING_ORBIT_ARC_WIDTH_DEFAULT 3
#define LOADING_ORBIT_ROTATE_TIME       900

static lv_coord_t loading_orbit_clamp(lv_coord_t v, lv_coord_t min_v)
{
    return v < min_v ? min_v : v;
}

static void loading_orbit_rotate_cb(void *var, int32_t v)
{
    lv_arc_set_rotation((lv_obj_t *)var, (int16_t)v);
}

void lv_loading_orbit_set_opa(lv_obj_t *orbit, lv_opa_t opa)
{
    if (!orbit || !lv_obj_is_valid(orbit)) {
        return;
    }

    lv_obj_set_style_bg_opa(orbit, opa, 0);
    lv_obj_set_style_shadow_opa(orbit, (lv_opa_t)((opa * LV_OPA_10) / LV_OPA_COVER), 0);

    uint32_t child_cnt = lv_obj_get_child_cnt(orbit);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(orbit, i);
        if (!child || !lv_obj_is_valid(child)) {
            continue;
        }

        if (lv_obj_check_type(child, &lv_arc_class)) {
            lv_obj_set_style_arc_opa(child, (lv_opa_t)((opa * LV_OPA_70) / LV_OPA_COVER),
                                     LV_PART_MAIN);
            lv_obj_set_style_arc_opa(child, opa, LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_bg_opa(child, opa, 0);
            lv_obj_set_style_opa(child, opa, 0);
        }
    }
}

void lv_loading_orbit_set_indicator_color(lv_obj_t *orbit, lv_color_t color) //设置旋转环高亮颜色
{
    uint32_t child_cnt;

    if (!orbit || !lv_obj_is_valid(orbit)) {
        return;
    }

    child_cnt = lv_obj_get_child_cnt(orbit);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(orbit, i);
        if (!child || !lv_obj_is_valid(child)) {
            continue;
        }

        if (lv_obj_check_type(child, &lv_arc_class)) {
            lv_obj_set_style_arc_color(child, color, LV_PART_INDICATOR);
        }
    }
}

lv_obj_t *lv_loading_orbit_create_sized(lv_obj_t *parent, lv_coord_t size)
{
    lv_coord_t orbit_size = loading_orbit_clamp(size, 20);
    lv_coord_t ring_size = loading_orbit_clamp((orbit_size * LOADING_ORBIT_RING_RATIO_NUM) /
                                               LOADING_ORBIT_RING_RATIO_DEN, 12);
    lv_coord_t arc_width = loading_orbit_clamp((orbit_size * LOADING_ORBIT_ARC_WIDTH_DEFAULT) /
                                               LOADING_ORBIT_SIZE_DEFAULT, 2);
    lv_coord_t accent_size = loading_orbit_clamp(orbit_size / 5, 4);
    lv_coord_t accent_ofs = loading_orbit_clamp(orbit_size / 10, 3);

    lv_obj_t *host = lv_obj_create(parent);
    lv_obj_remove_style_all(host);
    lv_obj_set_size(host, orbit_size, orbit_size);
    lv_obj_clear_flag(host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(host, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(host, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(host, lv_color_hex(0xF5F8FC), 0);
    lv_obj_set_style_bg_opa(host, 220, 0);
    lv_obj_set_style_border_width(host, 0, 0);
    lv_obj_set_style_shadow_color(host, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(host, LV_OPA_10, 0);
    lv_obj_set_style_shadow_width(host, 8, 0);
    lv_obj_set_style_shadow_ofs_x(host, 1, 0);
    lv_obj_set_style_shadow_ofs_y(host, 2, 0);
    lv_obj_set_style_pad_all(host, 0, 0);

    lv_obj_t *arc = lv_arc_create(host);
    lv_obj_set_size(arc, ring_size, ring_size);
    lv_obj_center(arc);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_value(arc, 22);
    lv_obj_set_style_arc_width(arc, arc_width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, arc_width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xD7E1EB), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x2F7CF6), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    lv_obj_t *accent = lv_obj_create(host);
    lv_obj_remove_style_all(accent);
    lv_obj_set_size(accent, accent_size, accent_size);
    lv_obj_set_style_radius(accent, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0xC7EB00), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_align(accent, LV_ALIGN_TOP_RIGHT, -accent_ofs, accent_ofs);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, loading_orbit_rotate_cb);
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_time(&a, LOADING_ORBIT_ROTATE_TIME);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);

    return host;
}

lv_obj_t *lv_loading_orbit_create(lv_obj_t *parent)
{
    return lv_loading_orbit_create_sized(parent, LOADING_ORBIT_SIZE_DEFAULT);
}
