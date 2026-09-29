#include "app_setting_notice.h"
#include "workspace_service.h"
#include <string.h>
#include "un260/lv_components/ui_notice.h"
static uint8_t profile_command(const char *key)
{
    static const struct { const char *key; uint8_t command; } fields[] = {
        {"settings.speed", 0x16}, {"settings.sorting", 0x3A},
        {"settings.sound", 0x15}, {"settings.batch", 0x06},
        {"settings.add", 0x39}, {"settings.work_mode", 0x38}, {"settings.mode", 0x04}
    };
    if (!key) return 0;
    for (unsigned i=0; i<sizeof(fields)/sizeof(fields[0]); ++i)
        if (!strcmp(key,fields[i].key))
            return workspace_service_owns_command(fields[i].command) ? fields[i].command : 0;
    return 0;
}
void app_setting_notice_result(const char *key, const char *title, bool success)
{
    uint8_t command = profile_command(key);
    if (command) {
        /* Capture ownership before rejection ends the profile. Its global
         * owner publishes the aggregate result once, without a child notice. */
        if (!success) workspace_service_reject_command(command);
        return;
    }
    ui_notice_post(success ? UI_NOTICE_SUCCESS : UI_NOTICE_ERROR, key, title,
        success ? "Setting saved." : "Setting rejected. Previous value retained.");
}
void app_setting_notice_timeout(const char *key, const char *title)
{
    if (profile_command(key)) return;
    ui_notice_post(UI_NOTICE_WARNING, key, title, "No response received. Check device status.");
}
