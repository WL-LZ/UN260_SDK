#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "aic_ui/compiled_asset.h"
#include "test_page_background_asset.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/lv_core/page_02_list_search.c"
/* Include the unchanged production translation unit to inspect its private
 * projection; no test-only accessors or mock page implementation in firmware. */
#include "un260/lv_core/page_02_list.c"
#include "test_recycled_list_cases.h"
#include "test_alnum_keyboard_cases.h"

/* Host-only bridge: the production decoder uploads these exact BGRA arrays
 * into DMA frames. The software renderer reads them directly, without PNG I/O
 * or a replacement image. This does not exercise the board's DMA/GE decoder. */
#if LV_COLOR_DEPTH != 32
#error The compiled BGRA raster test requires LV_COLOR_DEPTH 32
#endif
static const char *const image_paths[]={
    LVGL_DIR "list_icons/receipt_24.png", LVGL_DIR "list_icons/barcode_24.png",
    LVGL_DIR "list_icons/warning_circle_24.png", LVGL_DIR "list_icons/barcode_36.png",
    LVGL_DIR "list_icons/warning_circle_36.png"
};
static unsigned image_opens[5];
static lv_res_t host_asset_info(lv_img_decoder_t *decoder,const void *src,lv_img_header_t *header)
{
    (void)decoder;
    if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE) return LV_RES_INV;
    const un260_compiled_asset_t *asset=test_page_asset_find(src);
    if(!asset || !asset->has_alpha || asset->stride!=asset->width*4) return LV_RES_INV;
    memset(header,0,sizeof(*header));
    header->w=asset->width;header->h=asset->height;header->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
    return LV_RES_OK;
}
static lv_res_t host_asset_open(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *dsc)
{
    if(host_asset_info(decoder,dsc->src,&dsc->header)!=LV_RES_OK) return LV_RES_INV;
    const un260_compiled_asset_t *asset=test_page_asset_find(dsc->src);
    dsc->img_data=asset->pixels;
    for(unsigned i=0;i<5;++i)
        if(!strcmp(dsc->src,image_paths[i])) ++image_opens[i];
    return LV_RES_OK;
}
static void host_asset_close(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *dsc)
{ (void)decoder;dsc->img_data=NULL; }
static void register_host_assets(void)
{
    lv_img_decoder_t *decoder=lv_img_decoder_create();assert(decoder);
    lv_img_decoder_set_info_cb(decoder,host_asset_info);
    lv_img_decoder_set_open_cb(decoder,host_asset_open);
    lv_img_decoder_set_close_cb(decoder,host_asset_close);
    for(unsigned i=0;i<5;++i) {
        const un260_compiled_asset_t *asset=un260_compiled_asset_find(image_paths[i]);
        if(!asset) fprintf(stderr,"Missing actual compiled List image: %s\n",image_paths[i]);
        assert(asset && asset->pixels && asset->has_alpha==1);
        unsigned side=i<3 ? 24 : 36;
        assert(asset->width==side && asset->height==side && asset->stride==side*4);
        lv_img_header_t header;
        assert(lv_img_decoder_get_info(image_paths[i],&header)==LV_RES_OK);
        assert(header.w==side && header.h==side && header.cf==LV_IMG_CF_TRUE_COLOR_ALPHA);
    }
    assert(!un260_compiled_asset_find(LVGL_DIR "list_icons/missing.png"));
    lv_img_header_t header;
    assert(host_asset_info(decoder,LVGL_DIR "list_icons/missing.png",&header)==LV_RES_INV);
}

