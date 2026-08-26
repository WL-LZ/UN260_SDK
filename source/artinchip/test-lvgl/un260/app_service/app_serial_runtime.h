#ifndef UN260_APP_SERVICE_APP_SERIAL_RUNTIME_H
#define UN260_APP_SERVICE_APP_SERIAL_RUNTIME_H

#include <stdbool.h>

bool app_serial_runtime_start(void);
void app_serial_runtime_stop(void);
bool app_serial_runtime_is_started(void);

#endif
