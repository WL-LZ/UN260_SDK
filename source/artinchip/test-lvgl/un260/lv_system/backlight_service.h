#ifndef UN260_BACKLIGHT_SERVICE_H
#define UN260_BACKLIGHT_SERVICE_H
#include <stdbool.h>
void backlight_service_init(void);
bool backlight_service_probe(void);
int backlight_service_max(void);
int backlight_service_level(void);
bool backlight_service_set(int level);
bool backlight_service_save(void);
/* Restore the platform device-tree default, including a saved percent for
 * the early renderer; only used by an acknowledged factory-reset recovery. */
bool backlight_service_restore_factory_default(void);
const char *backlight_service_device(void);
#endif
