#include "src/misc/lv_txt_ap.h"
/* Host-only actual List/LVGL coverage. Include after production units and
 * existing search helpers; no mock projection or replacement renderer. */
static lv_point_t serials_cases_point;
static lv_indev_state_t serials_cases_state = LV_INDEV_STATE_RELEASED;
static lv_indev_t *serials_cases_pointer_device;
static void serials_cases_read(lv_indev_drv_t *driver,lv_indev_data_t *data)
{(void)driver;data->point=serials_cases_point;data->state=serials_cases_state;}
static void serials_cases_pointer(int x,int y,bool down)
{
    serials_cases_point=(lv_point_t){x,y};
    serials_cases_state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
    tick(60);
}
static void serials_cases_tap_xy(int x,int y)
{serials_cases_pointer(x,y,true);serials_cases_pointer(x,y,false);tick(240);}
static void serials_cases_tap(lv_obj_t *object)
{
    assert(object && lv_obj_is_visible(object));
    assert(!lv_obj_has_state(object,LV_STATE_DISABLED));
    lv_obj_update_layout(view->page);lv_area_t area;lv_obj_get_coords(object,&area);
    assert(area.x1>=0&&area.x2<1280&&area.y1>=0&&area.y2<400);
    serials_cases_tap_xy((area.x1+area.x2)/2,(area.y1+area.y2)/2);
    ui_frame_commit_flush();
}
static void serials_cases_driver(void)
{
    static lv_indev_drv_t driver;lv_indev_drv_init(&driver);
    driver.type=LV_INDEV_TYPE_POINTER;driver.read_cb=serials_cases_read;
    serials_cases_pointer_device=lv_indev_drv_register(&driver);
    assert(serials_cases_pointer_device);
}
static void serials_cases_drag(lv_obj_t *object,int dx,int dy)
{
    lv_obj_update_layout(object);lv_area_t a;lv_obj_get_coords(object,&a);
    int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
    serials_cases_pointer(x,y,true);
    for(unsigned k=1;k<=6;++k)serials_cases_pointer(x+dx*(int)k/6,y+dy*(int)k/6,true);
    serials_cases_pointer(x+dx,y+dy,false);
    ui_frame_commit_flush();
}
static void serials_cases_fixture(counting_sim_t *data,unsigned count)
{
    counting_data_clear_serials(data);
    for(unsigned rank=0;rank<count;++rank){
        char text[40];snprintf(text,sizeof(text),"AB%010u",rank+1);
        const char *value=(rank==1||rank==8)?"DUP000000123":rank==4?"":rank==5?"WWWWWWWWWWWW":text;
        serial_search_cases_store(data,rank*2,value,rank%2?10:20);
    }
    /* An allocated serial with zero face value is absent; unallocated holes
     * are absent. The empty-but-valid serial at rank4 remains a record. */
    serial_search_cases_store(data,count*2+1,"NOT-A-NOTE",0);
    page_02_list_report_reset();serial_search_cases_flush();
    assert(view->data.serial_count==count);
    for(unsigned i=0;i<count;++i)assert(view->data.serial[i]==i*2);
}
static lv_obj_t *serials_cases_cell(lv_obj_t *row,unsigned group,unsigned field)
{
    assert(group<3 && field<3 && lv_obj_get_child_cnt(row)==3);
    lv_obj_t *block=lv_obj_get_child(row,group);
    assert(lv_obj_get_child_cnt(block)==3 && lv_obj_get_style_border_width(block,0)==0);
    lv_obj_t *label=lv_obj_get_child(block,field);
    assert(lv_obj_check_type(label,&lv_label_class));return label;
}
static void serials_cases_header(page_02_list_serials_t *s)
{
    lv_obj_update_layout(view->page);
    assert(s->back && s->main_button);
    assert(lv_obj_get_width(s->back)==76 && lv_obj_get_height(s->back)==42);
    assert(lv_obj_get_width(s->main_button)>=76 && lv_obj_get_height(s->main_button)==42);
    assert(lv_obj_get_child_cnt(s->back)==1 && lv_obj_get_child_cnt(s->main_button)==1);
    assert(lv_obj_get_style_bg_color(s->back,0).full==lv_color_hex(0xF5F6F7).full);
    assert(lv_obj_get_style_bg_color(s->main_button,0).full==lv_color_hex(0xF5F6F7).full);
    lv_area_t prev,next,panel;
    lv_obj_get_coords(s->previous,&prev);lv_obj_get_coords(s->next,&next);lv_obj_get_coords(s->panel,&panel);
    assert(lv_area_get_width(&prev)==38 && lv_area_get_width(&next)==38);
    assert(lv_area_get_height(&prev)==38 && lv_area_get_height(&next)==38);
    assert(prev.y1==next.y1 && prev.y2==next.y2);
    assert(abs((prev.y1-(panel.y1+343))-(panel.y2-prev.y2))<=1);
    assert(prev.x1-panel.x1==panel.x2-next.x2);
    lv_area_t title,main,back;
    lv_obj_get_coords(s->title,&title);lv_obj_get_coords(s->main_button,&main);lv_obj_get_coords(s->back,&back);
    assert(title.x2<main.x1 && main.x2+8==back.x1-1);
    assert(main.x1>=0 && back.x2<1280 && main.y1==12 && main.y2==53);
    lv_obj_t *label=lv_damped_button_get_label(s->main_button);lv_point_t size;
    lv_txt_get_size(&size,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    assert(size.x<=lv_obj_get_width(label) && size.y<=lv_obj_get_height(label));
}
/* Independent test oracle: form each page as a source-rank array, then
 * take contiguous column chunks. Do not call the production rank helpers. */
static unsigned serials_cases_expected_rank(const page_02_list_serials_t *s,unsigned visual,unsigned column)
{
    unsigned capacity=s->rows*s->columns;
    unsigned page_start=(visual/s->rows)*capacity;
    unsigned total=s->projection->serial_count;
    if(page_start>=total||column>=s->columns)return UINT_MAX;
    unsigned in_page=total-page_start;if(in_page>capacity)in_page=capacity;
    unsigned depth=in_page/s->columns+(in_page%s->columns!=0);
    unsigned offset=visual%s->rows;
    if(offset>=depth)return UINT_MAX;
    unsigned ordinal=column*depth+offset;
    return ordinal<in_page?page_start+ordinal:UINT_MAX;
}
static void serials_cases_rows(page_02_list_serials_t *s)
{
    assert(s && s->list && s->columns>=1 && s->columns<=3);
    serials_cases_header(s);
    const ui_list_window_t *w=lv_recycled_list_window(s->list);
    lv_obj_t *vp=lv_recycled_list_object(s->list);
    lv_obj_update_layout(view->page);
    assert(w->count==(s->projection->serial_count+s->columns-1)/s->columns);
    assert(w->rows>=1 && w->rows<=9 && w->paged);
    assert(lv_obj_get_child_cnt(vp)<=w->rows+2U);
    assert(lv_obj_get_style_border_width(s->root,0)==0 && lv_obj_get_style_border_width(s->panel,0)==0);
    unsigned checked=0;
    for(unsigned i=0;i<lv_obj_get_child_cnt(vp);++i){
        lv_obj_t *row=lv_obj_get_child(vp,i);
        if(lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)||!lv_obj_get_child_cnt(row))continue;
        assert(lv_obj_get_child_cnt(row)==3 && lv_obj_get_style_border_width(row,0)==0);
        int y=lv_obj_get_y(row);
        assert(y>-(int)w->row_height && y<=(int)(w->rows*w->row_height));
        unsigned group=(unsigned)((y+(int)w->offset)/(int)w->row_height);
        assert(group>=w->first && group<w->count);
        for(unsigned col=0;col<3;++col){
            unsigned rank=serials_cases_expected_rank(s,group,col);
            lv_obj_t *block=lv_obj_get_child(row,col);
            assert(lv_obj_get_child_cnt(block)==3 && lv_obj_get_style_border_width(block,0)==0);
            if(col>=s->columns||rank>=s->projection->serial_count){
                assert(lv_obj_has_flag(block,LV_OBJ_FLAG_HIDDEN));continue;
            }
            assert(!lv_obj_has_flag(block,LV_OBJ_FLAG_HIDDEN));
            for(unsigned field=0;field<3;++field){
                lv_obj_t *label=serials_cases_cell(row,col,field);
                assert(!lv_obj_has_flag(label,LV_OBJ_FLAG_HIDDEN));
                unsigned slot=s->projection->serial[rank];char text[40];
                if(field==0)snprintf(text,sizeof(text),"%u",slot+1);
                else if(field==2){
                    if(list_monetary_result_supported())snprintf(text,sizeof(text),"%.0f",(double)counting_data_current()->denom_mix[slot]);
                    else strcpy(text,"--");
                }
                const char *expected=field==1?counting_data_current()->sn_str[slot]:text;
                assert(!strcmp(lv_label_get_text(label),expected));
                assert(lv_obj_get_style_base_dir(label,0)==LV_BASE_DIR_LTR);
                if(field==0)assert(lv_obj_get_style_text_font(label,0)==&lv_font_instrument_sans_medium_14);
                if(field==1){
                    const lv_font_t *actual=lv_obj_get_style_text_font(label,0);
                    const lv_font_t *preferred=ui_message_font(&lv_font_instrument_sans_medium_20);
                    lv_point_t unwrapped;lv_txt_get_size(&unwrapped,expected,preferred,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
                    if(strlen(expected)<=12){
                        if(unwrapped.x<=lv_obj_get_width(label))assert(actual==preferred);
                        else assert(actual==ui_message_font(&lv_font_instrument_sans_medium_18)
                            || actual==ui_message_font(&lv_font_instrument_sans_medium_16)
                            || actual==ui_message_font(&lv_font_instrument_sans_medium_14));
                        lv_txt_get_size(&unwrapped,expected,actual,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
                        assert(unwrapped.x<=lv_obj_get_width(label) && unwrapped.y<=(int)s->row_height);
                    }else assert(actual==preferred);
                }
                lv_point_t size;lv_txt_get_size(&size,lv_label_get_text(label),
                    lv_obj_get_style_text_font(label,0),lv_obj_get_style_text_letter_space(label,0),
                    lv_obj_get_style_text_line_space(label,0),lv_obj_get_width(label),LV_TEXT_FLAG_NONE);
                assert(size.y<=lv_obj_get_height(label));
                lv_area_t a,b;lv_obj_get_coords(block,&a);lv_obj_get_coords(label,&b);
                assert(b.x1>=a.x1 && b.x2<=a.x2 && b.y1>=a.y1 && b.y2<=a.y2);
                if(field==1)assert(lv_obj_get_style_text_align(label,0)==LV_TEXT_ALIGN_LEFT);
                if(field==2)assert(lv_obj_get_style_text_align(label,0)==LV_TEXT_ALIGN_RIGHT);
            }
            if(col<s->columns&&rank<s->projection->serial_count)++checked;
        }
    }
    lv_obj_t *scrollbar=NULL;
    for(unsigned i=0;i<lv_obj_get_child_cnt(vp);++i){
        lv_obj_t *o=lv_obj_get_child(vp,i);
        if(!lv_obj_get_child_cnt(o)){assert(!scrollbar);scrollbar=o;}
    }
    assert(scrollbar && lv_obj_get_width(scrollbar)==8);
    assert(lv_obj_has_flag(scrollbar,LV_OBJ_FLAG_HIDDEN)==(w->paged||w->count<=w->rows));
    lv_area_t va,sa;lv_obj_get_coords(vp,&va);lv_obj_get_coords(scrollbar,&sa);
    assert(sa.x1>=va.x1 && sa.x2<=va.x2 && sa.y1>=va.y1 && sa.y2<=va.y2);
    unsigned groups=w->rows;
    if(groups>w->count-w->first)groups=w->count-w->first;
    unsigned expected=0;
    for(unsigned group=w->first;group<w->first+groups;++group)
        for(unsigned col=0;col<s->columns;++col){
            unsigned rank=serials_cases_expected_rank(s,group,col);
            if(rank==UINT_MAX)continue;
            ++expected;
        }
    assert(checked==expected);
    char range[96];
    snprintf(range,sizeof(range),ui_text_get(UI_TEXT_LIST_PAGE_FMT),
        (unsigned)ui_list_window_page_number(w),(unsigned)ui_list_window_pages(w));
#if LV_USE_ARABIC_PERSIAN_CHARS
    /* LVGL stores Arabic labels after standard shaping. Shape the independent
     * formatted expectation as well, retaining exact numbers and wording. */
    char shaped[384];assert(_lv_txt_ap_calc_bytes_cnt(range)+1<=sizeof(shaped));
    _lv_txt_ap_proc(range,shaped);
    assert(!strcmp(lv_label_get_text(s->range),shaped));
#else
    assert(!strcmp(lv_label_get_text(s->range),range));
#endif
}
static void serials_cases_base_hidden(bool hidden)
{
    for(unsigned i=0;i<PAGE_02_SECTION_COUNT;i++)
        assert(lv_obj_has_flag(view->section[i].panel,LV_OBJ_FLAG_HIDDEN)==hidden);
    for(unsigned i=0;i<LIST_ACTION_COUNT;i++)
        assert(lv_obj_has_flag(view->actions[i],LV_OBJ_FLAG_HIDDEN)==hidden);
}
static void serials_cases_full_surface(page_02_list_serials_t *s)
{
    lv_obj_update_layout(view->page);lv_refr_now(NULL);
    lv_area_t area;lv_obj_get_coords(s->root,&area);
    assert(area.x1==0 && area.y1==0 && area.x2==1279 && area.y2==399);
    assert(lv_obj_get_style_bg_opa(s->root,0)==LV_OPA_COVER);
    lv_obj_get_coords(s->panel,&area);
    assert(area.x1==16 && area.y1==8 && area.x2==1263 && area.y2==391);
    assert(lv_obj_get_style_bg_opa(s->panel,0)==LV_OPA_COVER);
    assert(lv_obj_get_child(view->page,-1)==s->root);
    serials_cases_base_hidden(true);
    assert(color_pixels(s->root,0x2BD900)==0 && color_pixels(s->root,0xF85820)==0);
    assert(color_pixels(s->title,0x17212A)>0);
    assert(color_pixels(s->range,0x7A8D9B)>0);
    lv_color_t *partial=malloc(sizeof(framebuffer));assert(partial);
    memcpy(partial,framebuffer,sizeof(framebuffer));
    render_page();assert(!memcmp(partial,framebuffer,sizeof(framebuffer)));free(partial);
}
static page_02_list_serials_t *serials_cases_open(void)
{
    assert(view && !view->serials && !view->search);
    lv_obj_update_layout(view->page);lv_area_t diagnostic;
    lv_obj_get_coords(view->section[PAGE_02_SECTION_B].details,&diagnostic);
    lv_point_t hit={(diagnostic.x1+diagnostic.x2)/2,(diagnostic.y1+diagnostic.y2)/2};
    assert(lv_indev_search_obj(lv_scr_act(),&hit)==view->section[PAGE_02_SECTION_B].details);
    assert(!lv_indev_search_obj(lv_layer_top(),&hit) && !serials_cases_pointer_device->proc.disabled);
    serials_cases_tap(view->section[PAGE_02_SECTION_B].details);
    assert(view->serials && lv_obj_is_visible(view->serials->root));
    assert(lv_obj_get_x(view->serials->root)==0 && lv_obj_get_y(view->serials->root)==0);
    assert(lv_obj_get_width(view->serials->root)==1280 && lv_obj_get_height(view->serials->root)==400);
    assert(lv_obj_has_flag(view->serials->root,LV_OBJ_FLAG_CLICKABLE));
    serials_cases_full_surface(view->serials);
    return view->serials;
}
static void serials_cases_back(void)
{
    assert(view->serials);
    lv_obj_t *back=view->serials->back;
    assert(back && lv_obj_get_width(back)==76 && lv_obj_get_height(back)==42);
    assert(lv_obj_get_child_cnt(back)==1);
    unsigned switches=host_navigation_switches;
    serials_cases_tap(back);assert(!view->serials);
    assert(host_navigation_switches==switches);
    serials_cases_base_hidden(false);
}
static void serials_cases_b_unchanged(uint32_t first,float offset,bool paged)
{
    const ui_list_window_t *b=lv_recycled_list_window(view->section[PAGE_02_SECTION_B].list);
    assert(b->first==first && b->offset==offset && b->paged==paged);
}
static void serials_cases_collect(page_02_list_serials_t *s,bool *accumulated,bool require_unique)
{
    const ui_list_window_t *w=lv_recycled_list_window(s->list);
    lv_obj_t *vp=lv_recycled_list_object(s->list);bool within[COUNTING_DATA_MAX_ITEMS]={0};
    for(unsigned ri=0;ri<lv_obj_get_child_cnt(vp);++ri){
        lv_obj_t *row=lv_obj_get_child(vp,ri);
        if(lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)||lv_obj_get_child_cnt(row)!=3)continue;
        int y=lv_obj_get_y(row);if(y<0||y>=(int)(w->rows*w->row_height))continue;
        for(unsigned col=0;col<s->columns;++col){
            lv_obj_t *block=lv_obj_get_child(row,col);if(lv_obj_has_flag(block,LV_OBJ_FLAG_HIDDEN))continue;
            char *end;unsigned slot=(unsigned)strtoul(lv_label_get_text(serials_cases_cell(row,col,0)),&end,10)-1;
            assert(!*end);unsigned rank=0;
            while(rank<s->projection->serial_count&&s->projection->serial[rank]!=slot)++rank;
            assert(rank<s->projection->serial_count&&!within[rank]);within[rank]=true;
            if(require_unique)assert(!accumulated[rank]);
            accumulated[rank]=true;
        }
    }
    for(unsigned group=w->first;group<ui_list_window_last(w);++group)
        for(unsigned col=0;col<s->columns;++col){
            unsigned rank=serials_cases_expected_rank(s,group,col);
            if(rank!=UINT_MAX)assert(within[rank]);
        }
}
static void serials_cases_complete_pages(page_02_list_serials_t *s)
{
    assert(lv_recycled_list_window(s->list)->paged);
    unsigned saved=lv_recycled_list_window(s->list)->first;
    bool all[COUNTING_DATA_MAX_ITEMS]={0};
    lv_recycled_list_page_step(s->list,-10000);
    unsigned pages=ui_list_window_pages(lv_recycled_list_window(s->list));
    for(unsigned page=0;page<pages;++page){
        serials_cases_rows(s);serials_cases_collect(s,all,true);
        if(page+1<pages)lv_recycled_list_page_step(s->list,1);
    }
    for(unsigned rank=0;rank<s->count;++rank)assert(all[rank]);
    assert(lv_recycled_list_scroll_to_index(s->list,saved));
    puts("PASS independent complete-page source sets: no omitted or duplicate notes across column-major pages");
}
static void serials_cases_nine_fixture(counting_sim_t *data)
{
    counting_data_clear_serials(data);
    for(unsigned rank=0;rank<9;++rank){char text[16];snprintf(text,sizeof(text),"IX%010u",rank+1);serial_search_cases_store(data,rank,text,20);}
    page_02_list_report_reset();serial_search_cases_flush();assert(view->data.serial_count==9);
}
static void serials_cases_nine_order(page_02_list_serials_t *s)
{
    static const unsigned expected[3][3]={{1,4,7},{2,5,8},{3,6,9}};
    serials_cases_rows(s);lv_obj_t *vp=lv_recycled_list_object(s->list);
    for(unsigned visual=0;visual<3;++visual){
        lv_obj_t *row=NULL;
        for(unsigned ri=0;ri<lv_obj_get_child_cnt(vp);++ri){lv_obj_t *candidate=lv_obj_get_child(vp,ri);
            if(!lv_obj_has_flag(candidate,LV_OBJ_FLAG_HIDDEN)&&lv_obj_get_child_cnt(candidate)==3&&lv_obj_get_y(candidate)==(int)(visual*30))row=candidate;}
        assert(row);for(unsigned col=0;col<3;++col){char text[16];snprintf(text,sizeof(text),"%u",expected[visual][col]);
            assert(!strcmp(lv_label_get_text(serials_cases_cell(row,col,0)),text));}
    }
}
static void test_list_serials_cases(counting_sim_t *data)
{
    serials_cases_nine_fixture(data);
    lv_recycled_list_set_paged(view->section[PAGE_02_SECTION_B].list,false);
    page_02_list_serials_t *example=serials_cases_open();
    serials_cases_nine_order(example);serials_cases_complete_pages(example);write_bmp("list-serial-table-nine-order");serials_cases_back();
    serials_cases_fixture(data,37);
    lv_recycled_list_t *b=view->section[PAGE_02_SECTION_B].list;
    lv_recycled_list_set_paged(b,false);assert(lv_recycled_list_scroll_to_index(b,2));
    uint64_t unchanged=serial_search_cases_hash(data);
    unsigned baseline=timers();
    page_02_list_serials_t *s=serials_cases_open();
    assert(s->columns==3 && s->rows==9 && s->row_height==30);
    assert(lv_recycled_list_window(s->list)->first==0 && lv_recycled_list_window(s->list)->paged);
    serials_cases_rows(s);write_bmp("list-serial-table-page-entry");
    /* Native hit testing over the former right action rail must stay inside
     * the full-screen viewer; Search/History/Print/Home never receive it. */
    unsigned h=history_clicks,p=print_clicks,home=home_clicks;
    const int covered_rail_y[]={80,156,248,320};
    for(unsigned i=0;i<4;++i){serials_cases_tap_xy(1216,covered_rail_y[i]);assert(view->serials==s);}
    assert(history_clicks==h && print_clicks==p && home_clicks==home && !view->search);
    serials_cases_b_unchanged(2,2*36,false);
    serials_cases_drag(lv_recycled_list_object(s->list),0,-120);tick(500);
    assert(lv_recycled_list_window(s->list)->first==0 && lv_recycled_list_window(s->list)->offset==0);
    assert(lv_recycled_list_window(s->list)->paged);
    serials_cases_rows(s);serials_cases_b_unchanged(2,2*36,false);
    lv_recycled_list_page_step(s->list,-10000);
    serials_cases_drag(lv_recycled_list_object(s->list),-320,0);
    assert(lv_recycled_list_window(s->list)->first==9);
    serials_cases_rows(s);write_bmp("list-serial-table-page2");
    serials_cases_drag(lv_recycled_list_object(s->list),320,0);
    assert(lv_recycled_list_window(s->list)->first==0);
    /* Vertical contacts cannot enable scrolling or move the fixed page. */
    serials_cases_drag(lv_recycled_list_object(s->list),0,-90);tick(500);
    assert(lv_recycled_list_window(s->list)->paged);
    assert(lv_recycled_list_window(s->list)->first==0 && lv_recycled_list_window(s->list)->offset==0);
    serials_cases_rows(s);serials_cases_full_surface(s);
    write_bmp("list-serial-table-vertical-stays-page");
    serials_cases_tap(s->next);
    assert(lv_recycled_list_window(s->list)->first==9);
    serials_cases_tap(s->previous);
    assert(lv_recycled_list_window(s->list)->first==0);
    lv_recycled_list_page_step(s->list,10000);
    assert(lv_recycled_list_window(s->list)->first==9);
    serials_cases_rows(s);write_bmp("list-serial-table-tail-odd");
    serials_cases_complete_pages(s);serials_cases_rows(s);
    assert(serial_search_cases_hash(data)==unchanged);
    serials_cases_back();assert(timers()==baseline);
    serials_cases_b_unchanged(2,2*36,false);
    /* Global ESC finds Back even though Main is created afterward; Main
     * must never acquire the common back marker. */
    s=serials_cases_open();serials_cases_header(s);
    unsigned switches=host_navigation_switches;
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    assert(!view->serials && host_navigation_switches==switches);
    assert(timers()==baseline);serials_cases_b_unchanged(2,2*36,false);
    /* Real pointer Main follows the original production route. The host
     * manager boundary can reject or accept, invoking actual suspend hooks
     * only on acceptance; neither is confused with ordinary HOME hiding. */
    s=serials_cases_open();
    lv_recycled_list_page_step(s->list,1);
    uint32_t rejected_first=lv_recycled_list_window(s->list)->first;
    host_navigation_current=UI_PAGE_LIST;host_navigation_target=UI_PAGE_LIST;
    host_navigation_rejected=true;host_navigation_apply_hooks=true;
    serials_cases_tap(s->main_button);
    assert(host_navigation_switches==switches+1 && host_navigation_target==UI_PAGE_MAIN);
    assert(host_navigation_current==UI_PAGE_LIST && view->serials==s && lv_obj_is_visible(s->root));
    assert(lv_recycled_list_window(s->list)->first==rejected_first && lv_recycled_list_window(s->list)->paged);
    serials_cases_b_unchanged(2,2*36,false);
    host_navigation_rejected=false;
    serials_cases_tap(s->main_button);
    assert(host_navigation_switches==switches+2 && host_navigation_target==UI_PAGE_MAIN);
    assert(host_navigation_current==UI_PAGE_MAIN && !view->serials && !visible());
    host_navigation_apply_hooks=false;host_navigation_current=UI_PAGE_LIST;
    assert(ui_page_02_list_resume() && !view->serials && timers()==baseline);
    serials_cases_b_unchanged(2,2*36,false);
    /* Entry selects the containing page even from a scrolled B bookmark;
     * Back preserves B's exact position and independent scroll mode. */
    assert(lv_recycled_list_scroll_to_index(b,15));s=serials_cases_open();
    assert(lv_recycled_list_window(s->list)->first==0);serials_cases_rows(s);serials_cases_back();
    serials_cases_b_unchanged(15,15*36,false);
    /* Paged entry chooses the containing viewer page; B stays at its own
     * containing page across Back and the next viewer is newly created. */
    lv_recycled_list_set_paged(b,true);assert(lv_recycled_list_scroll_to_index(b,25));
    assert(lv_recycled_list_window(b)->first==21);
    s=serials_cases_open();assert(lv_recycled_list_window(s->list)->paged);
    assert(lv_recycled_list_window(s->list)->first==0);
    serials_cases_b_unchanged(21,21*36,true);
    uint32_t stationary=lv_recycled_list_window(s->list)->first;
    serials_cases_drag(lv_recycled_list_object(s->list),0,-120);
    lv_timer_t *moving=recycled_cases_owned_timer(s->list);assert(!moving || moving->paused);
    assert(lv_recycled_list_window(s->list)->first==stationary && lv_recycled_list_window(s->list)->paged);
    ui_page_02_list_suspend();tick(500);
    assert(!moving || moving->paused);
    assert(ui_page_02_list_resume());
    assert(lv_recycled_list_window(s->list)->first==stationary && lv_recycled_list_window(s->list)->paged);
    lv_recycled_list_page_step(s->list,-10000);
    lv_recycled_list_page_step(s->list,1);
    uint32_t saved=lv_recycled_list_window(s->list)->first;
    ui_page_02_list_suspend();assert(view->serials==s && !lv_obj_is_visible(s->root));
    lv_timer_t *motion=recycled_cases_owned_timer(s->list);assert(!motion || motion->paused);
    serial_search_cases_store(data,74,"APP000000038",50);
    page_02_list_section_data_ready(PAGE_02_SECTION_B);tick(100);
    assert(s->count==37 && lv_recycled_list_window(s->list)->first==saved);
    assert(ui_page_02_list_resume());
    assert(view->serials==s && lv_obj_is_visible(s->root) && s->count==38);
    assert(lv_recycled_list_window(s->list)->paged && lv_recycled_list_window(s->list)->first==saved);
    serials_cases_rows(s);serials_cases_b_unchanged(21,21*36,true);
    serial_search_cases_store(data,76,"APP000000039",100);
    page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    assert(s->count==39 && lv_recycled_list_window(s->list)->first==saved);
    serials_cases_full_surface(s);
    /* A thirteenth tail note grows page depth4 to5 and moves column chunks;
     * every existing cell must rebind even when its visual group is stable. */
    serial_search_cases_store(data,78,"APP000000040",50);
    page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    assert(s->count==40 && lv_recycled_list_window(s->list)->first==saved);serials_cases_rows(s);
    serials_cases_complete_pages(s);
    /* Real report reset invalidates reused slots and resets viewer only as
     * specified, including a held input and previously nonzero bookmark. */
    lv_area_t pressed_area;lv_obj_get_coords(lv_recycled_list_object(s->list),&pressed_area);
    int pressed_x=(pressed_area.x1+pressed_area.x2)/2,pressed_y=(pressed_area.y1+pressed_area.y2)/2;
    serials_cases_pointer(pressed_x,pressed_y,true);
    serials_cases_pointer(pressed_x-100,pressed_y,true);
    counting_data_clear_serials(data);
    serial_search_cases_store(data,6,"NEW000000001",5);
    page_02_list_report_reset();serial_search_cases_flush();
    assert(view->serials==s && s->count==1 && lv_recycled_list_window(s->list)->first==0);
    serials_cases_pointer(pressed_x-100,pressed_y,false);tick(300);
    assert(lv_recycled_list_window(s->list)->first==0);
    serials_cases_rows(s);write_bmp("list-serial-table-reset");
    serials_cases_fixture(data,39);
    lv_recycled_list_page_step(s->list,1);
    assert(lv_recycled_list_window(s->list)->first==9);
    ui_page_02_list_suspend();counting_data_clear_serials(data);
    serial_search_cases_store(data,8,"HID000000001",5);
    page_02_list_report_reset();tick(100);
    assert(s->count==39 && lv_recycled_list_window(s->list)->first==9);
    assert(ui_page_02_list_resume());
    assert(s->count==1 && lv_recycled_list_window(s->list)->first==0 && lv_recycled_list_window(s->list)->paged);
    serials_cases_rows(s);
    serials_cases_back();assert(timers()==baseline);
    assert(lv_recycled_list_window(b)->first==0);
    /* Empty projection remains an accessible, scoped full-screen page. */
    counting_data_clear_serials(data);page_02_list_report_reset();serial_search_cases_flush();
    s=serials_cases_open();assert(s->count==0);
    assert(!lv_obj_has_flag(s->empty,LV_OBJ_FLAG_HIDDEN));
    serials_cases_rows(s);write_bmp("list-serial-table-empty");serials_cases_back();
    /* Both owner allocation and a later component failure leave no partial
     * root, extra timer or competing Back handler; retry uses the real path. */
    serials_cases_fixture(data,37);
    unsigned children=lv_obj_get_child_cnt(view->page);
    lv_mem_monitor_t oom_before,oom_after;lv_mem_monitor(&oom_before);
    fail_serials_owner_alloc=true;serials_cases_tap(view->section[PAGE_02_SECTION_B].details);
    assert(!fail_serials_owner_alloc && !view->serials && timers()==baseline);
    assert(lv_obj_get_child_cnt(view->page)==children);
    lv_mem_monitor(&oom_after);assert(oom_before.used_cnt==oom_after.used_cnt && oom_before.free_size==oom_after.free_size);
    fail_serials_viewport=true;serials_cases_tap(view->section[PAGE_02_SECTION_B].details);
    assert(!fail_serials_viewport && !view->serials && timers()==baseline);
    assert(lv_obj_get_child_cnt(view->page)==children);
    lv_mem_monitor(&oom_after);assert(oom_before.used_cnt==oom_after.used_cnt && oom_before.free_size==oom_after.free_size);
    s=serials_cases_open();serials_cases_rows(s);serials_cases_back();
    assert(serials_unavailable_notices==2);
    /* Failed locale rebinding keeps the prior pool hidden and an accessible
     * Back control; the next data commit retries even with identical geometry. */
    s=serials_cases_open();lv_recycled_list_t *old_list=s->list;
    ui_lang_set(LANGUAGE_CN);fail_serials_viewport=true;
    page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    assert(!fail_serials_viewport && s->failed && s->list==old_list);
    assert(lv_obj_has_flag(lv_recycled_list_object(old_list),LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(s->empty,LV_OBJ_FLAG_HIDDEN));
    assert(!strcmp(lv_label_get_text(lv_damped_button_get_label(s->back)),ui_tr("Back")));
    page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    assert(!s->failed && s->list!=old_list && !lv_obj_has_flag(lv_recycled_list_object(s->list),LV_OBJ_FLAG_HIDDEN));
    serials_cases_rows(s);
    ui_page_02_list_suspend();ui_lang_set(LANGUAGE_EN);assert(ui_page_02_list_resume());
    assert(!strcmp(lv_label_get_text(lv_damped_button_get_label(s->back)),ui_tr("Back")));
    serials_cases_back();
    /* Long model strings are a robustness fixture, beyond the current 12-byte
     * protocol field. They retain full text in a uniformly wrapped layout. */
    char long_serial[251];memset(long_serial,'W',250);long_serial[250]=0;
    serial_search_cases_store(data,0,long_serial,20);
    page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    s=serials_cases_open();assert(s->columns==1 && s->rows>=1 && s->rows<=9);
    serials_cases_rows(s);write_bmp("list-serial-table-long-model");
    assert(lv_recycled_list_scroll_to_index(s->list,30));
    unsigned long_bookmark=lv_recycled_list_window(s->list)->first;
    lv_recycled_list_t *old_single=s->list;
    counting_data_clear_serials(data);
    for(unsigned rank=0;rank<39;++rank){
        char text[16];snprintf(text,sizeof(text),"RESET%07u",rank);
        serial_search_cases_store(data,rank*2,text,20);
    }
    serials_viewport_failures=2;
    page_02_list_report_reset();serial_search_cases_flush();
    assert(serials_viewport_failures==0 && s->failed && s->reset_pending && s->list==old_single);
    assert(lv_recycled_list_window(s->list)->first==long_bookmark);
    page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    assert(!s->failed && !s->reset_pending && s->columns==3 && s->count==39);
    assert(lv_recycled_list_window(s->list)->first==0);
    serials_cases_rows(s);
    serials_cases_back();
    /* Maximum projection exercises the viewer pool and final pair, beyond
     * 255 pages, then a real mode transition discards the scoped child. */
    counting_data_clear_serials(data);
    for(unsigned i=0;i<COUNTING_DATA_MAX_ITEMS;++i){
        char text[16];snprintf(text,sizeof(text),"ZX%010u",i);
        serial_search_cases_store(data,i,text,20);
    }
    page_02_list_report_reset();serial_search_cases_flush();
    s=serials_cases_open();lv_recycled_list_page_step(s->list,100000);
    assert(s->count==10000 && ui_list_window_pages(lv_recycled_list_window(s->list))==371);
    assert(lv_recycled_list_window(s->list)->first==3330);
    serials_cases_rows(s);write_bmp("list-serial-table-10000-tail");
    counting_data_mark_multi_result(data);page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    assert(!list_monetary_result_supported());serials_cases_rows(s);
    counting_data_reset_result_scope(data);page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    assert(list_monetary_result_supported());serials_cases_rows(s);
    assert(currency_state_confirm_multi_selection());page_02_list_report_reset();serial_search_cases_flush();
    assert(!view->serials && view->multi_visible);
    assert(currency_state_leave_special_selection());counting_data_reset_result_scope(data);
    page_02_list_report_reset();serial_search_cases_flush();
    assert(!view->multi_visible);
    counting_data_clear_serials(data);page_02_list_report_reset();serial_search_cases_flush();
    /* Queued commit cannot outlive a parent that still owns a serial child. */
    s=serials_cases_open();(void)s;
    ui_frame_commit_begin_batch();page_02_list_section_mark_dirty(PAGE_02_SECTION_B);
    ui_page_02_list_destroy();ui_frame_commit_end_batch();ui_frame_commit_flush();tick(300);assert(!view);
    ui_page_02_list_create(lv_scr_act());serial_search_cases_flush();
    assert(timers()==baseline);
    puts("PASS actual List B serial viewer: native entry/swipes/Back/ESC, accepted and rejected Main routes, original NO/SN/DENOM ordered sparse/duplicate/empty records, column-major three-group table odd tail, fixed paging (vertical drags ignored), independent B bookmarks, hidden refresh, live append/reset, safe creation failure and full wrapped model text");
}


#ifdef LIST_REAL_FONTS
static void serials_cases_locale_layout(counting_sim_t *data)
{
    serials_cases_fixture(data,37);
    unsigned tested=0;
    for(size_t i=0;i<ui_lang_count();++i){
        const ui_locale_t *locale=ui_lang_at(i);
        if(!ui_lang_is_available(locale))continue;
        assert(ui_lang_prepare_resources(locale->id));
        ui_lang_set(locale->id);
        page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
        assert(view->language==locale->id);
        lv_obj_update_layout(view->page);
        lv_obj_t *entry=view->section[PAGE_02_SECTION_B].details;
        assert_caption_contained(entry);
        assert(lv_obj_get_y(entry)+(lv_obj_get_height(entry)/2)==24);
        page_02_list_serials_t *s=serials_cases_open();
        assert(s->language==locale->id);
        lv_obj_t *captions[]={s->title,s->badge_text,
            lv_damped_button_get_label(s->back),lv_damped_button_get_label(s->main_button),s->range};
        for(unsigned k=0;k<sizeof(captions)/sizeof(captions[0]);++k)label_fits(captions[k]);
        for(unsigned col=0;col<s->columns;++col)
            for(unsigned field=0;field<3;++field)label_fits(s->headers[col][field]);
        serials_cases_rows(s);
        unsigned first=lv_recycled_list_window(s->list)->first;
        serials_cases_drag(lv_recycled_list_object(s->list),0,-100);
        assert(lv_recycled_list_window(s->list)->paged && lv_recycled_list_window(s->list)->first==first);
        serials_cases_drag(lv_recycled_list_object(s->list),-320,0);
        assert(lv_recycled_list_window(s->list)->first==9);
        serials_cases_rows(s);
        serials_cases_tap(s->previous);
        assert(lv_recycled_list_window(s->list)->first==0);
        char name[80];snprintf(name,sizeof(name),"list-serial-table-locale-%s",locale->tag);write_bmp(name);
        printf("PASS List serial fixed pages: %s real fonts and single-line header/footer layout\n",locale->tag);
        serials_cases_back();
        ++tested;
    }
    ui_lang_set(LANGUAGE_EN);page_02_list_section_data_ready(PAGE_02_SECTION_B);serial_search_cases_flush();
    printf("PASS List serial locale layouts: %u released languages\n",tested);
}
#endif
