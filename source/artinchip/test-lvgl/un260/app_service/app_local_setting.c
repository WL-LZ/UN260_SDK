#include "app_local_setting.h"
#include "app_setting_notice.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_core/page_01_main_layout.h"
#include "un260/gesture/gesture_service.h"
#include "un260/gesture/touch_feedback.h"
#include "un260/lv_components/ui_notice.h"

typedef bool (*commit_fn)(uint8_t, uint8_t);
static bool layout(uint8_t a,uint8_t b)
{ (void)b;page_01_main_layout_set_enabled(a!=0);return page_01_main_layout_is_enabled()==(a!=0); }
static bool gestures(uint8_t a,uint8_t b)
{ (void)b;return gesture_service_set_enabled(a!=0); }
static bool popup(uint8_t a,uint8_t b)
{ (void)b;return ui_state_popup_auto_set_enabled(a!=0); }
static bool touch(uint8_t a,uint8_t b)
{ (void)b;return touch_feedback_set_enabled(a!=0); }
static bool top(uint8_t a,uint8_t b)
{ (void)b;return ui_state_top_popup_set_enabled(a!=0); }
static bool order(uint8_t a,uint8_t b)
{ return ui_state_quick_order_swap(a,b); }
/* One definition binds execution to feedback; adding only a switch handler
 * cannot accidentally omit the completion path. */
static const struct {commit_fn commit;const char *key,*title;bool persistent;} settings[]={
    [APP_LOCAL_LAYOUT]={layout,"settings.layout",UI_N_("Layout edit"),false},
    [APP_LOCAL_GESTURES]={gestures,"settings.gestures",UI_N_("Side / two-finger gestures"),true},
    [APP_LOCAL_FAULT_POPUP]={popup,"settings.fault_popup",UI_N_("Popup"),true},
    [APP_LOCAL_TOUCH]={touch,"settings.touch",UI_N_("Touch guide"),true},
    [APP_LOCAL_TOP_POPUP]={top,"settings.top_popup",UI_N_("Top popup"),true},
    [APP_LOCAL_SHORTCUT_ORDER]={order,"settings.shortcut_order",UI_N_("Shortcut order"),true}
};
_Static_assert(sizeof(settings)/sizeof(settings[0])==APP_LOCAL_SETTING_COUNT,"Missing local setting policy");
bool app_local_setting_apply(app_local_setting_t setting,uint8_t a,uint8_t b)
{
    if((unsigned)setting>=APP_LOCAL_SETTING_COUNT||!settings[setting].commit)return false;
    bool show=app_setting_notice_page_allowed();
    bool ok=settings[setting].commit(a,b);
    if(show){
        const char *detail=ok?(settings[setting].persistent?UI_N_("Setting saved."):UI_N_("Confirmed")):
            UI_N_("Could not save. Try again.");
        ui_notice_post_text(ok?UI_NOTICE_SUCCESS:UI_NOTICE_ERROR,settings[setting].key,settings[setting].title,detail);
    }
    return ok;
}
