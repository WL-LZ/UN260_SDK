#!/usr/bin/env python3
"""Production deferred page lifecycle with isolated object/service stubs."""
from pathlib import Path
import subprocess,tempfile
from test_main_top_gesture import function
root=Path(__file__).resolve().parents[1]
source=(root/'un260/lv_core/page_08_boot.c').read_text()
names=['ui_page_08_curr_defer_next_create','ui_page_08_curr_create',
       'ui_page_08_curr_prepare_step','ui_page_08_curr_set_covered','ui_page_08_curr_destroy']
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { bool hidden; } lv_obj_t;
static lv_obj_t object;
static struct {lv_obj_t *page; unsigned prepare_step;} g_boot_page;
static bool g_defer_next_create;
static unsigned background,progress,rows,resets,stops;
#define LV_OBJ_FLAG_SCROLLABLE 1
#define LV_OBJ_FLAG_HIDDEN 2
#define LV_SCROLLBAR_MODE_OFF 0
#define BOOT_STAGE_HANDSHAKE 0
#define BOOT_STAGE_SENSOR 1
#define BOOT_STAGE_IMAGE 5
#define BOOT_STAGE_DONE 6
#define HANDSHAKE_OK 1
static int page_08_curr_obj,page_08_curr_len;
static lv_obj_t *lv_scr_act(void){return &object;}
static lv_obj_t *lv_obj_create(lv_obj_t*p){assert(p);return &object;}
static bool lv_obj_is_valid(lv_obj_t*p){return p==&object;}
static void lv_obj_remove_style_all(lv_obj_t*p){(void)p;}
static void lv_obj_set_pos(lv_obj_t*p,int x,int y){(void)p;(void)x;(void)y;}
static void lv_obj_set_size(lv_obj_t*p,int x,int y){(void)p;(void)x;(void)y;}
static void lv_obj_set_scrollbar_mode(lv_obj_t*p,int x){(void)p;(void)x;}
static void lv_obj_clear_flag(lv_obj_t*p,int x){if(x==2)p->hidden=false;}
static void lv_obj_add_flag(lv_obj_t*p,int x){if(x==2)p->hidden=true;}
static void lv_obj_del(lv_obj_t*p){assert(p==&object);}
static void lv_ui_obj_init(lv_obj_t*p,int a,int b){(void)p;(void)a;(void)b;background++;}
static void boot_progress_create(lv_obj_t*p){(void)p;progress++;}
static void boot_selftest_list_create(lv_obj_t*p){(void)p;rows++;}
static void boot_selftest_list_reset(void){resets++;}
static void boot_selftest_list_sync_step(unsigned s){assert(s==0);}
static unsigned boot_service_self_test_sequence_index(void){return 0;}
static int boot_service_get_stage(void){return BOOT_STAGE_HANDSHAKE;}
static int boot_service_handshake_state(void){return 0;}
static void boot_progress_set(unsigned s){assert(s==0);}
static void boot_progress_handshake_tick_start(void){}
static void boot_progress_handshake_tick_stop(void){stops++;}
static void boot_page_context_reset(void){memset(&g_boot_page,0,sizeof(g_boot_page));}
void ui_page_08_curr_destroy(void);
bool ui_page_08_curr_prepare_step(void);
'''
fixture+='\n'.join(function(source,name) for name in names)
fixture+=r'''
int main(void){
 ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);
 assert(g_boot_page.page && !background && !progress && !rows);
 ui_page_08_curr_create(NULL);assert(!background);
 assert(!ui_page_08_curr_prepare_step() && background==1 && !progress);
 assert(!ui_page_08_curr_prepare_step() && progress==1 && !rows);
 assert(!ui_page_08_curr_prepare_step() && rows==1 && resets==1);
 assert(ui_page_08_curr_prepare_step());assert(ui_page_08_curr_prepare_step());
 ui_page_08_curr_destroy();assert(!g_boot_page.page);
 ui_page_08_curr_create(NULL);assert(background==2 && rows==2);
 ui_page_08_curr_destroy();ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);
 ui_page_08_curr_set_covered(true);assert(object.hidden && rows==2);
 ui_page_08_curr_set_covered(false);assert(!object.hidden && rows==3);
 ui_page_08_curr_destroy();assert(ui_page_08_curr_prepare_step());
 puts("PASS deferred first screen, four preparation steps, duplicate create, legacy synchronous creation, failed-overlay reveal, destruction");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-deferred-') as directory:
 p=Path(directory);(p/'test.c').write_text(fixture)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
