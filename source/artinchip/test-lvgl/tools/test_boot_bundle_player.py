#!/usr/bin/env python3
"""Exercise real unified server/client, bounded theme decoding and pixels.

The framebuffer is a mmap file, and only the hardware ioctls are substituted.
Socket messages, FD transfer, ownership locks, loader and the actual two
renderer paths are production code. This does not verify panel timing.
"""
from pathlib import Path
import array
import fcntl
import hashlib
import importlib.util
import json
import os
import socket
import struct
import subprocess
import tempfile
import time
import zlib
from PIL import Image

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_boot_bundle', root / 'tools/build_boot_bundle.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

fixture = r'''
#define _GNU_SOURCE
#define BOOT_LIGHT_HOST_TEST
#define UN260_BOOT_BUNDLE
#define main native_entry
#include "un260/lv_drivers/boot_light.c"
#undef main
#include "un260/lv_drivers/startup_visual.c"
#include <assert.h>
#include <stdarg.h>
void backlight_service_init(void) {}
int __real_open(const char*,int,...);
int __wrap_open(const char *path,int flags,...) {
 mode_t mode=0;if(flags&O_CREAT){va_list ap;va_start(ap,flags);mode=va_arg(ap,int);va_end(ap);}
 return __real_open(!strcmp(path,"/dev/fb0")?getenv("TEST_FB"):path,flags,mode);
}
int __real_clock_gettime(clockid_t,struct timespec*);
int __wrap_clock_gettime(clockid_t id,struct timespec *result) {
 int status=__real_clock_gettime(id,result);const char *path=getenv("TEST_CLOCK_PATH");
 if(status==0&&id==CLOCK_MONOTONIC&&path){
  char text[32]={0};int fd=__real_open(path,O_RDONLY);assert(fd>=0);
  ssize_t size=read(fd,text,sizeof(text)-1);close(fd);assert(size>0);
  unsigned long milliseconds=strtoul(text,NULL,10);
  result->tv_sec+=(time_t)(milliseconds/1000UL);result->tv_nsec+=(long)(milliseconds%1000UL)*1000000L;
  if(result->tv_nsec>=1000000000L){result->tv_nsec-=1000000000L;result->tv_sec++;}
 }
 return status;
}
int __wrap_ioctl(int fd,unsigned long request,...) {
 (void)fd;va_list ap;va_start(ap,request);void *p=va_arg(ap,void*);va_end(ap);
 if(request==FBIOGET_FSCREENINFO){struct fb_fix_screeninfo*f=p;memset(f,0,sizeof(*f));f->line_length=W*4;f->smem_len=BG_BYTES*2;return 0;}
 if(request==FBIOGET_VSCREENINFO){struct fb_var_screeninfo*v=p;memset(v,0,sizeof(*v));v->xres=W;v->yres=H;v->yres_virtual=H*2;v->bits_per_pixel=32;v->red.offset=16;v->green.offset=8;return 0;}
 if(request==FBIOPAN_DISPLAY){int n=__real_open(getenv("TEST_PANS"),O_WRONLY|O_CREAT|O_APPEND,0600);assert(n>=0);assert(write(n,"p",1)==1);close(n);return 0;}
 if(request==AICFB_WAIT_FOR_VSYNC){usleep(16000);return 0;}
 return -1;
}
int main(int argc,char**argv) {
 if(argc>1&&(!strcmp(argv[1],"render")||!strcmp(argv[1],"render-sequence"))) {
  assert(argc==6);boot_theme_pack_t pack={0};assert(boot_theme_pack_load_path(argv[2],&pack));
  assert(boot_theme_pack_select_language(&pack,argv[3]));
  assert(boot_theme_pack_freeze(&pack));assert(pack.data_bytes==(pack.mode==BOOT_THEME_MODE_LOGO?BOOT_THEME_BG_BYTES:BOOT_THEME_BASE_BYTES));
  assert(!boot_theme_pack_select_language(&pack,"ja"));
  enum {GUARD=64,STRIDE=W*4+64,PAGE=STRIDE*H};
  uint8_t *memory=malloc(PAGE*2+GUARD*2),*scratch=malloc(ACTIVE_STRIDE*pack.scene.active_h+GUARD*2);
  assert(memory&&scratch);memset(memory,0xa5,PAGE*2+GUARD*2);memset(scratch,0x5a,ACTIVE_STRIDE*pack.scene.active_h+GUARD*2);
  uint8_t *pixels=memory+GUARD,*canvas=scratch+GUARD;frame_state_t states[2]={{0},{0}};
  const char *cursor=argv[4];char *end=NULL;
  do {
   unsigned elapsed=(unsigned)strtoul(cursor,&end,10);assert(end!=cursor&&(!*end||*end==','));
   for(unsigned page=0;page<2;page++) {
    render_bundle(pixels+PAGE*page,STRIDE,&pack,canvas,elapsed,&states[page]);
    for(unsigned row=0;row<H;row++)for(unsigned col=W*4;col<STRIDE;col++)assert(pixels[PAGE*page+row*STRIDE+col]==0xa5);
   }
   if(!*end)break;
   assert(!strcmp(argv[1],"render-sequence"));cursor=end+1;
  }while(1);
  assert(!memcmp(pixels,pixels+PAGE,PAGE));
  for(unsigned i=0;i<GUARD;i++){assert(memory[i]==0xa5&&memory[GUARD+PAGE*2+i]==0xa5);assert(scratch[i]==0x5a&&scratch[GUARD+ACTIVE_STRIDE*pack.scene.active_h+i]==0x5a);}
  FILE*f=fopen(argv[5],"wb");assert(f);for(unsigned row=0;row<H;row++)assert(fwrite(pixels+row*STRIDE,1,W*4,f)==W*4);fclose(f);
  free(memory);free(scratch);boot_theme_pack_free(&pack);return 0;
 }
 if(argc>1&&!strcmp(argv[1],"client")) {
  assert(argc==3);boot_light_status_t status={0};assert(startup_visual_begin(&status)==1);
  assert(status.flags&BOOT_LIGHT_FIRST_FRAME);assert(g_inherited_fb<0);
  assert(startup_visual_poll(false,false)==0);
  bool error=!strcmp(argv[2],"error");int result=0;
  for(unsigned i=0;result==0;i++){assert(i<300);result=startup_visual_poll(true,error);usleep(10000);}
  assert(result==1&&g_inherited_fb>=0&&fcntl(g_inherited_fb,F_GETFD)>=0);
  assert(flock(g_lease,LOCK_EX|LOCK_NB)==0);return 0;
 }
 return native_entry(argc,argv);
}
'''

