#ifndef PAGE_05_SET_PASSWORD_H
#define PAGE_05_SET_PASSWORD_H
#include "lvgl/lvgl.h"

void ui_page_05_set_password_create(lv_obj_t* parent);
void ui_page_05_set_password_destroy(void);
bool ui_page_05_set_password_resume(void);
void ui_page_05_set_password_suspend(void);
void ui_page_05_set_password_open(void);
bool ui_page_05_set_password_is_open(void);
bool ui_page_05_set_password_request_back(void);


#endif /* PAGE_05_SET_PASSWORD_H */
