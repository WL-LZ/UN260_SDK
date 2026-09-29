#ifndef UN260_MACHINE_FAULT_ASSETS_H
#define UN260_MACHINE_FAULT_ASSETS_H
#include "lvgl/lvgl.h"
typedef struct { const lv_img_dsc_t *image; int16_t x, y; } machine_fault_asset_t;
extern const machine_fault_asset_t mf_asset_front, mf_asset_side, mf_asset_top, mf_asset_rear;
extern const machine_fault_asset_t mf_asset_lid[12], mf_asset_rear_tray, mf_asset_rear_face;
#endif