legacy = r'''
#define _GNU_SOURCE
#define BOOT_LIGHT_HOST_TEST
#define main unused_native_entry
#include "un260/lv_drivers/boot_light.c"
#undef main
#include <assert.h>
void backlight_service_init(void) {}
int main(int argc,char**argv) {
 assert(argc==4);uint8_t *assets=malloc(ASSET_BYTES),*pixels=malloc(BG_BYTES),*canvas=malloc(ACTIVE_BYTES);assert(assets&&pixels&&canvas);
 uLongf length=ASSET_BYTES;assert(uncompress(assets,&length,boot_light_packed,sizeof(boot_light_packed))==Z_OK&&length==ASSET_BYTES);
 unsigned language=0;for(unsigned i=0;i<BOOT_WELCOME_COUNT;i++)if(!strcmp(boot_welcome_tags[i],argv[1]))language=i;
 boot_welcome_native_apply(assets+BG_BYTES+ICON_BYTES+TEXT_BYTES,language);
 frame_state_t state={0};render(pixels,W*4,assets,canvas,(unsigned)strtoul(argv[2],NULL,10),&state);
 FILE*f=fopen(argv[3],"wb");assert(f);assert(fwrite(pixels,1,BG_BYTES,f)==BG_BYTES);fclose(f);free(assets);free(pixels);free(canvas);return 0;
}
'''


def run(args, **kwargs):
    return subprocess.run([str(x) for x in args], check=True, **kwargs)


