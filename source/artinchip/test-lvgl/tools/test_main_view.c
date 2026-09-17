#include <assert.h>
#include "un260/lv_components/ui_scrollbar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "aic_ui/compiled_asset.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_components/smart_island/smart_island_internal.h"
#include "un260/lv_core/page_01_main_detail.c"
#include "un260/lv_core/page_01_main.c"
#include "test_main_view_support.h"
#include "un260/gesture/gesture_service.h"
#include "un260/counting/counting_multi.h"
static bool (*multi_policy)(gesture_action_t);
void gesture_service_set_page_policy(uint32_t owner, bool (*drag)(void), bool (*action)(gesture_action_t)) {(void)owner;(void)drag;multi_policy=action;}
void gesture_service_clear_page_policy(uint32_t owner) {(void)owner;multi_policy=NULL;}
#include "un260/lv_components/lv_nav_button.c"
static unsigned display_back_requests;
bool ui_manager_pop_page(void) { ++display_back_requests; return true; }
#include "un260/lv_core/page_36_display_test.c"

static unsigned asset_opens;
const un260_compiled_asset_t *host_external_asset_find(const char *path);
void host_external_assets_release(void);
static const un260_compiled_asset_t *host_asset_find(const char *path)
{
    const un260_compiled_asset_t *asset=un260_compiled_asset_find(path);
    return asset?asset:host_external_asset_find(path);
}
static lv_res_t host_image_info(lv_img_decoder_t *decoder,const void *source,lv_img_header_t *header)
{
    (void)decoder;
    if(lv_img_src_get_type(source)!=LV_IMG_SRC_FILE) return LV_RES_INV;
    const un260_compiled_asset_t *asset=host_asset_find(source);
    if(!asset) fprintf(stderr,"Actual compiled/external asset missing: %s\n",(const char *)source);
    assert(asset);
    memset(header,0,sizeof(*header));header->w=asset->width;header->h=asset->height;
    header->cf=asset->has_alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
    return LV_RES_OK;
}
static lv_res_t host_image_open(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *description)
{
    if(host_image_info(decoder,description->src,&description->header)!=LV_RES_OK) return LV_RES_INV;
    const un260_compiled_asset_t *asset=host_asset_find(description->src);
    assert(asset && asset->stride==asset->width*4);
    description->img_data=asset->pixels;++asset_opens;return LV_RES_OK;
}
static void host_image_close(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *description)
{ (void)decoder;description->img_data=NULL; }

static lv_color_t framebuffer[1280*400];
static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *pixels)
{
    assert(area->x1>=0 && area->y1>=0 && area->x2<1280 && area->y2<400);
    int width=lv_area_get_width(area);
    for(int y=area->y1;y<=area->y2;++y)
        memcpy(framebuffer+y*1280+area->x1,pixels+(y-area->y1)*width,width*sizeof(*pixels));
    lv_disp_flush_ready(driver);
}
static void tick(unsigned ms)
{ for(unsigned i=0;i<ms;i+=20) { lv_tick_inc(20);ui_frame_commit_flush();lv_timer_handler(); } }
static void render(void)
{ lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL); }
static void write_bmp(const char *name)
{
    const char *directory=getenv("MAIN_RASTER_OUTPUT");if(!directory)return;
    render();char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",directory,name);
    FILE *file=fopen(path,"wb");assert(file);
    uint8_t header[54]={0};uint32_t size=54+sizeof(framebuffer),offset=54,dib=40,width=1280;
    int32_t height=-400;uint16_t planes=1,bits=32;
    header[0]='B';header[1]='M';memcpy(header+2,&size,4);memcpy(header+10,&offset,4);
    memcpy(header+14,&dib,4);memcpy(header+18,&width,4);memcpy(header+22,&height,4);
    memcpy(header+26,&planes,2);memcpy(header+28,&bits,2);
    assert(fwrite(header,1,sizeof(header),file)==sizeof(header));
    assert(fwrite(framebuffer,1,sizeof(framebuffer),file)==sizeof(framebuffer));fclose(file);
}
static unsigned timers(void)
{ unsigned n=0;for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n; }
static lv_point_t pointer_position;
static lv_indev_state_t pointer_state;
static void pointer_read(lv_indev_drv_t *driver,lv_indev_data_t *data)
{ (void)driver;data->point=pointer_position;data->state=pointer_state; }
static void pointer(int x,int y,bool pressed)
{ pointer_position=(lv_point_t){x,y};pointer_state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;tick(40); }
static void tap(int x,int y) { pointer(x,y,true);pointer(x,y,false); }
static void click_object(lv_obj_t *object)
{ lv_area_t a;lv_obj_get_coords(object,&a);tap((a.x1+a.x2)/2,(a.y1+a.y2)/2); }

