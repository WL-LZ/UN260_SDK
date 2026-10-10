#!/usr/bin/env python3
"""Real socket/FD/lease tests of the root player's immutable boot snapshot."""
from pathlib import Path
import array
import fcntl
import os
import socket
import struct
import subprocess
import tempfile
import threading
import time

root = Path(__file__).resolve().parents[1]
fixture = r'''
#include "un260/lv_drivers/startup_visual.c"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
int __real_fstat(int,struct stat*);
int __real___fxstat(int,int,struct stat*);
static int fixture_uid(int result,struct stat *st){
 const char *uid=getenv("TEST_WITNESS_UID");if(!result&&uid)st->st_uid=(uid_t)atoi(uid);return result;
}
int __wrap_fstat(int fd,struct stat *st){return fixture_uid(__real_fstat(fd,st),st);}
int __wrap___fxstat(int v,int fd,struct stat *st){return fixture_uid(__real___fxstat(v,fd,st),st);}
static unsigned descriptors(void){
 DIR*d=opendir("/proc/self/fd");assert(d);unsigned count=0;struct dirent*e;
 while((e=readdir(d)))if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))count++;
 closedir(d);return count;
}
int main(int argc,char **argv) {
 assert(argc==2);
 if(!strncmp(argv[1],"witness-",8)) {
  bool valid=!strcmp(argv[1],"witness-valid")||!strcmp(argv[1],"witness-busy");
  unsigned before=descriptors();
  for(unsigned i=0;i<20;i++)assert(root_frame_shown()==valid);
  assert(descriptors()==before);
  assert(startup_visual_begin(NULL)==(valid?1:0));
  if(valid){assert(g_unified&&!g_snapshot_valid&&g_inherited_fb==-1);
   assert(startup_visual_poll(false,false)==0);
   assert(startup_visual_poll(true,false)==(!strcmp(argv[1],"witness-busy")?0:1));
  }
  return 0;
 }
 if(!strcmp(argv[1],"shown-dead")) {
  assert(startup_visual_begin(NULL)==1&&!g_snapshot_valid);
  assert(startup_visual_poll(false,false)==0&&g_unified);usleep(50000);
  assert(startup_visual_poll(false,false)==0&&g_unified);
  assert(startup_visual_poll(true,false)==1&&g_inherited_fb==-1);return 0;
 }
 if(!strcmp(argv[1],"missing")||!strcmp(argv[1],"modern-dead")) {
  assert(startup_visual_begin(NULL)==0);
  uint32_t elapsed=0;assert(startup_visual_acquire(&elapsed)==0);return 0;
 }
 if(!strcmp(argv[1],"silent")||!strcmp(argv[1],"abi")) {
  assert(startup_visual_begin(NULL)==1&&!g_snapshot_valid);
  assert(startup_visual_poll(true,true)==0&&!g_snapshot_valid);
  assert(g_inherited_fb==-1);return 0;
 }
 if(!strcmp(argv[1],"busy")) {
  assert(startup_visual_begin(NULL)==0);
  uint32_t elapsed=0;assert(startup_visual_acquire(&elapsed)==-1);return 0;
 }
 boot_light_status_t snap={0};assert(sizeof(snap)==64);
 assert(startup_visual_begin(&snap)==1);
 bool pending_dead=!strcmp(argv[1],"pending-dead");
 bool delayed=!strcmp(argv[1],"delayed")||!strcmp(argv[1],"delayed-abi");
 if(delayed) {
  assert(!g_snapshot_valid&&g_inherited_fb==-1);
  assert(startup_visual_poll(true,true)==0&&g_snapshot_valid);
 } else if(!pending_dead) {
  assert(snap.origin_ms==1000&&snap.language==2&&snap.mode==BOOT_LIGHT_MODE_ANIMATION);
  assert(g_lease==-1&&g_inherited_fb==-1); /* P cannot end the native display. */
 }
 if(!strcmp(argv[1],"changed")||!strcmp(argv[1],"nofd")) {
  assert(startup_visual_poll(true,false)==0);assert(g_inherited_fb==-1);return 0;
 }
 if(!strcmp(argv[1],"dead")||!strcmp(argv[1],"early-dead")||pending_dead) {
  if(!strcmp(argv[1],"early-dead")) {
   assert(startup_visual_poll(false,false)==0);usleep(50000);
   assert(startup_visual_poll(false,false)==0&&g_unified);
   assert(g_lease>=0); /* Acquiring a dead player's lease cannot skip UI preparation. */
  }
  int owned=0;
  for(unsigned retry=0;retry<100&&!owned;retry++) {owned=startup_visual_poll(true,true);assert(owned>=0);usleep(5000);}
  assert(owned==1&&g_inherited_fb==-1);return 0;
 }
 if(!delayed)assert(startup_visual_poll(false,false)==0);
 if(!strcmp(argv[1],"normal")) assert(startup_visual_poll(true,false)==0);
 assert(startup_visual_poll(true,!strcmp(argv[1],"error"))==1);
 assert(g_inherited_fb>=0&&fcntl(g_inherited_fb,F_GETFD)>=0);
 int ready=open(getenv("TEST_CLIENT_READY"),O_CREAT|O_WRONLY,0600);assert(ready>=0);close(ready);
 for(unsigned n=0;access(getenv("TEST_CLIENT_RELEASE"),F_OK)!=0;n++) {
  assert(n<3000);usleep(1000);
 }
 assert(fcntl(g_inherited_fb,F_GETFD)>=0);
 puts("PASS inherited framebuffer stays alive while the UI owns the lease");
 return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='un260-root-handoff-') as directory:
    tmp = Path(directory)
    source = tmp/'client.c'
    source.write_text(fixture)
    exe = tmp/'client'
    names = {'BOOT_LIGHT_SOCKET':tmp/'socket', 'BOOT_LIGHT_LOCK':tmp/'lock',
             'BOOT_LIGHT_READY':tmp/'ready', 'BOOT_LIGHT_UNIFIED_READY':tmp/'unified',
             'STARTUP_VISUAL_LIVE_BUNDLE':tmp/'live'}
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',
                    '-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(root),
                    *['-D'+k+'="'+str(v)+'"' for k,v in names.items()],
                    str(source),'-Wl,--wrap=fstat','-Wl,--wrap=__fxstat','-o',str(exe)],check=True)
    env = dict(os.environ, UN260_BOOT_LIGHT_ACTIVE='1',
               TEST_WITNESS_UID='0',
               TEST_CLIENT_READY=str(tmp/'client-ready'),
               TEST_CLIENT_RELEASE=str(tmp/'client-release'))
    fb = tmp/'fb'
    fb.write_bytes(bytes(4096))
    subprocess.run([str(exe),'missing'],env=env,check=True)
    with (tmp/'lock').open('r+') as owner:
        fcntl.flock(owner,fcntl.LOCK_EX)
        subprocess.run([str(exe),'busy'],env=env,check=True)
    print('PASS absent renderer fallback and refusal to steal a live display lease')
    for marker in (tmp/'ready',tmp/'unified'):
        marker.write_text(str(os.getpid())+'\n')
    subprocess.run([str(exe),'modern-dead'],env=env,check=True)
    print('PASS modern renderer disappearance uses the free lease without reopening a held lock')

    for marker in (tmp/'ready',tmp/'unified'):
        marker.unlink(missing_ok=True)
    live=b'UN260_BOOT_ACTIVE_V1\n'+b'a'*64+b'\n'+b'b'*64+b'\nnone\nnone\n'
    witness_cases=[('valid',live,0o600,'0'),('truncated',live[:-1],0o600,'0'),
                   ('extra',live+b'\n',0o600,'0'),('nul',live.replace(b'a',b'\0',1),0o600,'0'),
                   ('uppercase',live.replace(b'b',b'B',1),0o600,'0'),
                   ('bad-tail',live[:-10]+b'nOne\nnone\n',0o600,'0'),
                   ('group-write',live,0o620,'0'),('setuid',live,0o4600,'0'),
                   ('setgid',live,0o2600,'0'),('foreign-owner',live,0o600,'1')]
    for name,payload,mode,uid in witness_cases:
        (tmp/'live').write_bytes(payload);(tmp/'live').chmod(mode)
        subprocess.run([str(exe),'witness-'+name],env=dict(env,TEST_WITNESS_UID=uid),check=True)
    (tmp/'live').unlink();(tmp/'saved-live').write_bytes(live)
    (tmp/'live').symlink_to(tmp/'saved-live')
    subprocess.run([str(exe),'witness-symlink'],env=env,check=True)
    (tmp/'live').unlink()
    subprocess.run([str(exe),'witness-absent'],env=env,check=True)
    (tmp/'live').write_bytes(live);(tmp/'live').chmod(0o600)
    with (tmp/'lock').open('r+') as owner:
        fcntl.flock(owner,fcntl.LOCK_EX)
        subprocess.run([str(exe),'witness-busy'],env=env,check=True)
    (tmp/'live').unlink()
    print('PASS 13 real RAM witness validation/FD/free-and-busy lease cases (UID mocked for unprivileged host)')
    for marker in (tmp/'ready',tmp/'unified'):
        marker.write_text(str(os.getpid())+'\n')

    for scenario in ('normal','error','changed','abi','nofd','dead','early-dead','silent','delayed','delayed-abi','pending-dead','shown-dead'):
        requests = []
        errors = []
        stop = threading.Event()
        listening = threading.Event()
        (tmp/'client-ready').unlink(missing_ok=True)
        (tmp/'client-release').unlink(missing_ok=True)
        if scenario=='shown-dead':
            (tmp/'live').write_bytes(live);(tmp/'live').chmod(0o600)

        def serve():
            try:
                with (tmp/'lock').open('r+') as owner, fb.open('rb') as frame:
                    fcntl.flock(owner,fcntl.LOCK_EX)
                    with socket.socket(socket.AF_UNIX,socket.SOCK_SEQPACKET) as server:
                        server.bind(str(tmp/'socket'))
                        server.listen(1)
                        server.settimeout(.1)
                        listening.set()
                        ready_requests = 0
                        while not stop.is_set():
                            try:
                                conn,_ = server.accept()
                            except socket.timeout:
                                continue
                            with conn:
                                request = conn.recv(1)
                                requests.append(request)
                                if scenario=='shown-dead':break
                                if scenario=='silent':
                                    continue
                                if scenario in ('delayed','pending-dead') and len(requests)<=3:
                                    continue
                                abi = 9 if scenario=='abi' or (scenario=='delayed-abi' and len(requests)<=3) else 1
                                identity = bytearray([0x41]*32)
                                flags = 1
                                if (scenario in ('dead','early-dead') and len(requests)>1) or (scenario=='pending-dead' and len(requests)>3):
                                    break
                                if scenario=='changed' and len(requests)>1:
                                    identity[0] = 0x42
                                handoff = False
                                if request==b'R':
                                    ready_requests += 1
                                    handoff = (scenario=='normal' and ready_requests>=2) or scenario in ('delayed','delayed-abi')
                                elif request==b'E':
                                    handoff = scenario=='error'
                                if scenario=='nofd' and request==b'R':
                                    flags |= 2
                                if handoff:
                                    flags |= 2
                                payload = struct.pack('<6IQ32s',0x554e4331,abi,flags,
                                                      1,2,2500,1000,identity)
                                if handoff:
                                    conn.sendmsg([payload],[(socket.SOL_SOCKET,socket.SCM_RIGHTS,
                                                           array.array('i',[frame.fileno()]))])
                                    # Match native order: stop drawing, pass FD, then release.
                                    time.sleep(.03)
                                    break
                                conn.send(payload)
            except BaseException as exc:
                errors.append(exc)
            finally:
                (tmp/'socket').unlink(missing_ok=True)

        thread = threading.Thread(target=serve)
        thread.start()
        assert listening.wait(2)
        process = subprocess.Popen([str(exe),scenario],env=env)
        try:
            if scenario in ('normal','error','delayed','delayed-abi'):
                deadline = time.monotonic()+3
                while not (tmp/'client-ready').exists():
                    assert process.poll() is None and time.monotonic()<deadline
                    time.sleep(.005)
                with (tmp/'lock').open('r+') as competitor:
                    try:
                        fcntl.flock(competitor,fcntl.LOCK_EX|fcntl.LOCK_NB)
                    except BlockingIOError:
                        pass
                    else:
                        raise AssertionError('second renderer could draw during UI ownership')
                (tmp/'client-release').touch()
            assert process.wait(timeout=4)==0
        finally:
            (tmp/'client-release').touch()
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=3)
            stop.set()
            thread.join(timeout=2)
            assert not thread.is_alive() and not errors, errors
            (tmp/'live').unlink(missing_ok=True)
        if scenario=='normal':
            assert requests==[b'P',b'P',b'R',b'R']
        if scenario=='error':
            assert requests==[b'P',b'P',b'E']
        if scenario=='silent':
            assert requests==[b'P']*4 and b'T' not in requests
        if scenario=='abi':
            assert requests==[b'P']*4 and b'R' not in requests and b'E' not in requests
        if scenario in ('delayed','delayed-abi'):
            assert requests==[b'P']*4+[b'R']
        if scenario=='pending-dead':
            assert requests==[b'P']*4
        if scenario=='shown-dead':
            assert requests==[b'P'] and b'T' not in requests and b'R' not in requests
        print('PASS root player '+scenario+': real socket, immutable snapshot and lease')
