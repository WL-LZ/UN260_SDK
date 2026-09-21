/* Hardware/dialog edges are captured, while the real per-button guard runs. */
#include "un260/lv_components/lv_settings.h"
static unsigned action_notices;
static char action_notice[256];
static void action_explanation(const char *reason)
{ ++action_notices;snprintf(action_notice,sizeof(action_notice),"%s",reason); }
void settings_detail_action_block(lv_obj_t *button,const char *reason)
{ lv_settings_action_block(button,reason,action_explanation); }
static bool action_blocked(lv_obj_t *button)
{
    assert(!lv_obj_has_state(button,LV_STATE_DISABLED));
    return lv_settings_action_block_reason(button)!=NULL;
}
bool ui_manager_suspend_to_home(void){ui_manager_switch(UI_PAGE_MAIN);return true;}
