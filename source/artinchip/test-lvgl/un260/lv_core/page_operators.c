#include "page_cash_tools.h"
#include "page_19_history.h"
#include "page_03_menu.h"
#include "lv_page_manager.h"
#include "cash_workspace_style.h"
#include "un260/app_service/cash_workspace_access.h"
#include "un260/app_service/workspace_service.h"
#include "un260/app_service/app_ui_runtime.h"
#include "un260/storage/workspace_store.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_components/lv_pin_keypad.h"
#include "un260/lv_components/ui_notice.h"
#include "lv_port_indev.h"
#include "operator_initial.h"
#include "un260/lv_components/ui_scrollbar.h"
#include "un260/storage/cashbook_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum {OP_BACK=1,OP_HISTORY,OP_BOOK,OP_PERSONAL,OP_EDIT,OP_NEW,OP_SWITCH,OP_ENABLE,OP_PROTECT,OP_DISABLE,OP_CLOSE,OP_SAVE,OP_NAME,OP_QUICK,OP_QR,OP_SWITCH_CONFIRM,OP_PREFERENCES,OP_CURRENT,OP_PEOPLE_MODE,OP_PEOPLE_PREV,OP_PEOPLE_NEXT,OP_PROFILE_MODE,OP_PROFILE_PREV,OP_PROFILE_NEXT,OP_DELETE,OP_DELETE_CONFIRM,OP_DELETE_RETRY,OP_PROFILE=100,OP_SELECT=200};
static struct {lv_obj_t *root,*body,*overlay,*name_button,*identity;lv_timer_t *timer;lv_pin_keypad_t pin;workspace_model_t *draft;uint32_t selected,revision,deleting,edit_id;lv_obj_t *people_scroll;lv_coord_t people_y;char delete_reason[160];bool queued,saving,force,people_paged,profile_paged;unsigned people_page,profile_page;char feedback[160];} op;
static void delete_form(void);static void draw(void *);static void action(lv_event_t *);
static void queue(void){if(op.root&&!op.queued){op.queued=true;lv_async_call(draw,NULL);}}
static void note(const char *s){snprintf(op.feedback,sizeof(op.feedback),"%s",s);ui_notice_post_text(UI_NOTICE_WARNING,"workspace.tools",UI_N_("Operators"),s);queue();}
static lv_obj_t *button(lv_obj_t *p,int x,int y,int w,int h,const char *s,unsigned id,bool primary)
{return cw_button(p,x,y,w,h,s,primary?CW_BLUE:CW_PANEL,primary?0xFFFFFF:CW_INK,action,(void *)(uintptr_t)id);}
static void hint(lv_obj_t *p,int x,int y,int w,const char *s){cw_label(p,x,y,w,s,&lv_font_instrument_sans_medium_12,CW_MUTED);}
static const workspace_user_t *selected(void){return workspace_find((workspace_model_t *)workspace_store_get(),op.selected);}
static bool user_disabled(const workspace_model_t *m,uint32_t id)
{for(unsigned i=0;m&&i<m->user_count;++i)if(m->users[i].id==id)return m->disabled[i];return true;}
static void close_overlay(void){lv_pin_keypad_destroy(&op.pin);if(op.overlay)lv_obj_del(op.overlay);op.overlay=op.name_button=NULL;cash_access_cancel();}
static lv_obj_t *overlay(const char *title)
{close_overlay();op.overlay=cw_box(op.root,0,0,1280,400,0x1B2732,0);lv_obj_set_style_bg_opa(op.overlay,56,0);lv_obj_add_flag(op.overlay,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(op.overlay,action,LV_EVENT_CLICKED,(void *)OP_CLOSE);lv_obj_t *panel=cw_box(op.overlay,330,16,620,368,0xFFFFFF,20);lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);cw_label(panel,26,24,460,title,&lv_font_instrument_sans_semibold_20,CW_INK);button(panel,490,14,104,42,ui_tr("Cancel"),OP_CLOSE,false);return panel;}

