#ifndef PAGE_08_BOOT_H
#define PAGE_08_BOOT_H
#include "lvgl/lvgl.h"
#include <stdint.h>
#include "un260/lv_resources/lv_img_init.h" 
#include "lv_page_event.h"
void ui_page_08_curr_create(lv_obj_t* parent);
void ui_page_08_curr_destroy(void);
/* Theme D only: retain a short visual bridge while MAIN/PURE becomes active. */
void ui_page_08_curr_start_handoff(void);
bool ui_page_08_curr_visual_is_quiet(void);
/* One-shot startup policy; construction still belongs to the UI thread. */
void ui_page_08_curr_defer_next_create(void);
bool ui_page_08_curr_prepare_step(void);
/* Startup overlay owns this cover; false restores the existing self-test page. */
void ui_page_08_curr_set_covered(bool covered);
void boot_progress_set(uint8_t percent);
void boot_progress_reset(void);
void boot_selftest_list_reset(void); // 重置自检卡片显示
void boot_selftest_list_sync_step(uint8_t step); // 根据步骤同步自检卡片
void boot_selftest_list_set_result(uint8_t index, uint8_t result); // 按协议结果更新单项状态
void boot_selftest_list_finish(void); // 刷新完成显示；第四版以服务快照为准，不覆盖失败结果
#endif // PAGE_08_BOOT_H
