/* Public pager API against real LVGL: geometry, pixels, input and lifetime. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_components/lv_content_pager.h"
#include "un260/lv_components/lv_capsule_pagination.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 400
static lv_color_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
static lv_color_t draw_pixels[SCREEN_WIDTH * 40];
static unsigned allocation_calls;
static unsigned object_creation_calls;
static bool fail_allocation;
void *__real_lv_mem_alloc(size_t size);
lv_obj_t *__real_lv_obj_create(lv_obj_t *parent);
lv_obj_t *__wrap_lv_obj_create(lv_obj_t *parent)
{
    ++object_creation_calls;
    return __real_lv_obj_create(parent);
}
void *__wrap_lv_mem_alloc(size_t size)
{
    ++allocation_calls;
    if(fail_allocation){fail_allocation=false;return NULL;}
    return __real_lv_mem_alloc(size);
}

static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *pixels)
{
    assert(area->x1>=0 && area->y1>=0 && area->x2<SCREEN_WIDTH && area->y2<SCREEN_HEIGHT);
    unsigned width=lv_area_get_width(area);
    for(int y=area->y1;y<=area->y2;++y)
        memcpy(framebuffer+y*SCREEN_WIDTH+area->x1,pixels+(y-area->y1)*width,width*sizeof(*pixels));
    lv_disp_flush_ready(driver);
}
static void advance(unsigned ms)
{
    while(ms){unsigned step=ms>5?5:ms;lv_tick_inc(step);lv_timer_handler();ms-=step;}
}
static void render(void)
{
    lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);
}
static unsigned objects(lv_obj_t *root)
{
    unsigned count=1;
    for(unsigned i=0;i<lv_obj_get_child_cnt(root);++i)count+=objects(lv_obj_get_child(root,i));
    return count;
}

typedef struct {
    lv_obj_t *root,*viewport,*pages[8];
    lv_coord_t width,height;
    uint8_t count;
} fixture_t;
static const uint32_t colors[]={0xBC3447,0x217952,0x296AC5,0xAD7930,0x834AAF,0x258C9C,0x854B3B,0x718332};
static fixture_t fixture(lv_coord_t x,lv_coord_t y,lv_coord_t width,lv_coord_t height,uint8_t count,bool circular)
{
    fixture_t f={.width=width,.height=height,.count=count};
    f.root=lv_content_pager_create(lv_scr_act(),x,y,width,height,count);assert(f.root);
    f.viewport=lv_content_pager_get_viewport(f.root);assert(f.viewport);
    for(unsigned i=0;i<count;++i){
        f.pages[i]=lv_content_pager_get_page(f.root,i);assert(f.pages[i]);
        lv_obj_set_style_bg_color(f.pages[i],lv_color_hex(colors[i]),0);
        lv_obj_set_style_bg_opa(f.pages[i],LV_OPA_COVER,0);
        lv_obj_t *mark=lv_obj_create(f.pages[i]);lv_obj_remove_style_all(mark);
        lv_obj_set_pos(mark,12,12);lv_obj_set_size(mark,14,14);
        lv_obj_set_style_bg_color(mark,lv_color_white(),0);lv_obj_set_style_bg_opa(mark,LV_OPA_COVER,0);
    }
    if(circular)assert(lv_content_pager_set_circular(f.root,true));
    render();return f;
}
static void identity(const fixture_t *f)
{
    assert(lv_content_pager_get_count(f->root)==f->count);
    assert(lv_obj_get_child_cnt(f->viewport)==f->count);
    for(unsigned i=0;i<f->count;++i){
        assert(lv_content_pager_get_page(f->root,i)==f->pages[i]);
        assert(lv_obj_get_parent(f->pages[i])==f->viewport);
        assert(lv_obj_get_width(f->pages[i])==f->width);
    }
}
static void pixel(const fixture_t *f,unsigned x,unsigned y,uint32_t color)
{
    lv_area_t bounds;lv_obj_get_coords(f->viewport,&bounds);
    assert(x<(unsigned)f->width && y<(unsigned)lv_obj_get_height(f->viewport));
    assert(framebuffer[(bounds.y1+y)*SCREEN_WIDTH+bounds.x1+x].full==lv_color_hex(color).full);
}
static void dots(const fixture_t *f,unsigned active)
{
    assert(f->count>1);render();
    lv_obj_t *indicator=lv_obj_get_child(f->root,1);
    assert(lv_obj_get_child_cnt(indicator)==f->count);
    int previous_x=-1;
    for(unsigned i=0;i<f->count;++i){
        lv_obj_t *dot=lv_obj_get_child(indicator,i);lv_area_t area;lv_obj_get_coords(dot,&area);
        assert(area.x1>previous_x);previous_x=area.x1;
        assert(framebuffer[((area.y1+area.y2)/2)*SCREEN_WIDTH+(area.x1+area.x2)/2].full==
               lv_color_hex(i==active?0xFFFFFF:0x7F8992).full);
    }
}
static void centered(const fixture_t *f,unsigned index)
{
    render();identity(f);
    assert(lv_content_pager_get_active(f->root)==index);
    assert(lv_obj_get_scroll_x(f->viewport)==0);
    assert(lv_obj_get_x(f->pages[index])==0 && !lv_content_pager_is_moving(f->root));
    unsigned y=lv_obj_get_height(f->viewport)/2;
    for(unsigned x=0;x<(unsigned)f->width;x+=7)pixel(f,x,y,colors[index]);
    dots(f,index);
}
static void drag_pixels(const fixture_t *f,unsigned index,unsigned neighbor)
{
    render();identity(f);
    assert(lv_obj_get_scroll_x(f->viewport)==0);
    int offset=lv_obj_get_x(f->pages[index]);
    assert(abs(offset)<f->width);
    unsigned y=lv_obj_get_height(f->viewport)/2;
    for(int x=0;x<f->width;++x){
        bool adjacent=offset<0?x>=f->width+offset:offset>0?x<offset:false;
        pixel(f,x,y,colors[adjacent?neighbor:index]);
    }
}
static void row_snapshot(const fixture_t *f,lv_color_t *out)
{
    render();lv_area_t bounds;lv_obj_get_coords(f->viewport,&bounds);
    int rows[]={18,lv_obj_get_height(f->viewport)/2,lv_obj_get_height(f->viewport)-6};
    for(unsigned i=0;i<3;++i)
        memcpy(out+i*f->width,framebuffer+(bounds.y1+rows[i])*SCREEN_WIDTH+bounds.x1,f->width*sizeof(*out));
}
static void update(const fixture_t *f,int32_t displacement)
{
    unsigned allocated=allocation_calls,created=object_creation_calls,count=objects(f->root);
    assert(lv_content_pager_drag_update(f->root,displacement));
    assert(allocation_calls==allocated && object_creation_calls==created && objects(f->root)==count);
}
static void animate(const fixture_t *f,unsigned ms)
{
    unsigned created=object_creation_calls,count=objects(f->root);
    while(ms){unsigned step=ms>20?20:ms;advance(step);render();ms-=step;}
    assert(object_creation_calls==created && objects(f->root)==count);
}
static void swipe(const fixture_t *f,int displacement,unsigned next)
{
    unsigned old=lv_content_pager_get_active(f->root);
    assert(lv_content_pager_drag_begin(f->root));advance(200);update(f,displacement);
    drag_pixels(f,old,next);
    assert(lv_content_pager_drag_end(f->root,false) && lv_content_pager_is_moving(f->root));
    assert(lv_content_pager_get_active(f->root)==old);dots(f,old);
    animate(f,60);assert(lv_content_pager_get_active(f->root)==old);dots(f,old);
    animate(f,400);centered(f,next);
}
static void test_cycles(void)
{
    fixture_t two=fixture(10,20,300,180,2,true);centered(&two,0);
    const unsigned next_two[]={1,0,1,0};
    for(unsigned i=0;i<4;++i)swipe(&two,-120,next_two[i]);
    for(unsigned i=0;i<4;++i)swipe(&two,120,next_two[i]);
    lv_obj_del(two.root);
    fixture_t three=fixture(10,20,300,180,3,true);centered(&three,0);
    const unsigned forward[]={1,2,0},backward[]={2,1,0};
    for(unsigned round=0;round<3;++round){
        for(unsigned i=0;i<3;++i)swipe(&three,-120,forward[i]);
        for(unsigned i=0;i<3;++i)swipe(&three,120,backward[i]);
    }
    assert(lv_content_pager_drag_begin(three.root));advance(200);update(&three,-40);drag_pixels(&three,0,1);
    advance(200);update(&three,40);drag_pixels(&three,0,2);
    assert(lv_content_pager_drag_end(three.root,true));advance(400);centered(&three,0);
    lv_obj_del(three.root);
    puts("PASS circular 2/3 pages: both directions, wraparound, stable page objects and gap-free pixels");
}
static void test_release_and_resistance(void)
{
    fixture_t f=fixture(10,20,300,180,2,true);
    assert(lv_content_pager_drag_begin(f.root));advance(250);update(&f,-40);
    assert(lv_content_pager_drag_end(f.root,false));advance(400);centered(&f,0);
    assert(lv_content_pager_drag_begin(f.root));advance(20);update(&f,-40);
    assert(lv_content_pager_drag_end(f.root,false));advance(400);centered(&f,1);
    assert(lv_content_pager_set_active(f.root,0,false));
    assert(lv_content_pager_drag_begin(f.root));advance(20);update(&f,-40);advance(180);
    assert(lv_content_pager_drag_end(f.root,false));advance(400);centered(&f,0);
    assert(lv_content_pager_drag_begin(f.root));
    for(unsigned i=0;i<25;++i){advance(20);update(&f,0);}
    advance(20);update(&f,-40);
    assert(lv_content_pager_drag_end(f.root,false));advance(400);centered(&f,1);
    assert(lv_content_pager_set_active(f.root,0,false));
    assert(lv_content_pager_drag_begin(f.root));
    const int32_t distances[]={270,300,360,600,3000,INT32_MAX};
    int offsets[6];
    for(unsigned i=0;i<6;++i){
        advance(20);update(&f,-distances[i]);drag_pixels(&f,0,1);
        offsets[i]=abs(lv_obj_get_x(f.pages[0]));
        if(i)assert(offsets[i]>=offsets[i-1]);
    }
    assert(offsets[1]-offsets[0]<distances[1]-distances[0]);
    assert((offsets[2]-offsets[1])*(distances[1]-distances[0])<
           (offsets[1]-offsets[0])*(distances[2]-distances[1]));
    assert(lv_content_pager_drag_end(f.root,true));advance(400);centered(&f,0);
    assert(lv_content_pager_drag_begin(f.root));advance(20);update(&f,INT32_MAX);drag_pixels(&f,0,1);
    advance(20);update(&f,INT32_MIN);drag_pixels(&f,0,1);
    assert(lv_content_pager_drag_end(f.root,true));advance(400);centered(&f,0);
    lv_obj_del(f.root);
    puts("PASS release: slow drag returns, flick and hold-then-flick advance, pause cancels stale velocity; progressive bounded resistance and no per-update allocation");
}
static void test_interrupt_finish_and_reset(void)
{
    fixture_t f=fixture(10,20,300,180,3,true);
    lv_color_t before[3*SCREEN_WIDTH],after[3*SCREEN_WIDTH];
    const unsigned interruption_ms[]={80,150};
    for(unsigned i=0;i<2;++i){
        assert(lv_content_pager_set_active(f.root,0,false));
        assert(lv_content_pager_drag_begin(f.root));advance(200);update(&f,-120);
        assert(lv_content_pager_drag_end(f.root,false));advance(interruption_ms[i]);
        assert(lv_content_pager_is_moving(f.root));row_snapshot(&f,before);
        assert(lv_content_pager_drag_begin(f.root));update(&f,0);row_snapshot(&f,after);
        assert(!memcmp(before,after,3*f.width*sizeof(*before)));
        unsigned current=lv_content_pager_get_active(f.root);
        advance(300);row_snapshot(&f,after);
        assert(!memcmp(before,after,3*f.width*sizeof(*before)) && lv_content_pager_get_active(f.root)==current);
        assert(lv_content_pager_drag_end(f.root,true));advance(400);centered(&f,current);
    }
    assert(lv_content_pager_set_active(f.root,0,false));
    assert(lv_content_pager_drag_begin(f.root));advance(200);update(&f,-120);
    assert(lv_content_pager_drag_end(f.root,false));
    assert(lv_content_pager_finish(f.root));centered(&f,1);
    assert(!lv_content_pager_finish(f.root));
    assert(lv_content_pager_set_active(f.root,0,false));
    assert(lv_content_pager_drag_begin(f.root));advance(200);update(&f,-100);
    assert(lv_content_pager_finish(f.root));centered(&f,0);
    assert(lv_content_pager_drag_begin(f.root));advance(200);update(&f,180);
    assert(lv_content_pager_finish(f.root));centered(&f,2);
    assert(lv_content_pager_set_active(f.root,0,true));advance(60);
    assert(lv_content_pager_is_moving(f.root));
    assert(lv_content_pager_set_active(f.root,1,false));advance(500);centered(&f,1);
    assert(!lv_content_pager_drag_end(f.root,false));
    assert(!lv_content_pager_set_active(f.root,3,true));centered(&f,1);
    assert(lv_content_pager_set_active(f.root,2,true));
    assert(lv_content_pager_finish(f.root));centered(&f,2);
    assert(lv_content_pager_set_circular(f.root,false));render();
    assert(lv_obj_has_flag(f.viewport,LV_OBJ_FLAG_SCROLLABLE));
    assert(lv_obj_get_scroll_snap_x(f.viewport)==LV_SCROLL_SNAP_CENTER);
    for(unsigned i=0;i<f.count;++i)assert(lv_obj_get_x(f.pages[i])==(int)i*f.width);
    assert(lv_content_pager_set_circular(f.root,true));centered(&f,2);
    lv_obj_del(f.root);advance(500);
    puts("PASS interruption: re-press preserves rendered pixels, cancelled drag returns, finish and reset settle without stale callbacks, native mode restores");
}

static lv_point_t pointer_point;
static lv_indev_state_t pointer_state=LV_INDEV_STATE_RELEASED;
static void read_pointer(lv_indev_drv_t *driver,lv_indev_data_t *data)
{(void)driver;data->point=pointer_point;data->state=pointer_state;}
static void point(int x,int y,bool pressed)
{pointer_point=(lv_point_t){x,y};pointer_state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;advance(40);}
static void native_geometry(const fixture_t *f,unsigned active)
{
    render();identity(f);
    assert(lv_content_pager_get_active(f->root)==active);
    assert(lv_obj_get_width(f->viewport)==f->width && lv_obj_get_height(f->viewport)==f->height-18);
    assert(lv_obj_has_flag(f->viewport,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_ONE));
    assert(lv_obj_get_scroll_snap_x(f->viewport)==LV_SCROLL_SNAP_CENTER);
    for(unsigned i=0;i<f->count;++i)assert(lv_obj_get_x(f->pages[i])==(int)i*f->width);
    assert(lv_obj_get_scroll_x(f->viewport)==(int)active*f->width);
    lv_obj_t *indicator=lv_obj_get_child(f->root,1);
    assert(lv_obj_get_y(indicator)==f->height-16 && lv_obj_get_x(indicator)==(f->width-lv_obj_get_width(indicator))/2);
    pixel(f,f->width/2,lv_obj_get_height(f->viewport)/2,colors[active]);
    dots(f,active);
}
static void test_default_native_pager(void)
{
    /* Actual pager dimensions/count used by the gesture guide and Innovation. */
    const int layouts[][5]={{18,56,744,202,3},{16,96,966,210,2}};
    for(unsigned n=0;n<2;++n){
        fixture_t f=fixture(layouts[n][0],layouts[n][1],layouts[n][2],layouts[n][3],layouts[n][4],false);
        native_geometry(&f,0);
        assert(!lv_content_pager_drag_begin(f.root) && !lv_content_pager_drag_update(f.root,200));
        assert(!lv_content_pager_drag_end(f.root,false) && !lv_content_pager_is_moving(f.root) && !lv_content_pager_finish(f.root));
        native_geometry(&f,0);
        assert(lv_content_pager_set_active(f.root,f.count-1,false));native_geometry(&f,f.count-1);
        assert(lv_content_pager_set_active(f.root,0,true));advance(1200);native_geometry(&f,0);
        lv_area_t area;lv_obj_get_coords(f.viewport,&area);int y=area.y1+lv_obj_get_height(f.viewport)/2;
        point(area.x1+f.width*5/6,y,true);point(area.x1+f.width/2,y,true);
        point(area.x1+f.width/6,y,true);point(area.x1+f.width/6,y,false);
        advance(1200);native_geometry(&f,1);
        lv_obj_del(f.root);advance(100);
    }
    fixture_t one=fixture(10,20,300,180,1,false);
    assert(!lv_content_pager_set_circular(one.root,true));
    assert(lv_obj_get_height(one.viewport)==180 && lv_obj_get_child_cnt(one.root)==1);
    lv_obj_del(one.root);
    puts("PASS default native compatibility: guide/Innovation pager geometry, indicators, programmatic paging and real pointer snap remain available");
}
static void test_delete_and_context_reuse(void)
{
    render();advance(100);unsigned baseline_objects=objects(lv_scr_act()),baseline_animations=lv_anim_count_running();
    lv_mem_monitor_t before,after;lv_mem_monitor(&before);
    fixture_t moving=fixture(10,20,300,180,2,true);
    assert(lv_content_pager_drag_begin(moving.root));advance(200);update(&moving,-120);
    assert(lv_content_pager_drag_end(moving.root,false));assert(lv_anim_count_running()>baseline_animations);
    lv_obj_del(moving.root);advance(500);
    assert(lv_anim_count_running()==baseline_animations && !lv_content_pager_is_moving(moving.root));
    unsigned count=objects(lv_scr_act()),allocated=allocation_calls;
    fail_allocation=true;
    assert(!lv_content_pager_create(lv_scr_act(),0,0,40,70,2));
    assert(!fail_allocation && allocation_calls==allocated+1 && objects(lv_scr_act())==count);
    assert(!lv_content_pager_create(NULL,0,0,40,70,2));
    assert(!lv_content_pager_create(lv_scr_act(),0,0,0,70,2));
    assert(!lv_content_pager_create(lv_scr_act(),0,0,40,70,0));
    assert(!lv_content_pager_create(lv_scr_act(),0,0,40,70,9));
    for(unsigned round=0;round<10;++round){
        fixture_t contexts[8];
        for(unsigned i=0;i<8;++i){
            contexts[i]=fixture(10,20,40,70,2,true);lv_obj_add_flag(contexts[i].root,LV_OBJ_FLAG_HIDDEN);
            assert(lv_content_pager_drag_begin(contexts[i].root));advance(200);update(&contexts[i],-20);
            assert(lv_content_pager_drag_end(contexts[i].root,false));
        }
        unsigned full_objects=objects(lv_scr_act());allocated=allocation_calls;
        assert(!lv_content_pager_create(lv_scr_act(),0,0,40,70,2));
        assert(objects(lv_scr_act())==full_objects && allocation_calls==allocated);
        for(unsigned i=0;i<8;i+=2)lv_obj_del(contexts[i].root);
        for(unsigned i=0;i<8;i+=2)contexts[i]=fixture(10,20,40,70,3,true);
        for(unsigned i=0;i<8;++i)lv_obj_del(contexts[i].root);
        advance(500);render();
        assert(objects(lv_scr_act())==baseline_objects && lv_anim_count_running()==baseline_animations);
    }
    lv_mem_monitor(&after);assert(after.free_size==before.free_size);
    puts("PASS lifetime: allocation failure, in-flight delete, eight-context exhaustion/reuse and unchanged memory after repeated reconstruction");
}
static void test_retarget_during_settle(void)
{
    fixture_t f=fixture(10,20,300,180,3,true);
    lv_content_pager_set_active(f.root,1,true);advance(60);render();
    int x=lv_obj_get_x(f.pages[0]);assert(x<0 && x>-300);
    lv_content_pager_set_active(f.root,1,true);render();assert(lv_obj_get_x(f.pages[0])==x);
    /* Repeated target must not prolong the original timeline. */
    advance(220);centered(&f,1);
    lv_content_pager_set_active(f.root,2,true);advance(60);render();x=lv_obj_get_x(f.pages[1]);
    lv_content_pager_set_active(f.root,1,true);render();assert(lv_obj_get_x(f.pages[1])==x);
    animate(&f,300);centered(&f,1);
    lv_content_pager_set_active(f.root,2,true);advance(60);render();x=lv_obj_get_x(f.pages[1]);
    lv_content_pager_set_active(f.root,0,true);render();assert(lv_obj_get_x(f.pages[1])==x);
    animate(&f,550);centered(&f,0);
    /* Explicit snapshot finalization discards any old deferred destination. */
    lv_content_pager_set_active(f.root,1,true);advance(50);
    lv_content_pager_set_active(f.root,2,true);lv_content_pager_finish(f.root);
    centered(&f,1);advance(600);centered(&f,1);
    lv_obj_del(f.root);
    puts("PASS animated retarget: repeated target keeps timeline, reverse retains pixels, third target waits, explicit finish cancels stale work");
}