with tempfile.TemporaryDirectory(prefix='un260-boot-bundle-player-') as directory:
    tmp = Path(directory)
    source = tmp / 'fixture.c'
    source.write_text(fixture)
    defines = []
    for key, value in [('BOOT_LIGHT_SOCKET', tmp / 'socket'), ('BOOT_LIGHT_LOCK', tmp / 'lock'),
                       ('BOOT_LIGHT_READY', tmp / 'ready'), ('BOOT_LIGHT_UNIFIED_READY', tmp / 'modern'),
                       ('BOOT_WELCOME_LOCALE_PATH', tmp / 'language')]:
        defines.append('-D' + key + '="' + str(value) + '"')
    executable = tmp / 'player'
    run(['cc', '-std=c11', '-O2', '-static', '-Wall', '-Wextra', '-Werror', '-I' + str(root), *defines,
         source, root / 'un260/lv_drivers/boot_theme_pack.c', '-Wl,--wrap=open', '-Wl,--wrap=ioctl',
         '-Wl,--wrap=clock_gettime', '-lz', '-o', executable])
    (tmp / 'legacy.c').write_text(legacy)
    packs = {}
    for theme in ['c', 'd']:
        payload, info = builder.theme_pack(root, theme)
        path = tmp / (theme + '.pack')
        path.write_bytes(payload)
        packs[theme] = path
        run([executable, '--theme', path, '--check'])
        reference = tmp / ('legacy-' + theme)
        run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-DUI_BOOT_ANIM_THEME=' + ('3' if theme == 'c' else '4'),
             '-I' + str(root), tmp / 'legacy.c', '-lz', '-o', reference])
        # Every language at representative entrance, settled and dot stages.
        for language in info['languages']:
            for elapsed in [450, 1500, 2300, 2850, 7900]:
                run([executable, 'render', path, language, elapsed, tmp / 'new.bgra'])
                run([reference, language, elapsed, tmp / 'old.bgra'])
                assert (tmp / 'new.bgra').read_bytes() == (tmp / 'old.bgra').read_bytes(), (theme, language, elapsed)
        print('PASS pixels, both pages, stride and memory guards:', theme, len(info['languages']), 'languages')
    # The preserved production legacy renderer is the pixel reference for
    # all three original D cycles, including stagger/start/end boundaries.
    d_reference = tmp / 'legacy-d'
    sample_times = sorted(set(range(2500, 7901, 15)) |
                          {450, 1499, 1500, 2299, 2300, 2499, 2500, 7899, 7900} |
                          {2500 + cycle * 1800 + edge
                           for cycle in range(3)
                           for edge in (0, 1, 149, 150, 299, 300, 899, 900,
                                        1049, 1050, 1199, 1200, 1799)})
    for elapsed in sample_times:
        run([executable, 'render', packs['d'], 'en', elapsed, tmp / 'new.bgra'])
        run([d_reference, 'en', elapsed, tmp / 'old.bgra'])
        assert (tmp / 'new.bgra').read_bytes() == (tmp / 'old.bgra').read_bytes(), elapsed
    print('PASS D first three cycles exact legacy pixels:', len(sample_times), 'sample/phase boundaries')

    # Exercise both persistent buffers, so the optimized dots-only path must
    # clear previous lifted pixels and preserve the already settled lettering.
    for elapsed in (10000, 20000, 40000):
        cycle_start = 2500 + ((elapsed - 2500) // 1800) * 1800
        frames = []
        for phase in (0, 1, 150, 300, 450, 900, 1200, 1799):
            when = cycle_start + phase
            sequence = ','.join(map(str, (450, 2300, 2500, 4300, 7900, when)))
            run([executable, 'render-sequence', packs['d'], 'en', sequence, tmp / 'new.bgra'])
            run([d_reference, 'en', 4300 + phase, tmp / 'old.bgra'])
            new_frame = (tmp / 'new.bgra').read_bytes()
            assert new_frame == (tmp / 'old.bgra').read_bytes(), (elapsed, phase)
            frames.append(new_frame)
        assert frames[0] != frames[4], ('waiting dots stopped', elapsed)
        run([executable, 'render-sequence', packs['d'], 'en',
             f'450,2300,{cycle_start + 1799},{cycle_start + 1800}', tmp / 'new.bgra'])
        assert (tmp / 'new.bgra').read_bytes() == frames[0], ('cycle wrap', elapsed)
    print('PASS D waits keep repeated motion at 10/20/40s; both buffers, dirty area, stagger and wrap pixels')

    logo_payload, _ = builder.theme_pack(root, 'd', 'logo')
    logo_path = tmp / 'logo.pack'
    logo_path.write_bytes(logo_payload)
    logo_offset = struct.unpack_from('<I', logo_payload, 52)[0]
    logo_raw = zlib.decompress(logo_payload[builder.HEADER_BYTES:])
    for elapsed in (450, 10000, 20000, 40000):
        run([executable, 'render-sequence', logo_path, 'en',
             '450,' + str(elapsed), tmp / 'logo.bgra'])
        assert (tmp / 'logo.bgra').read_bytes() == logo_raw[logo_offset:logo_offset + builder.BG_BYTES]
    print('PASS Logo exact surface and immutable, compact mode/locale snapshot')
    # Actual code publication is deterministic and rejects rewritten versions.
    version, manifest_sha, _ = builder.publish(executable, tmp / 'published', root)
    assert builder.publish(executable, tmp / 'published', root)[:2] == (version, manifest_sha)
    manifest = tmp / 'published/versions' / version / 'manifest'
    values = dict(line.split('=', 1) for line in manifest.read_text().splitlines())
    identity_material = ('player_sha256=' + values['player_sha256'] + '\ntheme_sha256=' + values['theme_sha256'] + '\nmode=' + values['mode'] + '\n').encode('ascii')
    assert hashlib.sha256(identity_material).hexdigest() == version
    logo_version, _, _ = builder.publish(executable, tmp / 'published', root, mode='logo')
    assert logo_version != version
    manifest.write_bytes(b'corrupt')
    try:
        builder.publish(executable, tmp / 'published', root)
        raise AssertionError('immutable version overwrite accepted')
    except AssertionError as error:
        assert 'collision' in str(error)
    print('PASS deterministic content version and immutable collision')
    configured = tmp / 'source'
    (configured / 'un260/lv_core/boot_anim').mkdir(parents=True)
    header = configured / 'un260/lv_core/boot_anim/boot_theme_config.h'
    header.write_text('#ifndef UI_BOOT_ANIM_THEME\n#define UI_BOOT_ANIM_THEME UI_BOOT_ANIM_THEME_D\n#endif\n')
    configuration = tmp / 'config.json'
    def config(value):
        configuration.write_text(json.dumps(value))
        return builder.read_config(configured, configuration)
    assert config({'schema': 1, 'mode': 'logo', 'logo_surface': None}) == ('d', 'logo', None)
    for invalid in [{'schema': True, 'mode': 'logo', 'logo_surface': None},
                    {'schema': 1, 'mode': 'animated', 'logo_surface': None},
                    {'schema': 1, 'mode': 'logo', 'logo_surface': '../outside.png'},
                    {'schema': 1, 'mode': 'logo', 'logo_surface': '/tmp/logo.png'},
                    {'schema': 1, 'mode': 'logo', 'logo_surface': 'x.png', 'theme': 'd'}]:
        try:
            config(invalid)
            raise AssertionError('invalid persistent configuration accepted')
        except (ValueError, FileNotFoundError):
            pass
    Image.new('RGB', (1280, 400)).save(configured / 'logo.png')
    assert config({'schema': 1, 'mode': 'logo', 'logo_surface': 'logo.png'})[2] == (configured / 'logo.png').resolve()
    Image.new('RGB', (1279, 400)).save(configured / 'wrong.png')
    try:
        config({'schema': 1, 'mode': 'logo', 'logo_surface': 'wrong.png'})
        raise AssertionError('invalid logo dimensions accepted')
    except ValueError:
        pass
    header.write_text('#ifndef UI_BOOT_ANIM_THEME\n#define UI_BOOT_ANIM_THEME UI_BOOT_ANIM_THEME_A\n#endif\n')
    try:
        config({'schema': 1, 'mode': 'animation', 'logo_surface': None})
        raise AssertionError('unsupported source theme silently defaulted')
    except ValueError:
        pass
    print('PASS persistent mode/logo configuration, paths, dimensions and source theme ownership')
    good = packs['d'].read_bytes()
    mutations = [('truncated', good[:-1]), ('trailing', good + b'x')]
    damaged = bytearray(good)
    damaged[-1] ^= 1
    mutations.append(('compressed corruption', bytes(damaged)))
    for offset, value, name in [(32, 0xffffffff, 'raw budget'), (36, 0xffffffff, 'packed budget'),
                                (56, 0xffffffff, 'table bounds'), (68, 0xffffffff, 'hold bounds'),
                                (140, 0xffffffff, 'text overflow'), (144, 0xffffffff, 'dot overflow')]:
        damaged = bytearray(good)
        struct.pack_into('<I', damaged, offset, value)
        mutations.append((name, bytes(damaged)))
    # A correctly compressed/CRC'd payload can still have unsafe metadata.
    unsafe = bytearray(zlib.decompress(good[builder.HEADER_BYTES:]))
    table_offset = struct.unpack_from('<I', good, 56)[0]
    struct.pack_into('<I', unsafe, table_offset + 16, 0xffffffff)
    compressed = zlib.compress(unsafe, 9)
    header = bytearray(good[:builder.HEADER_BYTES])
    struct.pack_into('<II', header, 36, len(compressed), zlib.crc32(unsafe))
    mutations.append(('native bounds after valid CRC', bytes(header) + compressed))
    for name, payload in mutations:
        path = tmp / 'bad.pack'
        path.write_bytes(payload)
        result = subprocess.run([str(executable), '--theme', str(path), '--check'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        assert result.returncode != 0, name
    theme_fd = os.open(packs['d'], os.O_RDONLY)
    run([executable, '--theme-fd', theme_fd, '--check'], pass_fds=(theme_fd,))
    os.close(theme_fd)
    assert subprocess.run([str(executable), '--theme', str(packs['d']), '--mode', 'logo', '--check'],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0
    assert subprocess.run([str(executable), '--theme', str(logo_path), '--mode', 'animation'],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0
    print('PASS corrupt/truncated/budget/geometry bounds and inherited theme FD')
    (tmp / 'fb').write_bytes(bytes(1280 * 400 * 4 * 2))
    environment = os.environ.copy()
    environment.update(TEST_FB=str(tmp / 'fb'), TEST_PANS=str(tmp / 'pans'), TEST_CLOCK_PATH=str(tmp / 'clock'))

    def set_clock(milliseconds):
        temporary_clock = tmp / 'clock.new'
        temporary_clock.write_text(str(milliseconds) + '\n')
        temporary_clock.replace(tmp / 'clock')

    def start(mode='animation'):
        set_clock(0)
        for name in ['ready', 'modern', 'socket', 'pans']:
            (tmp / name).unlink(missing_ok=True)
        (tmp / 'language').write_text('en\n')
        player = subprocess.Popen([str(executable), '--theme', str(logo_path if mode == 'logo' else packs['d']), '--mode', mode,
                                   '--bundle-id', 'a' * 64], env=environment)
        for _ in range(200):
            if (tmp / 'ready').exists():
                break
            assert player.poll() is None
            time.sleep(.01)
        else:
            player.terminate()
            raise AssertionError('no confirmed native first frame')
        assert (tmp / 'modern').read_text() == (tmp / 'ready').read_text() == str(player.pid) + '\n'
        return player

    def exchange(command):
        with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as client:
            client.connect(str(tmp / 'socket'))
            client.send(command)
            client.settimeout(1)
            reply, controls, flags, _ = client.recvmsg(64, socket.CMSG_SPACE(4))
            assert len(reply) == 64 and not flags
            values = struct.unpack('<6IQ32s', reply)
            fds = []
            for level, kind, data in controls:
                assert level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS
                received = array.array('i')
                received.frombytes(data)
                fds.extend(received)
            return values, fds

    player = start()
    first, fds = exchange(b'P')
    assert first[2] == 1 and first[3] == 1 and first[4] == 0 and not fds
    assert first[7] == bytes.fromhex('a' * 64)
    (tmp / 'language').write_text('ja\n')
    early, fds = exchange(b'R')
    assert early[2] == 1 and early[4] == 0 and early[6] == first[6] and not fds
    pans_before = len((tmp / 'pans').read_bytes())
    for _ in range(6):
        exchange(b'P')
        time.sleep(.02)
    assert len((tmp / 'pans').read_bytes()) > pans_before, 'probe traffic starved render'
    run([executable, 'client', 'ready'], env=environment)
    assert player.wait(timeout=3) == 0
    assert not (tmp / 'modern').exists() and not (tmp / 'ready').exists()
    print('PASS real begin/poll ready handoff, stable locale/identity, FD and lease')
    player = start()
    run([executable, 'client', 'error'], env=environment)
    assert player.wait(timeout=2) == 0
    print('PASS real error handoff bypasses animation minimum')
    player = start('logo')
    time.sleep(.15)
    assert len((tmp / 'pans').read_bytes()) == 2, 'static logo kept refreshing'
    run([executable, 'client', 'ready'], env=environment)
    assert player.wait(timeout=2) == 0
    assert not (tmp / 'modern').exists() and not (tmp / 'ready').exists()
    print('PASS static Logo once per buffer and immediate readiness handoff')
    for mode in ['animation', 'logo']:
        player = start(mode)
        initial, fds = exchange(b'P')
        assert not fds
        set_clock(25000)
        time.sleep(.08)
        assert player.poll() is None, 'unified player expired while the UI was still preparing'
        delayed, fds = exchange(b'P')
        assert not fds and delayed[5] >= 25000 and delayed[6] == initial[6]
        assert (tmp / 'modern').read_text() == (tmp / 'ready').read_text() == str(player.pid) + '\n'
        run([executable, 'client', 'ready'], env=environment)
        assert player.wait(timeout=2) == 0
    print('PASS animation and Logo keep one owner beyond 20s until readiness handoff (fake clock)')
    for phase in (0, 450, 1799):
        for action in ('ready', 'error'):
            player = start()
            set_clock(12000 + phase)
            probe, fds = exchange(b'P')
            assert not fds and probe[2] == 1
            run([executable, 'client', action], env=environment)
            assert player.wait(timeout=2) == 0
            assert not (tmp / 'ready').exists() and not (tmp / 'modern').exists()
    print('PASS R/E handoff during repeated waiting phases without an extra animation delay')
    # Keep the existing bounded lifetime for the standalone legacy renderer.
    hardware = fixture[fixture.index('int __real_open'):fixture.index('int main(int argc,char**argv)')]
    legacy_server_source = r'''
#define _GNU_SOURCE
#define BOOT_LIGHT_HOST_TEST
#define main native_entry
#include "un260/lv_drivers/boot_light.c"
#undef main
#include <assert.h>
#include <stdarg.h>
void backlight_service_init(void) {}
''' + hardware + 'int main(void){return native_entry();}\n'
    (tmp / 'legacy-server.c').write_text(legacy_server_source)
    legacy_server = tmp / 'legacy-server'
    run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I' + str(root), *defines,
         tmp / 'legacy-server.c', '-Wl,--wrap=open', '-Wl,--wrap=ioctl',
         '-Wl,--wrap=clock_gettime', '-lz', '-o', legacy_server])
    for name in ['ready', 'modern', 'socket', 'pans']:
        (tmp / name).unlink(missing_ok=True)
    set_clock(0)
    legacy_player = subprocess.Popen([str(legacy_server)], env=environment)
    for _ in range(200):
        if (tmp / 'ready').exists():
            break
        assert legacy_player.poll() is None
        time.sleep(.01)
    else:
        legacy_player.terminate()
        raise AssertionError('no legacy first frame')
    set_clock(25000)
    assert legacy_player.wait(timeout=2) == 1
    assert not (tmp / 'ready').exists() and not (tmp / 'modern').exists()
    print('PASS legacy keeps its 20s bounded lifetime (fake clock)')
    player = start('logo')
    player.terminate()
    assert player.wait(timeout=2) != 0
    assert not (tmp / 'modern').exists() and not (tmp / 'ready').exists() and not (tmp / 'socket').exists()
    lease = os.open(tmp / 'lock', os.O_RDWR)
    fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
    os.close(lease)
    print('PASS termination removes owned IPC and releases display lease')
print('PASS unified boot bundle player; host tests are not board timing verification')
