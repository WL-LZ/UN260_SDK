#!/usr/bin/env python3
"""Real pthread startup worker, UI ownership, timeouts and isolated cfg tests."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "un260/app_service/app_startup_runtime.h"
#include "un260/lv_system/user_cfg.h"
static atomic_bool release_io, serial_on;
static bool fail_create, fail_serial, fail_devices, history_ready;
static unsigned applied, joined, history_started;
static pthread_t ui_thread;
static void pause_tick(void) { struct timespec t={0,1000000}; nanosleep(&t,0); }
int __real_pthread_create(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
int __wrap_pthread_create(pthread_t*t,const pthread_attr_t*a,void*(*f)(void*),void*x)
{ return fail_create ? 11 : __real_pthread_create(t,a,f,x); }
int __real_pthread_join(pthread_t,void**);
int __wrap_pthread_join(pthread_t t,void**v) { joined++; return __real_pthread_join(t,v); }
void user_cfg_startup_read(user_cfg_startup_snapshot_t*s) {
    assert(!pthread_equal(pthread_self(),ui_thread));
    while(!atomic_load(&release_io)) pause_tick();
    *s=(user_cfg_startup_snapshot_t){.password="2468",.gesture=true};
}
void user_cfg_startup_apply(const user_cfg_startup_snapshot_t*s) {
    assert(pthread_equal(pthread_self(),ui_thread));
    assert(joined==1 && strcmp(s->password,"2468")==0 && s->gesture);
    applied++;
}
bool startup_devices_prepare(void) {
    assert(!pthread_equal(pthread_self(),ui_thread));
    return !fail_devices;
}
bool app_serial_runtime_start(void) {
    assert(!fail_devices);
    assert(!pthread_equal(pthread_self(),ui_thread));
    atomic_store(&serial_on,!fail_serial); return !fail_serial;
}
void app_serial_runtime_stop(void) { atomic_store(&serial_on,false); }
void ui_history_data_init_async(void) { history_started++; }
bool ui_history_data_init_poll(void) { return history_ready; }
bool ui_history_data_is_initialized(void) { return history_ready; }
int main(int argc,char**argv) {
    assert(argc==2); ui_thread=pthread_self();
    fail_create=!strcmp(argv[1],"create-failure");
    fail_serial=!strcmp(argv[1],"serial-failure");
    fail_devices=!strcmp(argv[1],"device-failure");
    bool timeout=!strcmp(argv[1],"io-timeout");
    bool history_timeout=!strcmp(argv[1],"history-timeout");
    uint32_t start=UINT32_MAX-100U;
    assert(app_startup_runtime_poll(start)==APP_STARTUP_WAITING);
    assert(app_startup_runtime_begin(start)==!fail_create);
    assert(!app_startup_runtime_begin(start) && history_started==1);
    if(fail_create) {
        assert(app_startup_runtime_poll(start)==APP_STARTUP_FAILED);
        assert(app_startup_runtime_is_settled() && !joined && !applied);
        return 0;
    }
    for(unsigned i=0;i<1000;i++)
        assert(app_startup_runtime_poll(start+i)==APP_STARTUP_WAITING);
    assert(!applied && !joined && !app_startup_runtime_is_settled());
    assert(!app_startup_runtime_can_process());
    if(timeout) {
        assert(app_startup_runtime_poll(start+15000)==APP_STARTUP_TIMED_OUT);
        assert(!applied && !joined);
    }
    atomic_store(&release_io,true);
    for(unsigned i=0;i<2000 && !app_startup_runtime_is_settled();i++) {
        app_startup_runtime_poll(start+(timeout?15001:1000)); pause_tick();
    }
    assert(joined==1 && app_startup_runtime_is_settled());
    if(timeout) {
        assert(!applied && !atomic_load(&serial_on));
    } else if(fail_serial || fail_devices) {
        assert(app_startup_runtime_poll(start+1100)==APP_STARTUP_FAILED);
        assert(applied==1 && !atomic_load(&serial_on));
    } else {
        assert(applied==1 && atomic_load(&serial_on));
        assert(app_startup_runtime_can_process());
        assert(app_startup_runtime_poll(start+1100)==APP_STARTUP_WAITING);
        if(history_timeout) {
            assert(app_startup_runtime_poll(start+15000)==APP_STARTUP_TIMED_OUT);
            assert(!atomic_load(&serial_on));
            assert(!app_startup_runtime_can_process());
        }
        history_ready=true;
        assert(app_startup_runtime_poll(start+15001)==
            (history_timeout?APP_STARTUP_TIMED_OUT:APP_STARTUP_READY));
    }
    for(int i=0;i<100;i++) app_startup_runtime_poll(start+20000);
    assert(joined==1 && applied==(timeout?0:1));
    puts("PASS startup worker: render-side polling never waits on held I/O, one publication/join, wrap-safe terminal readiness");
}
'''
cfg_fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_system/user_cfg.h"
int main(int argc,char**argv) {
    assert(argc==2);
    bool configured=!strcmp(argv[1],"configured");
    user_cfg_startup_snapshot_t s;
    user_cfg_startup_read(&s);
    assert(!strcmp(user_cfg_password_get(),"1111") && !user_cfg_gesture_enabled());
    assert(!strcmp(s.password,configured?"9876":"1111"));
    assert(s.screenshot==!configured && s.recording==configured && s.gesture==configured);
    user_cfg_startup_apply(&s);
    assert(!strcmp(user_cfg_password_get(),s.password));
    assert(user_cfg_gesture_enabled()==configured);
    user_cfg_startup_read(NULL); user_cfg_startup_apply(NULL);
    puts("PASS isolated preferences: read does not publish; legacy missing/invalid defaults preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-real-startup-') as directory:
    tmp=Path(directory)
    common=['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-pthread','-I'+str(root)]
    src=tmp/'worker.c'; src.write_text(fixture)
    exe=tmp/'worker'
    subprocess.run(common+[str(src),str(root/'un260/app_service/app_startup_runtime.c'),'-Wl,--wrap=pthread_create','-Wl,--wrap=pthread_join','-o',str(exe)],check=True)
    for case in ['success','create-failure','serial-failure','device-failure','io-timeout','history-timeout']:
        subprocess.run([str(exe),case],check=True,timeout=10)
        print('PASS',case,flush=True)
    cfgdir=tmp/'cfg'; cfgdir.mkdir()
    src.write_text(cfg_fixture)
    subprocess.run(common+['-DUI_STATE_DIR="'+str(cfgdir)+'"',str(src),str(root/'un260/lv_system/user_cfg.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),'missing'],check=True)
    (cfgdir/'password.cfg').write_text('9876\n')
    for name,value in [('screenshot',0),('screen_recording',1),('performance_monitor',1),('performance_profile',1),('gestures',1)]:
        (cfgdir/(name+'.cfg')).write_text(str(value))
    subprocess.run([str(exe),'configured'],check=True)
    for path in cfgdir.iterdir(): path.write_text('invalid')
    subprocess.run([str(exe),'invalid'],check=True)
