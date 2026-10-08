#!/usr/bin/env python3
"""Exercise the production retained-page entry function across locale changes."""
import re
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
text = (root / 'un260/lv_core/lv_page_manager.c').read_text()
match = re.search(r'static const char \*create_new_page\(ui_page_t page\)\n\{.*?^\}', text, re.M | re.S)
assert match
source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef unsigned ui_page_t;
enum { UI_PAGE_RETAINED=1, UI_DATA_TOPIC_NONE=0 };
static unsigned generation=1, creates, destroys, resumes, refreshes;
static unsigned rendered_generation;
static bool alive;
static bool g_page_cache_ready[1];
static uint32_t g_page_cache_language[1];
static unsigned g_page_data_dirty[1];
static uint32_t ui_lang_generation(void){return generation;}
static void *lv_scr_act(void){return NULL;}
static void ui_frame_commit_begin_sync(void){}
static void ui_frame_commit_end_sync(void){}
static void create(void *root){(void)root;creates++;alive=true;rendered_generation=generation;}
static void destroy(void){destroys++;alive=false;}
static bool resume(void){if(!alive)return false;resumes++;return true;}
static void refresh(unsigned topics){(void)topics;refreshes++;}
typedef struct {unsigned cache_policy;void (*create)(void *);void (*destroy)(void);bool (*resume)(void);void (*refresh_data)(unsigned);} ui_page_registration_t;
static const ui_page_registration_t g_page_registry[]={{UI_PAGE_RETAINED,create,destroy,resume,refresh}};
'''
source += match[0]
source += r'''
int main(void){
 assert(!strcmp(create_new_page(0),"CREATE"));
 assert(creates==1&&destroys==0&&rendered_generation==1);
 g_page_data_dirty[0]=1;
 assert(!strcmp(create_new_page(0),"RESUME"));
 assert(creates==1&&resumes==1&&refreshes==1);
 /* Startup restore/test selection can change locale without the Settings callback. */
 generation=2;
 assert(!strcmp(create_new_page(0),"CREATE"));
 assert(creates==2&&destroys==1&&rendered_generation==2);
 assert(!strcmp(create_new_page(0),"RESUME"));
 generation=3;
 assert(!strcmp(create_new_page(0),"CREATE"));
 assert(creates==3&&destroys==2&&rendered_generation==3);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'cache.c'
    c.write_text(source)
    executable = Path(directory) / 'cache'
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-fsanitize=undefined', str(c), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('PASS retained page locale generation: cold creation, same-language reuse, restore without callback, repeated changes')
