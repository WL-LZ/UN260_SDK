#include "page_01_multi.h"
#include "page_02_list.h"
#include "lv_page_manager.h"
#include "lv_page_event.h"
#include "un260/lv_components/ui_multi_detail.h"
#include "un260/gesture/gesture_service.h"
static ui_multi_detail_t *view;
static bool gesture(gesture_action_t action)
{return (action==GESTURE_ACTION_EXIT_PAGE||action==GESTURE_ACTION_HOME)&&page_01_multi_back();}
static void open_list(int index,int tab,void *ctx)
{(void)ctx;page_02_list_multi_open(index,tab);ui_manager_push_page(UI_PAGE_LIST);}
lv_obj_t *page_01_multi_create(lv_obj_t *parent)
{
    view=ui_multi_detail_create(parent,false,open_list,NULL);
    lv_obj_t *target=ui_multi_detail_currency_target(view);
    if(target)lv_obj_add_event_cb(target,page_01_curr_btn_event_cb,LV_EVENT_CLICKED,NULL);
    return ui_multi_detail_object(view);
}
void page_01_multi_destroy(void)
{gesture_service_clear_page_policy(UI_PAGE_MAIN);ui_multi_detail_destroy(view);view=NULL;}
void page_01_multi_refresh(void){ui_multi_detail_refresh(view);}
void page_01_multi_visible(bool visible)
{
    ui_multi_detail_visible(view,visible);
    if(visible)gesture_service_set_page_policy(UI_PAGE_MAIN,NULL,gesture);
    else gesture_service_clear_page_policy(UI_PAGE_MAIN);
}
bool page_01_multi_back(void){return ui_multi_detail_back(view);}
lv_obj_t *page_01_multi_scroll(void){return ui_multi_detail_scroll(view);}
