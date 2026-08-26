#ifndef PAGE_08_BOOT_H
#define PAGE_08_BOOT_H
#include "lvgl/lvgl.h"
#include <stdint.h>
#include "un260/lv_resources/lv_img_init.h" 
#include "lv_page_event.h"
void ui_page_08_curr_create(lv_obj_t* parent);
void ui_page_08_curr_destroy(void);
void boot_progress_set(uint8_t percent);
void boot_progress_reset(void);
void boot_selftest_list_reset(void); // 重置自检卡片显示
void boot_selftest_list_sync_step(uint8_t step); // 根据步骤同步自检卡片
void boot_selftest_list_set_result(uint8_t index, uint8_t result); // 按协议结果更新单项状态
void boot_selftest_list_finish(void); // 自检完成后全部置成功
#endif // PAGE_08_BOOT_H
