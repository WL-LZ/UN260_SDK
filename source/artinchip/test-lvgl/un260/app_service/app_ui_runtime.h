#ifndef UN260_APP_SERVICE_APP_UI_RUNTIME_H
#define UN260_APP_SERVICE_APP_UI_RUNTIME_H

#include <stdint.h>

void app_ui_runtime_init(void);
void app_ui_runtime_poll(uint32_t now_ms);

#endif