void perf_profile_watch_invalidation(const void *o,const char *n) { (void)o;(void)n; }
void perf_profile_unwatch_invalidation(const void *o) { (void)o; }
static unsigned history_clicks,print_clicks,home_clicks;
void page_02_history_btn_event_cb(lv_event_t *e) { (void)e; ++history_clicks; }
void page_01_print_btn_event_cb(lv_event_t *e) { (void)e; ++print_clicks; }
void page_01_back_btn_event_cb(lv_event_t *e) { (void)e; ++home_clicks; }
/* Host-only failure injection leaves the real search create/cleanup path intact. */
static bool fail_search_owner_alloc;
void *__real_lv_mem_alloc(size_t size);
void *__wrap_lv_mem_alloc(size_t size)
{
    if(fail_search_owner_alloc && size==sizeof(page_02_list_search_t)) {
        fail_search_owner_alloc=false;
        return NULL;
    }
    return __real_lv_mem_alloc(size);
}
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
static void render_page(void)
{
    lv_obj_update_layout(view->page);
    lv_obj_invalidate(view->page);lv_refr_now(NULL);
}
static void write_bmp(const char *name)
{
    const char *dir=getenv("LIST_RASTER_OUTPUT"); if(!dir) return;
    render_page();
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
    lv_txt_get_size(&size,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),
                   lv_obj_get_style_text_letter_space(label,0),lv_obj_get_style_text_line_space(label,0),
                   LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    if(size.x>lv_obj_get_width(label)) fprintf(stderr,"Text overflow '%s': %d > %d\n",lv_label_get_text(label),size.x,lv_obj_get_width(label));
    assert(size.x<=lv_obj_get_width(label));
}
static unsigned color_pixels(lv_obj_t *object,uint32_t color)
{
    lv_area_t area;lv_obj_get_coords(object,&area);unsigned count=0;
    assert(area.x1>=0 && area.x2<1280 && area.y1>=0 && area.y2<400);
    for(int y=area.y1;y<=area.y2;++y)
        for(int x=area.x1;x<=area.x2;++x)
            if(framebuffer[y*1280+x].full==lv_color_hex(color).full)++count;
    return count;
}
static void assert_icon(lv_obj_t *icon,uint32_t color,unsigned minimum_pixels,bool is_visible)
{
    assert(icon && !lv_obj_has_flag(icon,LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag_any(icon,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    assert(lv_obj_get_style_bg_opa(icon,0)==LV_OPA_TRANSP);
    assert(lv_obj_is_visible(icon)==is_visible);
    if(is_visible) {
        unsigned pixels=color_pixels(icon,color);
        if(pixels<minimum_pixels)
            fprintf(stderr,"Icon coverage: pixels=%u minimum=%u color=%06lX\n",pixels,minimum_pixels,(unsigned long)color);
        assert(pixels>=minimum_pixels);
    }
}
static void assert_home_icon(bool is_visible)
{
    lv_obj_t *home=view->actions[LIST_ACTION_MAIN], *caption=lv_damped_button_get_label(home);
    assert(lv_obj_get_child_cnt(home)==2 && lv_obj_get_child(home,0)==caption);
    assert(lv_obj_check_type(caption,&lv_label_class));
    assert(!strcmp(lv_label_get_text(caption),ui_text_get(UI_TEXT_LIST_MAIN)));
    lv_obj_t *icon=lv_obj_get_child(home,1);
    assert(!lv_obj_check_type(icon,&lv_label_class) && !lv_obj_check_type(icon,&lv_img_class));
    assert(lv_obj_get_child_cnt(icon)==0);
    assert(lv_obj_get_width(icon)==27 && lv_obj_get_height(icon)==27);
    assert(lv_obj_get_x(icon)==35 && lv_obj_get_y(icon)==17);
    assert(lv_obj_get_x(home)==1168 && lv_obj_get_y(home)==300);
    assert(lv_obj_get_width(home)==96 && lv_obj_get_height(home)==88);
    assert_icon(icon,0x657F90,21,is_visible);
}
static void list_home_reference_draw(lv_event_t *event)
{
    /* Independent oracle: unchanged original List house geometry. */
    static const lv_point_t segments[][2]={
        {{2,12},{13,2}},{{13,2},{24,12}},{{4,10},{4,24}},
        {{4,24},{22,24}},{{22,24},{22,10}}
    };
    lv_draw_line_dsc_t style;lv_draw_line_dsc_init(&style);
    style.color=lv_color_hex(0x657F90);style.width=2;
    style.round_start=style.round_end=1;
    for(unsigned i=0;i<5;++i)
        lv_draw_line(lv_event_get_draw_ctx(event),&style,&segments[i][0],&segments[i][1]);
}
static void test_home_glyph(void)
{
    render_page();assert_home_icon(true);
    lv_area_t area;lv_obj_get_coords(lv_obj_get_child(view->actions[LIST_ACTION_MAIN],1),&area);
    uint32_t actual[27*27],signature=0;
    for(unsigned y=0;y<27;++y) for(unsigned x=0;x<27;++x) {
        actual[y*27+x]=framebuffer[(area.y1+y)*1280+area.x1+x].full;
        signature=signature*33+actual[y*27+x];
    }
    lv_obj_t *reference=lv_obj_create(lv_scr_act());assert(reference);
    lv_obj_remove_style_all(reference);lv_obj_set_pos(reference,0,0);lv_obj_set_size(reference,27,27);
    lv_obj_set_style_bg_color(reference,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_bg_opa(reference,LV_OPA_COVER,0);
    lv_obj_clear_flag(reference,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(reference,list_home_reference_draw,LV_EVENT_DRAW_MAIN,NULL);
    render_page();
    for(unsigned y=0;y<27;++y) for(unsigned x=0;x<27;++x)
        assert(actual[y*27+x]==framebuffer[y*1280+x].full);
    lv_obj_del(reference);render_page();
    printf("HOME_GLYPH list %08lx (original List 27x27 reference)\n",(unsigned long)signature);
}
static void search_reference_draw(lv_event_t *event)
{
    /* A magnifying glass, not a reused history/print glyph. */
    lv_draw_arc_dsc_t circle;lv_draw_arc_dsc_init(&circle);
    circle.color=lv_color_hex(0x657F90);circle.width=2;
    const lv_point_t center={11,11};
    lv_draw_arc(lv_event_get_draw_ctx(event),&circle,&center,8,0,360);
    lv_draw_line_dsc_t handle;lv_draw_line_dsc_init(&handle);
    handle.color=lv_color_hex(0x657F90);handle.width=2;
    handle.round_start=handle.round_end=1;
    const lv_point_t start={17,17},end={24,24};
    lv_draw_line(lv_event_get_draw_ctx(event),&handle,&start,&end);
}
static void test_search_glyph(void)
{
    render_page();
    lv_obj_t *icon=lv_obj_get_child(view->actions[LIST_ACTION_SEARCH],1);
    assert(lv_obj_get_width(icon)==27 && lv_obj_get_height(icon)==27);
    assert_icon(icon,0x657F90,21,true);
    lv_area_t area;lv_obj_get_coords(icon,&area);
    uint32_t actual[27*27];
    for(unsigned y=0;y<27;++y) for(unsigned x=0;x<27;++x)
        actual[y*27+x]=framebuffer[(area.y1+y)*1280+area.x1+x].full;
    lv_obj_t *reference=lv_obj_create(lv_scr_act());assert(reference);
    lv_obj_remove_style_all(reference);lv_obj_set_pos(reference,0,0);lv_obj_set_size(reference,27,27);
    lv_obj_set_style_bg_color(reference,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_bg_opa(reference,LV_OPA_COVER,0);
    lv_obj_clear_flag(reference,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(reference,search_reference_draw,LV_EVENT_DRAW_MAIN,NULL);
    render_page();
    for(unsigned y=0;y<27;++y) for(unsigned x=0;x<27;++x)
        assert(actual[y*27+x]==framebuffer[y*1280+x].full);
    lv_obj_del(reference);render_page();
    puts("PASS Search magnifier matches the 27x27 circle-and-handle raster reference");
}
static void assert_png_icon(lv_obj_t *icon,unsigned index,bool is_visible)
{
    assert(icon && lv_obj_check_type(icon,&lv_img_class));
    assert(!lv_obj_has_flag_any(icon,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_is_visible(icon)==is_visible);
    assert(lv_obj_get_style_bg_opa(icon,0)==LV_OPA_TRANSP);
    assert(lv_obj_get_style_img_recolor_opa(icon,0)==LV_OPA_TRANSP);
    assert(lv_obj_get_style_img_opa(icon,0)==LV_OPA_COVER);
    assert(lv_img_get_angle(icon)==0 && lv_img_get_zoom(icon)==256);
    assert(lv_img_src_get_type(lv_img_get_src(icon))==LV_IMG_SRC_FILE);
    assert(!strcmp(lv_img_get_src(icon),image_paths[index]));
    const un260_compiled_asset_t *asset=un260_compiled_asset_find(lv_img_get_src(icon));
    assert(asset && lv_obj_get_width(icon)==(int)asset->width && lv_obj_get_height(icon)==(int)asset->height);
    if(!is_visible) return;
    assert(image_opens[index]>0);
    lv_area_t a;lv_obj_get_coords(icon,&a);
    assert(a.x1>=0 && a.x2<1280 && a.y1>=0 && a.y2<400);
    unsigned transparent=0,ink=0,antialiased=0;
    for(unsigned y=0;y<asset->height;++y) for(unsigned x=0;x<asset->width;++x) {
        const unsigned char *pixel=asset->pixels+y*asset->stride+x*4;
        unsigned alpha=pixel[3];
        lv_color_t actual=framebuffer[(a.y1+y)*1280+a.x1+x];
        if(!alpha) { ++transparent;assert(actual.full==lv_color_hex(0xFFFFFF).full); }
        else {
            ++ink;if(alpha<255) ++antialiased;
            /* Straight-alpha BGRA composed on the unchanged white panel.
             * Allow two levels for LVGL's integer opacity rounding. */
            int blue=(pixel[0]*alpha+255*(255-alpha)+127)/255;
            int green=(pixel[1]*alpha+255*(255-alpha)+127)/255;
            int red=(pixel[2]*alpha+255*(255-alpha)+127)/255;
            if(abs((int)actual.ch.blue-blue)>2 || abs((int)actual.ch.green-green)>2 ||
               abs((int)actual.ch.red-red)>2)
                fprintf(stderr,"PNG raster mismatch: %s pixel=%u,%u alpha=%u expected=%d,%d,%d actual=%u,%u,%u\n",
                        image_paths[index],x,y,alpha,red,green,blue,
                        actual.ch.red,actual.ch.green,actual.ch.blue);
            assert(abs((int)actual.ch.blue-blue)<=2);
            assert(abs((int)actual.ch.green-green)<=2);
            assert(abs((int)actual.ch.red-red)<=2);
        }
    }
    assert(transparent>0 && ink>20 && antialiased>0);
}
static void assert_background(lv_obj_t *object,uint32_t color,bool is_visible)
{
    assert(lv_obj_get_style_bg_color(object,0).full==lv_color_hex(color).full);
    assert(lv_obj_get_style_bg_opa(object,0)==LV_OPA_COVER);
    if(is_visible) {
        /* An interior pixel above the contents avoids rounded corners,
         * borders, labels and the button's animated child translation. */
        lv_area_t a;lv_obj_get_coords(object,&a);
        int x=(a.x1+a.x2)/2,y=a.y1+4;
        assert(x>=0 && x<1280 && y>=0 && y<400);
        assert(framebuffer[y*1280+x].full==lv_color_hex(color).full);
    }
}
static void assert_caption_contained(lv_obj_t *button)
{
    lv_obj_t *label=lv_damped_button_get_label(button);
    assert(label);label_fits(label);
    lv_point_t size;
    lv_txt_get_size(&size,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),
                   lv_obj_get_style_text_letter_space(label,0),lv_obj_get_style_text_line_space(label,0),
                   LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    assert(size.y<=lv_obj_get_height(label));
    lv_area_t caption,area;
    lv_obj_get_coords(label,&caption);lv_obj_get_coords(button,&area);
    assert(caption.x1>=area.x1 && caption.x2<=area.x2);
    assert(caption.y1>=area.y1 && caption.y2<=area.y2);
}
static lv_obj_t *only_image(lv_obj_t *parent)
{
    lv_obj_t *found=NULL;
    for(unsigned i=0;i<lv_obj_get_child_cnt(parent);++i) {
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(!lv_obj_check_type(child,&lv_img_class)) continue;
        assert(!found);found=child;
    }
    assert(found);return found;
}
static void assert_column_header_layout(list_section_t *s,bool page_visible)
{
    const int rule_y[]={48,75};
    lv_obj_t *rules[2]={NULL,NULL};
    for(unsigned i=0;i<lv_obj_get_child_cnt(s->panel);++i) {
        lv_obj_t *child=lv_obj_get_child(s->panel,i);
        if(lv_obj_get_height(child)!=1) continue;
        for(unsigned j=0;j<2;++j) if(lv_obj_get_y(child)==rule_y[j]) {
            assert(!rules[j]);rules[j]=child;
        }
    }
    lv_area_t top,bottom,title,body;
    for(unsigned j=0;j<2;++j) {
        lv_obj_t *rule=rules[j];
        assert(rule && lv_obj_get_x(rule)==12 && lv_obj_get_width(rule)==s->layout->width-36);
        assert(!lv_obj_has_flag_any(rule,LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        assert(lv_obj_is_visible(rule)==page_visible);
        assert(lv_obj_get_style_bg_color(rule,0).full==lv_color_hex(0xE7ECEF).full);
        assert(lv_obj_get_style_bg_opa(rule,0)==LV_OPA_COVER);
        assert(lv_obj_get_style_radius(rule,0)==0 && lv_obj_get_style_border_width(rule,0)==0);
        if(page_visible) assert(color_pixels(rule,0xE7ECEF)==(unsigned)lv_obj_get_width(rule));
    }
    lv_obj_get_coords(rules[0],&top);lv_obj_get_coords(rules[1],&bottom);
    lv_obj_get_coords(s->title,&title);
    assert(lv_obj_get_y(s->title)==16 && lv_obj_get_height(s->title)==28);
    assert(top.y1-title.y2-1==4);
    lv_obj_t *badge=lv_obj_get_child(s->panel,0),*icon=only_image(s->panel);
    lv_area_t badge_area,icon_area;
    lv_obj_get_coords(badge,&badge_area);lv_obj_get_coords(icon,&icon_area);
    assert(badge_area.y2<top.y1 && icon_area.y2<top.y1);
    lv_obj_t *viewport=lv_recycled_list_object(s->list);
    assert(BODY_Y==76 && ROWS==7 && ROW_HEIGHT==36);
    assert(lv_obj_get_y(viewport)==76 && lv_obj_get_height(viewport)==252);
    lv_obj_get_coords(viewport,&body);
    assert(bottom.y2+1==body.y1);
    for(unsigned col=0;col<3;++col) {
        lv_obj_t *header=s->headers[col];
        const lv_font_t *font=lv_obj_get_style_text_font(header,0);
        assert(font==&lv_font_instrument_sans_medium_12);
        int height=lv_font_get_line_height(font);
        assert(height>0 && height<=75-49);
        assert(lv_obj_get_height(header)==height);
        assert(lv_obj_get_y(header)==49+(75-49-height)/2);
        assert(lv_obj_get_style_pad_top(header,0)==0 && lv_obj_get_style_pad_bottom(header,0)==0);
        assert(lv_obj_get_style_text_line_space(header,0)==0);
        lv_area_t area;lv_obj_get_coords(header,&area);
        int above=area.y1-top.y2-1,below=bottom.y1-area.y2-1;
        assert(above>=0 && below>=0 && abs(above-below)<=1);
        assert(area.x1>=top.x1 && area.x2<=top.x2);
        assert(area.y1>title.y2 && area.y2<body.y1);
    }
}
static void assert_icons(bool serial_empty,bool reject_empty,bool page_visible)
{
    const bool empty[]={false,serial_empty,reject_empty};
    lv_obj_update_layout(view->page);
    if(page_visible) render_page();
    for(int i=0;i<3;++i) {
        list_section_t *s=&view->section[i];
        assert_column_header_layout(s,page_visible);
        lv_obj_t *badge=lv_obj_get_child(s->panel,0),*icon=only_image(s->panel);
        assert_png_icon(icon,i,page_visible);
        lv_area_t a,b,c;lv_obj_get_coords(badge,&a);lv_obj_get_coords(icon,&b);lv_obj_get_coords(s->title,&c);
        assert(lv_obj_get_x(s->title)==52 && lv_obj_get_width(s->title)==s->layout->width-104);
        assert(lv_obj_get_x(icon)==s->layout->width-42 && lv_obj_get_y(icon)==16);
        assert(c.x1-a.x2>=10 && b.x1-c.x2>=10);
        lv_area_t panel;lv_obj_get_coords(s->panel,&panel);
        assert(b.x2<=panel.x2 && b.y1>=panel.y1 && b.y2<=panel.y2);
        assert(!strcmp(lv_label_get_text(s->title),ui_text_get(s->layout->title)));
        label_fits(s->title);
        for(int col=0;col<3;++col) {
            assert(!strcmp(lv_label_get_text(s->headers[col]),ui_text_get(s->layout->columns[col])));
            label_fits(s->headers[col]);
        }
        if(i==PAGE_02_SECTION_A) { assert(!s->empty && !s->empty_text);continue; }
        assert(s->empty && s->empty_text);
        assert(lv_obj_get_x(s->empty)==22 && lv_obj_get_y(s->empty)==BODY_Y);
        assert(lv_obj_get_width(s->empty)==s->layout->width-58 && lv_obj_get_height(s->empty)==ROWS*ROW_HEIGHT);
        assert(!lv_obj_has_flag_any(s->empty,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        assert(lv_obj_get_style_bg_opa(s->empty,0)==LV_OPA_TRANSP);
        assert(lv_obj_get_style_flex_flow(s->empty,0)==LV_FLEX_FLOW_COLUMN);
        assert(lv_obj_get_style_pad_row(s->empty,0)==12);
        assert(lv_obj_get_child_cnt(s->empty)==2);
        assert(lv_obj_get_parent(s->empty_text)==s->empty);
        assert(!lv_obj_has_flag(s->empty,LV_OBJ_FLAG_HIDDEN)==empty[i]);
        lv_obj_t *empty_icon=lv_obj_get_child(s->empty,0);
        assert(lv_obj_get_child(s->empty,1)==s->empty_text);
        assert_png_icon(empty_icon,i+2,page_visible && empty[i]);
        assert(!lv_obj_has_flag(s->empty_text,LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_get_style_text_font(s->empty_text,0)==&lv_font_instrument_sans_medium_16);
        assert(lv_obj_get_style_text_color(s->empty_text,0).full==lv_color_hex(0x7A8D9B).full);
        assert(lv_obj_get_style_text_align(s->empty_text,0)==LV_TEXT_ALIGN_CENTER);
        assert(lv_obj_get_width(s->empty_text)==s->layout->width-58);
        assert(lv_obj_is_visible(s->empty_text)==(page_visible && empty[i]));
        assert(!strcmp(lv_label_get_text(s->empty_text),ui_text_get(i==PAGE_02_SECTION_B ?
                       UI_TEXT_LIST_NO_SERIAL_NUMBERS : UI_TEXT_LIST_NO_REJECT_DETAILS)));
        /* Hidden descendants are checked by ownership/visibility above, not
         * by a flex position that may not yet have been laid out on entry. */
        if(page_visible && empty[i]) {
            lv_obj_get_coords(s->empty,&a);lv_obj_get_coords(empty_icon,&b);lv_obj_get_coords(s->empty_text,&c);
            assert(b.x1>=a.x1 && b.x2<=a.x2 && c.x1>=a.x1 && c.x2<=a.x2);
            assert(b.y1>=a.y1 && b.y2<=a.y2 && c.y1>=a.y1 && c.y2<=a.y2);
            assert(c.y1-b.y2-1==12);
            assert(abs((b.x1+b.x2)-(a.x1+a.x2))<=1);
            assert(abs((c.x1+c.x2)-(a.x1+a.x2))<=1);
            assert(abs((b.y1+c.y2)-(a.y1+a.y2))<=1);
            label_fits(s->empty_text);
        }
    }
}
static void assert_action_rail(bool page_visible)
{
    const ui_text_id_t text[]={UI_TEXT_LIST_HISTORY,UI_TEXT_SERIAL_SEARCH,
                               UI_TEXT_LIST_PRINT,UI_TEXT_LIST_MAIN};
    const int order[]={LIST_ACTION_HISTORY,LIST_ACTION_SEARCH,
                       LIST_ACTION_PRINT,LIST_ACTION_MAIN};
    const int positions[]={12,108,204,300};
    assert(LIST_ACTION_COUNT==4);
    assert(lv_obj_get_child_cnt(view->page)==PAGE_02_SECTION_COUNT+LIST_ACTION_COUNT);
    lv_area_t rail[4];
    for(unsigned i=0;i<4;++i) {
        assert((unsigned)order[i]==i);
        lv_obj_t *button=view->actions[order[i]],*caption=lv_damped_button_get_label(button);
        assert(button && lv_obj_check_type(button,&lv_btn_class));
        assert(lv_obj_get_parent(button)==view->page && lv_obj_is_visible(button)==page_visible);
        assert(lv_obj_get_x(button)==1168 && lv_obj_get_y(button)==positions[i]);
        assert(lv_obj_get_width(button)==96 && lv_obj_get_height(button)==88);
        assert_background(button,0xFFFFFF,page_visible);
        assert(!strcmp(lv_label_get_text(caption),ui_text_get(text[i])));
        assert(lv_obj_get_style_text_color(caption,0).full==lv_color_hex(0x4C606E).full);
        assert_caption_contained(button);
        assert(lv_obj_get_child_cnt(button)==2 && lv_obj_get_child(button,0)==caption);
        lv_obj_t *icon=lv_obj_get_child(button,1);
        assert(lv_obj_get_width(icon)==27 && lv_obj_get_height(icon)==27);
        assert(lv_obj_get_x(icon)==35 && lv_obj_get_y(icon)==17);
        assert_icon(icon,0x657F90,21,page_visible);
        lv_obj_get_coords(button,&rail[i]);
        if(i) assert(rail[i].y1-rail[i-1].y2-1==8);
    }
    for(unsigned i=0;i<PAGE_02_SECTION_COUNT;++i) {
        lv_obj_t *panel=view->section[i].panel;
        lv_area_t area;lv_obj_get_coords(panel,&area);
        assert(area.y1==rail[0].y1 && area.y2==rail[3].y2);
        for(unsigned j=0;j<lv_obj_get_child_cnt(panel);++j) {
            lv_obj_t *child=lv_obj_get_child(panel,j);
            /* No Search button remains in B's title/header area. */
            if(lv_obj_get_y(child)<HEADER_TOP_Y)
                assert(!lv_obj_check_type(child,&lv_btn_class));
        }
    }
    lv_area_t c;lv_obj_get_coords(view->section[PAGE_02_SECTION_C].panel,&c);
    assert(rail[0].x1-c.x2-1==12 && rail[0].x2==1263);
}
static void assert_palette(bool page_visible)
{
    assert(!strcmp(lv_obj_get_style_bg_img_src(view->page,0),UI_USER_BACKGROUND_SRC));
    const uint32_t badge_colors[]={0x2BD900,0x0074F8,0xF85820};
    assert(lv_obj_get_style_bg_color(view->page,0).full==lv_color_hex(0xF2F5F7).full);
    assert(lv_obj_get_style_bg_opa(view->page,0)==LV_OPA_COVER);
    lv_obj_update_layout(view->page);
    if(page_visible) render_page();
    assert_home_icon(page_visible);
    assert_action_rail(page_visible);
    if(page_visible) assert((framebuffer[0].full&0xffffff)==(*(const uint32_t*)test_page_asset_find(UI_USER_BACKGROUND_SRC)->pixels&0xffffff));
    for(int i=0;i<3;++i) {
        list_section_t *s=&view->section[i];
        assert(lv_obj_get_style_bg_color(s->panel,0).full==lv_color_hex(0xFFFFFF).full);
        assert(lv_obj_get_style_bg_opa(s->panel,0)==LV_OPA_COVER);
        lv_obj_t *badge=lv_obj_get_child(s->panel,0),*letter=lv_obj_get_child(badge,0);
        assert(lv_obj_get_width(badge)==26 && lv_obj_get_height(badge)==26);
        assert(lv_obj_get_height(letter)==lv_font_get_line_height(&lv_font_instrument_sans_bold_14));
        assert(abs(2*lv_obj_get_y(letter)+lv_obj_get_height(letter)-26)<=1);
        assert(lv_obj_get_style_bg_color(badge,0).full==lv_color_hex(badge_colors[i]).full);
        assert(lv_obj_get_style_bg_opa(badge,0)==LV_OPA_COVER);
        assert(lv_obj_get_child_cnt(badge)==1 && lv_obj_check_type(letter,&lv_label_class));
        assert(lv_obj_get_style_text_color(letter,0).full==lv_color_hex(0xFFFFFF).full);
        assert(lv_obj_get_style_text_font(letter,0)==&lv_font_instrument_sans_bold_14);
        assert(!strcmp(lv_label_get_text(letter),ui_text_get((ui_text_id_t)(UI_TEXT_PAGE01_DETAIL_BTN_A+i))));
        label_fits(letter);
        if(page_visible) {
            assert(color_pixels(badge,badge_colors[i])>100);
            assert(color_pixels(badge,0xFFFFFF)>0);
        }
        if(i==PAGE_02_SECTION_A) continue;
        const ui_list_window_t *w=lv_recycled_list_window(s->list);
        lv_obj_t *label=lv_damped_button_get_label(s->mode);
        assert_background(s->mode,0xFFFFFF,page_visible);
        assert(lv_obj_get_style_text_color(label,0).full==lv_color_hex(0x4C606E).full);
        assert(!strcmp(lv_label_get_text(label),ui_text_get(w->paged ? UI_TEXT_LIST_PAGES : UI_TEXT_LIST_SCROLL)));
        if(view->language==LANGUAGE_EN)
            assert(!strcmp(lv_label_get_text(label),w->paged ? "PAGE" : "SCROLL"));
        label_fits(label);label_fits(s->range);
        assert(lv_obj_get_child_cnt(s->mode)==2);
        /* The shorter two-stroke toggle has only 18 pure-color pixels in
         * the SDK software raster; antialiased edge pixels are not counted. */
        assert_icon(lv_obj_get_child(s->mode,1),0x657F90,12,page_visible);
        lv_obj_t *arrows[]={s->previous,s->next};
        const bool enabled[]={w->count && w->first>0,ui_list_window_page_number(w)<ui_list_window_pages(w)};
        for(unsigned j=0;j<2;++j) {
            assert(lv_obj_has_flag(arrows[j],LV_OBJ_FLAG_HIDDEN)==!w->paged);
            assert(lv_damped_button_is_enabled(arrows[j])==enabled[j]);
            assert(lv_obj_has_flag(arrows[j],LV_OBJ_FLAG_CLICKABLE)==enabled[j]);
            assert_background(arrows[j],enabled[j] ? 0xFFFFFF : 0xF6F7F8,page_visible && w->paged);
            assert_icon(lv_obj_get_child(arrows[j],1),0x657F90,21,page_visible && w->paged);
        }
    }
    lv_obj_t *panel=view->section[0].panel;
    /* TOTAL remains on the plain panel: no added tint surface or marker. */
    for(unsigned i=0;i<lv_obj_get_child_cnt(panel);++i) {
        lv_obj_t *child=lv_obj_get_child(panel,i);
        if(lv_obj_get_y(child)>FOOTER_Y)
            assert(lv_obj_get_style_bg_opa(child,0)==LV_OPA_TRANSP);
    }
    if(page_visible) {
        assert(color_pixels(view->page,0xFF6A00)==0);
        assert(color_pixels(view->page,0xFFF1E8)==0);
        assert(color_pixels(view->page,0xFFF8F3)==0);
    }
    assert(!strcmp(lv_label_get_text(view->total_title),ui_text_get(UI_TEXT_LIST_TOTAL)));
    label_fits(view->total_title);
}
static void test_handdrawn_icons(void)
{
    lv_obj_t *icons[]={lv_obj_get_child(view->actions[LIST_ACTION_HISTORY],1),
        lv_obj_get_child(view->actions[LIST_ACTION_SEARCH],1),
        lv_obj_get_child(view->actions[LIST_ACTION_PRINT],1),
        lv_obj_get_child(view->actions[LIST_ACTION_MAIN],1),lv_obj_get_child(view->section[1].mode,1),
        lv_obj_get_child(view->section[2].mode,1)};
    render_page();
    for(unsigned i=0;i<sizeof(icons)/sizeof(icons[0]);++i)
        assert_icon(icons[i],0x657F90,i>=4 ? 12 : 21,true);
    test_home_glyph();test_search_glyph();
}
static void test_button_feedback(void)
{
    lv_obj_t *buttons[]={view->section[1].mode,view->section[2].mode,
                        view->actions[LIST_ACTION_HISTORY],view->actions[LIST_ACTION_SEARCH],
                        view->actions[LIST_ACTION_PRINT],view->actions[LIST_ACTION_MAIN]};
    for(unsigned i=0;i<sizeof(buttons)/sizeof(buttons[0]);++i) {
        lv_obj_t *button=buttons[i],*label=lv_damped_button_get_label(button);
        uint32_t normal=0xFFFFFF;
        lv_area_t before,after;lv_obj_get_coords(button,&before);
        lv_obj_add_state(button,LV_STATE_PRESSED);lv_event_send(button,LV_EVENT_PRESSED,NULL);tick(140);
        render_page();lv_obj_get_coords(button,&after);
        assert(before.x1==after.x1 && before.y1==after.y1 && before.x2==after.x2 && before.y2==after.y2);
        lv_color_t pressed=lv_color_darken(lv_color_hex(normal),(lv_opa_t)20);
        assert(lv_obj_get_style_bg_color(button,0).full==pressed.full);
        assert(framebuffer[(before.y1+4)*1280+(before.x1+before.x2)/2].full==pressed.full);
        assert(lv_obj_get_style_translate_y(button,0)==0 && lv_obj_get_style_opa(button,0)==LV_OPA_COVER);
        for(unsigned j=0;j<lv_obj_get_child_cnt(button);++j)
            assert(lv_obj_get_style_translate_y(lv_obj_get_child(button,j),0)==2);
        assert_caption_contained(button);
        if(i>=2) write_bmp((const char *[]){"list-history-pressed","list-search-pressed",
                                          "list-print-pressed","list-main-pressed"}[i-2]);
        lv_obj_clear_state(button,LV_STATE_PRESSED);
        lv_event_send(button,i%2 ? LV_EVENT_PRESS_LOST : LV_EVENT_RELEASED,NULL);tick(300);render_page();
        assert_background(button,normal,true);
        for(unsigned j=0;j<lv_obj_get_child_cnt(button);++j)
            assert(lv_obj_get_style_translate_y(lv_obj_get_child(button,j),0)==0);
        assert_caption_contained(button);
        lv_damped_button_set_enabled(button,false);render_page();
        assert_background(button,0xF6F7F8,true);
        assert(!lv_obj_has_flag(button,LV_OBJ_FLAG_CLICKABLE));
        assert(lv_obj_get_style_text_color(label,0).full==lv_color_hex(0xAAB5BE).full);
        lv_damped_button_set_enabled(button,true);render_page();assert_background(button,normal,true);
    }
    assert(history_clicks==0 && print_clicks==0 && home_clicks==0 && !view->search);
    assert(!lv_recycled_list_window(view->section[1].list)->paged);
    assert(!lv_recycled_list_window(view->section[2].list)->paged);
    assert_palette(true);
}
static void test_reject_summary_without_details(counting_sim_t *data)
{
    assert(counting_data_reject_pcs_count(data)==0);
    data->err_expected=14;page_02_list_section_data_ready(PAGE_02_SECTION_C);tick(100);
    assert(counting_data_reject_pcs_count(data)==14);
    assert(counting_data_error_detail_count(data)==0);
    assert(lv_recycled_list_window(view->section[PAGE_02_SECTION_C].list)->count==0);
    assert_icons(true,true,true);assert_palette(true);write_bmp("list-reject-summary-no-details");
    data->err_expected=0;page_02_list_section_data_ready(PAGE_02_SECTION_C);tick(100);
    assert(counting_data_reject_pcs_count(data)==0);
    assert_icons(true,true,true);assert_palette(true);write_bmp("list-reject-cleared-visible");
}
static void test_icon_visibility(counting_sim_t *data)
{
    assert_icons(true,true,true);assert_palette(true);write_bmp("list-empty");
    /* Check bounds for registered translations too. Existing CJK font coverage
     * is a separate limitation; this is not a claim of new CJK glyph support. */
    const language_t languages[]={LANGUAGE_CN,LANGUAGE_KR,LANGUAGE_EN};
    for(unsigned i=0;i<sizeof(languages)/sizeof(languages[0]);++i) {
        ui_lang_set(languages[i]);page_02_list_section_mark_dirty(PAGE_02_SECTION_A);tick(100);
        assert_icons(true,true,true);assert_palette(true);
    }
    /* Capacity, a missing serial, a zero denomination and a reject summary
     * are not detail rows. In particular, C must not imply zero rejects. */
    assert(counting_data_ensure_serial_capacity(data,3));
    data->denom_mix[0]=20;
    data->sn_str[1]=malloc(16);assert(data->sn_str[1]);strcpy(data->sn_str[1],"NO-DENOM");
    assert(counting_data_ensure_error_capacity(data,1));data->err_expected=14;
    page_02_list_report_reset();tick(100);
    assert(view->data.serial_count==0 && counting_data_error_detail_count(data)==0);
    assert(counting_data_reject_pcs_count(data)==14);
    assert(lv_recycled_list_window(view->section[PAGE_02_SECTION_C].list)->count==0);
    assert_icons(true,true,true);write_bmp("list-incomplete-details");
    data->sn_str[2]=malloc(16);assert(data->sn_str[2]);strcpy(data->sn_str[2],"VALID-SERIAL");
    data->denom_mix[2]=20;page_02_list_section_data_ready(PAGE_02_SECTION_B);tick(100);
    assert(view->data.serial_count==1 && view->data.serial[0]==2);
    assert_icons(false,true,true);write_bmp("list-one-serial");
    data->err_num=1;data->err_code[0]=0x15;data->err_pcs[0]=1;
    page_02_list_section_data_ready(PAGE_02_SECTION_C);tick(100);
    assert(lv_recycled_list_window(view->section[2].list)->count==1);
    assert_icons(false,false,true);write_bmp("list-one-detail");
    counting_data_clear_serials(data);page_02_list_section_data_ready(PAGE_02_SECTION_B);tick(100);
    assert_icons(true,false,true);
    counting_data_clear_errors(data);page_02_list_section_data_ready(PAGE_02_SECTION_C);tick(100);
    assert(counting_data_reject_pcs_count(data)==14);
    assert(lv_recycled_list_window(view->section[PAGE_02_SECTION_C].list)->count==0);
    assert_icons(true,true,true);write_bmp("list-cleared-details");
    ui_page_02_list_suspend();assert_icons(true,true,false);
    assert(counting_data_ensure_serial_capacity(data,1));
    data->sn_str[0]=malloc(16);assert(data->sn_str[0]);strcpy(data->sn_str[0],"LATE-SERIAL");
    data->denom_mix[0]=50;
    assert(counting_data_ensure_error_capacity(data,1));
    data->err_num=1;data->err_code[0]=0x11;data->err_pcs[0]=2;
    page_02_list_section_data_ready(PAGE_02_SECTION_B);page_02_list_section_data_ready(PAGE_02_SECTION_C);tick(100);
    assert(view->data.serial_count==0 && lv_recycled_list_window(view->section[2].list)->count==0);
    assert_icons(true,true,false);
    assert(ui_page_02_list_resume());assert_icons(false,false,true);write_bmp("list-resumed-details");
    ui_page_02_list_suspend();counting_data_clear_serials(data);counting_data_clear_errors(data);data->err_expected=0;
    page_02_list_report_reset();tick(100);
    assert(view->data.serial_count==1 && lv_recycled_list_window(view->section[2].list)->count==1);
    assert_icons(false,false,false);assert_palette(false);
    assert(counting_data_reject_pcs_count(data)==0);
    assert(ui_page_02_list_resume());assert_icons(true,true,true);assert_palette(true);
    assert(lv_recycled_list_window(view->section[PAGE_02_SECTION_C].list)->count==0);
    write_bmp("list-resumed-empty");
}
static void test_page_list_swipes(bool paged)
{
    lv_indev_drv_t driver;lv_indev_drv_init(&driver);
    driver.type=LV_INDEV_TYPE_POINTER;driver.disp=lv_disp_get_default();
    lv_indev_t indev={0};indev.driver=&driver;
    lv_recycled_list_t *b=view->section[1].list,*c=view->section[2].list;
    if(!paged) {
        /* Exercise the real page rows without changing their business data. */
        recycled_cases_input_xy(b,&indev,LV_EVENT_PRESSED,600,150,20);
        recycled_cases_input_xy(b,&indev,LV_EVENT_PRESSING,600,270,100);
        assert(lv_recycled_list_window(b)->offset==0);
        assert(lv_recycled_list_window(c)->offset==0);
        write_bmp("list-pull-top");
        recycled_cases_input_xy(b,&indev,LV_EVENT_RELEASED,600,270,0);
        tick(120);write_bmp("list-returning");
        tick(500);
        recycled_cases_input_xy(b,&indev,LV_EVENT_PRESSED,600,250,20);
        recycled_cases_input_xy(b,&indev,LV_EVENT_PRESSING,600,150,100);
        recycled_cases_input_xy(b,&indev,LV_EVENT_PRESS_LOST,600,150,0);
        assert(lv_recycled_list_window(b)->offset==100);
        write_bmp("list-scroll-middle");
        lv_recycled_list_refresh(b,lv_recycled_list_window(b)->count,true);
        return;
    }
    const struct {lv_recycled_list_t *list;int x,dx;uint32_t b_first,c_first;} cases[]={
        {b,600,-100,14,7},{b,600,100,7,7},{c,1000,100,7,0},{c,1000,-100,7,7}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        recycled_cases_input_xy(cases[i].list,&indev,LV_EVENT_PRESSED,cases[i].x,160,20);
        recycled_cases_input_xy(cases[i].list,&indev,LV_EVENT_PRESSING,cases[i].x+cases[i].dx,160,100);
        recycled_cases_input_xy(cases[i].list,&indev,LV_EVENT_RELEASED,cases[i].x+cases[i].dx,160,0);
        assert(lv_recycled_list_window(b)->first==cases[i].b_first);
        assert(lv_recycled_list_window(c)->first==cases[i].c_first);
    }
    puts("PASS actual List B/C independent swipe paging and vertical pull raster");
}

#include "test_serial_search_cases.h"

static void test_search_retry(void)
{
    assert(view && !view->search);
    unsigned baseline=timers();
    lv_obj_t *button=view->actions[LIST_ACTION_SEARCH];
    fail_search_owner_alloc=true;
    serial_search_cases_click(button);
    assert(!fail_search_owner_alloc && !view->search && timers()==baseline);
    assert(!strcmp(lv_label_get_text(lv_damped_button_get_label(button)),
                   ui_text_get(UI_TEXT_SERIAL_UNAVAILABLE)));
    assert(lv_damped_button_is_enabled(button));
    render_page();assert_caption_contained(button);
    lv_obj_add_state(button,LV_STATE_PRESSED);lv_event_send(button,LV_EVENT_PRESSED,NULL);tick(140);
    render_page();assert_caption_contained(button);write_bmp("list-search-retry-pressed");
    lv_obj_clear_state(button,LV_STATE_PRESSED);lv_event_send(button,LV_EVENT_RELEASED,NULL);tick(300);
    serial_search_cases_click(button);
    assert(view->search && lv_obj_is_visible(view->search->root));
    assert(!strcmp(lv_label_get_text(lv_damped_button_get_label(button)),
                   ui_text_get(UI_TEXT_SERIAL_SEARCH)));
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    serial_search_cases_flush();
    assert(!view->search && timers()==baseline);
    assert_palette(true);
    puts("PASS right-side Search allocation failure keeps RETRY visible/clickable and retry restores SEARCH");
}

static void test_search_background(void)
{
    write_bmp("list-background");
    lv_event_send(view->actions[LIST_ACTION_SEARCH],LV_EVENT_CLICKED,NULL);
    assert(view->search);
    page_02_list_search_t *s=view->search;
    assert(lv_obj_get_style_bg_color(s->root,0).full==lv_color_hex(0xF2F5F7).full);
    assert(lv_obj_get_style_bg_opa(s->root,0)==LV_OPA_COVER);
    lv_obj_t *cards[]={lv_obj_get_parent(s->contains),lv_obj_get_parent(s->input)};
    for(unsigned i=0;i<2;++i) {
        assert(lv_obj_get_style_bg_color(cards[i],0).full==lv_color_hex(0xFFFFFF).full);
        assert(lv_obj_get_style_bg_opa(cards[i],0)==LV_OPA_COVER);
    }
    render_page();
    assert((framebuffer[0].full&0xffffff)==(*(const uint32_t*)test_page_asset_find(UI_USER_BACKGROUND_SRC)->pixels&0xffffff));
    write_bmp("search-background");
    search_closed(UINT16_MAX,NULL);
    assert(!view->search);
}

int main(void)
{
    lv_init();register_host_assets();static lv_color_t pixels[1280*40];static lv_disp_draw_buf_t buf;
    lv_disp_draw_buf_init(&buf,pixels,NULL,1280*40);
    lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;
    driver.draw_buf=&buf;driver.flush_cb=flush;lv_disp_drv_register(&driver);
    lv_obj_t *probe=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(probe);lv_obj_set_size(probe,1280,400);
    for(unsigned theme=0;theme<2;theme++){
        ui_page_background_apply(probe,theme?UI_BACKGROUND_SETTINGS:UI_BACKGROUND_USER);
        lv_obj_update_layout(probe);lv_obj_invalidate(probe);lv_refr_now(NULL);
        const uint32_t *expected=(const uint32_t*)test_page_asset_find(theme?UI_SETTINGS_BACKGROUND_SRC:UI_USER_BACKGROUND_SRC)->pixels;
        for(unsigned i=0;i<1280*400;i++)assert((framebuffer[i].full&0xffffff)==(expected[i]&0xffffff));
    }
    lv_obj_del(probe);
    puts("PASS both full-screen background styles match the actual generated PNG pixels");
    test_alnum_keyboard_cases();
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
    test_handdrawn_icons();test_button_feedback();test_reject_summary_without_details(data);test_icon_visibility(data);
    test_search_retry();test_search_background();
    test_serial_search_cases(data);
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
    assert(view->data.denom[2]==3);assert_icons(false,false,true);write_bmp("list-scroll");
    test_page_list_swipes(false);
    lv_event_send(view->section[1].mode,LV_EVENT_CLICKED,NULL);
    lv_event_send(view->section[1].next,LV_EVENT_CLICKED,NULL);
    assert(lv_recycled_list_window(view->section[1].list)->first==7);
    assert(!lv_recycled_list_window(view->section[2].list)->paged);
    lv_event_send(view->section[2].mode,LV_EVENT_CLICKED,NULL);
    lv_event_send(view->section[2].next,LV_EVENT_CLICKED,NULL);
    assert(lv_recycled_list_window(view->section[2].list)->first==7);
    test_page_list_swipes(true);
    tick(100);assert_icons(false,false,true);assert_palette(true);write_bmp("list-pages");
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
    tick(100);assert_palette(true);write_bmp("list-large-last-page");
    lv_obj_t *vp=lv_recycled_list_object(view->section[1].list);
    for(unsigned i=0;i<lv_obj_get_child_cnt(vp);++i) {
        lv_obj_t *row=lv_obj_get_child(vp,i);if(lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)||lv_obj_get_child_cnt(row)!=3)continue;
        assert(lv_obj_get_y(row)>=0 && lv_obj_get_y(row)<252);
        label_fits(lv_obj_get_child(row,0));label_fits(lv_obj_get_child(row,2));
    }
    /* MULTI has no per-note currency from the current controller. The current
     * selector and a retained mixed result both prevent fabricated amounts. */
    assert(currency_state_confirm_multi_selection());
    page_02_list_report_reset();tick(40);
    assert(view->data.denom_count==0 && !strcmp(lv_label_get_text(view->amount),"--"));
    assert(!strcmp(lv_label_get_text(view->pcs),"10000"));
    for(unsigned i=0;i<lv_obj_get_child_cnt(vp);++i) {
        lv_obj_t *row=lv_obj_get_child(vp,i);
        if(lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)||lv_obj_get_child_cnt(row)!=3)continue;
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(row,2)),"--"));
        assert(strlen(lv_label_get_text(lv_obj_get_child(row,1)))>0);
    }
    write_bmp("list-multi-safe");
    counting_data_mark_multi_result(data);
    assert(currency_state_leave_special_selection());
    page_02_list_report_reset();tick(40);
    assert(!strcmp(lv_label_get_text(view->amount),"--"));
    counting_data_reset_result_scope(data);
    page_02_list_report_reset();tick(40);
    assert(view->data.denom_count==6 && strcmp(lv_label_get_text(view->amount),"--"));
    const int navigation_actions[]={LIST_ACTION_HISTORY,LIST_ACTION_PRINT,LIST_ACTION_MAIN};
    for(unsigned i=0;i<sizeof(navigation_actions)/sizeof(navigation_actions[0]);++i)
        lv_event_send(view->actions[navigation_actions[i]],LV_EVENT_CLICKED,NULL);
    assert(history_clicks==1 && print_clicks==1 && home_clicks==1);
    /* Queued callbacks must not outlive the owner. */
    ui_frame_commit_begin_batch();page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
    ui_page_02_list_destroy();ui_frame_commit_end_batch();ui_frame_commit_flush();tick(300);
    assert(!view);assert(timers()==baseline_timers);
    for(int i=0;i<5;++i) {ui_page_02_list_create(lv_scr_act());ui_page_02_list_suspend();ui_page_02_list_resume();ui_page_02_list_destroy();}
    assert(timers()==baseline_timers);
    counting_data_clear_serials(data);counting_data_clear_errors(data);
    puts("PASS actual List/LVGL raster, compiled PNG registry/alpha, four equal aligned History/Search/Print/Main buttons, bold white ABC color badges, double-rule centered column headers, complete pressed captions, reject-summary/detail semantics, vertical empty-state transitions, data projection, modes, 10000 rows, callbacks and lifecycle");
    return 0;
}
