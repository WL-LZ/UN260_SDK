/* Scope: recoverable private allocation versus production LVGL fatal OOM.
 * LVGL's production abort policy is retained; no allocation recovery is faked. */
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "un260/lv_components/ui_panel.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_lang.h"
static enum { FAIL_NONE,FAIL_STATE,FAIL_ROOT,FAIL_LABEL,FAIL_TIMER } failure;
static unsigned allocations;
static bool fail_next;
void *__real_lv_mem_alloc(size_t size);
void *__wrap_lv_mem_alloc(size_t size)
{
    if(failure)allocations++;
    if(fail_next || (failure==FAIL_STATE&&allocations==1) ||
       (failure==FAIL_ROOT&&allocations==2)){fail_next=false;return NULL;}
    return __real_lv_mem_alloc(size);
}
lv_obj_t *__real_lv_label_create(lv_obj_t *parent);
lv_obj_t *__wrap_lv_label_create(lv_obj_t *parent)
{if(failure==FAIL_LABEL)fail_next=true;return __real_lv_label_create(parent);}
lv_timer_t *__real_lv_timer_create(lv_timer_cb_t cb,uint32_t period,void *user);
lv_timer_t *__wrap_lv_timer_create(lv_timer_cb_t cb,uint32_t period,void *user)
{if(failure==FAIL_TIMER)fail_next=true;return __real_lv_timer_create(cb,period,user);}
uint8_t lv_port_indev_touch_count(void){return 1;}
language_t ui_lang_get(void){return LANGUAGE_EN;}
const lv_font_t *ui_message_font_sized(const lv_font_t *base,unsigned px){(void)px;return base;}
static lv_color_t pixels[1280*20];
static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *source)
{(void)area;(void)source;lv_disp_flush_ready(driver);}
static unsigned timers(void){unsigned n=0;for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))n++;return n;}
static void fatal_case(unsigned mode,const char *name)
{
    pid_t child=fork();assert(child>=0);
    if(!child){
        /* Expected assertion diagnostics are intentionally separate from PASS. */
        char path[64];snprintf(path,sizeof(path),"failure-%u.log",mode);
        if(!freopen(path,"w",stderr))_exit(21);
        failure=mode;allocations=0;
        ui_panel_t panel={0};(void)ui_panel_create(&panel,lv_scr_act(),NULL,NULL);
        _exit(20); /* Returning false would claim a recovery LVGL cannot make. */
    }
    int status=0;assert(waitpid(child,&status,0)==child);
    if(mode==FAIL_TIMER){
        assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGABRT);
        printf("PASS: %s allocation fails at LVGL malloc assertion (SIGABRT)\n",name);
    }else{
#if PANEL_FAILURE_SANITIZED
        assert(WIFEXITED(status)&&WEXITSTATUS(status)==1);
        char path[64],diagnostic[4096]={0};snprintf(path,sizeof(path),"failure-%u.log",mode);
        FILE *log=fopen(path,"r");assert(log);size_t read=fread(diagnostic,1,sizeof(diagnostic)-1,log);assert(!ferror(log));fclose(log);diagnostic[read]=0;
        assert(strstr(diagnostic,"member access within null pointer")&&strstr(diagnostic,"lv_obj_mark_layout_as_dirty"));
        printf("PASS: %s allocation exposes LVGL internal NULL initialization (expected sanitizer termination)\n",name);
#else
        assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGSEGV);
        printf("PASS: %s allocation exposes LVGL internal NULL initialization (SIGSEGV)\n",name);
#endif
    }
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);lv_init();static lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*20);
    static lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;driver.draw_buf=&buffer;driver.flush_cb=flush;
    assert(lv_disp_drv_register(&driver));lv_obj_t *screen=lv_scr_act();
    unsigned children=lv_obj_get_child_cnt(screen),before=timers();
    failure=FAIL_STATE;allocations=0;ui_panel_t panel={0};
    assert(!ui_panel_create(&panel,screen,NULL,NULL));
    assert(!panel.root&&!panel.content&&!panel.private_state);
    assert(lv_obj_get_child_cnt(screen)==children&&timers()==before);
    failure=FAIL_NONE;assert(ui_panel_create(&panel,screen,NULL,NULL));ui_panel_destroy(&panel);
    assert(!panel.root&&!panel.content&&!panel.private_state);
    assert(lv_obj_get_child_cnt(screen)==children&&timers()==before);
    puts("PASS: private state allocation false leaves no root/timer/reference; retry and destroy clean");
    fatal_case(FAIL_ROOT,"root object");fatal_case(FAIL_LABEL,"partially constructed label");fatal_case(FAIL_TIMER,"completed-tree timer");
    return 0;
}
