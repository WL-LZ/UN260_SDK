#ifndef UN260_STARTUP_DEVICES_H
#define UN260_STARTUP_DEVICES_H
#include <stdbool.h>
/* Worker-only on theme C; legacy themes call before their input setup. */
bool startup_devices_prepare(void);
#endif
