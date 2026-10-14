"""Exercise the actual receipt service and Print callback with isolated IO."""
from pathlib import Path
import os,subprocess,tempfile
from test_multi_result_safety import function
from test_i18n_support import with_i18n
root=Path(__file__).resolve().parents[1]
actual=function((root/'un260/lv_core/lv_page_event.c').read_text(),'page_01_print_btn_event_cb')
harness=r'''
#include <assert.h>
#include <stdio.h>
#include "tools/test_notice_sink.h"
#include "un260/app_service/receipt_print_service.c"
typedef struct {int code;} lv_event_t;
enum {LV_EVENT_CLICKED=1};
int lv_event_get_code(lv_event_t *e){return e->code;}
static counting_sim_t data={.total_amount=100,.total_pcs=10};
static ui_history_record_t saved={.valid=true,.record_no=8,.pcs=10,.amount=100,.currency="CNY"};
static bool busy,pending,multi,unsupported,fail,idle=true,available=true;
static uint32_t clock_ms,record_id=8;
static storage_job_status_t storage_status=STORAGE_JOB_SUCCEEDED;
static unsigned sent;
bool app_command_runtime_count_start_busy(void){return busy;}
bool print_config_pending(void){return pending;}
uint32_t app_clock_uptime_ms(void){return clock_ms;}
const counting_sim_t *counting_data_current(void){return &data;}
bool counting_data_monetary_result_supported(const counting_sim_t *d){assert(d==&data);return !unsupported;}
bool currency_state_multi_selected(void){return multi;}
void currency_state_get_active_code(char code[4]){strcpy(code,"CNY");}
uint32_t counting_history_current_record(void){return record_id;}
bool counting_history_is_idle(void){return idle;}
bool ui_history_data_is_available(void){return available;}
storage_job_status_t ui_history_data_status(void){return storage_status;}
bool ui_history_record_get_by_no(uint32_t id,ui_history_record_t *r){if(id!=8)return false;*r=saved;return true;}
void machine_time_get(machine_time_value_t *t){*t=(machine_time_value_t){2026,10,14,10,20,30};}
int protocol_send(uint8_t cmd,const uint8_t *p,uint16_t n){
 assert(cmd==0x3C&&n==9&&p[0]=='C'&&p[2]=='Y'&&p[3]==26&&p[4]==10&&p[5]==14&&p[8]==30);
 ++sent;return fail?-1:15;
}
'''
main=r'''
int main(void){
 lv_event_t e={LV_EVENT_CLICKED};page_01_print_btn_event_cb(&e);
 assert(sent==1&&test_notice_kind==UI_NOTICE_INFO&&receipt_print_status()->state==RECEIPT_PRINT_SENT);
 ui_notice_dismiss(NULL);assert(sent==1);
 page_01_print_btn_event_cb(&e);assert(sent==1&&test_notice_kind==UI_NOTICE_WARNING);
 clock_ms+=2000;fail=true;page_01_print_btn_event_cb(&e);
 assert(sent==2&&test_notice_kind==UI_NOTICE_ERROR&&receipt_print_status()->state==RECEIPT_PRINT_FAILED);
 fail=false;busy=true;assert(!receipt_print_request(8));busy=false;
 pending=true;assert(!receipt_print_request(8));pending=false;
 multi=true;assert(!receipt_print_request(8));multi=false;
 unsupported=true;assert(!receipt_print_request(8));unsupported=false;
 data.total_pcs=0;assert(!receipt_print_request(8));data.total_pcs=10;
 assert(!receipt_print_request(7));record_id=0;assert(!receipt_print_request(8));record_id=8;
 idle=false;assert(!receipt_print_request(8));idle=true;
 available=false;assert(!receipt_print_request(8));available=true;
 storage_status=STORAGE_JOB_FAILED;assert(!receipt_print_request(8));storage_status=STORAGE_JOB_SUCCEEDED;
 saved.pcs++;assert(!receipt_print_request(8));saved.pcs--;
 saved.amount++;assert(!receipt_print_request(8));saved.amount--;
 strcpy(saved.currency,"USD");assert(!receipt_print_request(8));strcpy(saved.currency,"CNY");
 saved.multi.enabled=true;assert(!receipt_print_request(8));saved.multi.enabled=false;
 assert(sent==2);assert(receipt_print_request(8));assert(sent==3&&receipt_print_status()->record_no==8);
 e.code=0;page_01_print_btn_event_cb(&e);assert(sent==3);
 sent_at=UINT32_MAX-999;clock_ms=500;assert(!receipt_print_request(8));
 clock_ms=1000;assert(receipt_print_request(8));assert(sent==4);
 puts("PASS receipt Print: stable identity, mismatch/busy/failure guards, payload, notices, debounce and clock wrap; no paper-success claim");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-print-') as d:
 d=Path(d);source=d/'test.c';binary=d/'test';source.write_text(harness+'\n'+actual+'\n'+main)
 command=[os.environ.get('CC','cc'),'-std=gnu11','-Wall','-Wextra','-Werror',f'-I{root}']
 if os.name!='nt':command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 subprocess.run([*command,*(str(p) for p in with_i18n([source],root)),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
