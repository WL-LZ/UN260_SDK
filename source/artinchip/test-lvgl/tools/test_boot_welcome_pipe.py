#!/usr/bin/env python3
"""Exercise the real early-renderer pipe and framebuffer handoff without a board."""
from pathlib import Path
import ast,os,socket,struct,subprocess,tempfile,time,array
root=Path(__file__).resolve().parents[1]
tree=ast.parse((root/'tools/test_boot_light.py').read_text())
fixture=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='fixture' for t in n.targets))
fixture=fixture.replace('#define UI_BOOT_ANIM_THEME 3','#define UI_BOOT_ANIM_THEME 4')
fixture=fixture.replace('#define main native_entry','#define UN260_EARLY_INIT\n#define boot_light_run native_entry')
fixture=fixture.replace('frame_state_t state={0};render(frame', 'boot_welcome_native_apply(data+16+BG_BYTES+ICON_BYTES+TEXT_BYTES,(unsigned)atoi(getenv("TEST_LOCALE")));\n  frame_state_t state={0};render(frame')
def wait_ready(p,path):
    limit=time.monotonic()+4
    while not path.exists():
        assert p.poll() is None and time.monotonic()<limit
        time.sleep(.01)
def word(data,page=0):
    return b''.join(data[(page*400+y)*1280*4+390*4:(page*400+y)*1280*4+890*4] for y in range(141,269))
with tempfile.TemporaryDirectory(prefix='un260-welcome-pipe-') as directory:
    p=Path(directory);(p/'test.c').write_text(fixture)
    defs=[]
    for name,value in [('BOOT_LIGHT_SOCKET',p/'socket'),('BOOT_LIGHT_LOCK',p/'lock'),('BOOT_LIGHT_READY',p/'ready'),('ASSET_PATH',root/'aic_ui/generated_assets/boot_theme_d/boot-light.bin')]:
        defs.append('-D'+name+'="'+str(value)+'"')
    exe=p/'test'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(root),*defs,str(p/'test.c'),'-Wl,--wrap=open','-Wl,--wrap=ioctl','-Wl,--wrap=sendmsg','-lz','-o',str(exe)],check=True)
    assets=(root/'aic_ui/generated_assets/boot_theme_d/boot-light.bin').read_bytes()[16:]
    for index,payload in [(4,struct.pack('I',4)),(6,struct.pack('I',6)),(0,b''),(0,b'\xff')]:
        fb=p/'fb';fb.write_bytes(bytes(1280*400*4*2));r,w=os.pipe2(os.O_NONBLOCK)
        env=dict(os.environ,TEST_FB=str(fb),UN260_BOOT_LOCALE_FD=str(r),TEST_LOCALE=str(index))
        process=subprocess.Popen([str(exe)],env=env,pass_fds=(r,));os.close(r)
        try:
            wait_ready(process,p/'ready');time.sleep(.25)
            before=fb.read_bytes()
            assert word(before,0)==word(assets) and word(before,1)==word(assets), 'English must not flash before saved locale arrives'
            if payload:os.write(w,payload)
            os.close(w);w=-1
            time.sleep(2.1)
            subprocess.run([str(exe),'render','4300',str(p/'expected')],env=env,check=True)
            actual=fb.read_bytes();expected=(p/'expected').read_bytes()
            assert word(actual,0)==word(expected) and word(actual,1)==word(expected)
            with socket.socket(socket.AF_UNIX,socket.SOCK_SEQPACKET) as client:
                client.connect(str(p/'socket'));client.send(b'T')
                reply,anc,flags,address=client.recvmsg(8,socket.CMSG_SPACE(4))
                magic,elapsed=struct.unpack('II',reply)
                assert magic==0x554e4238 and 2300<elapsed<6000
                for level,kind,data in anc:
                    if kind==socket.SCM_RIGHTS:
                        descriptors=array.array('i');descriptors.frombytes(data)
                        for fd in descriptors:os.close(fd)
            assert process.wait(timeout=3)==0
        finally:
            if w>=0:os.close(w)
            if process.poll() is None:process.terminate();process.wait(timeout=3)
print('PASS early locale pipe: no English flash, Arabic/French pixels, EOF/short-read fallback, continued timeline and real handoff')
