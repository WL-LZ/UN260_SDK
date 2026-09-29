"""Exercise the production Print callback: typed notices and unchanged payload."""
from pathlib import Path
import os
import subprocess
import tempfile
from test_multi_result_safety import function

root = Path(__file__).resolve().parents[1]
actual = function((root/'un260/lv_core/lv_page_event.c').read_text(),
                  'page_01_print_btn_event_cb')
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "tools/test_notice_sink.h"
typedef struct {int code;} lv_event_t;
enum {LV_EVENT_CLICKED=1, UI_TEXT_WIDGET_MULTI_RESULT_UNSUPPORTED,
      UI_TEXT_WIDGET_PRINT_TOAST_COUNT_FIRST, UI_TEXT_WIDGET_PRINT_TOAST_PRINTING};
typedef struct {int x,w,h,auto_hide_ms,loader_color;const char *text;
                bool show_loader,align_center,use_text_area;} lv_print_toast_config_t;
typedef struct {int year;uint8_t month,day,hour,minute,second;} machine_time_value_t;
static struct {float total_amount;int total_pcs;} data={100,10};
static bool visible,multi,supported=true,send_fail;
static unsigned sent,toasts,inline_errors;
int lv_event_get_code(lv_event_t *e){return e->code;}
bool currency_state_multi_selected(void){return multi;}
bool counting_data_monetary_result_supported(const void *p){assert(p==&data);return supported;}
void *counting_data_current(void){return &data;}
const char *ui_text_get(int id){(void)id;return "Print feedback";}
bool page_03_menu_report_setting_error(const char *s){assert(s&&*s);if(visible)inline_errors++;return visible;}
bool page_03_menu_is_visible(void){return visible;}
lv_print_toast_config_t lv_print_toast_get_default_config(void){return (lv_print_toast_config_t){0};}
void lv_print_toast_show_with_config(const lv_print_toast_config_t *c){assert(c->text);toasts++;}
void lv_print_toast_show(const char *s){assert(s);toasts++;}
int lv_color_hex(int c){return c;}
void machine_time_get(machine_time_value_t *t){*t=(machine_time_value_t){2026,9,28,10,20,30};}
void currency_state_get_active_code(char *c){c[0]='C';c[1]='N';c[2]='Y';c[3]=0;}
int protocol_send(uint8_t cmd,const uint8_t *p,uint16_t n){
 assert(cmd==0x3C&&n==9&&p[0]=='C'&&p[2]=='Y'&&p[3]==26&&p[8]==30);
 sent++;return send_fail?-1:0;
}
'''.replace('void *counting_data_current(void)', '__typeof__(data) *counting_data_current(void)')
main = r'''
int main(void){
 lv_event_t event={LV_EVENT_CLICKED};
 visible=true;page_01_print_btn_event_cb(&event);assert(sent==1&&test_notice_count==1&&test_notice_kind==UI_NOTICE_INFO);
 ui_notice_dismiss(NULL);assert(sent==1&&!test_notice_visible);
 send_fail=true;page_01_print_btn_event_cb(&event);assert(sent==2&&test_notice_count==2&&test_notice_kind==UI_NOTICE_ERROR);
 multi=true;page_01_print_btn_event_cb(&event);assert(sent==2&&test_notice_count==3&&test_notice_kind==UI_NOTICE_WARNING);
 multi=false;data.total_amount=data.total_pcs=0;page_01_print_btn_event_cb(&event);
 assert(sent==2&&test_notice_count==4&&test_notice_kind==UI_NOTICE_WARNING);
 visible=false;page_01_print_btn_event_cb(&event);assert(test_notice_count==5&&sent==2);
 data.total_amount=100;data.total_pcs=10;send_fail=false;page_01_print_btn_event_cb(&event);
 assert(sent==3&&test_notice_count==6);send_fail=true;page_01_print_btn_event_cb(&event);assert(test_notice_count==7);
 event.code=0;page_01_print_btn_event_cb(&event);assert(sent==4&&test_notice_count==7);
 assert(!toasts&&!inline_errors);
 puts("PASS actual Print: shared typed notice; dismissal never sends; no data/multi blocked; payload unchanged");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-menu-print-') as directory:
    directory = Path(directory)
    source, executable = directory/'test.c', directory/'test'
    source.write_text(harness+'\n'+actual+'\n'+main)
    command=[os.environ.get('CC','cc'),'-std=gnu11','-Wall','-Wextra','-Werror',f'-I{root}']
    if os.name!='nt': command += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
    if os.name=='nt': executable=executable.with_suffix('.exe')
    subprocess.run([*command,str(source),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
