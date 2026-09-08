#include "un260/lv_components/lv_card_surface.h"

void lv_card_surface_apply(lv_obj_t *surface,
                           const lv_card_surface_style_t *style)
{
    int16_t radius;
    int16_t limit;
    uint8_t border_width;
    const lv_style_selector_t selector = LV_PART_MAIN | LV_STATE_DEFAULT;

    if (surface == NULL || style == NULL ||
        style->width <= 0 || style->height <= 0) return;

    limit = (style->width < style->height ? style->width : style->height) / 2;
    radius = style->radius < 0 ? 0 : style->radius;
    if (radius > limit) radius = limit;
    border_width = style->border_width;
    if (border_width > limit) border_width = (uint8_t)limit;

    lv_obj_set_size(surface, style->width, style->height);
    lv_obj_set_style_radius(surface, radius, selector);
    lv_obj_set_style_bg_color(surface, lv_color_hex(style->background), selector);
    lv_obj_set_style_bg_opa(surface, LV_OPA_COVER, selector);
    lv_obj_set_style_bg_grad_dir(surface, LV_GRAD_DIR_NONE, selector);
    /* LVGL's main-part border is drawn inside the object's coordinates.
     * No separate outline or larger hit-box decoration is introduced. */
    lv_obj_set_style_border_width(surface, border_width, selector);
    lv_obj_set_style_border_color(surface, lv_color_hex(style->border), selector);
    lv_obj_set_style_border_opa(surface, LV_OPA_COVER, selector);
    lv_obj_set_style_border_side(surface, LV_BORDER_SIDE_FULL, selector);
    lv_obj_set_style_border_post(surface, false, selector);
    lv_obj_set_style_pad_all(surface, 0, selector);
    lv_obj_set_style_shadow_width(surface, 0, selector);
    lv_obj_set_style_shadow_opa(surface, LV_OPA_TRANSP, selector);
    lv_obj_set_style_outline_width(surface, 0, selector);
}

void lv_card_surface_focus_mark_apply(lv_obj_t *mark, bool focused)
{
    const lv_style_selector_t selector = LV_PART_MAIN | LV_STATE_DEFAULT;
    if (mark == NULL) return;

    /* This stays within the existing card footprint and is captured together
     * with its static content. No outside shadow, animation or draw hook. */
    lv_obj_set_size(mark, 28, 3);
    lv_obj_set_style_radius(mark, LV_RADIUS_CIRCLE, selector);
    lv_obj_set_style_bg_color(mark, lv_color_hex(0x4D5965), selector);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, selector);
    lv_obj_set_style_bg_grad_dir(mark, LV_GRAD_DIR_NONE, selector);
    lv_obj_set_style_border_width(mark, 0, selector);
    lv_obj_set_style_pad_all(mark, 0, selector);
    lv_obj_set_style_shadow_width(mark, 0, selector);
    lv_obj_set_style_shadow_opa(mark, LV_OPA_TRANSP, selector);
    lv_obj_set_style_outline_width(mark, 0, selector);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(mark, LV_ALIGN_BOTTOM_MID, 0, -11);
    if (focused) lv_obj_clear_flag(mark, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
}
