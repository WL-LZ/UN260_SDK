#ifndef TEST_NOTICE_SINK_H
#define TEST_NOTICE_SINK_H
#include <stdio.h>
#include <string.h>
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_system/ui_i18n.h"
/* View/service fixtures record the presentation boundary. Component animation,
 * queue and touch behavior are exercised by the dedicated notice tests. */
static unsigned test_notice_count;
static unsigned test_notice_dialog_holds;
static bool test_notice_visible;
static ui_notice_kind_t test_notice_kind;
static char test_notice_key[UI_NOTICE_KEY_CAPACITY];
static char test_notice_title[UI_NOTICE_TITLE_CAPACITY];
static char test_notice_detail[UI_NOTICE_DETAIL_CAPACITY];
void ui_notice_post(ui_notice_kind_t kind,const char *key,const char *title,const char *detail)
{
    test_notice_count++;test_notice_visible=true;test_notice_kind=kind;
    snprintf(test_notice_key,sizeof(test_notice_key),"%s",key?key:"");
    snprintf(test_notice_title,sizeof(test_notice_title),"%s",title?title:"");
    snprintf(test_notice_detail,sizeof(test_notice_detail),"%s",detail?detail:"");
}
void ui_notice_post_text(ui_notice_kind_t kind,const char *key,const char *title,const char *detail)
{ui_notice_post(kind,key,ui_tr(title),ui_tr(detail));}
void ui_notice_post_message(ui_notice_kind_t kind,const char *key,const char *title,const ui_message_t *message)
{char detail[UI_NOTICE_DETAIL_CAPACITY];ui_message_render(message,detail,sizeof(detail));ui_notice_post(kind,key,ui_tr(title),detail);}
void ui_notice_language_changed(void){}
void ui_notice_dismiss(const char *key)
{if(!key||!strcmp(key,test_notice_key))test_notice_visible=false;}
void ui_notice_clear(const char *key){ui_notice_dismiss(key);}
void ui_notice_init(void){}
void ui_notice_deinit(void){test_notice_visible=false;}
void ui_notice_set_suspended(uint32_t reason,bool suspended){(void)reason;(void)suspended;}
void ui_notice_dialog_acquire(void){test_notice_dialog_holds++;}
void ui_notice_dialog_release(void){if(test_notice_dialog_holds)test_notice_dialog_holds--;}
bool ui_notice_is_visible(void){return test_notice_visible;}
bool ui_notice_show(const ui_notice_config_t *config)
{if(!config)return false;ui_notice_post(config->kind,config->key,config->title,config->detail);return true;}
#endif
