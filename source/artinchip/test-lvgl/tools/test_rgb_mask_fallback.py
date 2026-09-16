"""Production DMA->software conversion, simulated mappings; pixel/stride and failure tests."""
from pathlib import Path
import subprocess,tempfile
source=(Path(__file__).resolve().parents[1]/'lv_ge2d.c').read_text()
start=source.index('static bool draw_dma_frame_software(');opening=source.index('{',start);end=opening+1;depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
code=source[start:end]
stub=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define MPP_FMT_RGB_888 8
#define MPP_FMT_ARGB_8888 0
#define MPP_DMA_BUF_FD 1
#define CACHE_INVALID 1
#define IMAGE_MEM_CPU 1

typedef struct {int x,y;} lv_point_t;
typedef struct {int zoom;lv_point_t pivot;} lv_draw_img_dsc_t;
typedef struct {int x1,y1,x2,y2;} lv_area_t;
typedef struct {const lv_area_t *clip_area;} lv_draw_ctx_t;
static bool _lv_area_intersect(lv_area_t*r,const lv_area_t*a,const lv_area_t*b){r->x1=a->x1>b->x1?a->x1:b->x1;r->y1=a->y1>b->y1?a->y1:b->y1;r->x2=a->x2<b->x2?a->x2:b->x2;r->y2=a->y2<b->y2?a->y2:b->y2;return r->x1<=r->x2&&r->y1<=r->y2;}
static bool cached;static unsigned draws;static uint8_t cached_data[16];
#define LV_IMG_ZOOM_NONE 256
#define LV_IMG_CF_TRUE_COLOR_ALPHA 4
typedef int lv_img_cf_t;
struct mpp_frame {struct {int buf_type;int fd[1];struct {int width,height;}size;int format;uint32_t stride[1];}buf;};
static struct mpp_frame*ge_prepare_scaled(const struct mpp_frame*f,const lv_draw_img_dsc_t*d,const lv_area_t*a,lv_area_t*b){(void)d;(void)a;if(cached){*b=(lv_area_t){10,20,11,21};return (struct mpp_frame*)f;}return NULL;}
static const uint8_t*ge_scaled_pixels(const struct mpp_frame*f){(void)f;return cached_data;}
static uint8_t input[64],output[64];static unsigned used,maps;static bool deny;
static void ge_offscreen_fail(const char*s){(void)s;}
static void*dmabuf_mmap(int fd,int bytes){(void)fd;(void)bytes;maps++;return input;}
static void dmabuf_munmap(void*p,int bytes){(void)p;(void)bytes;maps--;}
static void dmabuf_sync(int fd,int mode){(void)fd;(void)mode;}
static bool image_mem_acquire(int type,unsigned n){(void)type;if(deny)return false;used+=n;return true;}
static void image_mem_release(int type,unsigned n){(void)type;used-=n;}
#define lv_mem_alloc malloc
#define lv_mem_free free
static void lv_draw_sw_img_decoded(lv_draw_ctx_t*c,const lv_draw_img_dsc_t*d,const lv_area_t*a,const uint8_t*p,lv_img_cf_t cf){(void)d;(void)cf;draws++;if(cached){assert(c->clip_area->x1>=a->x1&&c->clip_area->y1>=a->y1&&c->clip_area->x2<=a->x2&&c->clip_area->y2<=a->y2);}memcpy(output,p,16);}
'''
tests=r'''
int main(void){struct mpp_frame f={.buf={.buf_type=1,.fd={1},.size={2,2},.format=8,.stride={8}}};
uint8_t pixels[]={1,2,3,4,5,6,99,99,7,8,9,10,11,12,99,99};memcpy(input,pixels,16);
assert(draw_dma_frame_software(0,0,0,&f,0));uint8_t expected[]={1,2,3,255,4,5,6,255,7,8,9,255,10,11,12,255};assert(!memcmp(output,expected,16));assert(!used&&!maps);
deny=true;assert(!draw_dma_frame_software(0,0,0,&f,0));assert(!used&&!maps);deny=false;
f.buf.stride[0]=5;assert(!draw_dma_frame_software(0,0,0,&f,0));assert(!used&&!maps);
f.buf.format=0;f.buf.stride[0]=8;memcpy(input,expected,16);assert(draw_dma_frame_software(0,0,0,&f,0));assert(!memcmp(output,expected,16));assert(!used&&!maps);
cached=true;lv_draw_img_dsc_t d={.zoom=104};lv_area_t clip={8,18,13,23};lv_draw_ctx_t ctx={&clip};assert(draw_dma_frame_software(&ctx,&d,&clip,&f,0));assert(ctx.clip_area==&clip);
unsigned before=draws;clip=(lv_area_t){12,22,13,23};assert(draw_dma_frame_software(&ctx,&d,&clip,&f,0));assert(draws==before&&ctx.clip_area==&clip);
clip=(lv_area_t){11,21,15,26};assert(draw_dma_frame_software(&ctx,&d,&clip,&f,0));assert(draws==before+1&&ctx.clip_area==&clip);return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(stub+code+tests)
    subprocess.run(['gcc','-std=c11','-Wall','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
    # Mutation guard: removing the source-bound clip must fail this regression.
    broken=code.replace('draw_ctx->clip_area=&clipped;', '')
    (p/'broken.c').write_text(stub+broken+tests)
    subprocess.run(['gcc','-std=c11','-Wall','-Werror',str(p/'broken.c'),'-o',str(p/'broken')],check=True)
    result=subprocess.run([str(p/'broken')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    assert result.returncode != 0, 'Regression did not detect the old out-of-bounds clip'
print('PASS RGB888 padded rows, opaque alpha, ARGB8888, invalid stride, budget failure, cached raster clip bounds, empty/partial clips, context restoration and cleanup')
