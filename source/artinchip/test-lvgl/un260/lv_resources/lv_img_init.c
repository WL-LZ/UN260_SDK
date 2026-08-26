#include "lv_img_init.h"
#include "un260/lv_resources/lv_image_declear.h"
#include "un260/currency/currency_state.h"
#include"un260/lv_system/lv_str.h"
#include "../aic_ui/aic_ui.h"


void lv_ui_obj_init(lv_obj_t* parent, ui_element_t* element, int count)
{
    lv_obj_t* obj = NULL;
    for (int i = 0; i < count; i++)
    {
        ui_element_t* info = &element[i];
        switch (info->obj_type) {
        case LV_OBJ_TYPE_NONE:
            printf("lv_obj init fail, %s\n", info->obj_name);
            continue;

        case LV_OBJ_TYPE_BUTTON:
            obj = lv_btn_create(parent);

            if (info->label_item.label != NULL) {
                lv_obj_t* label = lv_label_create(obj);
                const char* tmp_btn_label = get_str_by_name(info->label_item.label);
                if(!tmp_btn_label)
                lv_label_set_text(label, info->label_item.label);
                else
                {
                    lv_label_set_text(label, tmp_btn_label);

                }
                lv_obj_center(label);
                lv_obj_set_style_text_color(label,
                    lv_color_make(info->label_item.label_r,
                        info->label_item.label_g,
                        info->label_item.label_b), 0);
            }

            // 两个风格按钮
            if (info->btn_style == UI_BTN_STYLE_APPLE) {
                static lv_style_t style_apple_btn;
                static bool apple_inited = false;
                if (!apple_inited) {
                    lv_style_init(&style_apple_btn);
                    lv_style_set_radius(&style_apple_btn, 20);
                    lv_style_set_bg_color(&style_apple_btn, lv_color_make(255, 255, 255));
                    lv_style_set_bg_grad_color(&style_apple_btn, lv_color_make(235, 236, 240));
                    lv_style_set_bg_grad_dir(&style_apple_btn, LV_GRAD_DIR_VER);
                    lv_style_set_bg_opa(&style_apple_btn, LV_OPA_80);
                    lv_style_set_shadow_color(&style_apple_btn, lv_color_black());
                    lv_style_set_shadow_opa(&style_apple_btn, LV_OPA_20);
                    lv_style_set_shadow_width(&style_apple_btn, 8);
                    lv_style_set_shadow_ofs_y(&style_apple_btn, 2);
                    apple_inited = true;
                }
                lv_obj_add_style(obj, &style_apple_btn, 0);
            }
            else if (info->btn_style == UI_BTN_STYLE_PRESS_FEEL) {
                static lv_style_t style_btn_default;
                static lv_style_t style_btn_pressed;
                static bool press_style_inited = false;
                if (!press_style_inited) {
                    lv_style_init(&style_btn_default);
                    lv_style_set_bg_opa(&style_btn_default, LV_OPA_TRANSP);  // 默认透明
                    lv_style_set_shadow_opa(&style_btn_default, LV_OPA_0);

                    lv_style_init(&style_btn_pressed);
                    lv_style_set_bg_color(&style_btn_pressed, lv_color_make(235, 235, 235));
                    lv_style_set_bg_opa(&style_btn_pressed, LV_OPA_70);
                    lv_style_set_transform_zoom(&style_btn_pressed, 240);
                    lv_style_set_shadow_width(&style_btn_pressed, 0);

                    press_style_inited = true;
                }
                lv_obj_add_style(obj, &style_btn_default, LV_STATE_DEFAULT);
                lv_obj_add_style(obj, &style_btn_pressed, LV_STATE_PRESSED);
            }
            else if (info->btn_style == UI_BTN_STYLE_ANDROID) {

                static lv_style_t style_glass_default, style_glass_pressed;
                static bool style_inited = false;

                if (!style_inited) {
                    // 默认
                    lv_style_init(&style_glass_default);
                    lv_style_set_bg_color(&style_glass_default, lv_color_white());
                    lv_style_set_bg_opa(&style_glass_default, LV_OPA_100);
                    lv_style_set_radius(&style_glass_default, 10);
                    //lv_style_set_shadow_width(&style_glass_default, 6);
                    //lv_style_set_shadow_color(&style_glass_default, lv_color_hex(0xAAAAAA));
                    //lv_style_set_shadow_ofs_x(&style_glass_default, 2);
                    //lv_style_set_shadow_ofs_y(&style_glass_default, 1);
                    // 按下效果
                    lv_style_init(&style_glass_pressed);
                    lv_style_set_bg_opa(&style_glass_pressed, LV_OPA_80);
                    lv_style_set_transform_zoom(&style_glass_pressed, 253);
                    lv_style_set_shadow_width(&style_glass_pressed, 5);
                    lv_style_set_bg_color(&style_glass_pressed, lv_color_hex(0x7BCD82));
                    lv_style_set_shadow_color(&style_glass_default, lv_color_hex(0xAAAAAA));
                    style_inited = true;
                }

                // 清除旧样式
                lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
                lv_obj_remove_style_all(obj);       
                lv_obj_add_style(obj, &style_glass_default, LV_STATE_DEFAULT);
                lv_obj_add_style(obj, &style_glass_pressed, LV_STATE_PRESSED);

            }
            else if(info->btn_style == UI_BTN_STYLE_NO_FEEDBACK)
            {
                static lv_style_t style_no_feedback_default;
                static bool style_no_feedback_init = false;
                if (!style_no_feedback_init)
                {
                    lv_style_init(&style_no_feedback_default);
                    lv_style_set_border_opa(&style_no_feedback_default, LV_OPA_0);
                }
                lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
                lv_obj_remove_style_all(obj);
                lv_obj_add_style(obj, &style_no_feedback_default, LV_STATE_DEFAULT);
            }
            else if (info->btn_style == UI_BTN_STYLE_NO_FEEDBACK)
            {
                static lv_style_t style_no_feedback_default;
                static bool style_no_feedback_init = false;
                if (!style_no_feedback_init)
                {
                    lv_style_init(&style_no_feedback_default);
                    lv_style_set_border_opa(&style_no_feedback_default, LV_OPA_0);
                }
                lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
                lv_obj_remove_style_all(obj);
                lv_obj_add_style(obj, &style_no_feedback_default, LV_STATE_DEFAULT);
            }

            // 文字字体设置
            if (info->label_item.font) {
                lv_obj_set_style_text_font(obj, info->label_item.font, 0);
            }
            break;

        case LV_OBJ_TYPE_IMAGE:
            obj = lv_img_create(parent);
            
            // 优先使用路径加载（如果obj_name以.png结尾或不包含扩展名）
            if (info->obj_name && strlen(info->obj_name) > 0) {
                char img_path[256];
                // 如果obj_name已经包含.png扩展名，直接使用
                if (strstr(info->obj_name, ".png")) {
                    snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s", info->obj_name);
                } else {
                    // 如果没有扩展名，添加.png
                    snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s.png", info->obj_name);
                }
                lv_img_set_src(obj, img_path);
            }
            // 如果没有obj_name，回退到使用嵌入式数据
            else if (info->img_src) {
                lv_img_set_src(obj, info->img_src);
            }
            break;

        case LV_OBJ_TYPE_LABEL:
            obj = lv_label_create(parent);
            if (info->label_item.label) {

                const char* tmp_label = get_str_by_name(info->label_item.label);
                if(tmp_label)
                lv_label_set_text(obj, tmp_label);
                else
                {
                    lv_label_set_text(obj, info->label_item.label);

                }
            }
            lv_obj_set_style_text_color(obj,
                lv_color_make(info->label_item.label_r,
                    info->label_item.label_g,
                    info->label_item.label_b), 0);
            lv_obj_set_style_text_align(obj, info->label_item.align, 0);
            if (info->label_item.font) {
                lv_obj_set_style_text_font(obj, info->label_item.font, 0);
            }
            if (info->obj_style.use_custom_style) {
                lv_obj_set_style_bg_color(obj,
                    lv_color_make(info->obj_item.color_r,
                        info->obj_item.color_g,
                        info->obj_item.color_b), 0);
                lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);  // 背景完全不透明
                lv_obj_set_style_radius(obj, info->obj_style.radius, 0);
                lv_obj_set_style_border_width(obj, info->obj_style.border_width, 0);
                lv_obj_set_style_border_color(obj, lv_color_hex(0xCCCCCC), 0); // 可选
            }
            break;
            break;
        }

        if (obj) {
            lv_obj_set_pos(obj, info->obj_item.x, info->obj_item.y);
            if(info->obj_type!=LV_OBJ_TYPE_IMAGE)
            {
            lv_obj_set_size(obj, info->obj_item.w, info->obj_item.h);            
            lv_obj_set_style_bg_color(obj,
                lv_color_make(info->obj_item.color_r,
                    info->obj_item.color_g,
                    info->obj_item.color_b), 0);
            if (info->obj_style.use_custom_style) {
                lv_obj_set_style_radius(obj, info->obj_style.radius, 0);
                lv_obj_set_style_border_width(obj, info->obj_style.border_width, 0);
                lv_obj_set_style_opa(obj, info->obj_style.opacity, 0);
            }
            if (info->event_cb) {
                lv_event_code_t event_code = (info->event_code != 0) ?
                    info->event_code : LV_EVENT_CLICKED;
                lv_obj_add_event_cb(obj, info->event_cb, event_code, info->user_data);
                lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            }
            }
            info->obj_ref = obj;
        }

    }

}


