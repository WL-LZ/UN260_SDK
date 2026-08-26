#ifndef PAGE_00_BOOT_ANIM_H
#define PAGE_00_BOOT_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"
#include <stdbool.h>

#define UI_BOOT_ANIM_THEME_A 1
#define UI_BOOT_ANIM_THEME_B 2

/* Change this single macro to select the boot animation at build time. */
#ifndef UI_BOOT_ANIM_THEME
#define UI_BOOT_ANIM_THEME UI_BOOT_ANIM_THEME_B
#endif

#if UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_A && \
    UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_B
#error "UI_BOOT_ANIM_THEME must be UI_BOOT_ANIM_THEME_A or UI_BOOT_ANIM_THEME_B"
#endif

void ui_page_00_boot_anim_create(lv_obj_t* parent);
void ui_page_00_boot_anim_destroy(void);
bool ui_page_00_boot_anim_is_active(void);

#ifdef __cplusplus
}
#endif

#endif