static void test_capsule_repeat(void)
{
    lv_obj_t *root=lv_capsule_pagination_create(lv_scr_act());assert(root);
    assert(lv_capsule_pagination_set_count(root,3));render();
    lv_obj_t *dot=lv_obj_get_child(root,1);
    int small=lv_obj_get_width(dot),large=lv_obj_get_width(lv_obj_get_child(root,0));
    assert(lv_capsule_pagination_set_active_page(root,1));advance(80);render();
    int width=lv_obj_get_width(dot);assert(width>small && width<large);
    assert(lv_capsule_pagination_set_active_page(root,1));render();assert(lv_obj_get_width(dot)==width);
    advance(240);render();assert(lv_obj_get_width(dot)==large && lv_anim_count_running()==0);
    assert(lv_capsule_pagination_set_active_page(root,2));advance(80);render();
    width=lv_obj_get_width(dot);assert(lv_capsule_pagination_set_active_page(root,1));render();assert(lv_obj_get_width(dot)==width);
    advance(320);render();assert(lv_obj_get_width(dot)==large);
    assert(lv_capsule_pagination_set_active_page(root,2));advance(40);
    assert(lv_capsule_pagination_set_active_page_now(root,2));render();
    assert(lv_obj_get_width(lv_obj_get_child(root,2))==large);
    lv_obj_del(root);advance(400);assert(lv_anim_count_running()==0);
    puts("PASS capsule repeat/reverse: same target retains width and deadline, reverse retains width, explicit now and deletion remain immediate");
}

