#!/usr/bin/env python3
"""Exercise the production fault-popup commit against failed and successful writes."""
from pathlib import Path
import subprocess,tempfile
from test_main_view import function
root=Path(__file__).resolve().parents[1]
commit=function((root/'un260/lv_system/ui_state_runtime.c').read_text(),'ui_state_popup_auto_set_enabled')
code=r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_system/ui_state_store.h"
static ui_persist_state_t s_ui_state,disk;
static bool popup=true,save_ok;
static unsigned saves,changes;
static void ui_state_ensure_loaded(void){}
static void ui_state_pull_common_runtime(void){}
bool ui_state_store_save(const ui_persist_state_t *next){saves++;if(!save_ok)return false;disk=*next;return true;}
void fault_popup_set_auto_enabled(bool value){popup=value;changes++;}
"""+commit+r"""
int main(void){
 s_ui_state.page06.reserved06_enable=1;disk=s_ui_state;
 assert(!ui_state_popup_auto_set_enabled(false));
 assert(popup&&s_ui_state.page06.reserved06_enable==1&&disk.page06.reserved06_enable==1&&changes==0);
 save_ok=true;assert(ui_state_popup_auto_set_enabled(false));
 assert(!popup&&s_ui_state.page06.reserved06_enable==0&&disk.page06.reserved06_enable==0&&changes==1);
 save_ok=false;assert(!ui_state_popup_auto_set_enabled(true));assert(!popup&&changes==1);
 save_ok=true;assert(ui_state_popup_auto_set_enabled(true));assert(popup&&changes==2&&saves==4);
 puts("PASS actual popup setter: save failure preserves both live and saved state; success commits once");
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
