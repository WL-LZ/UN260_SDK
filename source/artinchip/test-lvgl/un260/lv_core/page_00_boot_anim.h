#ifndef PAGE_00_BOOT_ANIM_H
#define PAGE_00_BOOT_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"
#include <stdbool.h>

#define UI_BOOT_ANIM_THEME_A 1
#define UI_BOOT_ANIM_THEME_B 2
#define UI_BOOT_ANIM_THEME_C 3

/* Change this single macro to select the boot animation at build time. */
#ifndef UI_BOOT_ANIM_THEME
#define UI_BOOT_ANIM_THEME UI_BOOT_ANIM_THEME_C
#endif

#if UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_A && \
    UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_B && \
    UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_C
#error "UI_BOOT_ANIM_THEME must be A (1), B (2), or C (3)"
#endif

void ui_page_00_boot_anim_create(lv_obj_t* parent);
void ui_page_00_boot_anim_destroy(void);
bool ui_page_00_boot_anim_is_active(void);
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_C
/* Called outside page creation so a failed registered intro can safely exit. */
void ui_page_00_boot_anim_poll(void);
/* Opt into readiness-driven handover after create. Unmanaged previews keep
 * their original duration. These APIs are UI-thread only. */
void ui_page_00_boot_anim_set_startup_ready(bool ready);
void ui_page_00_boot_anim_adopt_elapsed(uint32_t elapsed_ms);
void ui_page_00_boot_anim_set_startup_error(bool diagnostics_available);
#else
static inline void ui_page_00_boot_anim_poll(void) {}
#endif

#ifdef __cplusplus
}
#endif

#endif
