#include "un260/lv_system/ui_i18n.h"
#include "app_setting_notice.h"
#include "workspace_service.h"
#include "setting_service.h"
#include "count_mode_service.h"
#include "app_fault_recovery.h"
#include "un260/lv_core/lv_page_manager.h"
#include <string.h>
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_components/lv_fault_popup.h"
/* This is application presentation policy. Protocol state is always consumed,
 * including rejected profile steps, whether or not a banner is permitted. */
/* 1: secondary page, 2: Main, 3: automatic mode follow-on after currency ACK. */
static uint8_t request_origin[256];
bool app_setting_notice_page_allowed(void)
{
    ui_page_t page = ui_manager_get_current_page();
    return page > UI_PAGE_MAIN && page < UI_PAGE_COUNT &&
        page != UI_PAGE_PURE && page != UI_PAGE_BOOT && page != UI_PAGE_STANDBY;
}
static void request_started(uint8_t command)
{
    if (command == 0x06) {
        ui_notice_clear("settings.batch");
        ui_notice_clear("workspace.batch");
    }
    fault_popup_resolve_setting_rejection(command);
    request_origin[command] = command == 0x04 && count_mode_service_currency_mode() ? 3 :
        app_setting_notice_page_allowed() ? 1 : 2;
}
void app_setting_notice_init(void)
{
    memset(request_origin,0,sizeof(request_origin));
    setting_service_set_request_observer(request_started);
}
static uint8_t notice_command(const char *key)
{
    static const struct {const char *key;uint8_t command;} fields[] = {
        {"settings.speed",0x16},{"settings.sorting",0x3A},{"settings.sound",0x15},
        {"settings.batch",0x06},{"settings.add",0x39},{"settings.work_mode",0x38},
        {"settings.mode",0x04},{"settings.double_note",0x31},{"settings.flap",0x42},
        {"settings.reject_pocket",0x08},{"settings.impeller",0x4B}
    };
    for (unsigned i=0;key && i<sizeof(fields)/sizeof(fields[0]);++i)
        if (!strcmp(key,fields[i].key)) return fields[i].command;
    return 0;
}
bool app_setting_notice_allowed(const char *key)
{
    if (key && !strcmp(key,"settings.mode") && request_origin[0x04] == 3) return true;
    return app_setting_notice_page_allowed() && request_origin[notice_command(key)] != 2;
}
static uint8_t profile_command(const char *key)
{
    uint8_t command=notice_command(key);
    return command && workspace_service_owns_command(command) ? command : 0;
}
void app_setting_notice_result(const char *key, const char *title, bool success)
{
    if (success) app_fault_recovery_resolve_setting_rejection(notice_command(key));
    if (success && !strcmp(key,"settings.mode") && request_origin[0x04] == 3) {
        ui_notice_clear(key);
        return;
    }
    uint8_t command = profile_command(key);
    if (command) {
        /* Capture ownership before rejection ends the profile. Its global
         * owner publishes the aggregate result once, without a child notice. */
        if (!success) workspace_service_reject_command(command);
        return;
    }
    if (!success && app_fault_recovery_report_setting_rejection(notice_command(key))) {
        ui_notice_clear(key);
        return;
    }
    if (!app_setting_notice_allowed(key)) {
        ui_notice_clear(key);
        return;
    }
    if (!success && fault_popup_get_auto_enabled()) {
        fault_popup_report_setting_rejection(notice_command(key),0);
        ui_notice_clear(key);
        return;
    }
    const char *detail = success ?
        (notice_command(key) == 0x38 ? UI_N_("Start mode confirmed") : UI_N_("Setting saved.")) :
        UI_N_("Setting rejected. Previous value retained.");
    ui_notice_post_text(success ? UI_NOTICE_SUCCESS : UI_NOTICE_ERROR, key, title, detail);
}
void app_setting_notice_timeout(const char *key, const char *title)
{
    if (profile_command(key)) return;
    if (!app_setting_notice_allowed(key)) {
        ui_notice_clear(key);
        return;
    }
    ui_notice_post_text(UI_NOTICE_WARNING, key, title, UI_N_("No response received. Check device status."));
}
