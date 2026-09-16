#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define LV_UNUSED(x) (void)(x)
#define LV_RES_OK 0
typedef struct { int16_t x,y; } lv_point_t;
typedef struct { int unused; } lv_indev_t;
typedef struct lv_obj_t lv_obj_t;
typedef struct lv_timer_t { void *user_data; void (*cb)(struct lv_timer_t *); } lv_timer_t;
lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *), uint32_t period, void *data);
void lv_timer_del(lv_timer_t *timer);
typedef int lv_event_code_t;
enum {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED};
typedef int ui_text_id_t;
enum {UI_TEXT_GESTURE_HOME_TITLE,UI_TEXT_GESTURE_HOME_BODY,UI_TEXT_GESTURE_EXIT_TITLE,UI_TEXT_GESTURE_EXIT_BODY};
typedef enum {UI_PAGE_MAIN,UI_PAGE_SETTING,UI_PAGE_INNOVATION_CENTER,UI_PAGE_BOOT_ANIM,UI_PAGE_BOOT,
 UI_PAGE_UPGRADE,UI_PAGE_MAIN_UPGRADE,UI_PAGE_IMAGE_UPGRADE,UI_PAGE_UI_UPGRADE,UI_PAGE_CIS_CALIB,
 UI_PAGE_MOTOR_TEST,UI_PAGE_AGING_SETTING,UI_PAGE_FACTORY_SETTING,UI_PAGE_MENU,UI_PAGE_STANDBY_SETTING,UI_PAGE_STANDBY} ui_page_t;
typedef bool (*lv_port_pointer_observer_t)(lv_indev_t*,lv_event_code_t,const lv_point_t*,uint8_t,void*);
void lv_port_indev_set_pointer_observer(lv_port_pointer_observer_t,void*);
uint8_t lv_port_indev_touch_points(lv_point_t*,int32_t*,uint8_t);
uint32_t lv_tick_get(void);
uint32_t lv_tick_elaps(uint32_t);
int lv_async_call(void(*)(void*),void*);
ui_page_t ui_manager_get_current_page(void);
void ui_manager_clear_stack(void);
void ui_manager_switch(ui_page_t);
void ui_manager_push_page(ui_page_t);
bool ui_manager_pop_page(void);
bool page_06_settings_back_sub_page(void);
bool user_cfg_gesture_enabled(void);
bool user_cfg_gesture_save(bool);
#endif
