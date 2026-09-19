/* Actual LVGL views with memory-only report fixtures; no protocol/flash writes. */
#include "un260/lv_core/page_02_list_search.c"
#include "un260/lv_components/ui_multi_detail.c"
#include "un260/counting/counting_data_store_internal.h"
#include "test_history_view_support.h"
#include <assert.h>
#include <stdlib.h>
static counting_multi_t multi_fixture;
static multi_serial_t serial_fixture[2][16];
static multi_serial_cache_t serial_cache[2];
static multi_reject_cache_t rejects_fixture;
const counting_multi_t *counting_multi_current(void){return &multi_fixture;}
const multi_serial_cache_t *counting_multi_serials(unsigned i){return &serial_cache[i];}
const multi_reject_cache_t *counting_multi_rejects(void){return &rejects_fixture;}
uint32_t counting_multi_extra_revision(void){return multi_fixture.revision;}
bool counting_multi_extra_busy(void){return false;}
bool counting_multi_query_busy(void){return false;}
bool app_command_runtime_count_start_busy(void){return false;}
bool counting_multi_request(unsigned i,uint32_t t){(void)i;(void)t;return false;}
bool counting_multi_serial_request(unsigned i,uint32_t t){(void)i;(void)t;return false;}
bool counting_multi_reject_request(uint32_t t){(void)t;return false;}
static void settle(void){ui_frame_commit_flush();history_test_tick(120);}
static void click(lv_obj_t *o){assert(o);history_test_click(o);settle();}
static lv_obj_t *button_named(const char *caption){return history_test_button(lv_scr_act(),caption);}
static void choice(const char *caption){click(button_named(caption));}
static void apply(void){lv_obj_t *b=button_named(ui_text_get(UI_TEXT_MULTI_SEARCH_APPLY));if(!b)b=button_named(ui_text_get(UI_TEXT_SERIAL_SEARCH));click(b);}
static void assert_pair(lv_obj_t *buttons[2],bool vertical,unsigned active)
{
    lv_obj_t *parent=lv_obj_get_parent(buttons[0]);
    assert(parent==lv_obj_get_parent(buttons[1]));lv_obj_update_layout(parent);
    assert(lv_obj_get_style_bg_color(parent,0).full==lv_color_hex(0xE7EDF0).full);
    assert(lv_obj_get_style_bg_color(buttons[active],0).full==lv_color_hex(0xFFFFFF).full);
    assert(lv_obj_has_state(buttons[active],LV_STATE_CHECKED));
    if(vertical)assert(lv_obj_get_y(buttons[1])==lv_obj_get_height(buttons[0]));
    else assert(lv_obj_get_x(buttons[1])==lv_obj_get_width(buttons[0]));
    assert(lv_obj_get_style_border_width(lv_obj_get_child(parent,-1),0)==3);
}
static unsigned assert_choice_keys(lv_obj_t *o)
{
    if(!lv_obj_is_visible(o))return 0;
    unsigned values=0;
    if(lv_obj_check_type(o,&lv_btn_class)){
        lv_obj_t *l=lv_damped_button_get_label(o);
        if(l){const char *s=lv_label_get_text(l);
            assert(strcmp(s,"SPACE")&&strcmp(s,"Aa")&&strcmp(s,"Clear text"));
            if(strlen(s)==1)assert(*s>='0'&&*s<='9');
            if(*s>='0'&&*s<='9'){
                ++values;assert(lv_obj_get_width(o)>=44&&lv_obj_get_height(o)>=44);
            }
        }
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)values+=assert_choice_keys(lv_obj_get_child(o,i));
    return values;
}
static uint16_t located;
static void list_closed(uint16_t slot,void *ctx){(void)ctx;located=slot;}
static void test_list_modes(void)
{
    counting_sim_t *data=counting_data_mutable();counting_data_clear_serials(data);
    assert(counting_data_ensure_serial_capacity(data,16));
    for(unsigned i=0;i<15;++i){
        char serial[20];snprintf(serial,sizeof(serial),"SN%04u",i+1);
        data->sn_str[i]=malloc(strlen(serial)+1);assert(data->sn_str[i]);strcpy(data->sn_str[i],serial);
        data->denom_mix[i]=i<5?10:50;
    }
    const int directory[]={100,50,20,10,5,1};data->denom_number=6;
    for(unsigned i=0;i<6;++i)data->denom[i].value=directory[i];
    data->total_pcs=15;
    page_02_list_search_t *s=page_02_list_search_create(lv_scr_act(),list_closed,NULL);assert(s);settle();
    assert_pair(s->modes,false,0);
    click(s->input);assert(lv_alnum_keyboard_set_text(s->keyboard,"SN0001"));apply();
    assert(s->result.matched_count==1&&s->slots[0]==0);
    click(s->exclude);assert(s->query.exclude_text);
    click(s->modes[1]);assert(s->denomination_mode&&s->result.matched_count==15);assert_pair(s->modes,false,1);
    click(s->input);assert(!lv_alnum_keyboard_is_visible(s->keyboard));
    click(history_test_button(s->grid,"50"));assert(s->result.matched_count==10&&s->denomination_query.denomination_count==1);
    for(unsigned i=0;i<10;++i)assert(s->slots[i]==i+5);
    assert(!strcmp(data->sn_str[5],"SN0006")&&data->total_pcs==15);
    history_test_bmp("search-denomination-single");
    click(history_test_button(s->grid,"10"));assert(s->result.matched_count==15&&s->denomination_query.denomination_count==2);
    history_test_bmp("search-denomination-multiple");
    click(history_test_button(s->grid,"50"));assert(s->result.matched_count==5&&s->slots[0]==0);
    click(history_test_button(s->grid,"10"));assert(s->result.matched_count==15&&!s->denomination_query.denomination_count);
    click(history_test_button(s->grid,"100"));assert(!s->result.matched_count);click(history_test_button(s->grid,"100"));click(history_test_button(s->grid,"50"));
    click(s->sort);assert(s->slots[0]==14&&s->slots[9]==5);
    click(s->modes[0]);assert(s->query.exclude_text&&s->result.matched_count==14);
    history_test_bmp("search-serial-results");
    click(s->input);assert(lv_alnum_keyboard_set_text(s->keyboard,"DRAFT"));history_test_tap(20,200);settle();
    assert(!strcmp(s->query.text,"SN0001")&&s->result.matched_count==14);
    click(s->modes[1]);assert(s->result.matched_count==10);
    data->sn_str[15]=malloc(8);strcpy(data->sn_str[15],"LATE050");data->denom_mix[15]=50;
    page_02_list_search_data_changed(s);settle();assert(s->result.matched_count==11&&s->slots[0]==15);
    located=UINT16_MAX;list_closed(s->slots[0],NULL);assert(located==15);
    for(unsigned i=0;i<12;++i){click(s->modes[0]);click(s->modes[1]);assert(s->result.matched_count==11);}
    click(s->all);assert(s->result.matched_count==16);
    click(s->modes[0]);click(s->input);page_02_list_search_data_changed(s);
    assert(s->data_dirty);page_02_list_search_destroy(s);settle();assert(!ui_frame_commit_pending());
    counting_data_clear_serials(data);
    puts("PASS List: multi-select/deselect, empty=all, no keypad, independent modes, original NO, late data, lifetime");
}
static void test_main_modes(void)
{
    memset(&multi_fixture,0,sizeof(multi_fixture));multi_fixture.count=2;multi_fixture.generation=1;
    strcpy(multi_fixture.currencies[0].code,"CNY");strcpy(multi_fixture.currencies[1].code,"USD");
    for(unsigned c=0;c<2;++c){
        multi_fixture.currencies[c].status=MULTI_DETAIL_READY;multi_fixture.currencies[c].pcs=15;
        const unsigned directory[]={100,50,20,10,5,1};multi_fixture.currencies[c].denom_count=6;
        for(unsigned i=0;i<6;++i)multi_fixture.currencies[c].denom[i].value=directory[i];
        serial_cache[c]=(multi_serial_cache_t){.status=MULTI_DETAIL_READY,.count=15,.rows=serial_fixture[c]};
        for(unsigned i=0;i<15;++i){serial_fixture[c][i].number=100+i;serial_fixture[c][i].value=i<5?10:50;snprintf(serial_fixture[c][i].text,21,"SN%04u",i+1);}
    }
    for(unsigned expanded=0;expanded<2;++expanded){
        ui_multi_detail_t *v=ui_multi_detail_create(lv_scr_act(),expanded,NULL,NULL);assert(v);
        ui_multi_detail_visible(v,true);ui_multi_detail_select(v,0,1);settle();
        assert(!strcmp(lv_label_get_text(v->search_text),ui_text_get(UI_TEXT_QUERY)));
        ui_multi_detail_search(v);settle();history_test_bmp(expanded?"multi-list-search-keyboard":"main-search-keyboard");
        history_test_tap(200,243);settle();assert(lv_alnum_keyboard_is_choice_mode(v->keyboard));
        assert(assert_choice_keys(lv_obj_get_child(lv_scr_act(),-1))==6);
        choice("50");history_test_bmp(expanded?"multi-list-denomination-single":"main-denomination-single");
        assert(v->count==15);apply();assert(v->count==10);
        assert(!strcmp(lv_label_get_text(v->search_text),"50"));
        for(unsigned i=0;i<10;++i)assert(v->indices[i]==i+5);
        lv_obj_t *row=lv_obj_get_child(lv_recycled_list_object(v->list),0);
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(row,0)),"105"));
        assert(!strcmp(lv_label_get_text(lv_obj_get_child(row,1)),"SN0006"));
        history_test_bmp(expanded?"multi-list-denomination-results":"main-denomination-results");
        ui_multi_detail_search(v);settle();choice("10");history_test_tap(20,200);settle();
        assert(v->count==10&&v->denomination_count==1&&v->denomination_query[0]==50);
        ui_multi_detail_search(v);settle();assert(lv_alnum_keyboard_is_choice_mode(v->keyboard));
        assert(lv_alnum_keyboard_get_choices(v->keyboard,NULL,0)==2); /* uncommitted draft retained */
        ++multi_fixture.revision;ui_multi_detail_refresh(v);
        assert(lv_alnum_keyboard_get_choices(v->keyboard,NULL,0)==2);
        history_test_bmp(expanded?"multi-list-denomination-multiple":"main-denomination-multiple");
        apply();assert(v->count==15&&v->denomination_count==2);
        assert(!strcmp(lv_label_get_text(v->search_text),"10,50"));
        ui_multi_detail_search(v);settle();choice("50");apply();assert(v->count==5);
        ui_multi_detail_search(v);settle();choice("10");apply();assert(v->count==15&&!v->denomination_count);
        ui_multi_detail_search(v);settle();choice("5");choice("10");apply();
        assert(!strcmp(lv_label_get_text(v->search_text),"5,10"));
        history_test_bmp(expanded?"multi-list-denomination-caption":"main-denomination-caption");
        ui_multi_detail_search(v);settle();choice(ui_text_get(UI_TEXT_QUERY_SERIAL));
        assert(lv_alnum_keyboard_set_text(v->keyboard,"SN0001"));apply();assert(v->count==1&&v->indices[0]==0);
        assert(!strcmp(lv_label_get_text(v->search_text),"SN0001"));history_test_bmp(expanded?"multi-list-serial-caption":"main-serial-caption");
        ui_multi_detail_search(v);settle();choice(ui_text_get(UI_TEXT_QUERY_DENOMINATION));
        history_test_tap(20,200);settle(); /* mode remembered, active serial filter unchanged */
        assert(v->count==1&&!v->denomination_mode);
        ui_multi_detail_select(v,-1,0);ui_multi_detail_select(v,0,1);assert(v->count==1);
        ui_multi_detail_search(v);settle();assert(lv_alnum_keyboard_is_choice_mode(v->keyboard));
        assert(lv_alnum_keyboard_get_choices(v->keyboard,NULL,0)==2);
        ui_multi_detail_select(v,1,1);assert(v->count==15&&!v->denomination_mode&&!v->query[0]);
        ui_multi_detail_search(v);settle();assert(!lv_alnum_keyboard_is_choice_mode(v->keyboard));
        assert(!lv_alnum_keyboard_get_text(v->keyboard)[0]&&!lv_alnum_keyboard_get_choices(v->keyboard,NULL,0));
        ui_multi_detail_select(v,0,1);assert(v->count==15&&!v->query[0]&&!v->denomination_count);
        ui_multi_detail_search(v);settle();assert(!lv_alnum_keyboard_is_choice_mode(v->keyboard));
        ++multi_fixture.generation;ui_multi_detail_refresh(v);
        assert(v->selected==-1&&!lv_alnum_keyboard_is_visible(v->keyboard));
        ui_multi_detail_select(v,0,1);assert(v->count==15&&!v->query[0]&&!v->denomination_count);
        ui_multi_detail_search(v);settle();uint32_t options[30],selected[30];
        for(unsigned i=0;i<30;++i)options[i]=100+i;
        assert(lv_alnum_keyboard_set_choices(v->keyboard,options,30,options,30));
        lv_alnum_keyboard_set_choice_mode(v->keyboard,true);settle();
        assert(lv_alnum_keyboard_get_choices(v->keyboard,selected,30)==30&&!memcmp(options,selected,sizeof(options)));
        options[1]=options[0];assert(!lv_alnum_keyboard_set_choices(v->keyboard,options,30,NULL,0));
        assert(lv_alnum_keyboard_get_choices(v->keyboard,NULL,0)==30);
        history_test_bmp("main-denomination-capacity");
        assert(lv_alnum_keyboard_set_choices(v->keyboard,NULL,0,NULL,0));settle();
        assert(!lv_alnum_keyboard_get_choices(v->keyboard,NULL,0));history_test_bmp("main-denomination-empty");
        ui_multi_detail_destroy(v);settle();
    }
    puts("PASS Main: draft/mode memory, commit-only filtering, caption 5,10, currency reset, original NO, generation/capacity/lifetime");
}
void search_modes_test(void)
{
    unsigned baseline=history_test_timers();test_list_modes();test_main_modes();settle();
    assert(history_test_timers()==baseline);
}
