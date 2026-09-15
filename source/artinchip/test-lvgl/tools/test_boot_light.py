#!/usr/bin/env python3
"""Native renderer and real IPC/lease tests, using a fake framebuffer only."""
from pathlib import Path
import fcntl,os,subprocess,tempfile,time
from PIL import Image,ImageChops
root=Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(root/'tools/build_boot_light_assets.py')],check=True)
fixture=r'''
#define _GNU_SOURCE
#define UI_BOOT_ANIM_THEME 3
#define BOOT_LIGHT_HOST_TEST
#define main native_entry
#include "un260/lv_drivers/boot_light.c"
#undef main
#include "un260/lv_drivers/startup_visual.c"
#include <assert.h>
#include <stdarg.h>
static unsigned mock_offset;
static volatile unsigned mock_pan_interrupts,mock_vsync_interrupts;
ssize_t __real_sendmsg(int,const struct msghdr*,int);
ssize_t __wrap_sendmsg(int fd,const struct msghdr *msg,int flags) {
 if(getenv("TEST_LEGACY_REPLY")&&msg->msg_iovlen==1&&
    msg->msg_iov[0].iov_len==sizeof(boot_light_reply_t))
  ((boot_light_reply_t*)msg->msg_iov[0].iov_base)->magic=0x554e4231U;
 return __real_sendmsg(fd,msg,flags);
}
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
 if(request==FBIOPAN_DISPLAY){if(mock_pan_interrupts){mock_pan_interrupts--;errno=EINTR;return -1;}mock_offset=((struct fb_var_screeninfo*)p)->yoffset;return 0;}
 if(request==AICFB_WAIT_FOR_VSYNC){if(mock_vsync_interrupts){mock_vsync_interrupts--;errno=EINTR;return -1;}usleep(16000);return 0;}
 return -1;
}
/* Frozen scalar reference of the original framebuffer renderer. Keeping this
 * separate catches dirty-region, alpha and two-page timeline regressions. */
static void reference_blend(uint8_t *dst,const uint8_t *src,unsigned count,unsigned opacity) {
 for(unsigned i=0;i<count;i++,dst+=4,src+=4) {
  unsigned a=(src[3]*opacity+127)/255;
  for(unsigned c=0;c<3;c++)dst[c]=(src[c]*a+dst[c]*(255-a)+127)/255;
  dst[3]=255;
 }
}
static void reference_sprite(uint8_t *frame,const uint8_t *src,unsigned w,unsigned h,unsigned x,unsigned y,float opacity) {
 unsigned a=(unsigned)(255*opacity+.5f);if(!a)return;
 for(unsigned row=0;row<h;row++)reference_blend(frame+(y+row)*W*4+x*4,src+row*w*4,w,a);
}
static void reference_render(uint8_t *frame,const uint8_t *assets,uint32_t elapsed) {
 memcpy(frame,assets,BG_BYTES);
 float p=progress(elapsed,BOOT_ENTRANCE_START,BOOT_ENTRANCE_DURATION),q=1-p;
 reference_sprite(frame,assets+BG_BYTES,96,96,592,82+(unsigned)(BOOT_WELCOME_MOTION_SCALE*6*q*q*q+.5f),ease(p));
 reference_sprite(frame,assets+BG_BYTES+ICON_BYTES,500,BOOT_TEXT_HEIGHT,390,BOOT_TEXT_Y+(unsigned)(BOOT_WELCOME_MOTION_SCALE*4*q*q*q+.5f),ease(p)*(1-ease(progress(elapsed,BOOT_BRAND_EXIT_START,BOOT_BRAND_EXIT_DURATION))));
 p=progress(elapsed,BOOT_WELCOME_START,BOOT_WELCOME_DURATION);q=1-p;
 reference_sprite(frame,assets+BG_BYTES+ICON_BYTES+TEXT_BYTES,500,BOOT_TEXT_HEIGHT,390,BOOT_TEXT_Y+(unsigned)(BOOT_WELCOME_MOTION_SCALE*4*q*q*q+.5f),ease(p));
 for(unsigned i=0;i<3;i++) {
  unsigned phase=elapsed<BOOT_DOT_START?0:(elapsed-BOOT_DOT_START+BOOT_DOT_PERIOD-i*160)%BOOT_DOT_PERIOD;
  float jump=phase>=660?0:phase<240?ease((float)phase/240):1-ease((float)(phase-240)/420);
  float alpha=ease(progress(elapsed,BOOT_DOT_START+i*120,450))*(.4f+.6f*jump);
  unsigned x=619+i*17,y=BOOT_DOT_Y-(unsigned)(BOOT_WELCOME_MOTION_SCALE*6*jump+.5f);
  for(unsigned yy=0;yy<7;yy++)for(unsigned xx=0;xx<7;xx++) {
   int dx=(int)xx-3,dy=(int)yy-3;if(dx*dx+dy*dy>12)continue;
   const uint8_t color[4]={0x20,0x58,0xf8,255};
   reference_blend(frame+(y+yy)*W*4+(x+xx)*4,color,1,(unsigned)(alpha*255+.5f));
  }
 }
}
static void expect_fill(const uint8_t *p,size_t n,uint8_t value) {
 for(size_t i=0;i<n;i++)assert(p[i]==value);
}
static void pixel_equivalence(void) {
 enum {GUARD=64,STRIDE=W*4+64,PAGE_BYTES=STRIDE*H};
 uint8_t *assets=malloc(ASSET_BYTES),*reference=malloc(BG_BYTES);
 uint8_t *allocation=malloc(PAGE_BYTES*2+GUARD*2),*scratch=malloc(ACTIVE_BYTES+GUARD*2),*inactive=malloc(PAGE_BYTES);
 assert(assets&&reference&&allocation&&scratch&&inactive);
 uLongf bytes=ASSET_BYTES;assert(uncompress(assets,&bytes,boot_light_packed,sizeof(boot_light_packed))==Z_OK&&bytes==ASSET_BYTES);
 memset(allocation,0xa5,PAGE_BYTES*2+GUARD*2);memset(scratch,0x5a,ACTIVE_BYTES+GUARD*2);
 uint8_t *pages=allocation+GUARD,*canvas=scratch+GUARD;frame_state_t states[2]={{0},{0}};
 const unsigned times[]={0,149,150,151,449,450,700,949,950,979,980,1200,2699,2700,2701,3000,3299,3300,3330,3449,3450,3451,3800,4149,4150,4151,4300,4349,4350,4351,4470,4499,4500,4589,4590,4799,4800,5010,5170,5330,5700,5849,5850,5851,9999,19999,4300,4351,4499,100,5000,19999,2701,4150,4351};
 for(unsigned step=0;step<sizeof(times)/sizeof(times[0]);step++) {
  /* Alternate pages, occasionally leave one stale across multiple phases. */
  unsigned page=step%7==0?0:step%2;uint8_t *target=pages+page*PAGE_BYTES;
  memcpy(inactive,pages+(page^1U)*PAGE_BYTES,PAGE_BYTES);
  reference_render(reference,assets,times[step]);
  render(target,STRIDE,assets,canvas,times[step],&states[page]);
  assert(!memcmp(inactive,pages+(page^1U)*PAGE_BYTES,PAGE_BYTES));
  for(unsigned y=0;y<H;y++) {
   assert(!memcmp(target+y*STRIDE,reference+y*W*4,W*4));
   expect_fill(pages+y*STRIDE+W*4,64,0xa5);
   expect_fill(pages+PAGE_BYTES+y*STRIDE+W*4,64,0xa5);
  }
  expect_fill(allocation,GUARD,0xa5);expect_fill(allocation+GUARD+PAGE_BYTES*2,GUARD,0xa5);
  expect_fill(scratch,GUARD,0x5a);expect_fill(scratch+GUARD+ACTIVE_BYTES,GUARD,0x5a);
 }
 free(assets);free(reference);free(allocation);free(scratch);free(inactive);
}
int main(int argc,char**argv) {
 if(argc==1)return native_entry();
 if(!strcmp(argv[1],"equivalence")){pixel_equivalence();return 0;}
 if(!strcmp(argv[1],"interrupt")) {
  struct fb_var_screeninfo var={0};int zero=0;
  mock_pan_interrupts=1;mock_vsync_interrupts=2;
  assert(display_ioctl(-1,FBIOPAN_DISPLAY,&var)==0&&!mock_pan_interrupts);
  assert(display_ioctl(-1,AICFB_WAIT_FOR_VSYNC,&zero)==0&&!mock_vsync_interrupts);
  stopped=1;mock_vsync_interrupts=1;
  assert(display_ioctl(-1,AICFB_WAIT_FOR_VSYNC,&zero)==-1&&errno==EINTR);
  return 0;
 }
 if(!strcmp(argv[1],"render")) {
  assert(argc==4);FILE*f=fopen(ASSET_PATH,"rb");assert(f);
  uint8_t *data=malloc(16+ASSET_BYTES),*frame=malloc(BG_BYTES),*canvas=malloc(ACTIVE_BYTES);assert(data&&frame&&canvas);
  assert(fread(data,1,16+ASSET_BYTES,f)==16+ASSET_BYTES);fclose(f);
  frame_state_t state={0};render(frame,W*4,data+16,canvas,(unsigned)strtoul(argv[2],NULL,10),&state);
  f=fopen(argv[3],"wb");assert(f);assert(fwrite(frame,1,BG_BYTES,f)==BG_BYTES);fclose(f);free(data);free(frame);free(canvas);return 0;
 }
 uint32_t elapsed=0;int result=startup_visual_acquire(&elapsed);
 if(!strcmp(argv[1],"client")){
  assert(result==1 && elapsed>0 && g_inherited_fb>=0);
  const char *ready=getenv("TEST_CLIENT_READY"),*release=getenv("TEST_CLIENT_RELEASE");assert(ready&&release);
  int fd=open(ready,O_CREAT|O_WRONLY,0600);assert(fd>=0);close(fd);
  for(unsigned i=0;access(release,F_OK)!=0;i++){assert(i<3000);usleep(1000);}
  assert(fcntl(g_inherited_fb,F_GETFD)>=0);
 }
 else if(!strcmp(argv[1],"busy"))assert(result==-1);
 else assert(result==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='un260-light-test-') as directory:
    tmp=Path(directory);src=tmp/'test.c';src.write_text(fixture);exe=tmp/'test'
    defines=['-DBOOT_WELCOME_MOTION_SCALE=0'] if os.environ.get('TEST_REDUCED_MOTION')=='1' else []
    for name,value in [('BOOT_LIGHT_SOCKET',tmp/'socket'),('BOOT_LIGHT_LOCK',tmp/'lock'),('BOOT_LIGHT_READY',tmp/'ready'),('ASSET_PATH',root/'aic_ui/generated_assets/boot_theme_c/boot-light.bin')]:
        defines.append('-D'+name+'="'+str(value)+'"')
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(root),*defines,str(src),'-Wl,--wrap=open','-Wl,--wrap=ioctl','-Wl,--wrap=sendmsg','-lz','-o',str(exe)],check=True)
    fb=tmp/'fb';fb.write_bytes(bytes(1280*400*4*2))
    env=dict(os.environ,TEST_FB=str(fb),UN260_BOOT_LIGHT_ACTIVE='1',UN260_BOOT_TRACE='1',TEST_CLIENT_READY=str(tmp/'client-ready'),TEST_CLIENT_RELEASE=str(tmp/'client-release'))
    subprocess.run([str(exe),'equivalence'],env=env,check=True)
    subprocess.run([str(exe),'interrupt'],env=env,check=True)
    print('PASS exact legacy pixels across phases, forward/backward jumps, stale pages, padded stride, guards and interrupted vsync')
    native=subprocess.Popen([str(exe)],env=env)
    client=None
    try:
        deadline=time.monotonic()+5
        while not (tmp/'ready').exists():
            assert native.poll() is None
            assert time.monotonic()<deadline
            time.sleep(.01)
        time.sleep(.1)
        client=subprocess.Popen([str(exe),'client'],env=env)
        assert native.wait(timeout=3)==0
        deadline=time.monotonic()+3
        while not (tmp/'client-ready').exists():
            assert client.poll() is None and time.monotonic()<deadline
            time.sleep(.005)
        # The client still owns the lease and inherited framebuffer reference.
        assert subprocess.run([str(exe)],env=env,timeout=2).returncode==1
        (tmp/'client-release').touch()
        assert client.wait(timeout=3)==0
        assert not (tmp/'ready').exists() and not (tmp/'socket').exists()
        print('PASS real handoff, inherited framebuffer lifetime, single writer, cleanup')
    finally:
        (tmp/'client-release').touch()
        if client is not None and client.poll() is None:
            client.wait(timeout=3)
        if native.poll() is None:native.terminate();native.wait(timeout=3)
    subprocess.run([str(exe),'fallback'],env=env,check=True)
    legacy=subprocess.Popen([str(exe)],env=dict(env,TEST_LEGACY_REPLY='1'))
    try:
        deadline=time.monotonic()+3
        while not (tmp/'ready').exists():
            assert legacy.poll() is None and time.monotonic()<deadline
            time.sleep(.01)
        subprocess.run([str(exe),'fallback'],env=env,check=True)
        assert legacy.wait(timeout=3)==0
        print('PASS old visual revision does not adopt incompatible elapsed time')
    finally:
        if legacy.poll() is None:
            legacy.terminate();legacy.wait(timeout=3)
    with (tmp/'lock').open('r+') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        subprocess.run([str(exe),'busy'],env=env,check=True)
        off=subprocess.run([str(exe)],env=dict(env,UN260_BOOT_FRAME_TRACE='0'),capture_output=True,text=True)
        assert off.returncode==1 and 'BOOT_FRAMES ' not in off.stderr
        on=subprocess.run([str(exe)],env=dict(env,UN260_BOOT_FRAME_TRACE='1'),capture_output=True,text=True)
        assert on.returncode==1 and on.stderr.count('BOOT_FRAMES ')==1 and 'frames=0 ' in on.stderr
    print('PASS missing renderer fallback and refusal to draw with an active owner')
    output=Path('/tmp/un260-boot-light-20260914/raster');output.mkdir(parents=True,exist_ok=True)
    for name,elapsed in [('brand',2000),('welcome',4300),('waiting',5000)]:
        raw=tmp/'frame.raw';subprocess.run([str(exe),'render',str(elapsed),str(raw)],check=True)
        image=Image.frombytes('RGBA',(1280,400),raw.read_bytes(),'raw','BGRA').convert('RGB')
        image.save(output/(name+'.png'))
    print('PASS native raster frames generated for comparison (not physical display FPS)')
