#!/usr/bin/env python3
"""Real subprocess fault injection; no framebuffer or controller commands."""
from pathlib import Path
import os, signal, subprocess, tempfile, time
root=Path(__file__).resolve().parents[1]
harness=r'''
#define _GNU_SOURCE
#include "un260/app_service/runtime_guard.h"
#include "lv_conf.h"
#include "un260/app_service/runtime_guard.c"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char *mode;
static int app(void) {
 FILE *pid=fopen(PID_PATH,"w");fprintf(pid,"%ld",(long)getpid());fclose(pid);
 runtime_guard_count(123,456.5f,1,true,true,false);
 unsigned char frame[]={0xfd,0xdf,6,0x0a,1,0};
 for(unsigned i=0;i<2100;i++)runtime_guard_rx(frame,sizeof(frame));
 runtime_guard_tx(frame,sizeof(frame),0);
 runtime_guard_heartbeat();
 if(!strcmp(mode,"normal"))return 0;
 if(!strcmp(mode,"corrupt")){shared->count_slot=UINT32_MAX;raise(SIGSEGV);}
 if(!strcmp(mode,"segv"))raise(SIGSEGV);
 if(!strcmp(mode,"abort")){LV_ASSERT_HANDLER}
 if(!strcmp(mode,"tx"))runtime_guard_fault(RUNTIME_FAULT_UART_TX);
 for(;;){if(!strcmp(mode,"alive")||!strcmp(mode,"tx"))runtime_guard_heartbeat();usleep(10000);}
}
int main(int argc,char **argv){if(argc!=2)return 1;mode=argv[1];if(!strcmp(mode,"ack"))return runtime_guard_reconcile();return runtime_guard_run(app);}
'''
with tempfile.TemporaryDirectory(prefix='un260-guard-') as temporary:
    t=Path(temporary);d=t/'fault';pid=t/'child';exe=t/'guard';src=t/'main.c';src.write_text(harness)
    subprocess.run(['cc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-DUN260_RUNTIME_GUARD_ENABLED=1',
        '-DRUNTIME_GUARD_TEST=1','-DRUNTIME_GUARD_STALL_MS=250','-DRUNTIME_GUARD_START_MS=500',
        f'-DRUNTIME_GUARD_DIR="{d}"',f'-DRUNTIME_GUARD_LOCK="{t / "lock"}"',f'-DPID_PATH="{pid}"',
        '-I'+str(root),str(src),'-o',str(exe)],check=True)
    def run(mode,expected):
        result=subprocess.run([str(exe),mode],timeout=4)
        assert result.returncode==expected,(mode,result.returncode)
    run('normal',0);assert not (d/'pending').exists()
    for mode,needle in [('corrupt','snapshot_valid=0'),('segv','signal=11'),('abort','signal=6'),('hang','fault=3'),('tx','fault=1')]:
        run(mode,2);assert (d/'pending').exists()
        evidence=(d/'evidence.txt').read_text();assert needle in evidence
        if mode!='corrupt':assert 'pcs=123 amount=456.5' in evidence and 'busy=1 pending=1' in evidence
        assert 'overwritten=52' in evidence and 'completed=0' in evidence
        pid.unlink();run('normal',2);assert not pid.exists(),'protected start ran app'
        run('ack',0);assert not (d/'pending').exists()
        assert list(d.glob('reconciled-*/evidence.txt'))
    proc=subprocess.Popen([str(exe),'alive'])
    for _ in range(100):
        if pid.exists():break
        time.sleep(.01)
    assert pid.exists();run('ack',1)
    proc.send_signal(signal.SIGTERM);assert proc.wait(timeout=4)==0
    assert not (d/'pending').exists(),'service stop mistaken for crash'
    # A crafted symlink must be treated as a hold, never followed or removed.
    (d/'pending').symlink_to(t/'missing');run('normal',2)
    print('PASS guard: SIGSEGV, abort, heartbeat stall, uncertain TX, count/ring evidence, protected reboot, manual archived reconciliation, service stop, live lock, symlink hold')
