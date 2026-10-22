#include "ui_display_mode.h"

/* UI-thread owned; intentionally session-only until a persisted preference
 * is requested. The existing Pure preference belongs to another feature. */
static ui_display_mode_t display_mode = UI_DISPLAY_MODE_FULLSCREEN;

ui_display_mode_t ui_display_mode_get(void)
{
    return display_mode;
}

bool ui_display_mode_set(ui_display_mode_t mode)
{
    if (mode != UI_DISPLAY_MODE_FULLSCREEN && mode != UI_DISPLAY_MODE_PANEL)
        return false;
    display_mode = mode;
    return true;
}

bool ui_display_mode_is_panel(void)
{
    return display_mode == UI_DISPLAY_MODE_PANEL;
}