static bool draft(void){op.edit_id=0;if(op.draft)free(op.draft);op.draft=malloc(sizeof(*op.draft));if(!op.draft)return false;*op.draft=*workspace_store_get();return true;}
static void save(void){if(!op.draft)return;if(!op.deleting&&!memcmp(op.draft,workspace_store_get(),sizeof(*op.draft))){free(op.draft);op.draft=NULL;op.edit_id=0;close_overlay();queue();return;}if(!(op.deleting?cash_workspace_delete(op.deleting):cash_workspace_save(op.draft,op.force))){note(UI_N_("Save was not accepted. Original settings remain."));return;}app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,UI_N_("Saving workspace..."));op.saving=true;op.revision=workspace_store_revision();if(op.edit_id&&!op.deleting)op.selected=op.edit_id;op.edit_id=0;op.deleting=0;op.feedback[0]=0;free(op.draft);op.draft=NULL;close_overlay();queue();}
static void pin_cancel(void *unused){(void)unused;op.deleting=0;close_overlay();if(op.draft){free(op.draft);op.draft=NULL;}queue();}
static void pin_confirm(const char *pin,void *unused){(void)unused;if(!cash_access_verify(CASH_ACCESS_WORKSPACE,pin)){lv_pin_keypad_clear(&op.pin);lv_pin_keypad_set_error(&op.pin,ui_tr("Incorrect PIN or temporarily locked. Try again later."));return;}save();}
static void authorize(bool force)
{op.force=force;const char *reason=workspace_service_switch_blocker();if(reason){note(reason);return;}if(!force&&!cash_access_required(CASH_ACCESS_WORKSPACE)){save();return;}close_overlay();lv_pin_keypad_create(&op.pin,op.root,215,18);lv_pin_keypad_config_t c={.title=ui_tr("Management authorization"),.prompt=ui_tr("Authorize this action without changing operator."),.confirm_cb=pin_confirm,.cancel_cb=pin_cancel,.compact=true};lv_pin_keypad_show(&op.pin,&c,"");}
static void edit_form(bool fresh)
{op.deleting=0;if(!draft())return;lv_obj_t *o=overlay(ui_tr(fresh?UI_N_("New operator"):UI_N_("Operator details")));op.edit_id=fresh?0:op.selected;const workspace_user_t *u=fresh?NULL:workspace_find(op.draft,op.edit_id);op.name_button=button(o,26,86,568,54,u?u->name:ui_tr("Enter name"),OP_NAME,false);hint(o,26,165,568,ui_tr("Historical names and stable IDs are retained."));hint(o,26,191,568,ui_tr("Operators with records are disabled, never deleted."));if(u){bool disabled=false;for(unsigned i=0;i<op.draft->user_count;++i)if(op.draft->users[i].id==u->id)disabled=op.draft->disabled[i];button(o,26,298,176,44,ui_tr(disabled?UI_N_("Restore"):UI_N_("Disable")),OP_DISABLE,false);button(o,216,298,176,44,ui_tr("Delete operator"),OP_DELETE,false);}button(o,434,298,160,44,ui_tr("Save"),OP_SAVE,true);}
static void delete_form(void)
{
    const workspace_user_t *u=selected();if(!u)return;
    const char *reason=cash_workspace_delete_blocker(op.deleting);
    snprintf(op.delete_reason,sizeof(op.delete_reason),"%s",reason?reason:"");
    lv_obj_t *o=overlay(ui_tr("Delete operator"));
    cw_label(o,26,86,568,u->name,&lv_font_instrument_sans_semibold_24,CW_INK);
    hint(o,26,142,568,ui_tr(reason?(user_disabled(workspace_store_get(),op.deleting)?UI_N_("Already disabled. Existing records are retained."):UI_N_("Disabling prevents future use and retains records.")):UI_N_("Deletes this operator and their personal presets.")));
    hint(o,26,169,568,ui_tr(reason?UI_N_("Disabling does not remove record links."):UI_N_("This cannot be undone. Other operators stay unchanged.")));
    lv_obj_t *message=cw_label(o,26,212,568,ui_tr(reason?reason:UI_N_("No linked records. You can delete this operator.")),&lv_font_instrument_sans_medium_15,CW_MUTED);
    lv_obj_set_height(message,56);lv_label_set_long_mode(message,LV_LABEL_LONG_WRAP);
    bool retry=cashbook_store_operator_reference(op.deleting)==CASHBOOK_REF_FAILED;
    button(o,26,298,160,44,ui_tr(retry?UI_N_("Retry"):UI_N_("Cancel")),retry?OP_DELETE_RETRY:OP_CLOSE,false);
    lv_obj_t *confirm=button(o,434,298,160,44,ui_tr("Delete"),OP_DELETE_CONFIRM,true);
    if(reason)lv_obj_add_state(confirm,LV_STATE_DISABLED);
}
static void name_value(const char *value,void *unused)
{(void)unused;if(!op.draft)return;if(!workspace_name_valid(value)){note(UI_N_("Use 1-24 characters, with no leading or trailing spaces."));return;}if(op.edit_id){workspace_user_t *u=workspace_find(op.draft,op.edit_id);if(u)snprintf(u->name,sizeof(u->name),"%s",value);}else if(!workspace_add_user(op.draft,value,&op.edit_id)){note(UI_N_("Use a unique name. Up to 8 operators are supported."));return;}workspace_user_t *u=workspace_find(op.draft,op.edit_id);if(u&&op.name_button){lv_label_set_text(lv_obj_get_child(op.name_button,0),u->name);}}
static void personal(void)
{const workspace_user_t *u=selected();if(!u)return;lv_obj_t *o=overlay(u->name);hint(o,26,68,568,ui_tr("Apply a saved profile explicitly; wait for the controller reply."));lv_obj_t *profiles=cw_box(o,26,94,568,96,0xFFFFFF,0);if(!op.profile_paged){lv_obj_add_flag(profiles,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_USER_4);lv_obj_set_scroll_dir(profiles,LV_DIR_VER);lv_port_indev_set_drag_obj(profiles,true);}unsigned first=op.profile_paged?op.profile_page*4:0;for(unsigned i=first;i<u->profile_count&&(!op.profile_paged||i<first+4);++i)button(profiles,((i-first)%2)*290,((i-first)/2)*48,278,42,u->profiles[i].name,OP_PROFILE+i,false);button(o,26,196,150,30,ui_tr(op.profile_paged?UI_N_("Page view"):UI_N_("Scroll view")),OP_PROFILE_MODE,false);button(o,478,196,52,30,"<",OP_PROFILE_PREV,false);button(o,542,196,52,30,">",OP_PROFILE_NEXT,false);button(o,26,232,278,44,ui_tr(u->quick_enabled?UI_N_("Quick controls: on"):UI_N_("Quick controls: off")),OP_QUICK,false);button(o,316,232,278,44,ui_tr(u->qr_after_count?UI_N_("QR after counting: on"):UI_N_("QR after counting: off")),OP_QR,false);button(o,26,296,568,44,ui_tr("Edit presets / Batch"),OP_PREFERENCES,false);}
static void action(lv_event_t *e)
{
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;unsigned id=(uintptr_t)lv_event_get_user_data(e);if(op.saving){note(UI_N_("Wait for the workspace save to finish."));return;}
    if(id==OP_BACK){op.deleting=0;close_overlay();if(!ui_manager_pop_page())ui_manager_switch(UI_PAGE_MAIN);return;}
    if(id==OP_CLOSE){op.deleting=0;close_overlay();if(op.draft){free(op.draft);op.draft=NULL;}queue();return;}
    const workspace_model_t *m=workspace_store_get();const workspace_user_t *u=selected();
    if(id>=OP_SELECT&&id<OP_SELECT+WORKSPACE_USERS){if(id-OP_SELECT<m->user_count)op.selected=m->users[id-OP_SELECT].id;op.feedback[0]=0;queue();return;}
    if(id==OP_HISTORY&&u){ui_page_19_history_open_operator(u->id);return;}
    if(id==OP_BOOK&&u){ui_cash_journal_filter_operator(u->id);ui_manager_push_page(UI_PAGE_CASH_JOURNAL);return;}
    if(id==OP_CURRENT){const workspace_user_t *active=workspace_active(m);if(active&&active->id){op.selected=active->id;for(unsigned i=0;i<m->user_count;++i)if(m->users[i].id==active->id){op.people_page=i/3;op.people_y=i*64;if(op.people_scroll)lv_obj_scroll_to_y(op.people_scroll,op.people_y,LV_ANIM_OFF);}}op.feedback[0]=0;queue();return;}
    if(id==OP_PREFERENCES){const workspace_user_t *active=workspace_active(m);if(!u||!active||u->id!=active->id){note(UI_N_("Switch to this operator before editing presets."));return;}close_overlay();page_03_menu_open_batch();return;}
    if(id==OP_PERSONAL){op.profile_page=0;personal();return;}
    if(id==OP_PROFILE_MODE||id==OP_PROFILE_PREV||id==OP_PROFILE_NEXT){if(!u)return;if(id==OP_PROFILE_MODE){op.profile_paged=!op.profile_paged;op.profile_page=0;}else{op.profile_paged=true;if(id==OP_PROFILE_PREV&&op.profile_page)--op.profile_page;if(id==OP_PROFILE_NEXT&&(op.profile_page+1)*4<u->profile_count)++op.profile_page;}personal();return;}
    if(id==OP_PEOPLE_MODE||id==OP_PEOPLE_PREV||id==OP_PEOPLE_NEXT){if(id==OP_PEOPLE_MODE){op.people_paged=!op.people_paged;op.people_page=0;op.people_y=0;if(op.people_scroll)lv_obj_scroll_to_y(op.people_scroll,0,LV_ANIM_OFF);}else{op.people_paged=true;if(id==OP_PEOPLE_PREV&&op.people_page)--op.people_page;if(id==OP_PEOPLE_NEXT&&(op.people_page+1)*3<m->user_count)++op.people_page;}queue();return;}
    if(id==OP_EDIT||id==OP_NEW){edit_form(id==OP_NEW);return;}
    if(id==OP_NAME){const workspace_user_t *n=op.draft&&op.edit_id?workspace_find(op.draft,op.edit_id):NULL;settings_detail_keyboard_show(ui_tr("Operator name"),n?n->name:"",24,SETTINGS_DETAIL_KEYBOARD_TEXT,name_value,NULL);return;}
    if(id==OP_DELETE||id==OP_DELETE_RETRY){if(!u)return;op.deleting=u->id;cashbook_store_check_operator(u->id);delete_form();return;}
    if(id==OP_DELETE_CONFIRM){if(!op.deleting)return;const char *reason=cash_workspace_delete_blocker(op.deleting);if(reason){delete_form();return;}authorize(false);return;}
    if(id==OP_SAVE){if(!op.edit_id||!op.draft||!workspace_model_valid(op.draft)){note(UI_N_("Check the operator name and settings."));return;}authorize(false);return;}
    if(id==OP_DISABLE){if(!op.draft||!op.selected)return;if(op.draft->operators_enabled&&op.selected==op.draft->active_id){note(UI_N_("Switch operator before disabling the current person."));return;}for(unsigned i=0;i<op.draft->user_count;++i)if(op.draft->users[i].id==op.selected)op.draft->disabled[i]=!m->disabled[i];authorize(false);return;}
    if(id==OP_PROTECT){if(!draft())return;op.draft->management_protected=!m->management_protected;authorize(true);return;}
    if(id==OP_ENABLE){if(!draft())return;if(!workspace_set_operators_enabled(op.draft,!m->operators_enabled)){note(UI_N_("Restore an operator before enabling operator mode."));return;}op.edit_id=0;authorize(false);return;}
    if(id==OP_SWITCH&&u){if(user_disabled(m,u->id)||u->id==m->active_id)return;const char *reason=workspace_service_switch_blocker();if(reason){note(reason);return;}if(!m->operators_enabled){note(UI_N_("Enable operators first."));return;}lv_obj_t *o=overlay(ui_tr("Switch operator?"));hint(o,26,92,568,u->name);hint(o,26,143,568,ui_tr("New records use this person; balances and old records stay unchanged."));hint(o,26,173,568,ui_tr("Unconfirmed calculator drafts will be cleared."));button(o,402,298,192,44,ui_tr("Confirm switch"),OP_SWITCH_CONFIRM,true);return;}
    if(id==OP_SWITCH_CONFIRM&&u){if(!m->operators_enabled||user_disabled(m,u->id)||u->id==m->active_id){close_overlay();queue();return;}if(!workspace_service_switch(u->id)){note(UI_N_("Switch was not accepted. Check the current task."));return;}app_ui_runtime_notice_started(APP_UI_NOTICE_OPERATOR_SWITCH,UI_N_("Switching workspace..."));op.saving=workspace_store_busy();op.revision=workspace_store_revision();close_overlay();queue();return;}
    if(id==OP_QUICK||id==OP_QR){if(!u||!draft())return;workspace_user_t *n=workspace_find(op.draft,u->id);if(id==OP_QUICK)n->quick_enabled=!n->quick_enabled;else n->qr_after_count=!n->qr_after_count;if(!workspace_store_save(op.draft)){note(UI_N_("Could not save preferences."));return;}app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,UI_N_("Saving workspace..."));op.saving=true;op.revision=workspace_store_revision();if(op.edit_id&&!op.deleting)op.selected=op.edit_id;op.edit_id=0;op.deleting=0;op.feedback[0]=0;free(op.draft);op.draft=NULL;close_overlay();queue();return;}
    if(id>=OP_PROFILE&&id<OP_PROFILE+WORKSPACE_PROFILES&&u){if(!m->operators_enabled||u->id!=m->active_id){note(UI_N_("Switch to this operator before applying a profile."));return;}unsigned i=id-OP_PROFILE;if(i<u->profile_count&&!workspace_service_apply(&u->profiles[i],app_clock_uptime_ms()))note(UI_N_("Finish counting, then apply the profile again."));close_overlay();return;}
}
static void draw(void *unused)
{
    (void)unused;op.queued=false;if(!op.root)return;if(op.people_scroll)op.people_y=lv_obj_get_scroll_y(op.people_scroll);op.people_scroll=NULL;lv_obj_clean(op.body);const workspace_model_t *m=workspace_store_get();if(!workspace_store_ready()||!m){hint(op.body,20,70,1100,ui_tr("The workspace is not available yet."));return;}if(!workspace_find((workspace_model_t *)m,op.selected)||!op.selected)op.selected=m->users[0].id;const workspace_user_t *u=selected();const workspace_user_t *active=workspace_active(m);if(op.identity){lv_obj_t *label=lv_obj_get_child(op.identity,0);const char *name=active&&active->id?active->name:ui_tr("Device workspace");if(strcmp(lv_label_get_text(label),name))lv_label_set_text(label,name);}
    lv_obj_t *people=cw_box(op.body,0,0,250,304,0xEDF1F6,17);lv_obj_t *scroll=cw_box(people,10,10,230,192,0xEDF1F6,0);if(!op.people_paged){op.people_scroll=scroll;lv_obj_add_flag(scroll,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_USER_4);lv_obj_set_scroll_dir(scroll,LV_DIR_VER);lv_port_indev_set_drag_obj(scroll,true);}
    op.people_page=LV_MIN(op.people_page,(m->user_count-1)/3);unsigned first=op.people_paged?op.people_page*3:0;for(unsigned i=first;i<m->user_count&&(!op.people_paged||i<first+3);++i){const workspace_user_t *p=&m->users[i];lv_obj_t *row=button(scroll,0,(i-first)*64,230-UI_SCROLLBAR_GUTTER,60,"",OP_SELECT+i,false);lv_obj_set_style_bg_color(row,lv_color_hex(p->id==op.selected?0xFFFFFF:0xEDF1F6),0);lv_obj_t *avatar=cw_box(row,12,12,39,39,0xEBE5F7,12);char first[5];operator_initial(p->name,first);lv_obj_t *number=cw_label(avatar,0,0,39,first,&lv_font_instrument_sans_semibold_18,0x8059CA);lv_obj_set_style_text_align(number,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(number);cw_label(row,64,12,158-UI_SCROLLBAR_GUTTER,p->name,&lv_font_instrument_sans_medium_15,CW_INK);hint(row,64,36,158-UI_SCROLLBAR_GUTTER,ui_tr(m->disabled[i]?UI_N_("Disabled"):m->operators_enabled&&p->id==m->active_id?UI_N_("Current operator"):UI_N_("Available")));}
    if(op.people_scroll){lv_obj_update_layout(scroll);lv_obj_scroll_to_y(scroll,op.people_y,LV_ANIM_OFF);}
    button(people,12,208,122,30,ui_tr(op.people_paged?UI_N_("Page view"):UI_N_("Scroll view")),OP_PEOPLE_MODE,false);button(people,142,208,44,30,"<",OP_PEOPLE_PREV,false);button(people,194,208,44,30,">",OP_PEOPLE_NEXT,false);
    button(people,12,248,226,42,ui_tr("Add operator"),OP_NEW,false);
    lv_obj_t *detail=cw_box(op.body,268,0,622,304,0xFFFFFF,17);cw_label(detail,20,24,500,u?u->name:"",&lv_font_instrument_sans_semibold_18,CW_INK);hint(detail,20,57,580,ui_tr("Viewing this person. Use Switch operator to start using them."));const char *title[]={UI_N_("Counting history"),UI_N_("Cash entries"),UI_N_("Personal preferences"),UI_N_("Operator details")};const char *sub[]={UI_N_("Counts performed by this operator"),UI_N_("Cash confirmed by this operator"),UI_N_("Profiles / QR / Quick controls"),UI_N_("Name and enabled state")};const unsigned ids[]={OP_HISTORY,OP_BOOK,OP_PERSONAL,OP_EDIT};for(unsigned i=0;i<4;++i){lv_obj_t *card=button(detail,20+(i%2)*296,94+(i/2)*77,284,67,"",ids[i],false);cw_label(card,16,13,252,ui_tr(title[i]),&lv_font_instrument_sans_medium_15,CW_INK);hint(card,16,40,252,ui_tr(sub[i]));}hint(detail,20,270,390,ui_tr(!m->operators_enabled?UI_N_("Enable operators in device management."):user_disabled(m,op.selected)?UI_N_("Restore this operator in Operator details."):UI_N_("New records use the current operator.")));const char *switch_title=!m->operators_enabled?UI_N_("Operators off"):user_disabled(m,op.selected)?UI_N_("Disabled"):m->active_id==op.selected?UI_N_("Current operator"):UI_N_("Switch operator");lv_obj_t *switch_button=button(detail,454,255,148,42,ui_tr(switch_title),OP_SWITCH,true);if(!m->operators_enabled||user_disabled(m,op.selected)||m->active_id==op.selected)lv_obj_add_state(switch_button,LV_STATE_DISABLED);
    lv_obj_t *settings=cw_box(op.body,908,0,324,304,CW_PANEL,17);cw_label(settings,20,20,284,ui_tr("Device management"),&lv_font_instrument_sans_semibold_18,CW_INK);
    hint(settings,20,47,284,ui_tr("Applies to all operators"));
    cw_label(settings,20,80,224,ui_tr("Enable operators"),&lv_font_instrument_sans_medium_15,CW_INK);hint(settings,20,104,235,ui_tr("Off: new records use the device workspace."));lv_obj_t *enabled=lv_settings_toggle(settings,258,81,m->operators_enabled,action,(void *)OP_ENABLE);lv_obj_set_style_bg_color(enabled,lv_color_hex(m->operators_enabled?CW_BLUE:0xCDD7DD),0);
    cw_label(settings,20,142,235,ui_tr("Management protection"),&lv_font_instrument_sans_medium_15,CW_INK);hint(settings,20,166,235,ui_tr("Authorize sensitive changes only."));lv_obj_t *protected=lv_settings_toggle(settings,258,143,m->management_protected,action,(void *)OP_PROTECT);lv_obj_set_style_bg_color(protected,lv_color_hex(m->management_protected?CW_BLUE:0xCDD7DD),0);
    cw_box(settings,20,207,284,1,0xE2E8EF,0);hint(settings,20,224,284,ui_tr("Balances and saved records stay unchanged."));
    lv_obj_t *feedback=cw_label(settings,20,251,284,ui_tr(op.saving?UI_N_("Saving workspace..."):op.feedback),&lv_font_instrument_sans_medium_12,CW_MUTED);lv_label_set_long_mode(feedback,LV_LABEL_LONG_WRAP);lv_obj_set_height(feedback,45);
}
static void tick(lv_timer_t *t){(void)t;
    if(op.deleting&&op.overlay){if(cashbook_store_operator_reference(op.deleting)==CASHBOOK_REF_UNKNOWN&&!cashbook_store_busy())cashbook_store_check_operator(op.deleting);const char *reason=cash_workspace_delete_blocker(op.deleting);if(strcmp(op.delete_reason,reason?reason:""))delete_form();}
if(op.revision!=workspace_store_revision()){op.revision=workspace_store_revision();op.saving=workspace_store_busy();queue();}if(op.saving&&!workspace_store_busy()){op.saving=false;if(!workspace_store_last_success())snprintf(op.feedback,sizeof(op.feedback),"%s",workspace_store_message());queue();}}
static void deleted(lv_event_t *e){if(lv_event_get_target(e)!=op.root)return;lv_async_call_cancel(draw,NULL);if(op.timer)lv_timer_del(op.timer);op.timer=NULL;op.root=op.body=op.overlay=op.identity=op.people_scroll=NULL;memset(&op.pin,0,sizeof(op.pin));if(op.draft)free(op.draft);op.draft=NULL;op.deleting=op.edit_id=0;op.queued=false;cash_access_cancel();}
void ui_operators_create(lv_obj_t *parent){if(op.root)return;op.root=cw_root(parent);lv_nav_back_text_button_create(op.root,24,15,ui_message_font(&lv_font_instrument_sans_medium_16),action,(void *)OP_BACK);cw_label(op.root,114,23,360,ui_tr("Operators"),&lv_font_instrument_sans_semibold_24,CW_INK);hint(op.root,900,29,164,ui_tr("Current operator"));const workspace_user_t *active=workspace_active(workspace_store_get());op.identity=button(op.root,1076,15,180,42,active&&active->id?active->name:ui_tr("Device workspace"),OP_CURRENT,false);lv_obj_set_style_bg_color(op.identity,lv_color_hex(0xF0EDF9),0);lv_obj_set_style_text_color(lv_obj_get_child(op.identity,0),lv_color_hex(0x7358AD),0);op.body=cw_box(op.root,24,76,1232,304,0xFFFFFF,0);lv_obj_set_style_bg_opa(op.body,LV_OPA_TRANSP,0);lv_obj_add_event_cb(op.root,deleted,LV_EVENT_DELETE,NULL);op.revision=workspace_store_revision();op.timer=lv_timer_create(tick,200,NULL);draw(NULL);}
void ui_operators_destroy(void){if(!op.root)return;settings_detail_keyboard_hide();close_overlay();lv_obj_del(op.root);}
