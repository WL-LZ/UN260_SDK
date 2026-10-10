#!/usr/bin/env python3
"""Actual current client vs old IMG's T/8-byte/SCM_RIGHTS display protocol."""
import array
import fcntl
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import threading
import time

SDK = Path(__file__).resolve().parents[2]
APP = SDK / 'source/artinchip/test-lvgl'
CLIENT = r'''
#include "un260/lv_drivers/startup_visual.c"
#include <assert.h>
#include <stdio.h>
int main(int argc,char **argv) {
 assert(argc==2);
 assert(startup_visual_begin(NULL)==0);
 assert(!g_unified&&g_inherited_fb==-1&&g_lease==-1);
 uint32_t elapsed=0xdecafbadU;
 int got=startup_visual_acquire(&elapsed);
 if(!strcmp(argv[1],"held")) {
  assert(got==-1&&elapsed==0xdecafbadU);
  assert(g_inherited_fb>=0&&fcntl(g_inherited_fb,F_GETFD)>=0);
  assert(flock(g_lease,LOCK_EX|LOCK_NB)==-1);
  assert(errno==EWOULDBLOCK||errno==EAGAIN);
  return 0;
 }
 bool matching=!strcmp(argv[1],"matching");
 assert(got==(matching?1:0));
 assert(elapsed==(matching?2500U:0xdecafbadU));
 assert(g_lease>=0);
 bool missing=!strcmp(argv[1],"nofd");
 assert(missing?g_inherited_fb==-1:g_inherited_fb>=0);
 if(!missing)assert(fcntl(g_inherited_fb,F_GETFD)&FD_CLOEXEC);
 int ready=open(getenv("TEST_CLIENT_READY"),O_CREAT|O_WRONLY,0600);assert(ready>=0);close(ready);
 for(unsigned i=0;access(getenv("TEST_CLIENT_RELEASE"),F_OK);i++){assert(i<3000);usleep(1000);}
 if(!missing){
  char text[10]={0};assert(pread(g_inherited_fb,text,9,0)==9);
  assert(!memcmp(text,"FRAME-FD!",9));
 }
 puts("PASS legacy T acquired lease; old art elapsed rejected; framebuffer fd stays live");
 return 0;
}
'''


with tempfile.TemporaryDirectory(prefix='un260-old-t-handoff-') as name:
    tmp = Path(name)
    source = tmp / 'client.c'
    source.write_text(CLIENT)
    exe = tmp / 'client'
    paths = {name: tmp / item for name, item in (
        ('BOOT_LIGHT_SOCKET', 'socket'), ('BOOT_LIGHT_LOCK', 'lock'),
        ('BOOT_LIGHT_READY', 'ready'), ('BOOT_LIGHT_UNIFIED_READY', 'unified'),
        ('STARTUP_VISUAL_LIVE_BUNDLE', 'live'))}
    subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all', '-I' + str(APP),
                    *['-D' + key + '="' + str(path) + '"' for key, path in paths.items()],
                    str(source), '-o', str(exe)], check=True)
    fb = tmp / 'framebuffer'
    fb.write_bytes(b'FRAME-FD!' + bytes(4096))
    env = dict(os.environ, UN260_BOOT_LIGHT_ACTIVE='1',
               TEST_CLIENT_READY=str(tmp / 'client-ready'),
               TEST_CLIENT_RELEASE=str(tmp / 'client-release'))
    for scenario in ('old', 'matching', 'nofd', 'held'):
        requests, errors = [], []
        listening, stop, handed = threading.Event(), threading.Event(), threading.Event()
        (tmp / 'client-ready').unlink(missing_ok=True)
        (tmp / 'client-release').unlink(missing_ok=True)

        def serve():
            try:
                with (tmp / 'lock').open('a+') as owner, fb.open('rb') as frame:
                    fcntl.flock(owner, fcntl.LOCK_EX)
                    with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as server:
                        server.bind(str(tmp / 'socket'))
                        server.listen(2)
                        server.settimeout(.1)
                        listening.set()
                        while not stop.is_set():
                            try:
                                conn, _ = server.accept()
                            except socket.timeout:
                                continue
                            with conn:
                                request = conn.recv(1)
                                requests.append(request)
                                if request != b'T':
                                    continue  # Exact old IMG ignores the modern P request.
                                magic = 0x554e4238 if scenario == 'matching' else 0x554e4237
                                payload = struct.pack('<II', magic, 2500)
                                if scenario == 'nofd':
                                    conn.send(payload)
                                else:
                                    conn.sendmsg([payload], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                                             array.array('i', [frame.fileno()]))])
                                handed.set()
                                if scenario == 'held':
                                    assert stop.wait(3)
                                else:
                                    time.sleep(.03)  # Native stops drawing before closing its lease.
                                break
            except BaseException as exc:
                errors.append(exc)
            finally:
                (tmp / 'socket').unlink(missing_ok=True)

        thread = threading.Thread(target=serve)
        thread.start()
        assert listening.wait(2)
        process = subprocess.Popen([str(exe), scenario], env=env)
        try:
            assert handed.wait(2)
            if scenario != 'held':
                deadline = time.monotonic() + 3
                while not (tmp / 'client-ready').exists():
                    assert process.poll() is None and time.monotonic() < deadline
                    time.sleep(.005)
                thread.join(timeout=1)
                assert not thread.is_alive()  # Sender's framebuffer reference is gone.
                with (tmp / 'lock').open('r+') as competitor:
                    try:
                        fcntl.flock(competitor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    except BlockingIOError:
                        pass
                    else:
                        raise AssertionError('second writer could steal UI display lease')
                (tmp / 'client-release').touch()
            assert process.wait(timeout=4) == 0
        finally:
            stop.set()
            (tmp / 'client-release').touch()
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=2)
            thread.join(timeout=2)
            assert not thread.is_alive() and not errors, errors
        assert requests == [b'P', b'T'], requests
        print('PASS legacy startup protocol scenario=' + scenario)
