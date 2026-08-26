#ifndef LV_CAPSULE_PAGINATION_H
#define LV_CAPSULE_PAGINATION_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

lv_obj_t *lv_capsule_pagination_create(lv_obj_t *parent); // 创建胶囊圆点分页器
bool lv_capsule_pagination_set_count(lv_obj_t *pagination, uint8_t count); // 设置总页数并重建圆点
bool lv_capsule_pagination_set_active_page(lv_obj_t *pagination, uint8_t index); // 动画切换激活页
bool lv_capsule_pagination_set_active_page_now(lv_obj_t *pagination, uint8_t index); // 直接切换激活页
uint8_t lv_capsule_pagination_get_count(lv_obj_t *pagination); // 获取总页数
uint8_t lv_capsule_pagination_get_active_page(lv_obj_t *pagination); // 获取当前激活页

#endif // LV_CAPSULE_PAGINATION_H