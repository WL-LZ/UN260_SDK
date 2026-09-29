#ifndef UN260_APP_SERVICE_APP_UI_RUNTIME_H
#define UN260_APP_SERVICE_APP_UI_RUNTIME_H

#include <stdint.h>

void app_ui_runtime_init(void);
void app_ui_runtime_poll(uint32_t now_ms);

typedef enum {
    APP_UI_NOTICE_WORKSPACE_STORE,
    APP_UI_NOTICE_BATCH_SAVE,
    APP_UI_NOTICE_RECORD_STORE,
    APP_UI_NOTICE_PROFILE_APPLY
} app_ui_notice_operation_t;

/* Register only after a user-requested async service operation was accepted.
 * The global UI runtime owns its final notice even if the page is hidden. */
void app_ui_runtime_notice_started(app_ui_notice_operation_t operation,
                                   const char *detail);

#endif
