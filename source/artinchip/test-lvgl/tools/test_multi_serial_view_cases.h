/* Real MULTI protocol cache + production component + native LVGL pointer.
 * The ordinary List suite remains the reference for independent B behaviour. */
static void multi_test_raster(const char *name)
{
#ifdef LIST_REAL_FONTS
    char localized[120];snprintf(localized,sizeof(localized),"%s-locale-%u",name,(unsigned)ui_lang_get());write_bmp(localized);
#else
    write_bmp(name);
#endif
}
static unsigned multi_open_calls;
static void multi_test_open(int currency,int tab,void *ctx)
{(void)ctx;assert(currency==0&&tab==1);++multi_open_calls;}
static void multi_test_fixture(unsigned count)
{
    counting_multi_reset();counting_multi_begin(false);
    uint8_t info[16]={0xfd,0xdf,16,14,'C','N','Y',0,0,0x12,0x34,0,0,0,1};
    info[12]=count;assert(counting_multi_info(info,sizeof(info)));
    uint8_t end[13]={0xfd,0xdf,13,14,0,0,0,0,0,0,0,2};
    assert(counting_multi_info(end,sizeof(end)));
    assert(counting_multi_serial_request(0,1));
    uint8_t f[25]={0xfd,0xdf,25,13};
    assert(counting_multi_extra_reply(13,f,sizeof(f),2));
    for(unsigned i=0;i<count;i++){
        f[4]=i+1;char text[24];snprintf(text,sizeof(text),"%7uAB%010u",i%3?10:20,i+1);
        memcpy(f+5,text,19);assert(counting_multi_extra_reply(13,f,sizeof(f),3+i));
    }
    memset(f+4,255,20);assert(counting_multi_extra_reply(13,f,sizeof(f),300));
    assert(counting_multi_serials(0)->count==count);
}
static void multi_test_rows(ui_multi_detail_t *v,bool *seen)
{
    lv_obj_update_layout(v->root);
    const ui_list_window_t *window=lv_recycled_list_window(v->serial_list);
    unsigned page=ui_list_window_page_number(window)-1;
    unsigned first=page*v->serial_rows*v->serial_columns;
    unsigned remaining=LV_MIN(v->count-first,v->serial_rows*v->serial_columns);
    unsigned depth=(remaining+v->serial_columns-1)/v->serial_columns;
    lv_obj_t *viewport=lv_recycled_list_object(v->serial_list);
    for(unsigned i=0;i<lv_obj_get_child_cnt(viewport);i++){
        lv_obj_t *row=lv_obj_get_child(viewport,i);
        if(lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)||lv_obj_get_child_cnt(row)!=3)continue;
        int visual=lv_obj_get_y(row)/30;
        if(visual<0||visual>=(int)v->serial_rows)continue;
        for(unsigned col=0;col<3;col++){
            lv_obj_t *cells=lv_obj_get_child(row,col);
            unsigned offset=col*depth+visual;
            if(col>=v->serial_columns||(unsigned)visual>=depth||offset>=remaining){
                assert(lv_obj_has_flag(cells,LV_OBJ_FLAG_HIDDEN));continue;
            }
            assert(!lv_obj_has_flag(cells,LV_OBJ_FLAG_HIDDEN));
            unsigned rank=first+offset;
            const multi_serial_t *source=&counting_multi_serials(0)->rows[v->indices[rank]];
            if(seen){assert(!seen[source->number-1]);seen[source->number-1]=true;}
            char expected[40];snprintf(expected,sizeof(expected),"%u",source->number);
            assert(!strcmp(lv_label_get_text(lv_obj_get_child(cells,0)),expected));
            assert(!strcmp(lv_label_get_text(lv_obj_get_child(cells,1)),source->text));
            snprintf(expected,sizeof(expected),"%u",source->value);
            assert(!strcmp(lv_label_get_text(lv_obj_get_child(cells,2)),expected));
            for(unsigned field=0;field<3;field++){
                lv_obj_t *o=lv_obj_get_child(cells,field);label_fits(o);
                assert(lv_obj_get_style_base_dir(o,0)==LV_BASE_DIR_LTR);
                lv_area_t a,b;lv_obj_get_coords(cells,&a);lv_obj_get_coords(o,&b);
                assert(b.x1>=a.x1&&b.x2<=a.x2&&b.y1>=a.y1&&b.y2<=a.y2);
            }
            assert(lv_obj_get_style_text_align(lv_obj_get_child(cells,2),0)==LV_TEXT_ALIGN_RIGHT);
        }
    }
}
static void test_multi_serial_view_cases(void)
{
    unsigned baseline=timers();
    for(unsigned expanded=0;expanded<2;expanded++){
        multi_test_fixture(53);
        ui_multi_detail_t *v=ui_multi_detail_create(view->page,expanded,multi_test_open,NULL);assert(v);
        ui_multi_detail_visible(v,true);ui_multi_detail_select(v,0,1);tick(250);
        assert(v->serial_columns==(expanded?3:2)&&v->serial_rows==(expanded?8:5));
        assert(ui_multi_detail_scroll(v)==lv_recycled_list_object(v->serial_list));
        const ui_list_window_t *window=lv_recycled_list_window(v->serial_list);
        assert(window->paged&&window->first==0&&v->count==53);
        assert(lv_obj_get_child_cnt(v->back)==1&&lv_obj_get_width(v->back)==76);
        lv_obj_t *buttons[]={v->previous,v->next,v->details};
        for(unsigned i=1;i<3;i++)assert(lv_obj_get_style_bg_color(buttons[i],0).full==lv_color_hex(0xF5F6F7).full);
        multi_test_rows(v,NULL);multi_test_raster(expanded?"multi-list-serial-page1":"multi-main-serial-page1");
        unsigned opens=multi_open_calls;
        serials_cases_drag(ui_multi_detail_scroll(v),-220,0);
        assert(window->first==v->serial_rows&&multi_open_calls==opens);
        multi_test_rows(v,NULL);multi_test_raster(expanded?"multi-list-serial-page2":"multi-main-serial-page2");
        serials_cases_drag(ui_multi_detail_scroll(v),220,0);assert(window->first==0);
        serials_cases_drag(ui_multi_detail_scroll(v),0,-80);assert(window->first==0&&window->paged);
        serials_cases_drag(ui_multi_detail_scroll(v),10,0);assert(window->first==0);
        serials_cases_tap(v->next);assert(window->first==v->serial_rows);
        serials_cases_tap(v->previous);assert(window->first==0);
        bool seen[MULTI_SERIAL_MAX]={0};unsigned pages=ui_list_window_pages(window);
        for(unsigned page=0;page<pages;page++){
            multi_test_rows(v,seen);lv_recycled_list_page_step(v->serial_list,1);
        }
        for(unsigned i=0;i<53;i++)assert(seen[i]);
        assert(!lv_damped_button_is_enabled(v->next));
        multi_test_raster(expanded?"multi-list-serial-tail":"multi-main-serial-tail");
        lv_recycled_list_page_step(v->serial_list,-1000);
        lv_obj_update_layout(v->root);lv_area_t a;lv_obj_get_coords(ui_multi_detail_scroll(v),&a);
        int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
        serials_cases_pointer(x,y,true);serials_cases_pointer(x-100,y,true);
        lv_event_send(ui_multi_detail_scroll(v),LV_EVENT_PRESS_LOST,NULL);
        serials_cases_pointer(x-100,y,false);assert(window->first==0);
        serials_cases_pointer(x,y,true);serials_cases_pointer(x-100,y,true);
        ui_multi_detail_visible(v,false);serials_cases_pointer(x-100,y,false);
        ui_multi_detail_visible(v,true);assert(window->first==0);
        /* Actual query submission retains source numbering and record order. */
        ui_multi_detail_search(v);assert(v->keyboard);
        lv_alnum_keyboard_set_choice_mode(v->keyboard,true);
        uint32_t denominations[]={10,20},selected[]={20};
        lv_alnum_keyboard_set_choices(v->keyboard,denominations,2,selected,1);
        search_submit("",v);lv_alnum_keyboard_hide(v->keyboard);
        assert(v->count==18&&window->first==0);multi_test_rows(v,NULL);
        for(unsigned i=0;i<v->count;i++)assert(counting_multi_serials(0)->rows[v->indices[i]].number==i*3+1);
        multi_test_raster(expanded?"multi-list-serial-filtered":"multi-main-serial-filtered");
        if(!expanded){serials_cases_tap(v->details);assert(multi_open_calls==opens+1);}
        ui_multi_detail_search(v);lv_alnum_keyboard_set_choice_mode(v->keyboard,false);
        lv_alnum_keyboard_set_text(v->keyboard,"NOTFOUND");
        lv_alnum_keyboard_hide(v->keyboard);assert(v->count==18);
        search_submit("NOTFOUND",v);assert(v->count==0&&window->count==0);
        assert(!lv_damped_button_is_enabled(v->previous)&&!lv_damped_button_is_enabled(v->next));
        serials_cases_drag(ui_multi_detail_scroll(v),-220,0);assert(window->first==0);
        assert(!lv_obj_has_flag(v->empty,LV_OBJ_FLAG_HIDDEN));
        multi_test_raster(expanded?"multi-list-serial-empty":"multi-main-serial-empty");
        /* A new counting generation invalidates the selected cache and draft. */
        counting_multi_begin(false);ui_multi_detail_refresh(v);
        assert(v->selected==-1&&!v->query[0]&&v->denomination_count==0);
        assert(lv_obj_has_flag(lv_recycled_list_object(v->serial_list),LV_OBJ_FLAG_HIDDEN));
        /* Hidden destruction cancels both viewport timers and query UI. */
        ui_multi_detail_visible(v,false);ui_multi_detail_destroy(v);tick(300);assert(timers()==baseline);
    }
    counting_multi_reset();
    fail_serials_viewport=true;
    assert(!ui_multi_detail_create(view->page,false,multi_test_open,NULL));
    assert(!fail_serials_viewport&&timers()==baseline);
    puts("PASS MULTI serial Main two/List three columns: native swipe/buttons, complete ordered pages, original NO after denomination filter, cancelled/vertical/short drag, hidden resume, Detail route and timer cleanup");
}
