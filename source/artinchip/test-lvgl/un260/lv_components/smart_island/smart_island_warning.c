#include "un260/lv_components/smart_island.h"
#include "un260/lv_components/ui_notice.h"

/* Legacy business callers share the top notification service. This adapter
 * owns no island scene, machine acknowledgement, animation, or timer. */
void smart_island_notify_warning_level(const char *message, smart_island_warning_level_t level)
{
    if ((unsigned)level > SMART_ISLAND_WARNING_LEVEL_ERROR) return;
    ui_notice_post(level == SMART_ISLAND_WARNING_LEVEL_ERROR ? UI_NOTICE_ERROR : UI_NOTICE_WARNING,
                   NULL, "Machine", message && message[0] ? message : "Unable to complete the operation.");
}
void smart_island_notify_warning(const char *message)
{
    smart_island_notify_warning_level(message, SMART_ISLAND_WARNING_LEVEL_WARNING);
}
