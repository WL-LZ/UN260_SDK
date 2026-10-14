#ifndef UN260_PAGE_TRAINING_H
#define UN260_PAGE_TRAINING_H
#include "lvgl/lvgl.h"
void ui_training_create(lv_obj_t *parent);
void ui_training_destroy(void);
bool ui_training_resume(void);
void ui_training_suspend(void);
#endif
