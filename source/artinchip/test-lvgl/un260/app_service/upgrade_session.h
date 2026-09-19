#ifndef UPGRADE_SESSION_H
#define UPGRADE_SESSION_H
#include <stdbool.h>
typedef enum {
    UPGRADE_SESSION_NONE,
    UPGRADE_SESSION_CONTROLLER,
    UPGRADE_SESSION_IMAGE,
    UPGRADE_SESSION_UI
} upgrade_session_owner_t;
/* Main-thread service: a timeout or view teardown never proves completion.
 * Release only on a terminal board reply, a reaped updater, or failed dispatch. */
bool upgrade_session_begin(upgrade_session_owner_t owner);
void upgrade_session_end(upgrade_session_owner_t owner);
upgrade_session_owner_t upgrade_session_owner(void);
#endif
