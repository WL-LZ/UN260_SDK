#ifndef LV_SELFTEST_LIST_H
#define LV_SELFTEST_LIST_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LV_SELFTEST_LIST_STATE_SUCCESS = 0,
    LV_SELFTEST_LIST_STATE_LOADING,
    LV_SELFTEST_LIST_STATE_PENDING,
    LV_SELFTEST_LIST_STATE_ERROR,
} lv_selftest_list_state_t;

typedef struct {
    lv_coord_t item_w;
    lv_coord_t item_h;
    lv_coord_t item_gap;
    lv_coord_t item_pad_x;
    lv_coord_t icon_size;
    lv_coord_t spinner_size;
    lv_coord_t name_gap;
    lv_coord_t state_w;
    lv_color_t success_border_color;
    lv_color_t loading_border_color;
    lv_color_t pending_border_color;
    lv_color_t error_border_color;
    lv_color_t success_bg_color;
    lv_color_t loading_bg_color;
    lv_color_t pending_bg_color;
    lv_color_t error_bg_color;
    lv_color_t success_text_color;
    lv_color_t loading_text_color;
    lv_color_t pending_text_color;
    lv_color_t error_text_color;
    lv_color_t success_state_color;
    lv_color_t loading_state_color;
    lv_color_t pending_state_color;
    lv_color_t error_state_color;
    const char *success_icon_path;
    const char *error_icon_path;
    uint16_t spinner_time;
} lv_selftest_list_config_t;

void lv_selftest_list_config_init(lv_selftest_list_config_t *cfg); // 初始化自检列表配置
lv_obj_t *lv_selftest_list_create(lv_obj_t *parent, uint8_t item_count); // 创建自检列表
lv_obj_t *lv_selftest_list_create_with_config(lv_obj_t *parent, uint8_t item_count,
                                              const lv_selftest_list_config_t *cfg); // 按配置创建自检列表
void lv_selftest_list_set_count(lv_obj_t *list, uint8_t item_count); // 设置自检列表行数
void lv_selftest_list_set_item(lv_obj_t *list, uint8_t index, const char *name,
                               lv_selftest_list_state_t state); // 设置单项名称和状态
void lv_selftest_list_set_item_name(lv_obj_t *list, uint8_t index, const char *name); // 设置单项名称
void lv_selftest_list_set_item_state(lv_obj_t *list, uint8_t index,
                                     lv_selftest_list_state_t state); // 设置单项状态

#endif // LV_SELFTEST_LIST_H
