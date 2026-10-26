/* Real impeller setting service, protocol owner and model; UI effects observed. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/app_service/setting_service.h"
#include "un260/app_service/app_setting_reply.h"
#include "un260/app_service/app_setting_runtime.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/print/print_config.h"
#include "un260/serial_number/serial_number.h"
#include "un260/currency/currency_service.h"
#include "un260/diagnostic/diagnostic.h"
#include "un260/boot/boot_service.h"
#include "un260/lv_components/fault_guide/fault_guide_catalog.h"

static uint64_t now=1000;
static unsigned sends,accepted[256],updates,visible_updates,successes,failures,timeouts,warnings,clears;
static unsigned sound_acks,gate_replies;
static uint8_t sound_status,last_tx[32];
static int last_tx_len;
static bool fail_send,page_alive=true,queued_reply;
uint64_t app_clock_monotonic_ms(void){return now;}
uint32_t app_clock_uptime_ms(void){return (uint32_t)now;}
void uart_debug_printf(const char *format,...){(void)format;}
int protocol_send(uint8_t command,const uint8_t *payload,uint16_t size)
{
 ++sends;last_tx_len=protocol_frame_build(last_tx,sizeof(last_tx),command,payload,size);
 assert(last_tx_len>0);
 return fail_send?-1:last_tx_len;
}
static void request_accepted(uint8_t command){++accepted[command];}
static setting_impeller_result_t page_result;
static void ui_page_40_set_impeller_on_update(const setting_impeller_result_t *result)
{assert(result);page_result=*result;++updates;if(page_alive)++visible_updates;}
static void app_setting_notice_result(const char *key,const char *title,bool success)
{assert(!strcmp(key,"settings.impeller")&&!strcmp(title,"Impeller motor speed"));if(success)++successes;else ++failures;}
static void app_setting_notice_timeout(const char *key,const char *title)
{assert(!strcmp(key,"settings.impeller")&&!strcmp(title,"Impeller motor speed"));++timeouts;}
static bool app_setting_notice_allowed(const char *key){(void)key;return page_alive;}
enum {UI_NOTICE_WARNING=1};
static void ui_notice_post_text(int kind,const char *key,const char *title,const char *body)
{assert(kind==UI_NOTICE_WARNING);assert(key&&title&&body);++warnings;}
static void ui_notice_clear(const char *key){assert(key);++clears;}
static void protocol_tx_gate_on_reply(uint8_t command){(void)command;++gate_replies;}
static void buzzer_service_on_reply(uint8_t status){++sound_acks;sound_status=status;}
static bool pending_reply(uint8_t command,uint64_t started,uint64_t deadline)
{assert(command==0x4B&&deadline-started==800);return queued_reply;}
#define UI_N_(s) s

/* Unrelated setting branches remain idle. No impeller owner/model function is stubbed. */
#define setting_reply_handle_reject_pocket(b,n) ((void)(b),(void)(n))
#define setting_reply_handle_double_note(b,n) ((void)(b),(void)(n))
#define setting_reply_handle_serial_number(b,n) ((void)(b),(void)(n))
#define setting_reply_handle_print(b,n) ((void)(b),(void)(n))
#define setting_reply_handle_flap(b,n) ((void)(b),(void)(n))
#define app_setting_runtime_handle_basic_reply(c,b,n) ((void)(c),(void)(b),(void)(n))
#define ui_factory_reset_on_controller_result(v) ((void)(v))
#define ui_page_30_set_factory_on_reply(v) ((void)(v))
#define ui_page_27_set_cfd_level_on_info(b,n) ((void)(b),(void)(n))
#define ui_page_26_set_aging_on_reply(v) ((void)(v))
#define ui_factory_reset_poll() ((void)0)
#define boot_service_get_stage() BOOT_STAGE_DONE
#define diagnostic_calibration_get_snapshot(p) memset((p),0,sizeof(*(p)))
#define work_mode_service_poll(t,b) ((void)(t),(void)(b))
#define app_command_runtime_count_start_busy() false
#define work_mode_service_take_failure() false
#define work_mode_service_status_msgid() ""
#define count_mode_service_mode_result(a,b) ((void)(a),(void)(b))
#define app_add_runtime_waiting_setting() false
#define app_add_runtime_setting_timeout() ((void)0)
#define page_01_main_refresh_start_state() ((void)0)
#define page_03_update_menu_button_states_refresh() ((void)0)
#define page_03_batch_set_result(a,b) ((void)(a),(void)(b))
#define ui_page_22_set_double_note_on_reply(p) ((void)(p))
#define ui_page_23_set_flap_on_reply(p) ((void)(p))
#define ui_page_24_set_reject_pocket_on_reply(p) ((void)(p))
#define serial_number_state_confirm(a,b) ((void)(a),(void)(b))
#define ui_page_25_set_serial_number_on_reply(a,b) ((void)(a),(void)(b))
#define count_mode_service_currency_result(p) ((void)(p))
#define page_07_curr_apply_switch_result(p) ((void)(p))
#define cfd_service_take_query_timeout() false
#define cfd_service_take_update_timeout() false
#define ui_page_27_set_cfd_level_on_request_failed() ((void)0)
#define ui_page_20_set_print_is_saving() false
#define ui_page_20_set_print_on_reply(p) ((void)(p))
#define ui_page_26_set_aging_on_timeout() ((void)0)
#define ui_factory_reset_on_controller_timeout() ((void)0)
#define ui_page_30_set_factory_on_timeout() ((void)0)
#define data_collection_request_take_timeout(t) ((void)(t),false)
#define page_06_data_collection_refresh() ((void)0)
bool serial_number_service_take_timeout(serial_number_setting_result_t *p){(void)p;return false;}
bool currency_service_take_switch_timeout(currency_switch_result_t *p){(void)p;return false;}
bool print_config_take_timeout(print_config_request_result_t *p){(void)p;return false;}
#define APP_SETTING_MODE_CLEAR_DELAY_MS 120
static bool g_mode_clear_scheduled,g_mode_clear_due;
static uint32_t g_mode_clear_tick;
#include "impeller_runtime_under_test.inc"

