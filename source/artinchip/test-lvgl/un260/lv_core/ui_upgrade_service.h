#ifndef UI_UPGRADE_SERVICE_H
#define UI_UPGRADE_SERVICE_H

#include <stdbool.h>

typedef enum {
    UI_UPGRADE_STAGE_NONE = 0,
    UI_UPGRADE_STAGE_VERIFY,
    UI_UPGRADE_STAGE_WRITE,
    UI_UPGRADE_STAGE_FINISH,
    UI_UPGRADE_STAGE_SUCCESS,
    UI_UPGRADE_STAGE_FAIL
} ui_upgrade_stage_t;

typedef enum {
    UI_UPGRADE_PACKAGE_HASH_NOT_CHECKED = 0,
    UI_UPGRADE_PACKAGE_HASH_MATCH,
    UI_UPGRADE_PACKAGE_HASH_DIFFERENT,
    UI_UPGRADE_PACKAGE_HASH_ERROR
} ui_upgrade_package_hash_status_t;

typedef enum {
    UI_UPGRADE_START_OK = 0,
    UI_UPGRADE_START_BUSY,
    UI_UPGRADE_START_SCRIPT_NOT_FOUND,
    UI_UPGRADE_START_PACKAGE_NOT_READY,
    UI_UPGRADE_START_STATUS_CLEANUP_FAILED,
    UI_UPGRADE_START_FORK_FAILED
} ui_upgrade_start_result_t;

typedef struct {
    bool usb_present;
    bool usb_mounted;
    bool package_found;
    ui_upgrade_package_hash_status_t package_hash_status;
} ui_upgrade_detect_info_t;

typedef struct {
    bool running;
    bool finished;
    bool success;
    int progress;
    ui_upgrade_stage_t stage;
    char step_text[64];
    char result_text[128];
} ui_upgrade_service_status_t;

void ui_upgrade_service_reset(void);
void ui_upgrade_service_detect(ui_upgrade_detect_info_t* info);
ui_upgrade_start_result_t ui_upgrade_service_start(void);
void ui_upgrade_service_poll(ui_upgrade_service_status_t* status);
void ui_upgrade_service_reboot(void);

#endif
