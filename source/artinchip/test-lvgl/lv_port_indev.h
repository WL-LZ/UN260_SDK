/**
 * @file lv_port_indev.h
 *
 */

#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_drv_conf.h"
#include "lvgl/lvgl.h"

void lv_port_indev_init(void);
void lv_port_indev_set_drag_obj(lv_obj_t *obj, bool enable);

/* Complete raw input frames, independent of object hit testing. True captures
 * the entire contact sequence until all fingers lift. */
typedef bool (*lv_port_pointer_observer_t)(lv_indev_t *indev,
                                           lv_event_code_t event_code,
                                           const lv_point_t *point,
                                           uint8_t touch_count,
                                           void *user_data);

void lv_port_indev_set_pointer_observer(lv_port_pointer_observer_t observer,
                                        void *user_data);
uint8_t lv_port_indev_touch_count(void);
uint8_t lv_port_indev_touch_points(lv_point_t *points, int32_t *ids, uint8_t capacity);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_PORT_INDEV_H */
