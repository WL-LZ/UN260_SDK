#include "page_14_main_upgrade.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/upgrade_page_runtime.h"
#include "un260/device_info/device_info.h"

static upgrade_page_runtime_t g_upgrade_runtime;

static const char *a1_text(uint8_t status)
{
    switch (status) {
    case 0x01: return "The controller accepted the update request.";
    case 0x02: return "Installation is in progress. Keep power connected.";
    case 0x03: return "Update complete. The update file has been kept.";
    case 0x04: return "Update complete. Keep the update file.";
    case 0xF1: return "The controller could not find the update file. Check that it is ready.";
    case 0xF2: return "The file does not match this board. Use the correct update file.";
    case 0xF3: return "The controller reported an update failure. Check the update file.";
    default: return "Waiting for an update status.";
    }
}

static bool a1_terminal(uint8_t status)
{
    return status == 0x03 || status == 0x04 ||
           status == 0xF1 || status == 0xF2 || status == 0xF3;
}

static const upgrade_page_runtime_config_t g_upgrade_config = {
    .command = 0xA1, .timeout_ms = 20000,
    .status_text = a1_text, .is_terminal = a1_terminal,
    .page_id = UI_PAGE_MAIN_UPGRADE,
    .owner = UPGRADE_SESSION_CONTROLLER,
};

void ui_page_14_main_upgrade_on_reply(uint8_t command, uint8_t status)
{
    if (command == 0xA1) upgrade_page_runtime_handle_reply(&g_upgrade_runtime, status);
}

void ui_page_14_main_upgrade_create(lv_obj_t *parent)
{
    upgrade_page_runtime_create(&g_upgrade_runtime, parent, &g_upgrade_config,
        "Controller update", device_info_is_valid() ? device_info_main_app() : NULL);
}

void ui_page_14_main_upgrade_destroy(void)
{
    upgrade_page_runtime_destroy(&g_upgrade_runtime);
}
