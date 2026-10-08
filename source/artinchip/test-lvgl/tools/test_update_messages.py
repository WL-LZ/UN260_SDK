#!/usr/bin/env python3
"""Verify updater text adapter preserves external data and handles UTF-8 bounds."""
from pathlib import Path
import os
import subprocess
import tempfile
from gen_update_messages import extract, outputs

ROOT = Path(__file__).resolve().parents[1]
CODE = r'''
#include <assert.h>
#include <string.h>
#include "un260/lv_system/ui_update_message.h"
const char *ui_tr_for(language_t lang,const char *key){
    (void)lang;
    if(!strcmp(key,"Unsafe install destination: %s"))return "PATH [%s]";
    if(!strcmp(key,"Insufficient root space: need %sKB, free %sKB; keep USB log for storage service"))return "need=%s, free=%s";
    if(!strcmp(key,"Upgrade interrupted"))return "更新中断";
    return key;
}
int main(void){
    char out[320];
    ui_update_message_render(0,"Unsafe install destination: customer-%n-name",out,sizeof(out));
    assert(!strcmp(out,"PATH [customer-%n-name]"));
    ui_update_message_render(0,"Insufficient root space: need 987KB, free 12KB; keep USB log for storage service",out,sizeof(out));
    assert(!strcmp(out,"need=987, free=12"));
    ui_update_message_render(0,"External error 0x42 %n",out,sizeof(out));
    assert(!strcmp(out,"External error 0x42 %n"));
    ui_update_message_render(0,"Upgrade interrupted",out,5);
    assert(!strcmp(out,"更"));
    ui_update_message_render(0,"Upgrade interrupted",out,1);assert(!*out);
    ui_update_message_render(0,NULL,out,sizeof(out));assert(!*out);
    ui_update_message_render(0,"test",NULL,0);
    return 0;
}
'''
assert extract("fail_update 'Visible failure'\necho 'Not a UI status'\nwrite_status 12 prepare check '' 'Preparing files'\n") == ['Preparing files', 'Visible failure']
for text in outputs(['Path $file end']).values():
    assert 'Path %s end' in text
with tempfile.TemporaryDirectory(prefix='un260-update-i18n-') as temp:
    temp = Path(temp); code = temp/'test.c'; exe = temp/('test.exe' if os.name=='nt' else 'test')
    code.write_bytes(CODE.encode('utf-8'))
    flags = ['-std=c11','-Wall','-Wextra','-Werror']
    if os.name != 'nt': flags += ['-fsanitize=address,undefined','-no-pie']
    subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(ROOT),str(code),str(ROOT/'un260/lv_system/ui_update_message.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    code.write_text(r'''
#include <assert.h>
#include <string.h>
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_update_message.h"
static const struct {const char *key,*text;} expected[]={
#include "un260/lv_system/ui_update_catalog.inc"
};
int main(void){
    for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);++i){
        if(i)assert(strcmp(expected[i-1].key,expected[i].key)<0);
        assert(!strcmp(ui_tr_for(LANGUAGE_EN,expected[i].key),expected[i].text));
    }
    assert(!strcmp(ui_tr_for(LANGUAGE_EN,NULL),""));
    assert(!strcmp(ui_tr_for(LANGUAGE_EN,"External %n"),"External %n"));
    char out[320];
    ui_update_message_render(LANGUAGE_EN,"Unsafe install destination: customer-%n-name",out,sizeof(out));
    assert(!strcmp(out,"Unsafe install destination: customer-%n-name"));
    ui_update_message_render(LANGUAGE_EN,"Upgrade interrupted",out,sizeof(out));
    assert(!strcmp(out,"Upgrade interrupted"));
    return 0;
}
''',encoding='utf-8')
    subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(ROOT),str(code),
                    str(ROOT/'un260/lv_system/ui_update_message.c'),
                    str(ROOT/'un260/lv_system/ui_update_catalog.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Updater i18n PASS: known templates, literal parameters, unknown diagnostics, UTF-8, extraction')
