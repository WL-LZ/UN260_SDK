#!/usr/bin/env python3
"""Real customer pack/parser and shared resource lifetime, with hostile inputs.

No device or page renderer is used. The optional native lifetime layer reuses
existing ASan/UBSan LVGL objects in place; only the changed owner is recompiled.
All artifacts go to --output-dir; this test never regenerates production assets.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
PARSER = r'''
#include "un260/boot/boot_customer_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned word(const unsigned char *p) {
 return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;
}
int main(int argc,char **argv) {
 assert(argc==3);FILE *f=fopen(argv[2],"rb");assert(f);
 assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>=0);rewind(f);
 unsigned char *p=malloc(n?n:1);assert(p);
 assert(fread(p,1,n,f)==(size_t)n);fclose(f);
 boot_customer_catalog_t cat;memset(&cat,0xa5,sizeof(cat));
 bool good=boot_customer_catalog_open(&cat,p,n);
 if(!strcmp(argv[1],"invalid")) {
  assert(!good);assert(!cat.data&&!cat.size&&!cat.count);free(p);return 0;
 }
 assert(good&&cat.data==p&&cat.size==(size_t)n&&cat.count==word(p+12));
 assert(!boot_customer_catalog_open(NULL,p,n));
 boot_customer_image_t image;
 assert(!boot_customer_catalog_find(NULL,1,"en",&image));
 assert(!boot_customer_catalog_find(&cat,1,"en",NULL));
 assert(!boot_customer_catalog_find(&cat,0,"en",&image));
 assert(!boot_customer_catalog_find(&cat,99,"en",&image));
 for(unsigned k=0;k<cat.count;k++) {
  const unsigned char *e=p+32+k*64;unsigned id=word(e);
  assert(boot_customer_catalog_find(&cat,id,(const char *)e+40,&image));
  assert(image.data==p+word(e+16)&&image.width==word(e+8)&&image.height==word(e+12));
  assert(image.format==word(e+4)&&image.color==word(e+32));
  assert(image.x==(int)word(e+24)&&image.y==(int)word(e+28));
 }
 for(unsigned id=1;id<=14;id++) {
  boot_customer_image_t en,other,no_tag;
  assert(boot_customer_catalog_find(&cat,id,"en",&en));
  assert(boot_customer_catalog_find(&cat,id,"not-installed",&other));
  assert(boot_customer_catalog_find(&cat,id,NULL,&no_tag));
  assert(en.data==other.data&&en.data==no_tag.data);
 }
 for(unsigned k=0;k<cat.count;k++) {
  const unsigned char *e=p+32+k*64;
  if(word(e)==BOOT_CUSTOMER_ART_READY_GRAY) {
   boot_customer_image_t a,b;
   assert(boot_customer_catalog_find(&cat,BOOT_CUSTOMER_ART_READY_GRAY,(const char *)e+40,&a));
   assert(boot_customer_catalog_find(&cat,BOOT_CUSTOMER_ART_READY_ORANGE,(const char *)e+40,&b));
   assert(a.data==b.data&&a.width==b.width&&a.height==b.height&&a.x==b.x&&a.y==b.y);
  }
 }
 assert(!boot_customer_catalog_open(&cat,NULL,0));assert(!cat.data&&!cat.count);
 free(p);return 0;
}
'''
LIFETIME = r'''
#include "un260/lv_core/boot_anim/boot_customer_art.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void *__real_malloc(size_t);void __real_free(void *);
void __real_lv_img_cache_invalidate_src(const void *);
static bool watch;
static unsigned calls,fail_at,live,invalidations;
static void *owned[64];
void *__wrap_malloc(size_t n) {
 if(!watch)return __real_malloc(n);
 ++calls;if(fail_at&&calls==fail_at)return NULL;
 void *p=__real_malloc(n);if(p){unsigned i=0;while(i<64&&owned[i])i++;assert(i<64);owned[i]=p;live++;}return p;
}
void __wrap_free(void *p) {
 if(p)for(unsigned i=0;i<64;i++)if(owned[i]==p){owned[i]=NULL;assert(live);live--;break;}
 __real_free(p);
}
void __wrap_lv_img_cache_invalidate_src(const void *p) {
 if(watch){assert(p);invalidations++;}__real_lv_img_cache_invalidate_src(p);
}
static void absent(void) {
 for(unsigned id=0;id<=15;id++){assert(!boot_customer_art_image(id));assert(!boot_customer_art_position(id,NULL,NULL));}
 assert(!live);
}
static void verify(void) {
 assert(!boot_customer_art_image(0)&&!boot_customer_art_image(15));
 for(unsigned id=1;id<=14;id++){
  const lv_img_dsc_t *d=boot_customer_art_image(id);lv_coord_t x,y;
  assert(d&&d->data&&d->header.cf==(id==1?LV_IMG_CF_TRUE_COLOR:LV_IMG_CF_TRUE_COLOR_ALPHA));
  if(id==1)for(unsigned i=3;i<d->data_size;i+=4)assert(d->data[i]==255);
  assert(d->data_size==(unsigned)d->header.w*d->header.h*4);
  assert(boot_customer_art_position(id,&x,&y));
  assert(x>=0&&y>=0&&x+d->header.w<=1280&&y+d->header.h<=400);
 }
 const lv_img_dsc_t *gray=boot_customer_art_image(11),*orange=boot_customer_art_image(14);
 assert(gray->header.w==orange->header.w&&gray->header.h==orange->header.h);
 assert(gray->data!=orange->data&&gray->data_size==orange->data_size);
 for(unsigned i=0;i<gray->data_size;i+=4){
  assert(gray->data[i]==0x34&&gray->data[i+1]==0x37&&gray->data[i+2]==0x35);
  assert(orange->data[i]==0x30&&orange->data[i+1]==0x6f&&orange->data[i+2]==0xe8);
  assert(gray->data[i+3]==orange->data[i+3]);
 }
}
int main(int argc,char **argv) {
 assert(argc==2);lv_init();watch=true;absent();
 if(strcmp(argv[1],"valid")) {
  for(unsigned i=0;i<3;i++){assert(!boot_customer_art_acquire("en"));absent();boot_customer_art_release();}
  watch=false;puts("PASS invalid blob cleanup");return 0;
 }
 assert(boot_customer_art_acquire("en"));verify();unsigned allocations=calls;
 const lv_img_dsc_t *bg=boot_customer_art_image(1);const void *pixels=bg->data;
 assert(boot_customer_art_acquire("not-installed"));assert(calls==allocations);
 assert(boot_customer_art_image(1)==bg&&bg->data==pixels);
 boot_customer_art_release();assert(!invalidations&&boot_customer_art_image(1)==bg);
 boot_customer_art_release();absent();assert(invalidations==14);
 boot_customer_art_release();assert(invalidations==14);
 for(unsigned n=1;n<=allocations;n++){
  calls=0;fail_at=n;assert(!boot_customer_art_acquire("en"));assert(calls==n);absent();
  fail_at=0;assert(boot_customer_art_acquire("not-installed"));verify();boot_customer_art_release();absent();
 }
 for(unsigned n=0;n<25;n++){
  calls=0;assert(boot_customer_art_acquire(n%2?"en":"not-installed"));verify();
  assert(calls==allocations);boot_customer_art_release();absent();
 }
 watch=false;printf("PASS shared owner, final cache invalidation, %u allocation failures/retries, 25 lifecycles\n",allocations);
 return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--lvgl-dir', type=Path, default=ROOT.parents[1]/'third-party/lvgl-8.3.2')
    parser.add_argument('--lvgl-cache', type=Path)
    parser.add_argument('--catalog-only', action='store_true')
    args = parser.parse_args()
    if not args.catalog_only and not args.lvgl_cache:
        parser.error('--lvgl-cache is required for real native lifetime validation; use --catalog-only explicitly otherwise')
    out = args.output_dir.resolve();out.mkdir(parents=True, exist_ok=True)
    work=out/'build';work.mkdir(exist_ok=True);temp=out/'temp';temp.mkdir(exist_ok=True)
    env=dict(os.environ,TMPDIR=str(temp),TEMP=str(temp),TMP=str(temp),PYTHONDONTWRITEBYTECODE='1')
    results=[]
    def run(name, command, *, expect=0):
        r=subprocess.run(list(map(str,command)),env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        (out/(name+'.log')).write_text(r.stdout)
        results.append({'name':name,'returncode':r.returncode,'passed':r.returncode==expect})
        print(('PASS ' if r.returncode==expect else 'FAIL ')+name,flush=True)
        if r.returncode!=expect: print(r.stdout[-3000:],flush=True)
        return r.returncode==expect
    sys.dont_write_bytecode=True
    spec=importlib.util.spec_from_file_location('customer_pack',ROOT/'tools/build_customer_boot_pack.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    raw=builder.customer_payload(ROOT);(out/'catalog.bin').write_bytes(raw)
    count=struct.unpack_from('<I',raw,12)[0]
    entries=[]
    for k in range(count):
        e=bytearray(raw[32+k*64:32+(k+1)*64]);offset,size=struct.unpack_from('<II',e,16)
        entries.append((e,raw[offset:offset+size]))
    def repack(items):
        start=32+64*len(items);cursor=start;table=[];pieces=[]
        for source,pixels in items:
            e=bytearray(source);struct.pack_into('<II',e,16,cursor,len(pixels));table.append(e);pieces.append(pixels);cursor+=len(pixels)
        return b'UNBCAT1\0'+struct.pack('<6I',cursor,len(items),64,start,0,0)+b''.join(table)+b''.join(pieces)
    def word(data,offset,value):
        p=bytearray(data);struct.pack_into('<I',p,offset,value);return p
    def field(index,offset,value):return word(raw,32+64*index+offset,value)
    def ident(e):return struct.unpack_from('<I',e)[0]
    def tag(e):return bytes(e[40:56]).split(b'\0')[0]
    common=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
    (work/'catalog.c').write_text(PARSER);exe=work/'catalog-test'
    if not run('catalog-build',common+['-I'+str(ROOT),work/'catalog.c',ROOT/'un260/boot/boot_customer_catalog.c','-o',exe]):return 1
    run('generated-catalog',[exe,'valid',out/'catalog.bin'])
    mutations={
        'empty':b'', 'short-header':raw[:31], 'truncated':raw[:-1],
        'truncated-correct-size':word(raw[:-1],8,len(raw)-1),
        'trailing-data':word(raw+b'\0',8,len(raw)+1),
        'wrong-magic':b'X'+raw[1:], 'wrong-size':word(raw,8,len(raw)+1),
        'zero-count':word(raw,12,0), 'excess-count':word(raw,12,65),
        'wrong-entry-size':word(raw,16,63), 'wrong-data-offset':word(raw,20,32),
        'header-reserved-24':word(raw,24,1), 'header-reserved-28':word(raw,28,1),
        'zero-id':field(0,0,0), 'unsupported-id':field(0,0,14),
        'zero-format':field(0,4,0), 'unsupported-format':field(0,4,3),
        'zero-width':field(0,8,0), 'large-width':field(0,8,1281),
        'zero-height':field(0,12,0), 'large-height':field(0,12,401),
        'offset-overflow':field(0,16,0xffffffff), 'offset-in-table':field(0,16,32),
        'bad-pixel-bytes':field(0,20,1), 'x-negative':field(0,24,0xffffffff),
        'y-negative':field(0,28,0xffffffff), 'x-intmax':field(0,24,0x7fffffff),
        'y-intmax':field(0,28,0x7fffffff), 'x-outside':field(0,24,1), 'y-outside':field(0,28,1),
        'color-overflow':field(0,32,0x1000000), 'entry-reserved-36':field(0,36,1),
        'entry-reserved-56':field(0,56,1), 'entry-reserved-60':field(0,60,1),
        'payload-over-budget':raw+b'\0'*(8*1024*1024+1-len(raw)),
    }
    dup=list(entries);dup.append(entries[1]);mutations['duplicate-id-tag']=repack(dup)
    for missing in range(1,14):
        mutations['missing-required-'+str(missing)]=repack([(e,p) for e,p in entries if ident(e)!=missing])
    for index,(e,pixels) in enumerate(entries):
        if 9<=ident(e)<=12 and tag(e)==b'en':
            changed=bytearray(raw);changed[32+index*64+40:32+index*64+56]=b'zz\0'+b'\0'*13
            mutations['missing-english-'+str(ident(e))]=changed
    titled=next(i for i,(e,p) in enumerate(entries) if ident(e)==9)
    changed=bytearray(raw);changed[32+titled*64+40:32+titled*64+56]=b'x'*16;mutations['unterminated-language']=changed
    changed=bytearray(raw);changed[32+titled*64+40:32+titled*64+56]=b'\0'*16;mutations['empty-title-language']=changed
    changed=bytearray(raw);changed[32+40:32+56]=b'en\0'+b'\0'*13;mutations['nonlocalized-tag']=changed
    for index in (0,next(i for i,(e,p) in enumerate(entries) if ident(e)==4)):
        changed=list(entries);e,pixels=changed[index];e=bytearray(e);w,h=struct.unpack_from('<II',e,8)
        struct.pack_into('<I',e,8,w-1);changed[index]=(e,pixels[:(w-1)*h*4])
        mutations['required-geometry-'+str(ident(e))]=repack(changed)
    for name,payload in mutations.items():
        (work/'mutation.bin').write_bytes(payload);run(name,[exe,'invalid',work/'mutation.bin'])
    lifecycle={}
    if not args.catalog_only:
        cache=args.lvgl_cache.resolve();conf=cache/'lv_conf.h';assert conf.is_file()
        objects={}
        for dep in cache.glob('*.d'):
            tokens=dep.read_text().replace('\\\n',' ').split()
            if len(tokens)>1:
                source=Path(tokens[1]);obj=dep.with_suffix('.o')
                if str(source).startswith(str(args.lvgl_dir/'src')+'/') and obj.exists():objects[str(source)]=obj
        assert objects,'No reusable LVGL objects in supplied cache'
        flags=common+['-I'+str(cache),'-DLV_DRV_CONF_H','-I'+str(args.lvgl_dir),'-I'+str(ROOT),'-DLV_CONF_PATH='+str(conf)]
        added=[]
        for i,source in enumerate(p for p in (args.lvgl_dir/'src').rglob('*.c') if str(p) not in objects):
            obj=work/('lvgl-missing-'+str(i)+'.o')
            if not run('lvgl-missing-'+str(i),flags+['-c',source,'-o',obj]):return 1
            added.append(obj)
        (work/'lifetime.c').write_text(LIFETIME)
        local=[]
        for name,source in [('lifetime',work/'lifetime.c'),('owner',ROOT/'un260/lv_core/boot_anim/boot_customer_art.c'),('catalog',ROOT/'un260/boot/boot_customer_catalog.c')]:
            obj=work/(name+'.o')
            if not run(name+'-compile',flags+['-c',source,'-o',obj]):return 1
            local.append(obj)
        generated=work/'generated-blob.c'
        if not run('production-blob-generator',[sys.executable,ROOT/'tools/build_customer_boot_pack.py','--root',ROOT,'--output',generated]):return 1
        content=generated.read_text();compressed=zlib.compress(raw,9)
        variants={'valid':content,'corrupt':content.replace('0x%02x'%compressed[0],'0x%02x'%(compressed[0]^0xff),1),
                  'raw-length':content.replace('= %dU;'%len(raw),'= %dU;'%(len(raw)+1)),
                  'raw-budget':content.replace('= %dU;'%len(raw),'= 8388609U;'),
                  'compressed-budget':content.replace('= sizeof(boot_customer_blob);','= 2097153U;')}
        for name,blob in variants.items():
            source=work/('blob-'+name+'.c');source.write_text(blob);binary=work/('lifetime-'+name)
            cmd=flags+[source,*local,*objects.values(),*added,'-Wl,--wrap=malloc','-Wl,--wrap=free','-Wl,--wrap=lv_img_cache_invalidate_src','-lm','-lz','-pthread','-o',binary]
            if run('lifetime-'+name+'-link',cmd):run('lifetime-'+name,[binary,name])
        lifecycle={'lvgl_objects_reused':len(objects),'extra_objects':len(added),'cache':str(cache),'configuration_sha256':hashlib.sha256(conf.read_bytes()).hexdigest(),'scope':'Real resource owner, actual LVGL cache invalidation, allocator fault injection; no page renderer or board'}
    summary={'payload_bytes':len(raw),'compressed_bytes':len(zlib.compress(raw,9)),'entry_count':count,'payload_sha256':hashlib.sha256(raw).hexdigest(),'mutation_cases':len(mutations),'passed':sum(r['passed'] for r in results),'failed':sum(not r['passed'] for r in results),'lifecycle':lifecycle,'results':results,'board_test':False}
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('SUMMARY %d passed / %d failed, %d catalog entries'%(summary['passed'],summary['failed'],count))
    return 1 if summary['failed'] else 0

if __name__=='__main__':
    raise SystemExit(main())
