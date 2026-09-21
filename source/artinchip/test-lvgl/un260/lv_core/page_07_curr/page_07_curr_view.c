#include "un260/lv_core/page_07_curr/page_07_curr_view.h"
#include "un260/lv_core/page_07_curr/page_07_curr_layout.h"
#include "un260/lv_resources/lv_img_init.h"

void page07_curr_view_set_img_target_width(lv_obj_t* img, const char* code, int target_w)
{
    lv_img_header_t info;
    if (lv_img_decoder_get_info(get_currency_img(code), &info) == LV_RES_OK && info.w > 0) {
        int zoom = (target_w * 256) / (int)info.w;
        if (zoom < 32) zoom = 32;
        lv_img_set_zoom(img, zoom);
    }
}

void page07_curr_view_set_image_unselected_style(lv_obj_t* img)
{
    lv_obj_set_style_img_recolor(img, lv_color_hex(CURR_IMG_UNSEL), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(img, LV_OPA_COVER, 0);
}

void page07_curr_view_set_image_selected_style(lv_obj_t* img)
{
    lv_obj_set_style_img_recolor(img, lv_color_hex(CURR_IMG_UNSEL), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(img, LV_OPA_COVER, 0);
}