static void fixture(bool full)
{
    counting_sim_t *data=counting_data_mutable();
    counting_data_clear_serials(data);counting_data_clear_errors(data);memset(data,0,sizeof(*data));
    const int values[]={1,2,5,10,20,50,100};data->denom_number=7;
    for(unsigned i=0;i<7;++i) {
        data->denom[i]=(denom_t){values[i],full?(uint16_t)(i*11):0,full?(float)(values[i]*i*11):0};
        data->total_pcs+=data->denom[i].pcs;data->total_amount+=data->denom[i].amount;
    }
    if(!full)return;
    assert(counting_data_ensure_serial_capacity(data,10000));
    for(unsigned i=0;i<10000;++i) {
        char text[24];snprintf(text,sizeof(text),"AB%010u",i+1);
        data->sn_str[i]=malloc(strlen(text)+1);assert(data->sn_str[i]);strcpy(data->sn_str[i],text);
        data->denom_mix[i]=100;
    }
    assert(counting_data_ensure_error_capacity(data,120));data->err_num=120;data->err_expected=9;
    for(unsigned i=0;i<120;++i) { data->err_pcs[i]=1;data->err_code[i]=i%50; }
}

static void assert_labels(lv_obj_t *parent)
{
    if(!lv_obj_is_visible(parent))return;
    if(lv_obj_check_type(parent,&lv_label_class)) {
        lv_area_t area;lv_obj_get_coords(parent,&area);
        assert(area.x1>=0 && area.y1>=0 && area.x2<1280 && area.y2<400);
    }
    for(unsigned i=0;i<lv_obj_get_child_cnt(parent);++i)assert_labels(lv_obj_get_child(parent,i));
}

