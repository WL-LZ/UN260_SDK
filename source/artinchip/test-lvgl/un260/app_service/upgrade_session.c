#include "upgrade_session.h"
static upgrade_session_owner_t active_owner;
bool upgrade_session_begin(upgrade_session_owner_t owner)
{
    if (owner == UPGRADE_SESSION_NONE || active_owner != UPGRADE_SESSION_NONE) return false;
    active_owner = owner;
    return true;
}
void upgrade_session_end(upgrade_session_owner_t owner)
{
    if (owner == active_owner) active_owner = UPGRADE_SESSION_NONE;
}
upgrade_session_owner_t upgrade_session_owner(void)
{
    return active_owner;
}
