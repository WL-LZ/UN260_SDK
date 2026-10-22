#ifndef UI_DISPLAY_MODE_H
#define UI_DISPLAY_MODE_H

#include <stdbool.h>

/* Presentation preference only. It never changes a machine counting mode. */
typedef enum {
    UI_DISPLAY_MODE_FULLSCREEN = 0,
    UI_DISPLAY_MODE_PANEL,
} ui_display_mode_t;

ui_display_mode_t ui_display_mode_get(void);
bool ui_display_mode_set(ui_display_mode_t mode);
bool ui_display_mode_is_panel(void);

#endif
