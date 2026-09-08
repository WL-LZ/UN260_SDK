#!/usr/bin/env python3
"""Host fault-injection tests of actual cache and decoder attempt source.
Not a board DMA/GE or visual test. Artifacts remain in /tmp for inspection.
"""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[1]
sdk = root.parents[2]
work = Path(tempfile.mkdtemp(prefix='un260-image-memory-'))

def run(name, source):
    path=work/(name+'.c');path.write_text(source)
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function','-fsanitize=address,undefined',str(path),'-o',str(work/name)],check=True)
    subprocess.run([str(work/name)],check=True)

cache=(sdk/'source/third-party/lvgl-8.3.2/src/draw/lv_img_cache.c').read_text()
cache='\n'.join(x for x in cache.splitlines() if not x.startswith('#include'))
stub=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define LV_IMG_CACHE_DEF_SIZE 16
#define LV_UNUSED(x) (void)(x)
#define LV_LOG_WARN(...) ((void)0)
#define LV_LOG_INFO(...) ((void)0)
#define LV_LOG_TRACE(...) ((void)0)
#define LV_ASSERT_MALLOC(x) assert(x)
#define lv_mem_alloc malloc
#define lv_mem_free free
#define lv_memset_00(p,n) memset(p,0,n)
#define LV_GC_ROOT(x) x
typedef int lv_res_t;
enum {LV_RES_INV=0,LV_RES_OK=1,LV_IMG_SRC_FILE,LV_IMG_SRC_VARIABLE};
typedef struct {unsigned full;}lv_color_t;
typedef int lv_img_src_t;
typedef struct {const void *src;lv_color_t color;int frame_id;unsigned time_to_open;
    struct {unsigned w,h;}header;unsigned bytes;}lv_img_decoder_dsc_t;
typedef struct {lv_img_decoder_dsc_t dec_dsc;int32_t life;bool retained;uint16_t pins;bool invalidate_pending;}_lv_img_cache_entry_t;
typedef uint32_t (*lv_img_cache_memory_cb_t)(const lv_img_decoder_dsc_t*);
static _lv_img_cache_entry_t *_lv_img_cache_array;
static unsigned closed, fake_bytes;
static bool pressure_in_open;
void lv_img_cache_invalidate_src(const void*);
uint32_t lv_img_cache_reclaim_bytes(uint32_t);
static lv_img_src_t lv_img_src_get_type(const void *p){return p?LV_IMG_SRC_FILE:0;}
static unsigned lv_tick_get(void){return 10;}
static unsigned lv_tick_elaps(unsigned t){return 10-t;}
static void lv_img_decoder_close(lv_img_decoder_dsc_t *d){assert(d->src);++closed;d->src=NULL;d->bytes=0;}
static int lv_img_decoder_open(lv_img_decoder_dsc_t *d,const void *src,lv_color_t c,int f){
    d->src=src;d->color=c;d->frame_id=f;d->bytes=fake_bytes;
    if(pressure_in_open){unsigned before=closed;lv_img_cache_reclaim_bytes(UINT32_MAX);assert(d->src==src);assert(closed==before);}
    return LV_RES_OK;
}
'''
test=r'''
static uint32_t bytes(const lv_img_decoder_dsc_t*d){return d->bytes;}
static void seed(unsigned i,unsigned amount,int life){_lv_img_cache_array[i]=(_lv_img_cache_entry_t){0};
    _lv_img_cache_array[i].dec_dsc.src="image";_lv_img_cache_array[i].dec_dsc.bytes=amount;_lv_img_cache_array[i].life=life;}
