#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/counting/counting_data_store_internal.h"
/* Include the unchanged production translation unit to inspect its private
 * projection; no test-only accessors or mock page implementation in firmware. */
#include "un260/lv_core/page_02_list.c"
#include "test_recycled_list_cases.h"

void perf_profile_watch_invalidation(const void *o,const char *n) { (void)o;(void)n; }
void perf_profile_unwatch_invalidation(const void *o) { (void)o; }
static unsigned history_clicks,print_clicks,home_clicks;
void page_02_history_btn_event_cb(lv_event_t *e) { (void)e; ++history_clicks; }
void page_01_print_btn_event_cb(lv_event_t *e) { (void)e; ++print_clicks; }
void page_01_back_btn_event_cb(lv_event_t *e) { (void)e; ++home_clicks; }
static lv_color_t framebuffer[1280*400];
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p)
{
    int width=lv_area_get_width(a);
    for(int y=a->y1;y<=a->y2;++y)
        memcpy(framebuffer+y*1280+a->x1,p+(y-a->y1)*width,width*sizeof(*p));
    lv_disp_flush_ready(d);
}
static void tick(unsigned ms)
{ for(unsigned i=0;i<ms;i+=20) { lv_tick_inc(20);lv_timer_handler(); } }
static void write_bmp(const char *name)
{
    const char *dir=getenv("LIST_RASTER_OUTPUT"); if(!dir) return;
    lv_obj_update_layout(view->page);
    lv_obj_invalidate(view->page); lv_refr_now(NULL);
    char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",dir,name);
    FILE *f=fopen(path,"wb");assert(f);
    uint32_t size=54+1280*400*4, offset=54, dib=40, width=1280;
    int32_t height=-400;uint16_t planes=1,bits=32;uint8_t header[54]={0};
    header[0]='B';header[1]='M';memcpy(header+2,&size,4);memcpy(header+10,&offset,4);
    memcpy(header+14,&dib,4);memcpy(header+18,&width,4);memcpy(header+22,&height,4);
    memcpy(header+26,&planes,2);memcpy(header+28,&bits,2);
    assert(fwrite(header,1,54,f)==54);
    assert(fwrite(framebuffer,1,1280*400*4,f)==1280*400*4);
    fclose(f);
}
static unsigned timers(void)
{ unsigned n=0; for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n; }
static void label_fits(lv_obj_t *label)
{
    lv_point_t size;
    lv_txt_get_size(&size,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    if(size.x>lv_obj_get_width(label)) fprintf(stderr,"Text overflow '%s': %d > %d\n",lv_label_get_text(label),size.x,lv_obj_get_width(label));
    assert(size.x<=lv_obj_get_width(label));
}
int main(void)
{
    lv_init();static lv_color_t pixels[1280*40];static lv_disp_draw_buf_t buf;
    lv_disp_draw_buf_init(&buf,pixels,NULL,1280*40);
    lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;
    driver.draw_buf=&buf;driver.flush_cb=flush;lv_disp_drv_register(&driver);
    test_recycled_list_cases();
    counting_sim_t *data=counting_data_mutable();
    unsigned baseline_timers=timers();
    ui_page_02_list_create(lv_scr_act());tick(100);assert(view);
    assert(!strcmp(lv_label_get_text(view->section[0].headers[0]),"DENOM"));
    assert(!strcmp(lv_label_get_text(view->pcs),"0"));
    assert(lv_recycled_list_window(view->section[0].list)->count==1);
    assert(lv_recycled_list_window(view->section[1].list)->count==0);
    for(int i=0;i<3;++i) {
        lv_obj_t *viewport=lv_recycled_list_object(view->section[i].list);
        assert(lv_obj_has_flag(viewport,LV_OBJ_FLAG_USER_4));
        assert(lv_obj_has_flag(viewport,LV_OBJ_FLAG_PRESS_LOCK));
    }
    write_bmp("list-empty");
    /* Real controller data with holes, independent tables and true totals. */
    data->denom_number=8;int values[]={100,50,0,20,10,5,1,0};
    int pcs[]={18,12,0,14,11,10,9,0};
    for(int i=0;i<8;++i) { data->denom[i].value=values[i];data->denom[i].pcs=pcs[i];data->denom[i].amount=values[i]*pcs[i]; }
    data->total_pcs=74;data->total_amount=2849;
    assert(counting_data_ensure_serial_capacity(data,74));
    for(int i=0;i<74;++i) { data->sn_str[i]=malloc(24);snprintf(data->sn_str[i],24,"DEMO%09d",i+1);data->denom_mix[i]=20; }
    assert(counting_data_ensure_error_capacity(data,9));data->err_num=9;data->err_expected=14;
    uint8_t codes[]={0x15,0x11,0x22,0x16,0x1c,0x24,0x25,0x27,0x28};
    uint8_t reject_pcs[]={3,2,1,2,1,1,2,1,1};
    for(int i=0;i<9;++i) { data->err_code[i]=codes[i];data->err_pcs[i]=reject_pcs[i]; }
    page_02_list_report_reset();tick(100);assert(view->data.denom_count==6);
    assert(view->data.denom[2]==3);write_bmp("list-scroll");
    lv_event_send(view->section[1].mode,LV_EVENT_CLICKED,NULL);
    lv_event_send(view->section[1].next,LV_EVENT_CLICKED,NULL);
    assert(lv_recycled_list_window(view->section[1].list)->first==7);
    assert(!lv_recycled_list_window(view->section[2].list)->paged);
    lv_event_send(view->section[2].mode,LV_EVENT_CLICKED,NULL);
    lv_event_send(view->section[2].next,LV_EVENT_CLICKED,NULL);
    assert(lv_recycled_list_window(view->section[2].list)->first==7);
    tick(100);write_bmp("list-pages");
    /* Late packet does not reset the viewing anchor; hidden pages defer work. */
    page_02_list_section_data_ready(PAGE_02_SECTION_B);
    assert(lv_recycled_list_window(view->section[1].list)->first==7);
    ui_page_02_list_suspend();data->total_pcs=10000;data->total_amount=1000000000;
    page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
    assert(!strcmp(lv_label_get_text(view->pcs),"74"));
    assert(ui_page_02_list_resume());lv_obj_update_layout(view->page);
    assert(!strcmp(lv_label_get_text(view->pcs),"10000"));label_fits(view->pcs);label_fits(view->amount);
    assert(lv_recycled_list_window(view->section[1].list)->first==7);
    /* Large results stay local-coordinate safe and paginate past 255 pages. */
    assert(counting_data_ensure_serial_capacity(data,10000));
    for(int i=74;i<10000;++i) { data->sn_str[i]=malloc(16);strcpy(data->sn_str[i],"123456789ABCD");data->denom_mix[i]=100000; }
    page_02_list_section_data_ready(PAGE_02_SECTION_B);
    lv_recycled_list_page_step(view->section[1].list,100000);
    assert(ui_list_window_page_number(lv_recycled_list_window(view->section[1].list))==1429);
    tick(100);write_bmp("list-large-last-page");
    lv_obj_t *vp=lv_recycled_list_object(view->section[1].list);
    for(unsigned i=0;i<lv_obj_get_child_cnt(vp);++i) {
        lv_obj_t *row=lv_obj_get_child(vp,i);if(lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)||lv_obj_get_child_cnt(row)!=3)continue;
        assert(lv_obj_get_y(row)>=0 && lv_obj_get_y(row)<252);
        label_fits(lv_obj_get_child(row,0));label_fits(lv_obj_get_child(row,2));
    }
    for(int i=0;i<3;++i) lv_event_send(view->actions[i],LV_EVENT_CLICKED,NULL);
    assert(history_clicks==1 && print_clicks==1 && home_clicks==1);
    /* Queued callbacks must not outlive the owner. */
    ui_frame_commit_begin_batch();page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
    ui_page_02_list_destroy();ui_frame_commit_end_batch();ui_frame_commit_flush();tick(300);
    assert(!view);assert(timers()==baseline_timers);
    for(int i=0;i<5;++i) {ui_page_02_list_create(lv_scr_act());ui_page_02_list_suspend();ui_page_02_list_resume();ui_page_02_list_destroy();}
    assert(timers()==baseline_timers);
    counting_data_clear_serials(data);counting_data_clear_errors(data);
    puts("PASS actual List/LVGL raster, data projection, modes, 10000 rows, callbacks and lifecycle");
    return 0;
}
