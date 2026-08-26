#ifndef LV_QR_POPUP_H
#define LV_QR_POPUP_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

bool lv_qr_popup_show(const char* qr_text); //显示二维码弹窗
void lv_qr_popup_hide(void); //关闭二维码弹窗
bool lv_qr_popup_is_showing(void); //判断二维码弹窗是否显示中

#endif