static void test_main(void)
{
    fixture(false);assert(currency_state_confirm_active_code("USD"));
    prewarming=true;ui_main_create(lv_scr_act());assert(protocol_calls==0);
    prewarming=false;assert(page_01_main_resume());assert(protocol_calls==1);
    render();assert(asset_opens>0 && page_01_main_is_created());assert_labels(main_page);
    lv_obj_t *bordered_cards[]={s_summary_card,s_detail_card};
    for(unsigned i=0;i<2;++i) {
        assert(lv_obj_get_style_border_width(bordered_cards[i],0)==1);
        assert(lv_obj_get_style_border_color(bordered_cards[i],0).full==lv_color_hex(0xECF0F3).full);
        assert(lv_obj_get_style_border_opa(bordered_cards[i],0)==LV_OPA_COVER);
    }
    assert(lv_recycled_list_window(detail_view->section[0].list)->count==7);
    assert(lv_recycled_list_window(detail_view->section[0].list)->rows==6);
    assert(lv_obj_get_y(s_detail_btn_a)==29 && lv_obj_get_height(s_detail_btn_a)==34 && lv_obj_get_y(detail_view->root)==76);
    assert(lv_obj_get_height(detail_view->root)==244);
    assert(s_detail_tray && lv_obj_get_width(s_detail_tray)==526);
    assert(lv_obj_get_style_bg_color(s_detail_tray,0).full==lv_color_hex(0xE7EDF0).full);
    lv_obj_t *background = page_01_main_find_obj("page_01_back.png");
    assert(background && lv_obj_check_type(background, &lv_img_class));
    assert(!strcmp(lv_img_get_src(background), UI_USER_BACKGROUND_SRC));
    assert(lv_obj_get_style_text_font(s_curr_label,0)==&lv_font_main_currency_56);
    assert(lv_obj_get_child_cnt(s_detail_card)==0); /* No redundant section title. */
    write_bmp("main-empty-usd");
    const char *buttons[]={"mode_btn","setting_btn","list_btn","print_btn","menu_btn","start_btn","esc_btn"};
    for(unsigned i=0;i<7;++i) {
        lv_obj_t *b=page_01_main_find_obj(buttons[i]);
        assert(lv_obj_get_style_bg_color(b,0).full==lv_color_hex(i==5?0xDCEFD5:i<4?0xFFFFFF:0xE9EDF0).full);
        lv_obj_add_state(b,LV_STATE_PRESSED);lv_event_send(b,LV_EVENT_PRESSED,NULL);tick(140);
        for(unsigned j=0;j<lv_obj_get_child_cnt(b);++j)
            assert(lv_obj_get_style_translate_y(lv_obj_get_child(b,j),0)==0);
        lv_obj_clear_state(b,LV_STATE_PRESSED);lv_event_send(b,LV_EVENT_PRESS_LOST,NULL);tick(300);
        click_object(b);assert(callbacks[i]==1);
    }
    click_object(s_bottom_a_btn_mode);assert(callbacks[CB_BOTTOM_MODE]==1);
    click_object(s_bottom_a_btn_add);assert(callbacks[CB_ADD]==1);
    click_object(s_bottom_a_btn_work);assert(callbacks[CB_WORK]==1);
    click_object(s_bottom_a_btn_fo);assert(callbacks[CB_FO]==1);
    click_object(s_bottom_c_btn_batch);assert(callbacks[CB_BATCH]==1);
    click_object(s_bottom_c_btn_speed);assert(callbacks[CB_SPEED]==1);
    click_object(lv_obj_get_parent(s_curr_img));assert(callbacks[CB_CURRENCY]==1);
    unsigned opened=pushes;
    tap(850,90);assert(pushes==opened); /* Column header. */
    tap(606,180);assert(pushes==opened); /* Card left padding. */
    tap(850,328);assert(pushes==opened); /* Card bottom padding. */
    tap(780,32);assert(pushes==opened); /* Top-strip gap between tabs. */
    lv_obj_t *tabs[]={s_detail_btn_a,s_detail_btn_b,s_detail_btn_c};
    const char *titles[]={"REPORT","SERIAL","REJECT"};
    for(unsigned i=0;i<3;++i) {
        click_object(tabs[i]);render();assert(pushes==opened);
        for(unsigned j=0;j<3;++j)
            assert(lv_obj_get_style_bg_color(tabs[j],0).full==lv_color_hex(i==j?0xFFFFFF:0xE7EDF0).full);
        lv_obj_t *badge=lv_obj_get_child(tabs[i],0),*title=lv_obj_get_child(tabs[i],1);
        lv_area_t ba,ta;lv_obj_get_coords(badge,&ba);lv_obj_get_coords(title,&ta);
        assert(!strcmp(lv_label_get_text(title),titles[i]));
        assert(LV_ABS((ba.y1+ba.y2)-(ta.y1+ta.y2))<=1);
        tap(850,90);assert(pushes==opened);
        tap(850,180);assert(pushes==++opened && destination==UI_PAGE_LIST);
    }
    tap(1120,32);assert(s_detail_section==PAGE_01_DETAIL_SECTION_C && pushes==opened);
    tap(850,42);assert(s_detail_section==PAGE_01_DETAIL_SECTION_B && pushes==opened);
    tap(1220,32);assert(callbacks[CB_MENU]==2); /* Upper Menu area is not swallowed by PULL DOWN. */
    pointer(1120,32,true);pointer(1120,44,true);pointer(1120,44,false);assert(pushes==opened);
    click_object(s_detail_btn_b);assert(s_detail_section==PAGE_01_DETAIL_SECTION_B && pushes==opened);
    assert(lv_obj_is_visible(detail_view->section[1].empty));
    render();write_bmp("main-empty-serial");
    lv_obj_t *empty=detail_view->section[1].empty;
    lv_obj_t *empty_icon=lv_obj_get_child(empty,0), *empty_text=detail_view->section[1].empty_text;
    assert(lv_obj_get_y(empty_text)-lv_obj_get_y(empty_icon)-lv_obj_get_height(empty_icon)==12);
    assert(abs(lv_obj_get_y(empty_icon)+lv_obj_get_y(empty_text)+lv_obj_get_height(empty_text)-lv_obj_get_height(empty))<=2);
    page_01_detail_section_set(PAGE_01_DETAIL_SECTION_C,true);render();write_bmp("main-empty-reject");
    page_01_detail_section_set(PAGE_01_DETAIL_SECTION_B,true);render();
    tap(850,245);assert(pushes==++opened);
    fixture(true);ui_refresh_main_page();render();write_bmp("main-full-serial");
    assert(!strcmp(lv_label_get_text(s_total_pcs_label),"231"));
    assert(!strcmp(lv_label_get_text(s_total_amount_label),"10,692"));
    assert(lv_obj_get_style_text_font(s_total_pcs_label,0)==&lv_font_main_numeric_64);
    assert(lv_obj_get_style_text_font(s_total_amount_label,0)==&lv_font_main_numeric_64);
    pointer(850,180,true);
    for(unsigned packet=0;packet<4;++packet) { ui_refresh_main_page();tick(40); }
    pointer(850,180,false);assert(pushes==++opened); /* Full refresh/currency-layout chain retains stationary touch. */
    lv_recycled_list_t *serial=detail_view->section[1].list;
    assert(lv_recycled_list_scroll_to_index(serial,9999));
    float offset=lv_recycled_list_window(serial)->offset;
    click_object(s_detail_btn_c);assert(s_detail_section==PAGE_01_DETAIL_SECTION_C && pushes==opened);
    write_bmp("main-full-reject");
    click_object(s_detail_btn_a);write_bmp("main-full-report");
    click_object(s_detail_btn_b);assert(lv_recycled_list_window(serial)->offset==offset);
    pointer(850,260,true);pointer(850,195,true);pointer(850,195,false);assert(pushes==opened);
    lv_recycled_list_stop(serial);
    /* Innovation return reveals Main before the manager runs resume(). Its
     * first frame must already contain the active viewport at the old anchor. */
    offset=lv_recycled_list_window(serial)->offset;
    page_01_main_suspend();
    assert(!lv_obj_is_visible(page_01_main_scroll_obj()) && s_time_timer->paused);
    page_01_main_reveal_for_transition();
    assert(page_01_main_is_visible() && lv_obj_is_visible(page_01_main_scroll_obj()));
    assert(lv_obj_is_visible(s_detail_card) && lv_obj_is_visible(s_detail_btn_b));
    assert(!lv_obj_is_visible(s_multi_card) && s_time_timer->paused);
    assert(g_si_ctx.lifecycle.suspended); /* Reveal alone does not restart hidden-page work. */
    assert(lv_recycled_list_window(serial)->offset==offset);
    assert(lv_recycled_list_window(serial)->count==10000);
    unsigned visible_row=lv_recycled_list_window(serial)->first %
        (lv_recycled_list_window(serial)->rows + 1U);
    assert(lv_obj_is_visible(lv_obj_get_child(page_01_main_scroll_obj(),visible_row)));
    render();write_bmp("main-return-first-frame"); /* Deliberately no tick or resume first. */
    assert(page_01_main_resume());
    assert(lv_recycled_list_window(serial)->offset==offset);
    start_busy=true;page_01_main_refresh_start_state();assert(s_start_orbit);
    write_bmp("main-start-pending");
    page_01_main_suspend();assert(!page_01_main_is_visible() && s_start_orbit==NULL);
    fixture(false);ui_refresh_main_page();assert(page_01_main_resume());assert(s_start_orbit);
    start_busy=false;page_01_main_refresh_start_state();assert(s_start_orbit==NULL);
    counting_data_mutable()->total_pcs=1000000000;
    counting_data_mutable()->total_amount=1000000000.0f;
    ui_refresh_main_page();render();
    assert(lv_obj_get_style_text_font(s_total_pcs_label,0)==&lv_font_manrope_bold_32);
    assert(lv_obj_get_style_text_font(s_total_amount_label,0)==&lv_font_manrope_bold_32);
    assert_labels(main_page);write_bmp("main-large-values");
    fixture(false);ui_refresh_main_page();
    const char *currencies[]={"CNY","EUR","KRW","TRY","INR","PHP","GBP","PKR","ISK","SOS","MAD","DZD","AED","SAR","OMR","QAR","IQD"};
    for(unsigned i=0;i<sizeof(currencies)/sizeof(currencies[0]);++i) {
        assert(currency_state_confirm_active_code(currencies[i]));ui_refresh_main_page();render();
        assert(!strcmp(lv_label_get_text(s_curr_label),currency_metadata_symbol(currencies[i])));
        assert(lv_obj_is_visible(s_curr_label) && !lv_obj_is_visible(s_amount_unit_icon));
        lv_point_t size;
        lv_txt_get_size(&size,lv_label_get_text(s_curr_label),lv_obj_get_style_text_font(s_curr_label,0),
            0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        assert(size.x<=92 && size.y<=76); /* Multi-letter symbols cannot clip into the amount. */
        assert_labels(main_page);
        if(!strcmp(currencies[i],"CNY")) { tick(1000);write_bmp("main-currency-cny-large"); }
        if(!strcmp(currencies[i],"KRW")) { tick(1000);write_bmp("main-currency-krw-large"); }
    }
    assert(currency_state_confirm_active_code("INR"));ui_refresh_main_page();render();
    write_bmp("main-currency-inr");
    assert(currency_state_confirm_auto_selection());ui_refresh_main_page();tick(1000);write_bmp("main-auto");
    assert(strstr((const char *)lv_img_get_src(s_curr_img),"CURR_AUTO.png"));
    assert(!lv_obj_is_visible(s_curr_label) && !lv_obj_is_visible(s_amount_unit_icon));
    assert(currency_state_confirm_detected_code("CNY"));page_01_curr_img_refre();render();
    assert(!strcmp(lv_label_get_text(s_curr_label),currency_metadata_symbol("CNY")));
    assert(lv_obj_is_visible(s_curr_label));
    assert(strstr((const char *)lv_img_get_src(s_curr_img),"CURR_CNY.png"));
    write_bmp("main-auto-detected-cny");
    currency_state_begin_count_session();page_01_curr_img_refre();render();
    assert(!lv_obj_is_visible(s_curr_label) && !lv_obj_is_visible(s_amount_unit_icon));
    assert(strstr((const char *)lv_img_get_src(s_curr_img),"CURR_AUTO.png"));
    assert(currency_state_confirm_multi_selection());ui_refresh_main_page();render();
    counting_data_mark_multi_result(counting_data_mutable());
    smart_island_notify_count_start();tick(400);
    assert(!strcmp(lv_label_get_text(g_si_ctx.objects.counting_unit),"MULTI COUNT"));
    assert(lv_obj_is_visible(g_si_ctx.objects.counting_serial));
    assert(!lv_obj_is_visible(g_si_ctx.objects.counting_value));
    write_bmp("multi-island-counting");
    smart_island_notify_count_reset();tick(400);
    assert(s_multi_layout && lv_obj_is_visible(s_multi_card) && !lv_obj_is_visible(s_detail_btn_a) && !lv_obj_is_visible(s_detail_tray));
    assert(page_01_multi_scroll());
    page_01_main_suspend();page_01_main_reveal_for_transition();
    assert(lv_obj_is_visible(s_multi_card) && !lv_obj_is_visible(s_summary_card));
    assert(lv_obj_is_visible(page_01_main_scroll_obj()) && !lv_obj_is_visible(s_detail_btn_a));
    assert(s_time_timer->paused);
    render();write_bmp("main-multi-return-first-frame");
    assert(page_01_main_resume());
    tick(1000);write_bmp("main-multi");tap(750,245);assert(pushes==opened);
    counting_multi_begin(false);
    uint8_t multi_live[16]={0xfd,0xdf,16,14,'U','S','D',0,0,2,20,0,14,9,1,0};
    assert(counting_multi_info(multi_live,16));
    uint8_t multi_cny[16]={0xfd,0xdf,16,14,'C','N','Y',0,0,0,10,0,2,9,1,0};
    assert(counting_multi_info(multi_cny,16));
    uint8_t multi_end[13]={0xfd,0xdf,13,14,0,0,0,0,0,0,0,2,0};
    assert(counting_multi_info(multi_end,13));ui_refresh_main_page();render();write_bmp("multi-currency-list");
    tap(400,145);render();assert(counting_multi_query_busy());write_bmp("multi-query");
    uint8_t df[16]={0xfd,0xdf,16,11};
    counting_multi_denom(df,16,lv_tick_get());
    const unsigned vals[]={100,50,20,10,5,2},qty[]={3,3,2,3,2,1};
    for(unsigned i=0;i<6;i++){char b[12];snprintf(b,sizeof(b),"%8u%3u",vals[i],qty[i]);memcpy(df+4,b,11);counting_multi_denom(df,16,lv_tick_get());}
    memset(df+4,255,11);counting_multi_denom(df,16,lv_tick_get());ui_refresh_main_page();render();
    assert(counting_multi_current()->currencies[0].status==MULTI_DETAIL_READY);write_bmp("multi-usd-detail");
    pointer(800,252,true);pointer(800,205,true);pointer(800,150,true);pointer(800,150,false);tick(200);
    assert(lv_obj_get_scroll_y(page_01_multi_scroll())>0);
    assert(!counting_multi_query_busy());
    write_bmp("multi-usd-detail-scrolled");
    assert(multi_policy && !multi_policy(GESTURE_ACTION_EXPORT));
    assert(multi_policy(GESTURE_ACTION_HOME));render();write_bmp("multi-back");
    assert(!multi_policy(GESTURE_ACTION_EXIT_PAGE));
    smart_island_notify_warning("A long diagnostic message that must scroll without layout jumping during normal refresh");
    tick(300);render();
    lv_coord_t warning_width=lv_obj_get_width(g_si_ctx.objects.title);
    lv_coord_t warning_x=lv_obj_get_x(g_si_ctx.objects.title);
    smart_island_view_refresh_scene();render();
    assert(lv_obj_get_width(g_si_ctx.objects.title)==warning_width);
    assert(lv_obj_get_x(g_si_ctx.objects.title)==warning_x);
    smart_island_restore_idle();tick(400);
    /* Cached USD opens immediately even while the next currency is loading. */
    counting_multi_prefetch(lv_tick_get());assert(!counting_multi_query_busy());
    lv_tick_inc(250);counting_multi_prefetch(lv_tick_get());assert(counting_multi_query_busy());
    tap(400,145);render();
    assert(counting_multi_current()->currencies[0].status==MULTI_DETAIL_READY);
    assert(counting_multi_current()->currencies[1].status==MULTI_DETAIL_LOADING);
    assert(multi_policy(GESTURE_ACTION_HOME));
    counting_multi_reset();
    memset(df+4,255,11);counting_multi_denom(df,16,lv_tick_get());
    counting_data_mark_multi_result(counting_data_mutable());
    assert(currency_state_leave_special_selection());
    assert(currency_state_confirm_active_code("USD"));ui_refresh_main_page();
    assert(s_multi_layout && lv_obj_is_visible(s_multi_card) && !lv_obj_is_visible(s_summary_card));
    assert(page_01_multi_scroll());
    write_bmp("main-latched-multi-result");
    counting_data_reset_result_scope(counting_data_mutable());ui_refresh_main_page();
    assert(!s_multi_layout && lv_obj_is_visible(s_summary_card));
    fixture(true);ui_refresh_main_page();
    smart_island_notify_count_start();smart_island_update_counting(231,10692);
    smart_island_notify_serial_number(100,"AB1234567890");tick(280);
    write_bmp("main-island-counting");
    smart_island_notify_count_end("Count complete");tick(500);write_bmp("main-island-result");
    smart_island_restore_idle();tick(500);smart_island_open_action_page();tick(500);
    write_bmp("main-island-actions");smart_island_close();tick(500);
    smart_island_notify_warning("Reject pocket full");tick(100);write_bmp("main-island-warning");
    ui_main_destroy();assert(!page_01_main_is_created() && !detail_view);
}

static void test_lifecycle(void)
{
    unsigned baseline=timers();render();lv_mem_monitor_t before,after;lv_mem_monitor(&before);
    for(unsigned i=0;i<12;++i) {
        fixture(i%2!=0);saved_tab=i%3;ui_main_create(lv_scr_act());render();
        assert(page_01_detail_section_get()==(page_01_detail_section_t)saved_tab);
        page_01_main_suspend();tick(80);page_01_main_reveal_for_transition();
        assert(lv_obj_is_visible(page_01_main_scroll_obj()));
        assert(page_01_main_resume());
        smart_island_notify_count_start();tick(100);ui_main_destroy();tick(80);
        assert(timers()==baseline && !detail_view);
    }
    render();lv_mem_monitor(&after);assert(after.free_size==before.free_size);
}

int main(void)
{
    assert(sizeof(lv_coord_t)==2);lv_init();
    static lv_color_t pixels[1280*40];static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*40);
    static lv_disp_drv_t display;lv_disp_drv_init(&display);
    display.hor_res=1280;display.ver_res=400;display.draw_buf=&buffer;display.flush_cb=flush;
    assert(lv_disp_drv_register(&display));
    ui_scrollbar_init(lv_disp_get_default());
    lv_img_decoder_t *decoder=lv_img_decoder_create();assert(decoder);
    lv_img_decoder_set_info_cb(decoder,host_image_info);lv_img_decoder_set_open_cb(decoder,host_image_open);
    lv_img_decoder_set_close_cb(decoder,host_image_close);
    static lv_indev_drv_t driver;lv_indev_drv_init(&driver);driver.type=LV_INDEV_TYPE_POINTER;driver.read_cb=pointer_read;
    lv_indev_t *indev=lv_indev_drv_register(&driver);assert(indev);
    test_main();test_lifecycle();
    unsigned test_timer_count = timers();
    for(unsigned cycle = 0; cycle < 3; ++cycle) {
        ui_page_36_display_test_create(lv_scr_act());
        lv_obj_t *first = test_page;
        ui_page_36_display_test_create(lv_scr_act());
        assert(first == test_page);
        render();
        assert(framebuffer[230 * 1280 + 530].full == lv_color_hex(0xF6F1ED).full);
        assert(framebuffer[152 * 1280 + 1250].full == lv_color_hex(0xFFFFFF).full);
        if(cycle == 0) write_bmp("display-test");
        ui_page_36_display_test_destroy();
        ui_page_36_display_test_destroy();
        assert(test_page == NULL && timers() == test_timer_count);
    }
    counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    lv_indev_delete(indev);lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS actual Main/detail/Smart Island raster and pointer dispatch, all modes, pending START, retained lifecycle; DMA/controller/Innovation navigation remain board tests");
    return 0;
}
