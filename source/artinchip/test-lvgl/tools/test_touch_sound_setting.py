#!/usr/bin/env python3
"""Production preference load/save, corruption default and failed-save rollback."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "un260/lv_system/user_cfg.h"
int main(void){
 assert(user_cfg_touch_sound_enabled());assert(!user_cfg_touch_sound_load());assert(user_cfg_touch_sound_enabled());
 assert(user_cfg_touch_sound_save(false));assert(!user_cfg_touch_sound_enabled());
 assert(user_cfg_touch_sound_load()&&!user_cfg_touch_sound_enabled());
 assert(user_cfg_touch_sound_save(true));assert(user_cfg_touch_sound_load()&&user_cfg_touch_sound_enabled());
 FILE *f=fopen(UI_STATE_DIR "/touch_sound.cfg","w");assert(f);fputs("broken",f);fclose(f);
 user_cfg_touch_sound_load();assert(user_cfg_touch_sound_enabled());
 assert(mkdir(UI_STATE_DIR "/touch_sound.cfg.tmp",0700)==0);
 assert(!user_cfg_touch_sound_save(false)&&user_cfg_touch_sound_enabled());
 rmdir(UI_STATE_DIR "/touch_sound.cfg.tmp");unlink(UI_STATE_DIR "/touch_sound.cfg");
 puts("PASS independent preference: default ON, durable OFF/ON reload, malformed input fallback, failed save preserves live state");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-sound-setting-') as temp:
 p=Path(temp)/'test.c';p.write_text(code);exe=p.with_suffix('')
 subprocess.run(['cc','-std=gnu11','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DUI_STATE_DIR="'+temp+'/state"','-I'+str(root),str(p),str(root/'un260/lv_system/user_cfg.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
