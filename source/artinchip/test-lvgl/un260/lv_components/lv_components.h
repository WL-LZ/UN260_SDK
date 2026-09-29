#ifndef LV_COMPONENTS_H
#define LV_COMPONENTS_H
#include "lvgl/lvgl.h"
#include"un260/lv_resources/lv_img_init.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/lv_loading_orbit.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_components/lv_loading_grid.h"
#include "un260/lv_components/lv_debug_overlay.h"
#include "un260/lv_components/lv_upgrade_popup.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/lv_components/lv_selftest_list.h"
#include "un260/lv_components/lv_capsule_pagination.h"
#include "un260/app_service/setting_service.h"
const char* get_system_error_desc(uint8_t code);
const char* get_counting_error_desc(uint8_t type, uint8_t code);
void show_boot_selftest_error_popup(const char* msg);
#endif // !LV_COMPONENTS_H
