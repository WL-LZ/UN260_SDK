#ifndef UN260_UI_ICONS_H
#define UN260_UI_ICONS_H
#include "lvgl/lvgl.h"
#include "un260/lv_components/lv_damped_button.h"
#include <string.h>
/* Product outline icons: 24-unit grid, 1.8-unit stroke, rasterized once.
 * Assets live in lvgl_data/ui_icons; never use font glyphs as UI icons. */
#define UI_ICON(name) LVGL_DIR "ui_icons/" name ".png"
static inline lv_obj_t *ui_icon_create(lv_obj_t *parent, const char *source)
{
    lv_obj_t *image=lv_img_create(parent);
    if(image) {
        lv_img_set_src(image,source);
        lv_obj_clear_flag(image,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    return image;
}
/* Keep the original first-child label and callbacks, including back dispatch. */
static inline void ui_button_icon(lv_obj_t *button, const char *source, bool vertical)
{
    if(!button) return;
    lv_obj_t *image=NULL;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(button);++i) {
        lv_obj_t *child=lv_obj_get_child(button,i);
        if(lv_obj_check_type(child,&lv_img_class)) {image=child;break;}
    }
    if(!image) image=ui_icon_create(button,source);
    else if(!lv_img_get_src(image) || lv_img_src_get_type(lv_img_get_src(image))!=LV_IMG_SRC_FILE || strcmp(lv_img_get_src(image),source)) lv_img_set_src(image,source);
    if(!image) return;
    lv_obj_align(image,vertical ? LV_ALIGN_TOP_MID : LV_ALIGN_LEFT_MID,vertical ? 0 : 8,vertical ? 15 : 0);
    lv_obj_t *text=lv_damped_button_get_label(button);
    if(text) {
        lv_obj_set_style_text_align(text,LV_TEXT_ALIGN_CENTER,0);
        lv_obj_set_width(text,lv_obj_get_style_width(button,0)-(vertical ? 8 : 34));
        lv_obj_align(text,vertical ? LV_ALIGN_BOTTOM_MID : LV_ALIGN_RIGHT_MID,vertical ? 0 : -5,vertical ? -14 : 0);
    }
}
#endif
