#ifndef TEST_MAINTENANCE_STATE_H
#define TEST_MAINTENANCE_STATE_H
#include <stdbool.h>

/* Device-maintenance services are peripheral to these owner tests. Flags
 * retain control of each gate; no production navigation policy is copied. */
static bool host_factory_reset_blocked;
static bool host_touch_calibration_busy;
bool ui_factory_reset_blocks_navigation(void) { return host_factory_reset_blocked; }
bool touch_calibration_busy(void) { return host_touch_calibration_busy; }
#endif
