#!/usr/bin/env python3
"""ASan/UBSan radius-table generation, translated raster and metadata regression.

Extract the production functions; only allocation and basic LVGL types/macros
are provided by the harness. No hardware/display or full UI is simulated.
"""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',required=True,type=Path)
args=p.parse_args();src=args.source.read_text()
def function(name):
 import re
 matches=list(re.finditer(r'^(?:LV_ATTRIBUTE_FAST_MEM )?(?:static )?[^\n]*\b'+name+r'\([^;]*?\)\n\{',src,re.M))
 assert len(matches)==1,(name,len(matches))
 start=matches[0].start();pos=src.index('{',start);depth=1;i=pos+1
 while depth:
  if src[i]=='{':depth+=1
  if src[i]=='}':depth-=1
  i+=1
 return src[start:i].replace('    param->dsc.cb = (lv_draw_mask_xcb_t)lv_draw_mask_radius;','    /* Callback assignment has no ownership side effect. */')+'\n'
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
typedef int16_t lv_coord_t;
typedef uint8_t lv_opa_t;
typedef struct {lv_coord_t x,y;} lv_point_t;
typedef struct {lv_coord_t x1,y1,x2,y2;} lv_area_t;
typedef struct {uint8_t *buf,*cir_opa;uint16_t *x_start_on_y,*opa_start_on_y;int32_t life;uint32_t used_cnt;lv_coord_t radius;} _lv_draw_mask_radius_circle_dsc_t;
typedef struct {unsigned type;} _lv_draw_mask_common_dsc_t;
typedef struct {_lv_draw_mask_common_dsc_t dsc;struct {void *points;} cfg;} lv_draw_mask_polygon_param_t;
#define LV_DRAW_MASK_TYPE_RADIUS 1
#define LV_DRAW_MASK_TYPE_POLYGON 2
#define LV_CIRCLE_CACHE_SIZE 4
static _lv_draw_mask_radius_circle_dsc_t _lv_circle_cache[LV_CIRCLE_CACHE_SIZE];
#define LV_GC_ROOT(x) (x)
#define CIRCLE_CACHE_AGING(life,r) life=LV_MIN(life+((r)<16?1:((r)>>4)),1000)
typedef struct {_lv_draw_mask_common_dsc_t dsc;struct {lv_area_t rect;lv_coord_t radius;bool outer;} cfg;_lv_draw_mask_radius_circle_dsc_t *circle;} lv_draw_mask_radius_param_t;
typedef enum {LV_DRAW_MASK_RES_TRANSP,LV_DRAW_MASK_RES_FULL_COVER,LV_DRAW_MASK_RES_CHANGED} lv_draw_mask_res_t;
#define LV_MIN(a,b) ((a)<(b)?(a):(b))
#define LV_MAX(a,b) ((a)>(b)?(a):(b))
#define LV_CLAMP(a,b,c) LV_MIN(LV_MAX(a,b),c)
#define LV_ASSERT_MALLOC(p) assert(p)
#define LV_ATTRIBUTE_FAST_MEM
#define LV_OPA_MAX 252
#define LV_OPA_MIN 2
#define LV_UDIV255(x) (((x)*0x8081U)>>23)
#define lv_area_copy(d,s) (*(d)=*(s))
#define lv_area_get_width(a) ((lv_coord_t)((a)->x2-(a)->x1+1))
#define lv_area_get_height(a) ((lv_coord_t)((a)->y2-(a)->y1+1))
#define lv_memset_00(p,n) memset(p,0,n)
static unsigned alloc_calls, fail_allocation;
static void *lv_mem_alloc(size_t n){if(++alloc_calls==fail_allocation)return NULL;void *p=malloc(n);assert(p);memset(p,0x44,n);return p;}
#define lv_mem_free free
#define lv_mem_buf_get lv_mem_alloc
#define lv_mem_buf_release free
'''
post=r'''
static unsigned cases;
static void translated(_lv_draw_mask_radius_circle_dsc_t *c,int w,int h,int dx,int dy,int row,bool outer){
 lv_draw_mask_radius_param_t a={.cfg={{1000,1000,1000+w-1,1000+h-1},c->radius,outer},.circle=c};
 lv_draw_mask_radius_param_t b=a;b.cfg.rect=(lv_area_t){dx,dy,dx+w-1,dy+h-1};
 unsigned char *ma=malloc(w),*mb=malloc(w);assert(ma&&mb);
 memset(ma,255,w);memset(mb,255,w);
 int ra=lv_draw_mask_radius(ma,1000,1000+row,w,&a);
 int rb=lv_draw_mask_radius(mb,dx,dy+row,w,&b);
 assert(ra==rb&&!memcmp(ma,mb,w));free(ma);free(mb);cases++;
}
int main(void){
 for(int r=1;r<=200;r++){
  _lv_draw_mask_radius_circle_dsc_t c={0};circ_calc_aa4(&c,r);
  for(int y=0;y<r;y++){
   assert(c.opa_start_on_y[y+1]>=c.opa_start_on_y[y]);
   assert(c.opa_start_on_y[y+1]<=2*r+2);
  }
  int w=2*r+31,h=2*r+7;
  for(int row=-1;row<=h;row++)for(int outer=0;outer<2;outer++){
   translated(&c,w,h,-w/2,-h/2,row,outer);
   translated(&c,w,h,-32760,-32760,row,outer);
   translated(&c,w,h,30000,30000,row,outer);
  }
  /* Invalid metadata must not turn into opacity reads or mask writes. */
  lv_draw_mask_radius_param_t a={.cfg={{0,0,2*r-1,2*r-1},r,false},.circle=&c};
  unsigned char mask[400];memset(mask,255,sizeof(mask));
  unsigned short saved=c.opa_start_on_y[r];c.opa_start_on_y[r]=65535;
  assert(lv_draw_mask_radius(mask,0,0,2*r,&a)==LV_DRAW_MASK_RES_TRANSP);
  c.opa_start_on_y[r]=saved;a.circle=NULL;
  assert(lv_draw_mask_radius(mask,0,0,2*r,&a)==LV_DRAW_MASK_RES_TRANSP);
  a.cfg.outer=true;assert(lv_draw_mask_radius(mask,0,0,2*r,&a)==LV_DRAW_MASK_RES_FULL_COVER);
  /* Both failure positions preserve the old circle, without leaks. */
  unsigned char *old=c.buf;int old_radius=c.radius;
  fail_allocation=alloc_calls+1;assert(!circ_calc_aa4(&c,r+1));
  assert(c.buf==old&&c.radius==old_radius);
  fail_allocation=alloc_calls+2;assert(!circ_calc_aa4(&c,r+1));
  assert(c.buf==old&&c.radius==old_radius);
  fail_allocation=0;assert(circ_calc_aa4(&c,r+1));
  free(c.buf);
 }
 /* Fill every cache lease, then fail each allocation in an uncached circle.
  * The temporary descriptor and successful preceding allocations must unwind. */
 lv_area_t area={0,0,100,100};lv_draw_mask_radius_param_t owned[4],failed;
 for(unsigned pos=1;pos<=3;pos++){
  for(unsigned i=0;i<4;i++)lv_draw_mask_radius_init(&owned[i],&area,10+i,false);
  fail_allocation=alloc_calls+pos;
  lv_draw_mask_radius_init(&failed,&area,20,false);assert(failed.circle==NULL);
  lv_draw_mask_free_param(&failed);fail_allocation=0;
  for(unsigned i=0;i<4;i++){
   assert(owned[i].circle->used_cnt==1);
   lv_draw_mask_free_param(&owned[i]);lv_draw_mask_free_param(&owned[i]);
  }
  /* A failed cached replacement also returns its used count. */
  fail_allocation=alloc_calls+1;lv_draw_mask_radius_init(&failed,&area,21,false);
  assert(failed.circle==NULL);fail_allocation=0;
  for(unsigned i=0;i<4;i++)assert(_lv_circle_cache[i].used_cnt==0);
  _lv_draw_mask_cleanup();
 }
 puts("PASS allocation failure: cache leases, temporary descriptors, preserved old table, repeated cleanup");
 printf("PASS %u translated raster cases; radii 1..200; null/invalid table guards; ASan/UBSan\n",cases);
}
'''
names=['lv_draw_mask_free_param','_lv_draw_mask_cleanup','circ_init','circ_cont','circ_next','circ_calc_aa4','get_next_line','mask_mix','lv_draw_mask_radius','lv_draw_mask_radius_init']
with tempfile.TemporaryDirectory(prefix='un260-mask-host-') as t:
 file=Path(t)/'test.c';file.write_text(pre+'\n'.join(function(n) for n in names)+post)
 exe=Path(t)/'test'
 subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(file),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
