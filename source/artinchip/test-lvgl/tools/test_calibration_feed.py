"""Replay calibration start/no-note through the production failure callback."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
src=(root/'un260/app_service/app_counting_runtime.c').read_text()
callback=re.search(r'^static void app_counting_runtime_on_start_failure\(.*?^\}',src,re.M|re.S).group()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "un260/diagnostic/diagnostic.h"
#define WORK_MODE_OPERATION_CALIBRATION 1
#define SMART_ISLAND_WARNING_LEVEL_WARNING 1
#define SMART_ISLAND_WARNING_LEVEL_ERROR 2
#define UI_TEXT_WIDGET_FAULT_NO_NOTE_MAIN 1
#define UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR 2
#define DATA_COLLECT_MODE_NONE 0
static bool hold,pending;static unsigned refreshes;
void work_mode_service_hold_operation(unsigned owner,bool active){assert(owner==1);hold=active;}
void cis_calib_ui_refresh(void){refreshes++;}
void fault_popup_report_start_no_note(void){pending=true;}
void fault_popup_report_start_fault(unsigned t,unsigned c){(void)t;(void)c;pending=true;}
void uart_debug_printf(const char *f,...){(void)f;}
const char *ui_text_get(int k){(void)k;return "error";}
void smart_island_notify_warning_level(const char *s,int l){(void)s;(void)l;}
const char *get_counting_error_desc(unsigned t,unsigned c){(void)t;(void)c;return "fault";}
const char *app_counting_runtime_start_ui_error_desc(unsigned c){(void)c;return "fault";}
int data_collection_state_mode(void){return 0;}
void data_collection_state_set_status(const char *s){(void)s;}
void page_06_data_collection_refresh(void){}
'''+callback+r'''
int main(void){
 calibration_state_snapshot_t s;
 assert(diagnostic_calibration_begin(CALIB_TARGET_CB,100));hold=true;
 assert(diagnostic_calibration_allows_feed());
 uint8_t started[]={0xFD,0xDF,6,0x5B,1,0};
 assert(diagnostic_reply_dispatch(0x5B,started,6,120,NULL)==DIAGNOSTIC_REPLY_CALIBRATION_UPDATED);
 assert(diagnostic_calibration_allows_feed());
 /* User capture: TX 5F01, RX 5B01, RX 0A0102. */
 app_counting_runtime_on_start_failure(1,2);
 diagnostic_calibration_get_snapshot(&s);
 assert(!s.session_active&&!s.timed_out&&!hold&&!pending&&refreshes==1);
 assert(s.cb_state==CB_CALIB_FAIL_FEED&&s.feed_error_code==2);
 /* A retry is allowed, but no second RUN after actual feed starts. */
 assert(diagnostic_calibration_begin(CALIB_TARGET_CB,150));
 diagnostic_calibration_feed_started();assert(!diagnostic_calibration_allows_feed());
 assert(!diagnostic_calibration_feed_failed(1,2));
 started[4]=2;diagnostic_reply_dispatch(0x5B,started,6,200,NULL);
 diagnostic_calibration_get_snapshot(&s);assert(!s.session_active&&s.cb_state==CB_CALIB_SUCCESS);
 assert(diagnostic_calibration_begin(CALIB_TARGET_CB,220));hold=true;
 app_counting_runtime_on_start_failure(2,3);
 assert(!hold&&pending); /* real machine faults are not hidden */
 pending=false;assert(diagnostic_calibration_begin(CALIB_TARGET_CIS,250));hold=true;
 assert(!diagnostic_calibration_allows_feed());app_counting_runtime_on_start_failure(1,2);
 assert(hold&&pending); /* no cross-target release */
 diagnostic_calibration_end_session();pending=false;
 app_counting_runtime_on_start_failure(1,2);assert(pending); /* ordinary counting unchanged */
 puts("PASS white balance: Start -> feed, production no-note callback releases lease without hidden fault; mechanical/CIS/normal failures preserved");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 for opt in ('-O0','-O2'):
  subprocess.run(['cc',opt,'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root),str(p/'test.c'),str(root/'un260/diagnostic/diagnostic.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