const char* get_currency_img(const char* code)
{
    if (!code) return NULL;

    // 使用路径加载货币图片
    static char img_path[256];
    
    if (strcmp(code, CURRENCY_AUTO_CODE) == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_AUTO.png");
        return img_path;
    }
    else if (strcmp(code, "USD") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_USD.png");
        return img_path;
    }
    else if (strcmp(code, "CNY") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_CNY.png");
        return img_path;
    }
    else if (strcmp(code, "EGP") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_EGP.png");
        return img_path;
    }
    else if (strcmp(code, "GBP") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_GBP.png");
        return img_path;
    }
    else if (strcmp(code, "ISK") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_ISK.png");
        return img_path;
    }
    else if (strcmp(code, "PHP") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_PHP.png");
        return img_path;
    }
    else if (strcmp(code, "SOS") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_SOS.png");
        return img_path;
    }
    else if (strcmp(code, "TRY") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_TRY.png");
        return img_path;
    }
    else if (strcmp(code, "EUR") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_EUR.png");
        return img_path;
    }
    else if (strcmp(code, "AED") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_AED.png");
        return img_path;
    }
    else if (strcmp(code, "SAR") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_SAR.png");
        return img_path;
    }
    else if (strcmp(code, "OMR") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_OMR.png");
        return img_path;
    }
    else if (strcmp(code, "QAR") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_QAR.png");
        return img_path;
    }
    else if (strcmp(code, "MAD") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_MAD.png");
        return img_path;
    }
    else if (strcmp(code, "DZD") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_DZD.png");
        return img_path;
    }
    else if (strcmp(code, "INR") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_INR.png");
        return img_path;
    }
    else if (strcmp(code, "PKR") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_PKR.png");
        return img_path;
    }
    else if (strcmp(code, "IQD") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_IQD.png");
        return img_path;
    }
    // New conditions for missing currencies:
    else if (strcmp(code, "IRR") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_IRR.png");
        return img_path;
    }
    else if (strcmp(code, "CHF") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_CHF.png");
        return img_path;
    }   
    else if (strcmp(code, "SGD") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_SGD.png");
        return img_path;
    }
    else if (strcmp(code, "CAD") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_CAD.png");
        return img_path;
    }
    else if (strcmp(code, "CHF") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_CHF.png");
        return img_path;
    }
    else if (strcmp(code, "KRW") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_KRW.png");
        return img_path;
    }
    else if (strcmp(code, "MXN") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_MXN.png");
        return img_path;
    }
    else if (strcmp(code, "HKD") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_HKD.png");
        return img_path;
    }
    else if (strcmp(code, "PLN") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_PLN.png");
        return img_path;
    }
    else if (strcmp(code, "JPY") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_JPY.png");
        return img_path;
    }
    else if (strcmp(code, "ILS") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_ILS.png");
        return img_path;
    }
    else if (strcmp(code, "AUD") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_AUD.png");
        return img_path;
    }
    else if (strcmp(code, "RUB") == 0) {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_RUB.png");
        return img_path;
    }
        
    else {
        snprintf(img_path, sizeof(img_path), "L:/usr/local/share/lvgl_data/%s","CURR_IQD.png");
        return img_path;
    }

    return NULL;
}


