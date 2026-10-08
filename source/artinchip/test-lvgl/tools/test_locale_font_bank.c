#include "un260/font/ui_locale_font_bank.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned loads,frees,fail_at;
static unsigned read_calls;
static lv_fs_res_t (*source_read)(lv_fs_drv_t *,void *,void *,uint32_t,uint32_t *);
static lv_fs_res_t counted_read(lv_fs_drv_t *drv,void *file,void *buf,uint32_t n,uint32_t *read)
{++read_calls;return source_read(drv,file,buf,n,read);}
lv_font_t *__real_lv_font_load(const char *path);
void __real_lv_font_free(lv_font_t *font);
lv_font_t *__wrap_lv_font_load(const char *path){++loads;if(loads==fail_at)return NULL;return __real_lv_font_load(path);}
void __wrap_lv_font_free(lv_font_t *font){if(font)++frees;__real_lv_font_free(font);}
int main(int argc,char **argv){
 assert(argc==2);lv_init();
 lv_fs_drv_t *source=lv_fs_get_drv('S');assert(source&&source->read_cb);
 source_read=source->read_cb;source->read_cb=counted_read;
 unsigned original_cache=source->cache_size;
 assert(ui_locale_font_prepare(LANGUAGE_EN)&&loads==0);
 assert(!ui_locale_font_prepare(UI_LANGUAGE_INVALID));
 assert(ui_locale_font_get(LANGUAGE_CN,16)==NULL);
 char path[512];snprintf(path,sizeof(path),"%s/zh-Hans/font-12.bin",argv[1]);
 FILE *f=fopen(path,"r+b");assert(f);int original=fgetc(f);assert(original!=EOF);
 rewind(f);assert(fputc(original^1,f)!=EOF);assert(!fclose(f));
 assert(!ui_locale_font_prepare(LANGUAGE_CN)&&loads==0);
 f=fopen(path,"r+b");assert(f);assert(fputc(original,f)!=EOF);assert(!fclose(f));
 fail_at=4;assert(!ui_locale_font_prepare(LANGUAGE_CN));assert(loads==4&&frees==3);
 assert(ui_locale_font_get(LANGUAGE_CN,12)==NULL);
 fail_at=0;unsigned before=loads,reads_before=read_calls;
 assert(ui_locale_font_prepare_size(LANGUAGE_CN,22)&&loads==before+1);
 assert(read_calls-reads_before<4096); /* Includes CRC pass; never byte-at-a-time I/O. */
 assert(source->cache_size==original_cache); /* Images keep the original policy. */
 const lv_font_t *native=ui_locale_font_get(LANGUAGE_CN,22);assert(native);
 assert(ui_locale_font_get(LANGUAGE_CN,16)==NULL);
 assert(ui_locale_font_prepare_size(LANGUAGE_CN,22)&&loads==before+1);
 assert(!ui_locale_font_prepare_size(LANGUAGE_CN,99));
 fail_at=loads+3;assert(!ui_locale_font_prepare(LANGUAGE_CN));
 assert(ui_locale_font_get(LANGUAGE_CN,22)==native);
 assert(ui_locale_font_get(LANGUAGE_CN,10)==NULL);
 assert(frees==5);unsigned expected_frees=frees;
 fail_at=0;const lv_font_t *saved[4][15];
 const language_t langs[]={LANGUAGE_CN,LANGUAGE_KR,LANGUAGE_JA,LANGUAGE_AR};
 const unsigned sizes[]={10,12,14,16,18,20,22,24,26,28,32,36,37,50,55};
 for(unsigned l=0;l<4;++l){assert(ui_locale_font_prepare(langs[l]));
  for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);++s){saved[l][s]=ui_locale_font_get(langs[l],sizes[s]);assert(saved[l][s]);}}
 const lv_font_t *composed[4];
 for(unsigned l=0;l<4;++l){
  composed[l]=ui_locale_font_compose(&lv_font_montserrat_16,16,langs[l]);
  assert(composed[l]&&composed[l]!=&lv_font_montserrat_16);
  assert(composed[l]->fallback==ui_locale_font_get(langs[l],16));
  assert(composed[l]->line_height>=lv_font_montserrat_16.line_height);
 }
 assert(ui_locale_font_compose(&lv_font_montserrat_16,16,LANGUAGE_EN)==&lv_font_montserrat_16);
 assert(!ui_locale_font_compose(&lv_font_montserrat_16,99,LANGUAGE_AR));
 unsigned frozen=loads;
 for(unsigned repeat=0;repeat<20;++repeat)for(unsigned l=0;l<4;++l){
  assert(ui_locale_font_prepare(langs[l]));assert(composed[l]==ui_locale_font_compose(&lv_font_montserrat_16,16,langs[l]));for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);++s)assert(saved[l][s]==ui_locale_font_get(langs[l],sizes[s]));}
 assert(loads==frozen&&frees==expected_frees);assert(!ui_locale_font_get(LANGUAGE_JA,99));
 assert(ui_locale_font_get(LANGUAGE_CN,22)==native);
 puts("PASS font bank: corrupt-file rejection before load, partial allocation rollback, stable cached pointers, no reload on 80 switches");
 return 0;
}