#define CHECK(v) do{if(!(v)){fprintf(stderr,"FAIL %s:%d %s value=%u known=%d pending=%d sends=%u updates=%u\n",__func__,__LINE__,#v,machine_state_impeller_speed(),machine_state_impeller_speed_known(),setting_service_impeller_speed_is_pending(),sends,updates);return false;}}while(0)
static bool receive(uint8_t command,const uint8_t *payload,unsigned n)
{
 uint8_t frame[32];int len=protocol_frame_build(frame,sizeof(frame),command,payload,n);
 assert(len>0);frame[len-1]=0xA5; /* RX tail is not a documented checksum here. */
 assert(protocol_frame_is_valid(frame,len));
 app_serial_runtime_observe_reply(frame,(uint16_t)len);
 return app_setting_runtime_handle_reply(command,frame,(uint8_t)len);
}
static bool read_value(unsigned value){uint8_t p[]={1,(uint8_t)value};return receive(0x4B,p,2);}
static bool status(unsigned value){uint8_t p[]={(uint8_t)value};return receive(0x4B,p,1);}
static void poll(void){app_setting_runtime_poll((uint32_t)now);}
static bool tx_is(unsigned sub,unsigned value,unsigned len)
{
 return last_tx_len==(int)len && last_tx[0]==0xFD&&last_tx[1]==0xDF&&last_tx[2]==len&&
 last_tx[3]==0x4B&&last_tx[4]==sub&&(len!=7||last_tx[5]==value)&&last_tx[len-1]==0x0A;
}
static bool default_state(void)
{CHECK(machine_state_impeller_speed()==30&&!machine_state_impeller_speed_known());CHECK(!setting_service_impeller_speed_is_pending()&&sends==0);return true;}
static bool read_range(void)
{
 const unsigned values[]={0,30,50};
 for(unsigned i=0;i<3;i++){
  CHECK(setting_service_request_impeller_speed_read());CHECK(tx_is(1,0,6));
  CHECK(setting_service_impeller_speed_is_pending());CHECK(read_value(values[i]));
  CHECK(machine_state_impeller_speed()==values[i]&&machine_state_impeller_speed_known());
  CHECK(!setting_service_impeller_speed_is_pending()&&updates==i+1);
 }
 CHECK(sends==3&&accepted[0x4B]==3&&successes==0&&failures==0);return true;
}
static bool write_range(void)
{
 const unsigned values[]={15,30,50};
 for(unsigned i=0;i<3;i++){
  unsigned old=machine_state_impeller_speed();
  CHECK(setting_service_request_impeller_speed(values[i]));CHECK(tx_is(2,values[i],7));
  CHECK(machine_state_impeller_speed()==old&&setting_service_impeller_speed_is_pending());
  CHECK(status(2));CHECK(machine_state_impeller_speed()==values[i]&&machine_state_impeller_speed_known());
  CHECK(!setting_service_impeller_speed_is_pending());
 }
 CHECK(sends==3&&successes==3&&updates==3);return true;
}
static bool out_of_range(void)
{
 CHECK(!setting_service_request_impeller_speed(0)&&!setting_service_request_impeller_speed(14));
 CHECK(!setting_service_request_impeller_speed(51)&&!setting_service_request_impeller_speed(255));
 CHECK(sends==0&&!setting_service_impeller_speed_is_pending());
 CHECK(setting_service_request_impeller_speed_read());
 CHECK(read_value(51)&&read_value(255));
 CHECK(setting_service_impeller_speed_is_pending()&&updates==0&&!machine_state_impeller_speed_known());
 CHECK(read_value(50)&&machine_state_impeller_speed()==50);return true;
}
static bool operation_isolation(void)
{
 CHECK(setting_service_request_impeller_speed_read());CHECK(status(2));
 CHECK(setting_service_impeller_speed_is_pending()&&!machine_state_impeller_speed_known()&&updates==0);
 CHECK(read_value(20));CHECK(machine_state_impeller_speed()==20);
 CHECK(setting_service_request_impeller_speed(40));CHECK(read_value(0));
 CHECK(setting_service_impeller_speed_is_pending()&&machine_state_impeller_speed()==20&&updates==1);
 CHECK(status(2));CHECK(machine_state_impeller_speed()==40&&updates==2);return true;
}
static bool unowned_reply(void)
{
 CHECK(status(2)&&status(3)&&read_value(50));
 CHECK(machine_state_impeller_speed()==30&&!machine_state_impeller_speed_known()&&updates==0);
 CHECK(setting_service_request_impeller_speed(40)&&status(2));unsigned old=updates;
 CHECK(status(2)&&status(3)&&read_value(0));
 CHECK(machine_state_impeller_speed()==40&&machine_state_impeller_speed_known()&&updates==old&&successes==1);
 return true;
}
static bool explicit_write_failure(void)
{
 machine_state_confirm_impeller_speed(30);
 CHECK(setting_service_request_impeller_speed(40)&&status(3));
 CHECK(machine_state_impeller_speed()==30&&machine_state_impeller_speed_known());
 CHECK(!setting_service_impeller_speed_is_pending()&&failures==1&&successes==0&&updates==1);
 CHECK(setting_service_request_impeller_speed(30)&&sends==1);
 CHECK(setting_service_request_impeller_speed(40)&&status(2)&&machine_state_impeller_speed()==40);return true;
}
static bool read_failure(void)
{
 machine_state_confirm_impeller_speed(40);
 CHECK(setting_service_request_impeller_speed_read()&&status(3));
 CHECK(machine_state_impeller_speed()==40&&!machine_state_impeller_speed_known());
 CHECK(!setting_service_impeller_speed_is_pending()&&warnings==0&&timeouts==0&&failures==0&&updates==1&&page_result.reading&&!page_result.success&&!page_result.timeout);
 CHECK(setting_service_request_impeller_speed(40)&&sends==2&&status(2));
 CHECK(machine_state_impeller_speed_known());return true;
}
static bool send_failure(void)
{
 machine_state_confirm_impeller_speed(30);fail_send=true;
 CHECK(!setting_service_request_impeller_speed_read());
 CHECK(!setting_service_request_impeller_speed(40));
 CHECK(!setting_service_impeller_speed_is_pending()&&machine_state_impeller_speed_known());
 CHECK(machine_state_impeller_speed()==30&&accepted[0x4B]==0&&updates==0);
 fail_send=false;CHECK(setting_service_request_impeller_speed(40)&&status(2));
 CHECK(accepted[0x4B]==1&&machine_state_impeller_speed()==40);return true;
}
static bool write_timeout(void)
{
 machine_state_confirm_impeller_speed(30);
 CHECK(setting_service_request_impeller_speed(40));
 now+=799;poll();CHECK(setting_service_impeller_speed_is_pending()&&timeouts==0);
 now++;poll();CHECK(!setting_service_impeller_speed_is_pending()&&timeouts==1&&updates==1);
 CHECK(!page_result.reading&&page_result.timeout&&!page_result.success&&successes==0&&failures==0);
 CHECK(machine_state_impeller_speed()==30&&!machine_state_impeller_speed_known());
 poll();CHECK(timeouts==1&&updates==1);
 CHECK(status(2)&&!machine_state_impeller_speed_known()&&machine_state_impeller_speed()==30);
 CHECK(setting_service_request_impeller_speed(30)&&sends==2&&status(2));
 CHECK(machine_state_impeller_speed_known());return true;
}
static bool read_timeout(void)
{
 machine_state_confirm_impeller_speed(20);CHECK(setting_service_request_impeller_speed_read());
 now+=800;poll();CHECK(timeouts==0&&warnings==0&&failures==0&&updates==1&&!setting_service_impeller_speed_is_pending());
 CHECK(page_result.reading&&page_result.timeout&&!page_result.success);
 poll();CHECK(updates==1&&timeouts==0&&warnings==0);
 CHECK(read_value(50)&&updates==1&&machine_state_impeller_speed()==20);
 CHECK(machine_state_impeller_speed()==20&&!machine_state_impeller_speed_known());
 CHECK(setting_service_request_impeller_speed_read()&&read_value(0));return true;
}
static bool result_arrival_deadline(void)
{
 CHECK(setting_service_request_impeller_speed(40));now+=900;
 protocol_request_result_context_begin(1799);CHECK(status(2));protocol_request_result_context_end();
 CHECK(machine_state_impeller_speed()==40&&successes==1);
 CHECK(setting_service_request_impeller_speed(50));now+=800;
 protocol_request_result_context_begin(2700);CHECK(status(2));protocol_request_result_context_end();
 CHECK(setting_service_impeller_speed_is_pending()&&machine_state_impeller_speed()==40&&successes==1);
 poll();CHECK(!machine_state_impeller_speed_known()&&timeouts==1);
 return true;
}
static bool queued_reply_protection(void)
{
 protocol_request_set_pending_reply_probe(pending_reply);CHECK(setting_service_request_impeller_speed_read());
 queued_reply=true;now+=1000;poll();
 CHECK(setting_service_impeller_speed_is_pending()&&timeouts==0);
 protocol_request_result_context_begin(1799);CHECK(read_value(50));protocol_request_result_context_end();
 queued_reply=false;poll();CHECK(machine_state_impeller_speed()==50&&machine_state_impeller_speed_known()&&timeouts==0);
 return true;
}
static bool same_value(void)
{
 CHECK(setting_service_request_impeller_speed(30)&&sends==1);
 CHECK(status(2)&&machine_state_impeller_speed_known());
 CHECK(setting_service_request_impeller_speed(30)&&sends==1&&!setting_service_impeller_speed_is_pending());
 CHECK(accepted[0x4B]==1&&updates==1);
 CHECK(setting_service_request_impeller_speed_read());CHECK(!setting_service_request_impeller_speed(30));
 CHECK(sends==2&&setting_service_impeller_speed_is_pending());
 CHECK(read_value(30));return true;
}
static bool pending_is_local(void)
{
 CHECK(setting_service_request_impeller_speed_read());
 CHECK(!setting_service_request_impeller_speed_read()&&!setting_service_request_impeller_speed(40));
 CHECK(sends==1&&setting_service_request_beep(false)&&sends==2);
 CHECK(setting_service_impeller_speed_is_pending());
 bool target=true;CHECK(setting_service_take_beep_result(&target)&&!target);
 CHECK(read_value(30)&&!setting_service_impeller_speed_is_pending());
 return true;
}
static bool page_detached_reply(void)
{
 CHECK(setting_service_request_impeller_speed(50));page_alive=false;
 CHECK(status(2));CHECK(machine_state_impeller_speed()==50&&machine_state_impeller_speed_known());
 CHECK(!setting_service_impeller_speed_is_pending()&&updates==1&&visible_updates==0);
 page_alive=true;CHECK(setting_service_request_impeller_speed_read()&&read_value(20));
 CHECK(machine_state_impeller_speed()==20&&visible_updates==1);return true;
}
static bool page_detached_timeout(void)
{
 CHECK(setting_service_request_impeller_speed(50));page_alive=false;now+=800;poll();
 CHECK(!setting_service_impeller_speed_is_pending()&&!machine_state_impeller_speed_known());
 CHECK(machine_state_impeller_speed()==30&&updates==1&&visible_updates==0);
 page_alive=true;CHECK(setting_service_request_impeller_speed(30)&&status(2));
 CHECK(machine_state_impeller_speed_known());return true;
}
static bool read_detached_timeout(void)
{
 machine_state_confirm_impeller_speed(20);
 CHECK(setting_service_request_impeller_speed_read());page_alive=false;now+=800;poll();
 CHECK(updates==1&&visible_updates==0&&timeouts==0&&warnings==0&&failures==0);
 CHECK(page_result.reading&&page_result.timeout&&!machine_state_impeller_speed_known());
 CHECK(read_value(50)&&updates==1&&machine_state_impeller_speed()==20);
 page_alive=true;CHECK(setting_service_request_impeller_speed_read()&&read_value(30));
 CHECK(visible_updates==1&&machine_state_impeller_speed()==30&&machine_state_impeller_speed_known());
 return true;
}
static bool cancel_owner(void)
{
 machine_state_confirm_impeller_speed(20);CHECK(setting_service_request_impeller_speed(50));
 setting_service_cancel_all();
 CHECK(!setting_service_impeller_speed_is_pending()&&!machine_state_impeller_speed_known());
 CHECK(status(2)&&machine_state_impeller_speed()==20&&updates==0);
 CHECK(setting_service_request_impeller_speed_read()&&read_value(30));return true;
}
static bool invalid_frames(void)
{
 CHECK(setting_service_request_impeller_speed_read());
 uint8_t b[9]={0xFD,0xDF,7,0x4B,1,50,0xA5};
 for(unsigned n=0;n<=9;n++){if(n==7)continue;b[2]=(uint8_t)n;app_setting_runtime_handle_reply(0x4B,b,(uint8_t)n);}
 b[2]=7;b[3]=0x4A;app_setting_runtime_handle_reply(0x4B,b,7);
 b[3]=0x4B;b[2]=6;app_setting_runtime_handle_reply(0x4B,b,7);
 CHECK(!app_setting_reply_handle_detail(0x4B,NULL,7));
 CHECK(setting_service_impeller_speed_is_pending()&&updates==0);
 const uint8_t unknown[]={4};CHECK(receive(0x4B,unknown,1));
 CHECK(setting_service_impeller_speed_is_pending()&&updates==0);
 CHECK(read_value(50)&&machine_state_impeller_speed()==50);return true;
}
static bool sound_command_isolation(void)
{
 CHECK(setting_service_request_impeller_speed(40));const uint8_t on[]={1},off[]={2};
 CHECK(!receive(0x4A,on,1)&&sound_acks==1&&sound_status==1);
 CHECK(!receive(0x4A,off,1)&&sound_acks==2&&sound_status==2);
 CHECK(setting_service_impeller_speed_is_pending()&&machine_state_impeller_speed()==30&&updates==0);
 CHECK(status(2)&&sound_acks==2&&machine_state_impeller_speed()==40);
 CHECK(accepted[0x4A]==0&&last_tx[3]==0x4B);return true;
}
static bool rejection_guide(void)
{
 machine_fault_key_t key={.source=MACHINE_FAULT_SETTING,.code=0x4B,.type=0};
 mf_guide_t guide;char text[64];fault_guide_lookup(key,&guide);fault_guide_format_code(key,text,sizeof(text));
 CHECK(!strcmp(guide.title.key,"Impeller motor speed")&&!strcmp(text,"0x4B/0x03"));
 CHECK(guide.step_count==1&&!strcmp(guide.steps[0].body.key,"Setting rejected. Previous value retained."));
 key.code=0x42;fault_guide_format_code(key,text,sizeof(text));CHECK(!strcmp(text,"0x42/0x02"));
 key.code=0x3A;fault_guide_format_code(key,text,sizeof(text));CHECK(!strcmp(text,"0x3A/0x00..03/0x02"));return true;
}
static const struct {const char *name;bool (*run)(void);} cases[]={
 {"default_state",default_state},{"read_range",read_range},{"write_range",write_range},
 {"out_of_range",out_of_range},{"operation_isolation",operation_isolation},{"unowned_reply",unowned_reply},
 {"explicit_write_failure",explicit_write_failure},{"read_failure",read_failure},{"send_failure",send_failure},
 {"write_timeout",write_timeout},{"read_timeout",read_timeout},{"result_arrival_deadline",result_arrival_deadline},
 {"queued_reply_protection",queued_reply_protection},{"same_value",same_value},{"pending_is_local",pending_is_local},
 {"page_detached_reply",page_detached_reply},{"page_detached_timeout",page_detached_timeout},{"read_detached_timeout",read_detached_timeout},
 {"cancel_owner",cancel_owner},{"invalid_frames",invalid_frames},{"sound_command_isolation",sound_command_isolation},
 {"rejection_guide",rejection_guide},
};
int main(int argc,char **argv)
{
 if(argc!=2)return 2;
 setting_service_set_request_observer(request_accepted);
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)if(!strcmp(argv[1],cases[i].name)){
  bool passed=cases[i].run();setting_service_cancel_all();protocol_request_result_context_end();
  protocol_request_set_pending_reply_probe(NULL);
  if(passed)printf("PASS impeller protocol %s\n",argv[1]);
  return passed?0:1;
 }
 return 2;
}
