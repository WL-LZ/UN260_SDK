"""Execute production decoded-image routing with simulated GE/DMA endpoints.
Not a board pixel or DMA-coherency test.
"""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
src=(root/'lv_ge2d.c').read_text()
start=src.rindex('LV_ATTRIBUTE_FAST_MEM void lv_draw_aic_img_decoded(')
opening=src.index('{',start);depth=1;end=opening+1
while depth:
    depth+=(src[end]=='{')-(src[end]=='}');end+=1
actual=src[start:end]
stub=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define LV_ATTRIBUTE_FAST_MEM
#define LV_OPA_TRANSP 0
#define LV_BLEND_MODE_NORMAL 0
#define LV_IMG_ZOOM_NONE 256
#define LV_LOG_ERROR(...) ((void)0)
typedef int lv_img_cf_t;
typedef struct {int x;} lv_area_t;
typedef struct _lv_draw_ctx_t {lv_area_t*clip_area;} lv_draw_ctx_t;
typedef struct {int recolor_opa,blend_mode,angle,zoom,opa;} lv_draw_img_dsc_t;
typedef struct {int opa,blend_mode;const void*src_buf;void*mask_buf;const lv_area_t*blend_area;} lv_draw_sw_blend_dsc_t;
struct mpp_frame {int marker;};
static bool file_type,masked,registered;
static int cpu,sw,hw;
static const char*g_profile_image_source;
static struct mpp_frame frame={123};
static const struct mpp_frame*ge_dma_image_lookup(const void*p,const char**name){(void)p;*name="test";return registered?&frame:0;}
static void lv_area_copy(lv_area_t*a,const lv_area_t*b){*a=*b;}
static bool lv_draw_mask_is_any(const lv_area_t*a){(void)a;return masked;}
static bool draw_target_is_display_buffer(lv_draw_ctx_t*c){(void)c;return true;}
static bool is_fix_angle(int a){return a==0;}
static void ge_run_blit(lv_draw_ctx_t*c,const lv_draw_img_dsc_t*d,struct mpp_frame*f,lv_area_t*a,const lv_area_t*b){(void)c;(void)d;(void)a;(void)b;assert(f->marker==123);hw++;}
static void ge_run_rotate(lv_draw_ctx_t*c,const lv_draw_img_dsc_t*d,struct mpp_frame*f,lv_area_t*a,const lv_area_t*b){ge_run_blit(c,d,f,a,b);}
static void draw_dma_frame_software(lv_draw_ctx_t*c,const lv_draw_img_dsc_t*d,const lv_area_t*a,const struct mpp_frame*f,lv_img_cf_t cf){(void)c;(void)d;(void)a;(void)cf;assert(f->marker==123);sw++;}
static void lv_draw_sw_img_decoded(lv_draw_ctx_t*c,const lv_draw_img_dsc_t*d,const lv_area_t*a,const uint8_t*p,lv_img_cf_t cf){(void)c;(void)d;(void)a;(void)p;(void)cf;cpu++;}
#define lv_memset_00(p,n) memset(p,0,n)
'''
test=r'''
int main(void){lv_area_t a={0};lv_draw_ctx_t c={&a};lv_draw_img_dsc_t d={.zoom=104,.opa=255};
file_type=true;masked=true;lv_draw_aic_img_decoded(&c,&d,&a,(void*)&frame,1);assert(sw==1&&hw==0&&cpu==0);
masked=false;lv_draw_aic_img_decoded(&c,&d,&a,(void*)&frame,1);assert(hw==1);
d.recolor_opa=50;lv_draw_aic_img_decoded(&c,&d,&a,(void*)&frame,1);assert(sw==2);
file_type=false;lv_draw_aic_img_decoded(&c,&d,&a,(void*)&frame,1);assert(cpu==1);
registered=true;masked=true;lv_draw_aic_img_decoded(&c,&d,&a,(void*)&frame,1);assert(sw==3);
return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'route.c').write_text(stub+actual+test)
    subprocess.run(['gcc','-std=c11','-Wall','-Werror','-fsanitize=undefined',str(p/'route.c'),'-o',str(p/'route')],check=True)
    subprocess.run([str(p/'route')],check=True)
print('PASS production image route: rounded mask, recolour, hardware fast path, CPU binary, registered DMA')