int main(void){
    lv_img_cache_set_size(4);lv_img_cache_set_memory_cb(bytes);
    seed(0,4*1024*1024,10);seed(1,4*1024*1024,20);
    _lv_img_cache_array[0].retained=true;
    assert(lv_img_cache_reserve_bytes(2*1024*1024,8*1024*1024));
    assert(closed==1 && !_lv_img_cache_array[0].dec_dsc.src);
    lv_img_cache_pin(&_lv_img_cache_array[1]);
    assert(!lv_img_cache_reserve_bytes(6*1024*1024,8*1024*1024));
    assert(lv_img_cache_reclaim_bytes(UINT32_MAX)==0);
    lv_img_cache_invalidate_src(NULL);assert(_lv_img_cache_array[1].dec_dsc.src && closed==1);
    lv_img_cache_set_size(8);assert(entry_cnt==4);
    lv_img_cache_unpin(&_lv_img_cache_array[1]);assert(closed==2);
    for(int i=0;i<4;i++){seed(i,4096,i);lv_img_cache_pin(&_lv_img_cache_array[i]);}
    assert(_lv_img_cache_open("new",(lv_color_t){0},0)==NULL);
    for(int i=0;i<4;i++)lv_img_cache_unpin(&_lv_img_cache_array[i]);
    assert(lv_img_cache_reclaim_bytes(UINT32_MAX)==16384);
    assert(!lv_img_cache_reserve_bytes(UINT32_MAX,8*1024*1024));
    pressure_in_open=true;fake_bytes=4096;
    _lv_img_cache_entry_t *e=_lv_img_cache_open("active",(lv_color_t){0},0);
    assert(e && !e->pins && e->dec_dsc.bytes==4096);
    assert(lv_img_cache_memory_used()==4096);
    lv_img_cache_invalidate_src(NULL);free(_lv_img_cache_array);
    puts("cache PASS: actual-byte callback, retained eviction, budget, pinned/opening exclusion, deferred invalidation, resize guard");
}
'''
run('cache',stub+cache+test)

src=(root/'aic_ui/aic_dec.c').read_text()
attempt=src[src.index('static lv_res_t aic_decoder_attempt('):src.index('/* Small failure backoff')]
wrapper=src[src.index('/* Small failure backoff'):src.index('static void aic_decoder_close(')]
decoder_stub=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
enum {LV_RES_INV=0,LV_RES_OK=1,LV_FS_RES_OK=0,LV_FS_MODE_RD=2};
enum mpp_codec_type {MPP_CODEC_VIDEO_DECODER_PNG,MPP_CODEC_VIDEO_DECODER_MJPEG};
enum {MPP_FMT_ARGB_8888,MPP_FMT_RGB_888,MPP_FMT_YUV420P,MPP_FMT_YUV422P,MPP_FMT_YUV444P,MPP_FMT_YUV400};
enum {MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR,PACKET_FLAG_EOS,LV_IMG_CF_TRUE_COLOR_ALPHA,LV_IMG_CF_TRUE_COLOR};
#define AIC_IMAGE_CACHE_BUDGET (8U*1024U*1024U)
typedef int lv_res_t;typedef int lv_fs_file_t;
typedef struct {const char *src;const unsigned char *img_data;struct {int cf;}header;}lv_img_decoder_dsc_t;
struct mpp_frame {struct {struct {int width,height;}size;int format,stride[3];}buf;};
struct frame_allocator {int dummy;};
struct mpp_packet {void *data;unsigned size,flag;};
struct decode_config {int pix_fmt,bitstream_buffer_size,extra_frame_num,packet_count;};
struct mpp_decoder {void *pm;unsigned char *packet;unsigned capacity;};
static const char *inject;
static unsigned files,heaps,dma,decs,allocation_attempts;
static struct mpp_frame *live_output;
static bool match(const char *s){return inject && !strcmp(inject,s);}
static uint64_t app_clock_monotonic_us(void){return 1;}
static unsigned app_clock_elapsed_us32(uint64_t a,uint64_t b){return b-a;}
static bool perf_profile_is_enabled(void){return false;}
static void perf_profile_report_image_decode(const char *s,unsigned t,unsigned b){(void)s;(void)t;(void)b;}
static int lv_fs_open(lv_fs_file_t *f,const char*s,int m){(void)f;(void)s;(void)m;if(match("file-open"))return 1;files++;return 0;}
static void lv_fs_close(lv_fs_file_t *f){(void)f;assert(files==1);files--;}
static int png_get_img_size(lv_fs_file_t*f,int*w,int*h,int*fmt){(void)f;*w=1230;*h=369;*fmt=MPP_FMT_ARGB_8888;return match("header");}
static int jpeg_get_img_size(lv_fs_file_t*f,int*w,int*h,int*fmt){return png_get_img_size(f,w,h,fmt);}
static int get_file_size(lv_fs_file_t*f,unsigned*n){(void)f;*n=16384;return match("file-size");}
static uint32_t frame_dma_bytes(const struct mpp_frame *f){return f->buf.stride[0]*f->buf.size.height;}
static bool lv_img_cache_reserve_bytes(unsigned b,unsigned m){assert(b==1818432 && m==8388608);return !match("cache-budget");}
static int dmabuf_device_open(void){if(match("heap-open")){errno=ENOMEM;return -1;}heaps++;return 9;}
static void dmabuf_device_close(int h){assert(h==9 && heaps==1);heaps--;}
static int mpp_buf_alloc(int h,void*b){(void)b;assert(h==9);allocation_attempts++;if(match("output-dma")){errno=ENOMEM;return -1;}dma++;return 0;}
static void mpp_buf_free(void*b){(void)b;assert(!decs && dma==1);dma--;}
static struct mpp_decoder *mpp_decoder_create(enum mpp_codec_type t){(void)t;if(match("decoder-create"))return NULL;decs++;return calloc(1,sizeof(struct mpp_decoder));}
static struct frame_allocator *open_allocator(struct mpp_frame*f){live_output=f;if(match("allocator"))return NULL;return calloc(1,sizeof(struct frame_allocator));}
static int mpp_decoder_control(struct mpp_decoder*d,int c,void*a){(void)d;(void)c;(void)a;return match("decoder-control")?-1:0;}
static int mpp_decoder_init(struct mpp_decoder*d,struct decode_config*c){d->capacity=c->bitstream_buffer_size;d->pm=match("pm-null")?NULL:d;return match("decoder-init")?-1:0;}
static int mpp_decoder_get_packet(struct mpp_decoder*d,struct mpp_packet*p,unsigned n){assert(d->pm && d->capacity>=n+8);d->packet=malloc(d->capacity);assert(d->packet);memset(d->packet,0xA5,d->capacity);p->data=match("packet-null")?NULL:d->packet;return match("packet")?1:0;}
static int lv_fs_read(lv_fs_file_t*f,void*d,unsigned n,unsigned*r){(void)f;(void)d;*r=match("short-read")?n-1:n;return match("file-read")?1:0;}
static int mpp_decoder_put_packet(struct mpp_decoder*d,struct mpp_packet*p){assert(p->data==d->packet);for(unsigned i=0;i<8;i++)assert(d->packet[p->size+i]==0);return match("put-packet")?1:0;}
static int mpp_decoder_decode(struct mpp_decoder*d){(void)d;return match("decode")?-1:0;}
static int mpp_decoder_get_frame(struct mpp_decoder*d,struct mpp_frame*f){(void)d;(void)f;return match("get-frame")?1:0;}
static int mpp_decoder_put_frame(struct mpp_decoder*d,struct mpp_frame*f){(void)d;(void)f;return match("put-frame")?1:0;}
static void mpp_decoder_destory(struct mpp_decoder*d){assert(decs==1 && dma==1 && live_output);decs--;free(d->packet);free(d);}
typedef int lv_img_decoder_t;
typedef struct {unsigned stride,height;}un260_compiled_asset_t;
static const un260_compiled_asset_t *un260_compiled_asset_find(const void*s){(void)s;return NULL;}
static int compiled_asset_open(lv_img_decoder_dsc_t*d,const un260_compiled_asset_t*a){(void)d;(void)a;return LV_RES_INV;}
static unsigned now,reclaim_calls,freed_on_reclaim;
static bool recover_on_reclaim;
static unsigned lv_tick_get(void){return now;}
static unsigned lv_tick_elaps(unsigned t){return now-t;}
static unsigned lv_img_cache_memory_used(void){return 0;}
static unsigned lv_img_cache_reclaim_bytes(unsigned n){(void)n;reclaim_calls++;if(recover_on_reclaim)inject=NULL;return freed_on_reclaim;}
'''
decoder_test=r'''
int main(void){
    const char*failures[]={"file-open","header","file-size","cache-budget","heap-open","output-dma",
        "decoder-create","allocator","decoder-control","decoder-init","pm-null","packet","packet-null",
        "file-read","short-read","put-packet","decode","get-frame","put-frame"};
    for(unsigned i=0;i<sizeof(failures)/sizeof(*failures);i++){
        inject=failures[i];lv_img_decoder_dsc_t d={.src="L:/fault_popup_bg.png"};const char *stage;int error;unsigned bytes;
        assert(aic_decoder_attempt(&d,&stage,&error,&bytes)==LV_RES_INV);
        assert(!d.img_data && !files && !heaps && !dma && !decs);
    }
    inject=NULL;lv_img_decoder_dsc_t d={.src="L:/valid.png"};const char *stage;int error;unsigned bytes;
    assert(aic_decoder_attempt(&d,&stage,&error,&bytes)==LV_RES_OK && d.img_data && dma==1);
    mpp_buf_free(&((struct mpp_frame*)d.img_data)->buf);free((void*)d.img_data);
    assert(!dma && !files && !heaps && !decs);
    d=(lv_img_decoder_dsc_t){.src="L:/retry.png"};inject="output-dma";
    freed_on_reclaim=2097152;recover_on_reclaim=true;
    unsigned before=allocation_attempts;
    assert(aic_decoder_open(NULL,&d)==LV_RES_OK && allocation_attempts==before+2 && reclaim_calls==1);
    mpp_buf_free(&((struct mpp_frame*)d.img_data)->buf);free((void*)d.img_data);
    d=(lv_img_decoder_dsc_t){.src="L:/fail.png"};inject="output-dma";recover_on_reclaim=false;
    before=allocation_attempts;
    assert(aic_decoder_open(NULL,&d)==LV_RES_INV && allocation_attempts==before+2);
    assert(aic_decoder_open(NULL,&d)==LV_RES_INV && allocation_attempts==before+2);
    now+=1001;inject=NULL;
    assert(aic_decoder_open(NULL,&d)==LV_RES_OK && allocation_attempts==before+3);
    mpp_buf_free(&((struct mpp_frame*)d.img_data)->buf);free((void*)d.img_data);
    before=reclaim_calls;inject="file-read";d=(lv_img_decoder_dsc_t){.src="L:/bad-file.png"};
    assert(aic_decoder_open(NULL,&d)==LV_RES_INV && reclaim_calls==before);
    puts("decoder PASS: 19 injected failure stages, positive non-success codes, null packet manager, short read, cleanup order, success ownership");
    puts("retry PASS: recovery, one retry maximum, 1s backoff, resumed success, no eviction for file errors");
}
'''
decoder_stub = '#include "' + str(root / 'aic_ui/image_memory.c') + '"\n' + decoder_stub
decoder_test = decoder_test.replace('mpp_buf_free(&((struct mpp_frame*)d.img_data)->buf);',
    'image_mem_release(IMAGE_MEM_IMAGE, frame_dma_bytes((struct mpp_frame*)d.img_data)); mpp_buf_free(&((struct mpp_frame*)d.img_data)->buf);')
run('decoder',decoder_stub+attempt+wrapper+decoder_test)
print('Test artifacts:',work)
