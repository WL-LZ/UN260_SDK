#ifndef UN260_APP_SERVICE_APP_SETTING_REPLY_H
#define UN260_APP_SERVICE_APP_SETTING_REPLY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_SETTING_REPLY_ACTION_NONE = 0,
    APP_SETTING_REPLY_ACTION_SCHEDULE_MODE_CLEAR = 1 << 0,
} app_setting_reply_action_t;

app_setting_reply_action_t app_setting_reply_handle_basic(uint8_t cmd,
                                                          const uint8_t *buf,
                                                          uint8_t len);
bool app_setting_reply_handle_detail(uint8_t cmd,
                                     const uint8_t *buf,
                                     uint8_t len);

#endif
