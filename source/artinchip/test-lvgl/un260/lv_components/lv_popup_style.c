#include "lv_popup_style.h"

void lv_popup_style(lv_obj_t *root, lv_obj_t *card)
{
    lv_obj_set_style_bg_color(root, lv_color_hex(0x182B3B), 0);
    lv_obj_set_style_bg_opa(root, 71, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 22, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xECF0F3), 0);
    lv_obj_set_style_shadow_width(card, 32, 0);
    lv_obj_set_style_shadow_ofs_y(card, 8, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x152B3E), 0);
    lv_obj_set_style_shadow_opa(card, 25, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
}
