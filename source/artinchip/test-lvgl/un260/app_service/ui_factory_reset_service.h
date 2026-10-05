#ifndef UN260_UI_FACTORY_RESET_SERVICE_H
#define UN260_UI_FACTORY_RESET_SERVICE_H
#include <stdbool.h>
typedef enum {
    UI_FACTORY_RESET_IDLE,
    UI_FACTORY_RESET_WAIT_CONTROLLER,
    UI_FACTORY_RESET_SAVING,
    UI_FACTORY_RESET_LOCAL_FAILED,
    UI_FACTORY_RESET_RESTART_REQUIRED
} ui_factory_reset_phase_t;
ui_factory_reset_phase_t ui_factory_reset_get_phase(void);
const char *ui_factory_reset_blocker(void);
bool ui_factory_reset_request(void);
bool ui_factory_reset_blocks_navigation(void);
/* The protocol dispatcher calls these only for its matching request result. */
void ui_factory_reset_on_controller_result(bool success);
void ui_factory_reset_on_controller_timeout(void);
bool ui_factory_reset_retry_local(void);
void ui_factory_reset_poll(void);
#endif
