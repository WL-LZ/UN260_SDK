#ifndef LV_IMG_INIT_H
#define LV_IMG_INIT_H

#include "lvgl/lvgl.h"

typedef enum {
    LV_OBJ_TYPE_NONE = 0,
    LV_OBJ_TYPE_BUTTON ,
    LV_OBJ_TYPE_IMAGE,
    LV_OBJ_TYPE_LABEL,

}   lv_obj_type_t;


//文本元素
typedef struct {
    const char* label;
    uint8_t label_r;
    uint8_t label_g;
    uint8_t label_b;
    const lv_font_t* font;
    lv_text_align_t align;
} label_item_t;


//对象元素
typedef struct {
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
} obj_item_t;


//对象风格
typedef struct {
    uint8_t opacity;
    uint8_t radius;
    uint8_t border_width;
    bool use_custom_style;
} obj_style_t;

typedef enum {
    UI_BTN_STYLE_NONE = 0,   
    UI_BTN_STYLE_APPLE,
    UI_BTN_STYLE_PRESS_FEEL,
    UI_BTN_STYLE_ANDROID,
    UI_BTN_STYLE_NO_FEEDBACK,
} ui_btn_style_t;


//ui元素
typedef struct {
    const char* obj_name;
    lv_obj_type_t obj_type;
    const lv_img_dsc_t* img_src;

    obj_item_t obj_item;
    label_item_t label_item;  
    obj_style_t obj_style;

    lv_event_cb_t event_cb;
    lv_event_code_t event_code;
    void* user_data;
    lv_obj_t* obj_ref;
    ui_btn_style_t btn_style;

} ui_element_t;

// 页面对象存储
typedef struct {
    char* page_name;                    // 页面名称
    ui_element_t* elements;             // 页面包含的UI元素
    int element_count;                  // 元素数量
    lv_obj_t* page_obj;                 // 页面对象引用
    bool is_active;                     // 页面是否激活
} ui_page_item_t;




void lv_ui_obj_init(lv_obj_t* parent, ui_element_t* element, int count);
const char* get_currency_img(const char* code);

#endif
