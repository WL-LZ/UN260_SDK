#!/usr/bin/env python3
"""Actual header parsers + SDK status enums + real asset bytes, no header stub."""
from pathlib import Path
import re, subprocess, tempfile
from PIL import Image
root=Path(__file__).resolve().parents[1]
sdk=root.parents[2]
lv=sdk/'source/third-party/lvgl-8.3.2/src/misc'
enums=''
for name,symbol,typ in [('lv_types.h','LV_RES_INV','lv_res_t'),('lv_fs.h','LV_FS_RES_OK','lv_fs_res_t')]:
    text=(lv/name).read_text()
    enums+=re.search(r'enum\s*\{[^}]*\b'+symbol+r'\b[^}]*\};\s*typedef uint8_t '+typ+r';',text).group()+'\n'
src=(root/'aic_ui/aic_dec.c').read_text()
body=src[src.index('static inline uint64_t stream_to_u64'):src.index('static lv_res_t jpeg_decoder_info')]
body+=src[src.index('static lv_fs_res_t png_get_img_size'):src.index('static lv_fs_res_t get_file_size')]
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define PNG_HEADER_SIZE 33
#define PNGSIG 0x89504e470d0a1a0aull
#define JPEG_SOI 0xFFD8
#define JPEG_SOF 0xFFC0
enum mpp_pixel_format {MPP_FMT_YUV420P,MPP_FMT_YUV422P,MPP_FMT_YUV444P,MPP_FMT_YUV400,MPP_FMT_RGB_888,MPP_FMT_ARGB_8888};
typedef FILE *lv_fs_file_t;
static lv_fs_res_t lv_fs_read(lv_fs_file_t*f,void*b,uint32_t n,uint32_t*r){*r=fread(b,1,n,*f);return ferror(*f)?LV_FS_RES_FS_ERR:LV_FS_RES_OK;}
static lv_fs_res_t lv_fs_seek(lv_fs_file_t*f,int n,int w){return fseek(*f,n,w)?LV_FS_RES_FS_ERR:LV_FS_RES_OK;}
'''
main=r'''
int main(int argc,char**argv){
 assert(LV_RES_OK==1 && LV_FS_RES_OK==0 && LV_RES_INV==0);
 assert(argc==5);lv_fs_file_t f=fopen(argv[1],"rb");assert(f);
 int w=0,h=0;enum mpp_pixel_format fmt=0;
 int ret=atoi(argv[2])?jpeg_get_img_size(&f,&w,&h,&fmt):png_get_img_size(&f,&w,&h,&fmt);fclose(f);
 if(atoi(argv[3])<0){if(ret==LV_FS_RES_OK){fprintf(stderr,"accepted bad header: %s\n",argv[1]);return 2;}}
 else if(ret!=LV_FS_RES_OK || w!=atoi(argv[3]) || h!=atoi(argv[4])){
 fprintf(stderr,"header FAIL %s ret=%d expected FS_OK=0 width=%d height=%d\n",argv[1],ret,w,h);return 3;}
 return 0;
}
'''
work=Path(tempfile.mkdtemp(prefix='un260-image-headers-'))
(work/'test.c').write_text('#include <stdint.h>\n'+enums+pre+body+main)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(work/'test.c'),'-o',str(work/'test')],check=True)
count=0
for p in sorted((root/'aic_ui/lvgl_data').rglob('*')):
    if p.suffix.lower() not in ('.png','.jpg','.jpeg'):continue
    with Image.open(p) as im:w,h=im.size
    subprocess.run([str(work/'test'),str(p),str(int(p.suffix.lower()!='.png')),str(w),str(h)],check=True);count+=1
for fmt in ('RGB','RGBA'):
    for suffix in ('.png','.jpg'):
        if fmt=='RGBA' and suffix=='.jpg':continue
        p=work/(fmt+suffix);Image.new(fmt,(48,32)).save(p)
        subprocess.run([str(work/'test'),str(p),str(int(suffix=='.jpg')),'48','32'],check=True)
        raw=p.read_bytes()
        for n in (0,1,8,16,24):
            bad=work/('bad'+suffix);bad.write_bytes(raw[:n])
            subprocess.run([str(work/'test'),str(bad),str(int(suffix=='.jpg')),'-1','0'],check=True)
print(f'PASS: {count} real images, RGB/RGBA PNG/JPEG, truncated inputs, real SDK enums')
