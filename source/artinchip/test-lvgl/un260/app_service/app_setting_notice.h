#ifndef APP_SETTING_NOTICE_H
#define APP_SETTING_NOTICE_H
#include <stdbool.h>
void app_setting_notice_init(void);
bool app_setting_notice_page_allowed(void);
bool app_setting_notice_allowed(const char *key);
/* Protocol owners call only after consuming a matching request. */
void app_setting_notice_result(const char *key, const char *title, bool success);
void app_setting_notice_timeout(const char *key, const char *title);
#endif
