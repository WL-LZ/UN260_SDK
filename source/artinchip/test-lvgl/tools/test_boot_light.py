#!/usr/bin/env python3
"""Native renderer and real IPC/lease tests, using a fake framebuffer only."""
from pathlib import Path
import fcntl,os,subprocess,tempfile,time
from PIL import Image,ImageChops
root=Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(root/'tools/build_boot_light_assets.py')],check=True)
fixture=r'''
#define _GNU_SOURCE
#define BOOT_LIGHT_HOST_TEST
#define main native_entry
#include "un260/lv_drivers/boot_light.c"
#undef main
#include "un260/lv_drivers/startup_visual.c"
#include <assert.h>
#include <stdarg.h>
static unsigned mock_offset;
void backlight_service_init(void) {}
int __real_open(const char*,int,...);
int __wrap_open(const char *path,int flags,...) {
 mode_t mode=0;if(flags&O_CREAT){va_list ap;va_start(ap,flags);mode=va_arg(ap,int);va_end(ap);}
 return __real_open(!strcmp(path,"/dev/fb0")?getenv("TEST_FB"):path,flags,mode);
}
int __wrap_ioctl(int fd,unsigned long request,...) {
 (void)fd;va_list ap;va_start(ap,request);void *p=va_arg(ap,void*);va_end(ap);
 if(request==FBIOGET_FSCREENINFO){struct fb_fix_screeninfo*f=p;memset(f,0,sizeof(*f));f->line_length=W*4;f->smem_len=BG_BYTES*2;return 0;}
 if(request==FBIOGET_VSCREENINFO){struct fb_var_screeninfo*v=p;memset(v,0,sizeof(*v));v->xres=W;v->yres=H;v->yres_virtual=H*2;v->bits_per_pixel=32;v->red.offset=16;v->green.offset=8;v->yoffset=mock_offset;return 0;}
 if(request==FBIOPAN_DISPLAY){mock_offset=((struct fb_var_screeninfo*)p)->yoffset;return 0;}
 if(request==AICFB_WAIT_FOR_VSYNC){usleep(16000);return 0;}
 return -1;
}
int main(int argc,char**argv) {
 if(argc==1)return native_entry();
 if(!strcmp(argv[1],"render")) {
  assert(argc==4);FILE*f=fopen(ASSET_PATH,"rb");assert(f);
  uint8_t *data=malloc(16+ASSET_BYTES),*frame=malloc(BG_BYTES);assert(data&&frame);
  assert(fread(data,1,16+ASSET_BYTES,f)==16+ASSET_BYTES);fclose(f);
  render(frame,W*4,data+16,(unsigned)strtoul(argv[2],NULL,10),true);
  f=fopen(argv[3],"wb");assert(f);assert(fwrite(frame,1,BG_BYTES,f)==BG_BYTES);fclose(f);free(data);free(frame);return 0;
 }
 uint32_t elapsed=0;int result=startup_visual_acquire(&elapsed);
 if(!strcmp(argv[1],"client")){assert(result==1 && elapsed>0 && g_inherited_fb>=0);usleep(300000);assert(fcntl(g_inherited_fb,F_GETFD)>=0);}
 else if(!strcmp(argv[1],"busy"))assert(result==-1);
 else assert(result==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='un260-light-test-') as directory:
    tmp=Path(directory);src=tmp/'test.c';src.write_text(fixture);exe=tmp/'test'
    defines=[]
    for name,value in [('BOOT_LIGHT_SOCKET',tmp/'socket'),('BOOT_LIGHT_LOCK',tmp/'lock'),('BOOT_LIGHT_READY',tmp/'ready'),('ASSET_PATH',root/'aic_ui/lvgl_data/boot_theme_c/boot-light.bin')]:
        defines.append('-D'+name+'="'+str(value)+'"')
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(root),*defines,str(src),'-Wl,--wrap=open','-Wl,--wrap=ioctl','-lz','-o',str(exe)],check=True)
    fb=tmp/'fb';fb.write_bytes(bytes(1280*400*4*2))
    env=dict(os.environ,TEST_FB=str(fb),UN260_BOOT_LIGHT_ACTIVE='1',UN260_BOOT_TRACE='1')
    native=subprocess.Popen([str(exe)],env=env)
    try:
        deadline=time.monotonic()+5
        while not (tmp/'ready').exists():
            assert native.poll() is None
            assert time.monotonic()<deadline
            time.sleep(.01)
        time.sleep(.1)
        client=subprocess.Popen([str(exe),'client'],env=env)
        assert native.wait(timeout=3)==0
        # The client still owns the lease and inherited framebuffer reference.
        assert subprocess.run([str(exe)],env=env,timeout=2).returncode==1
        assert client.wait(timeout=3)==0
        assert not (tmp/'ready').exists() and not (tmp/'socket').exists()
        print('PASS real handoff, inherited framebuffer lifetime, single writer, cleanup')
    finally:
        if native.poll() is None:native.terminate();native.wait(timeout=3)
    subprocess.run([str(exe),'fallback'],env=env,check=True)
    with (tmp/'lock').open('r+') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        subprocess.run([str(exe),'busy'],env=env,check=True)
    print('PASS missing renderer fallback and refusal to draw with an active owner')
    output=Path('/tmp/un260-boot-light-20260914/raster');output.mkdir(parents=True,exist_ok=True)
    for name,elapsed in [('brand',2000),('welcome',4300),('waiting',5000)]:
        raw=tmp/'frame.raw';subprocess.run([str(exe),'render',str(elapsed),str(raw)],check=True)
        image=Image.frombytes('RGBA',(1280,400),raw.read_bytes(),'raw','BGRA').convert('RGB')
        image.save(output/(name+'.png'))
    print('PASS native raster frames generated for comparison (not physical display FPS)')