static void test_visible_page_count(void)
{
    fixture_t f=fixture(10,20,300,180,3,true);
    assert(lv_content_pager_set_active(f.root,2,false));
    assert(lv_content_pager_drag_begin(f.root));advance(200);update(&f,-100);
    unsigned made=object_creation_calls;
    assert(lv_content_pager_set_count(f.root,1));
    assert(!lv_content_pager_is_moving(f.root) && lv_content_pager_get_active(f.root)==0);
    assert(!lv_content_pager_drag_begin(f.root));
    assert(lv_obj_has_flag(f.pages[1],LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(f.pages[2],LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(lv_obj_get_child(f.root,1),LV_OBJ_FLAG_HIDDEN));
    assert(!lv_content_pager_set_count(f.root,0) && !lv_content_pager_set_count(f.root,4));
    assert(lv_content_pager_set_count(f.root,3));centered(&f,0);
    assert(!lv_obj_has_flag(f.pages[1],LV_OBJ_FLAG_HIDDEN));
    assert(object_creation_calls==made);swipe(&f,-120,1);lv_obj_del(f.root);
    puts("PASS page visibility: shrink during drag, clamp active, one-page no swipe, restore page identity/dots without allocation");
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);lv_init();
    static lv_disp_draw_buf_t buffer;static lv_disp_drv_t display;
    lv_disp_draw_buf_init(&buffer,draw_pixels,NULL,SCREEN_WIDTH*40);lv_disp_drv_init(&display);
    display.hor_res=SCREEN_WIDTH;display.ver_res=SCREEN_HEIGHT;display.draw_buf=&buffer;display.flush_cb=flush;
    assert(lv_disp_drv_register(&display));
    lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x102030),0);lv_obj_set_style_bg_opa(lv_scr_act(),LV_OPA_COVER,0);
    static lv_indev_drv_t pointer;lv_indev_drv_init(&pointer);pointer.type=LV_INDEV_TYPE_POINTER;pointer.read_cb=read_pointer;
    lv_indev_t *input=lv_indev_drv_register(&pointer);assert(input);
    assert(!lv_content_pager_set_circular(NULL,true) && !lv_content_pager_drag_begin(NULL));
    assert(!lv_content_pager_drag_update(NULL,10) && !lv_content_pager_drag_end(NULL,false));
    assert(!lv_content_pager_is_moving(NULL) && !lv_content_pager_finish(NULL));
    test_visible_page_count();test_capsule_repeat();test_retarget_during_settle();test_cycles();test_release_and_resistance();test_interrupt_finish_and_reset();test_default_native_pager();test_delete_and_context_reuse();
    lv_indev_delete(input);lv_deinit();
    puts("PASS content pager public API on real LVGL; board input latency and display performance require board validation");
}
