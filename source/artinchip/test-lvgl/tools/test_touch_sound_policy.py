#!/usr/bin/env python3
"""Exercise exact production policy, stubbing model getters and UART readiness."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'un260/app_service/app_touch_sound.c').read_text()
source=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
head=r"""
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
typedef int ui_page_t;
enum{UI_PAGE_MAIN,UI_PAGE_BOOT,UI_PAGE_BOOT_ANIM,UI_PAGE_STANDBY,UPGRADE_SESSION_NONE=0};
static bool muted,offline,counting,aging,fault,operation,upgrading,resetting,calibrating,boot;
static int test_page;static unsigned sounds,cancels;
#define user_cfg_touch_sound_enabled() (!muted)
#define user_cfg_touch_sound_load() ((void)0)
#define protocol_send_is_ready() (!offline)
#define app_command_runtime_count_start_busy() counting
#define machine_state_aging_running() aging
#define machine_fault_count() fault
#define work_mode_service_operation_active() operation
#define upgrade_session_owner() upgrading
#define ui_factory_reset_blocks_navigation() resetting
#define touch_calibration_busy() calibrating
#define ui_page_00_boot_anim_is_active() boot
#define ui_manager_get_current_page() test_page
#define buzzer_service_click() (++sounds)
#define buzzer_service_cancel() (++cancels)
static void touch_sound_init(void (*f)(void)){(void)f;}
static void touch_sound_feedback(void){}
static void touch_sound_contact(bool down){(void)down;}
static void lv_port_indev_set_contact_observer(void (*f)(bool)){(void)f;}
static void lv_port_indev_set_feedback_observer(void (*f)(void)){(void)f;}
"""
tail=r"""
int main(void){
 app_touch_sound_init();app_touch_sound_poll();assert(!sounds);
 request_click();app_touch_sound_poll();assert(sounds==1);app_touch_sound_poll();assert(sounds==1);
 bool *blocks[]={&muted,&offline};
 for(unsigned i=0;i<2;i++){
  request_click();*blocks[i]=true;app_touch_sound_poll();assert(sounds==1);*blocks[i]=false;app_touch_sound_poll();assert(sounds==1);
 }
 muted=true;unsigned cancellations=cancels;app_touch_sound_poll();assert(cancels==cancellations+1);muted=false;
 /* An accepted button remains audible when its action changes machine/page
    state. Disabled controls and gestures are filtered by the input owner. */
 bool *states[]={&counting,&aging,&fault,&operation,&upgrading,&resetting,&calibrating,&boot};
 for(unsigned i=0;i<sizeof(states)/sizeof(*states);i++){
  unsigned before=sounds;request_click();*states[i]=true;app_touch_sound_poll();assert(sounds==before+1);*states[i]=false;
 }
 for(test_page=UI_PAGE_BOOT;test_page<=UI_PAGE_STANDBY;test_page++){unsigned before=sounds;request_click();app_touch_sound_poll();assert(sounds==before+1);}
 unsigned before=sounds;request_click();request_click();request_click();app_touch_sound_poll();assert(sounds==before+1);
 puts("PASS uniform accepted-tap policy: independent mute/offline only, no action/page-dependent omissions, no stale replay, coalescing");
}
"""
with tempfile.TemporaryDirectory(prefix='un260-sound-policy-') as temp:
 p=Path(temp)/'policy.c';p.write_text(head+source+tail);exe=p.with_suffix('')
 subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
