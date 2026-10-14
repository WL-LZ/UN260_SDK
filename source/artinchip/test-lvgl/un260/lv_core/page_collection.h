#ifndef PAGE_COLLECTION_H
#define PAGE_COLLECTION_H
#include "un260/lv_components/lv_settings.h"
void page_collection_create(lv_settings_frame_t *frame);
void page_collection_refresh(void);
void page_collection_main_badge_create(lv_obj_t *parent);
void page_collection_poll(void);
#endif
