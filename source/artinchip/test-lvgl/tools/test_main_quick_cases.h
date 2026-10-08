#ifdef HOST_PAGER_FAILURE_INJECTION
#include "un260/lv_components/lv_content_pager.h"
static bool host_pager_create_fails;
lv_obj_t *__real_lv_content_pager_create(lv_obj_t *,lv_coord_t,lv_coord_t,lv_coord_t,lv_coord_t,uint8_t);
lv_obj_t *__wrap_lv_content_pager_create(lv_obj_t *p,lv_coord_t x,lv_coord_t y,lv_coord_t w,lv_coord_t h,uint8_t n)
{
    if(host_pager_create_fails){host_pager_create_fails=false;return NULL;}
    return __real_lv_content_pager_create(p,x,y,w,h,n);
}
#endif
#include "un260/lv_components/lv_content_pager.h"
static void quick_assert_clear_backdrop(void)
{
    assert(lv_obj_get_style_bg_opa(quick.root,0)==LV_OPA_TRANSP);
    assert(lv_obj_get_style_opa(quick.root,0)==LV_OPA_COVER);
}
static void quick_test_clear_backdrop_raster(void)
{
    assert(lv_obj_get_style_bg_opa(quick.root,0)==LV_OPA_TRANSP);
    write_bmp("quick-underlay");
    /* Compare the complete exposed strip, not just the object's style: both
     * the snapshot and the live sheet must leave Main's pixels untouched. */
    static lv_color_t underlay[1280*72];
    render();memcpy(underlay,framebuffer+328*1280,sizeof(underlay));
    lv_obj_clear_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
#ifndef HOST_SKIN_FALLBACK
    extern unsigned host_quick_captures;
    unsigned captures=host_quick_captures;
    lv_obj_clear_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);
    for(int i=0;i<=4;++i) {
        qc_position(-328+82*i);quick_assert_clear_backdrop();render();
        assert(!memcmp(underlay,framebuffer+328*1280,sizeof(underlay)));
        /* Removing dimming must not fade the sheet's text or surface. */
        /* Inner border is visible at the edge; sample untouched sheet fill. */
        if(i)assert(framebuffer[10*1280+10].full==lv_color_hex(0xF0F4F7).full);
        char name[40];snprintf(name,sizeof(name),"quick-progress-%03d",25*i);write_bmp(name);
    }
    for(int i=4;i>=0;--i) {
        qc_position(-328+82*i);quick_assert_clear_backdrop();render();
        assert(!memcmp(underlay,framebuffer+328*1280,sizeof(underlay)));
    }
    qc_position(-400);assert(quick.y==-328);quick_assert_clear_backdrop();
    qc_position(50);assert(quick.y==0);quick_assert_clear_backdrop();
    assert(host_quick_captures==captures);
    lv_obj_add_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);
#endif
    lv_obj_clear_flag(quick.sheet,LV_OBJ_FLAG_HIDDEN);render();
    assert(!memcmp(underlay,framebuffer+328*1280,sizeof(underlay)));
    qc_close_now();render();
    assert(!memcmp(underlay,framebuffer+328*1280,sizeof(underlay)));
}
static void quick_open_test(void)
{
    tap(640,8);tick(240);render();
    assert(quick.active && !quick.moving && lv_obj_is_visible(quick.sheet));
    quick_assert_clear_backdrop();assert(quick.y==0);
    assert(!editor.enabled && !editor.editing);
}
static void quick_close_test(void)
{
    assert(page_01_main_quick_request_back());tick(200);assert(!quick.active && !quick.moving);
    quick_assert_clear_backdrop();assert(quick.y==-328);
}
static void quick_flip_test(bool next)
{
    bool top_enabled=ui_notice_get_enabled(),popup_enabled=fault_popup_get_auto_enabled();
    uint8_t target=lv_quick_controls_page(&quick.controls)^1U;
    pointer(next?1050:150,110,true);
    pointer(next?550:850,110,true);
    pointer(next?550:850,110,false);
    assert(lv_content_pager_is_moving(quick.controls.pager));
    tick(320);render();
    assert(quick.active&&!quick.moving&&lv_quick_controls_page(&quick.controls)==target);
    assert(ui_notice_get_enabled()==top_enabled&&fault_popup_get_auto_enabled()==popup_enabled);
}
static void quick_clipped_background_test(void)
{
    /* Partial refresh must keep the first-column fills while their rounded
     * rectangles cross the viewport edge. Sample the clear strip above text. */
    pointer(1100,110,true);
    const int offsets[]={40,100,180,260,180,100,40,0};
    for(unsigned step=0;step<sizeof(offsets)/sizeof(offsets[0]);step++){
        pointer(1100-offsets[step],110,true);lv_obj_update_layout(lv_scr_act());lv_refr_now(NULL);
        lv_area_t viewport;lv_obj_get_coords(quick.controls.viewport,&viewport);
        for(unsigned row=0;row<2;row++){
            lv_obj_t *tile=quick.controls.tiles[quick.controls.state.order[row*4]];
            lv_area_t area;lv_obj_get_coords(tile,&area);
            int left=area.x1+14;if(left<viewport.x1)left=viewport.x1;
            int right=area.x2-14;if(right>viewport.x2)right=viewport.x2;
            assert(left<=right);int x=(left+right)/2,y=area.y1+15;
            assert(framebuffer[y*1280+x].full==lv_color_hex(0xFFFFFF).full);
        }
        if(step==2)write_bmp("quick-swipe-clipped-column");
    }
    pointer(1100,110,false);tick(320);assert(lv_quick_controls_page(&quick.controls)==0);
}
static void quick_circular_motion_test(void)
{
    quick_clipped_background_test();
    /* Both boundary directions and repeated laps use exactly nine cards. */
    for(unsigned lap=0;lap<3;lap++){
        quick_flip_test(false);quick_flip_test(true);
        quick_flip_test(true);quick_flip_test(false);
    }
    assert(lv_quick_controls_page(&quick.controls)==0);
    pointer(1050,110,true);pointer(990,110,true);tick(180);
    pointer(990,110,false);assert(lv_content_pager_is_moving(quick.controls.pager));
    tick(320);assert(lv_quick_controls_page(&quick.controls)==0);
    /* Re-grabbing the decelerating page keeps its position and consumes the
     * whole contact, so it cannot accidentally activate a moving shortcut. */
    bool top=ui_notice_get_enabled(),popup=fault_popup_get_auto_enabled();
    pointer(1050,110,true);pointer(550,110,true);pointer(550,110,false);tick(60);
    lv_obj_update_layout(lv_scr_act());lv_area_t before,after;
    lv_obj_get_coords(quick.controls.tiles[QUICK_LAYOUT],&before);
    pointer(600,110,true);lv_obj_update_layout(lv_scr_act());
    lv_obj_get_coords(quick.controls.tiles[QUICK_LAYOUT],&after);
    assert(before.x1==after.x1);pointer(600,110,false);tick(320);
    assert(top==ui_notice_get_enabled()&&popup==fault_popup_get_auto_enabled());
    lv_quick_controls_reset(&quick.controls);
    /* A closing sheet captures a settled page and cancels hidden work. */
    pointer(1050,110,true);pointer(550,110,true);pointer(550,110,false);tick(40);
    assert(lv_content_pager_is_moving(quick.controls.pager));
    quick_close_test();assert(!lv_content_pager_is_moving(quick.controls.pager));
    quick_open_test();assert(lv_quick_controls_page(&quick.controls)==0);
    pointer(1050,110,true);pointer(550,110,true);pointer(550,110,false);tick(40);
    pointer(500,140,true);pointer(500,15,true);pointer(500,15,false);tick(320);
    assert(!quick.active&&!lv_content_pager_is_moving(quick.controls.pager));
    quick_open_test();
    /* Even a very short contact may arrive as press/release only. */
    lv_point_t press={1000,110},release={940,110};
    assert(lv_quick_controls_pointer(&quick.controls,LV_EVENT_PRESSED,&press,1));
    lv_tick_inc(20);
    assert(lv_quick_controls_pointer(&quick.controls,LV_EVENT_RELEASED,&release,0));
    tick(320);assert(lv_quick_controls_page(&quick.controls)==1);
    lv_quick_controls_reset(&quick.controls);
    pointer(1000,110,true);
    for(unsigned sample=0;sample<13;sample++)pointer(1000,110,true);
    pointer(940,110,true);pointer(940,110,false);tick(320);
    assert(lv_quick_controls_page(&quick.controls)==1);
    lv_quick_controls_reset(&quick.controls);
}
static void quick_test_recovery(void)
{
    host_quick_enabled=false;
    tap(640,8);tick(240);assert(!quick.active&&!quick.moving);
    host_quick_enabled=true;
    /* A held contact may leave Main before its RELEASE is delivered. Main's
     * policy is detached while hidden, so its private state cannot rely on
     * seeing that RELEASE. The first new contact on resume must work. */
    pointer(640,8,true);
    page_01_main_suspend();
    pointer(640,8,false);
    page_01_main_resume();
    tap(640,8);tick(240);
    bool recovered=quick.active;
    printf("QUICK lifecycle first-contact recovery: %s\n",recovered?"PASS":"FAIL");
    if(quick.active)quick_close_test();

    /* The edge affordance means the top edge, not just the Menu rectangle.
     * Exercise both otherwise-unreachable halves and a center non-grip start. */
    const int xs[]={180,440,900};
    bool full_edge=true;
    for(unsigned i=0;i<sizeof(xs)/sizeof(xs[0]);++i) {
        pointer(xs[i],8,true);pointer(xs[i],130,true);pointer(xs[i],130,false);tick(220);
        bool opened=quick.active;
        printf("QUICK top-edge x=%d: %s\n",xs[i],opened?"PASS":"FAIL");
        full_edge&=opened;
        if(opened)quick_close_test();
    }
    assert(recovered && full_edge);

    /* Outside the small existing tap handle, the edge is only a candidate:
     * short taps, sideways movement and content scrolling remain native. */
    tap(440,8);assert(!quick.active && !quick.moving);
    pointer(440,8,true);pointer(540,8,true);pointer(540,8,false);tick(220);
    assert(!quick.active && !quick.moving);
    pointer(180,64,true);pointer(180,180,true);pointer(180,180,false);tick(220);
    assert(!quick.active && !quick.moving);

    /* An already-open island owns its modal surface; the drawer cannot take
     * that gesture, and closing the island restores the next fresh swipe. */
    smart_island_open_info_page();tick(400);assert(smart_island_is_expanded());
    pointer(180,8,true);pointer(180,130,true);pointer(180,130,false);tick(220);
    assert(!quick.active && !quick.moving);
    smart_island_close();tick(400);
    pointer(180,8,true);pointer(180,130,true);pointer(180,130,false);tick(220);
    assert(quick.active);quick_close_test();

    /* A foreign modal still owns the edge even though its backdrop is
     * transparent. Hidden/deleted blockers must release it immediately. */
    lv_obj_t *modal=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(modal);
    lv_obj_set_size(modal,1280,400);lv_obj_add_flag(modal,LV_OBJ_FLAG_CLICKABLE);
    pointer(900,8,true);pointer(900,130,true);pointer(900,130,false);tick(220);
    assert(!quick.active && !quick.moving);
    lv_obj_del(modal);
    pointer(900,8,true);pointer(900,130,true);pointer(900,130,false);tick(220);
    assert(quick.active);quick_close_test();

    /* Resume during the same held contact must not turn PRESSING into a
     * fresh edge pull. The following physical contact is accepted. */
    pointer(640,8,true);page_01_main_suspend();page_01_main_resume();
    pointer(640,140,true);pointer(640,140,false);tick(220);
    assert(!quick.active && !quick.moving);
    quick_open_test();quick_close_test();
    puts("PASS quick recovery: first contact after hidden release, full top-edge intent, native horizontal/content gestures, island/modal ownership, held-contact resume");
}
static void test_main_quick(void)
{
    extern unsigned host_quick_captures;
    fixture(true);currency_state_leave_special_selection();currency_state_confirm_active_code("EUR");
    ui_main_create(lv_scr_act());page_01_main_resume();device_info_init("1.0.0");
    host_gestures=true;page_01_main_layout_set_enabled(true);
    page_01_main_quick_schedule_preload();tick(220);render();
    assert(quick.root && !lv_obj_is_visible(quick.root));
#ifdef HOST_PAGER_FAILURE_INJECTION
    /* Rebuilding a retained drawer can fail after a language change. Neither
     * that resource failure nor the live-snapshot fallback may lock navigation. */
    language_t previous_language=ui_lang_get();ui_lang_set(LANGUAGE_KR);host_pager_create_fails=true;
    qc_settle(true);assert(!quick.root&&!quick.active&&!quick.moving&&!quick.navigation_blocked);
    assert(!host_pager_create_fails);
    ui_lang_set(previous_language);page_01_main_quick_schedule_preload();tick(220);render();
    assert(quick.root&&!lv_obj_is_visible(quick.root));
    /* A rebuild while already open must also release the existing navigation
     * owner and cancel its sorting draft when no pager can be allocated. */
    quick_open_test();qc_edit(true);assert(quick.arranging&&quick.navigation_blocked);
    ui_lang_set(LANGUAGE_KR);host_pager_create_fails=true;qc_settle(true);
    assert(!quick.root&&!quick.active&&!quick.moving&&!quick.arranging&&!quick.navigation_blocked);
    assert(!host_pager_create_fails&&quick.y==CLOSED_Y);
    ui_lang_set(previous_language);page_01_main_quick_schedule_preload();tick(220);render();
    assert(quick.root&&!lv_obj_is_visible(quick.root));
#endif
    quick_test_clear_backdrop_raster();
    unsigned captures=host_quick_captures, sent=protocol_calls;
    tap(640,8);
#ifndef HOST_SKIN_FALLBACK
    assert(quick.moving && lv_obj_has_flag(quick.sheet,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_is_visible(quick.surface.image));
    tick(40);
    assert(quick.y>-328 && quick.y<0);quick_assert_clear_backdrop();
#endif
    tick(240);render();assert(quick.active && protocol_calls==sent+1);
    quick_assert_clear_backdrop();assert(quick.y==0);
    assert(host_quick_captures==captures); /* warmed sheet reused, no per-frame capture */
    assert(lv_obj_get_style_border_width(quick.sheet,0)==0);assert_labels(quick.sheet);
    lv_obj_t *inner_outline=lv_obj_get_child(quick.sheet,-1);
    assert(lv_obj_get_style_border_width(inner_outline,0)==1);
    assert(lv_obj_get_style_bg_opa(inner_outline,0)==LV_OPA_TRANSP);
    assert(!lv_obj_has_flag(inner_outline,LV_OBJ_FLAG_CLICKABLE));
    assert(lv_obj_get_width(quick.sheet)==1280 && lv_obj_get_x(quick.sheet)==0 && lv_obj_get_y(quick.sheet)==0);
    assert(lv_obj_get_style_pad_left(quick.sheet,0)==0);
    assert(lv_obj_get_style_pad_top(quick.sheet,0)==0);
    assert(lv_obj_get_height(quick.sheet)==328);
    lv_obj_t *page_indicator=lv_obj_get_child(quick.controls.pager,1);lv_area_t indicator_bounds;
    lv_obj_get_coords(page_indicator,&indicator_bounds);
    assert(indicator_bounds.x1==230&&indicator_bounds.y1==279&&indicator_bounds.x2<320&&indicator_bounds.y2<311);
    assert(lv_obj_get_child_cnt(page_indicator)==2&&lv_obj_is_visible(page_indicator));
    for(unsigned i=0;i<8;i++){
        unsigned id=quick.controls.state.order[i];lv_area_t a;lv_obj_get_coords(quick.controls.tiles[id],&a);
        if(a.x1!=25+(int)(i%4)*1242/4 || a.y1!=65+(int)(i/4)*106)
            fprintf(stderr,"Quick tile %u (id %u): actual %d,%d expected %d,%d\n",i,id,a.x1,a.y1,25+(int)(i%4)*1242/4,65+(int)(i/4)*106);
        assert(a.x1==25+(int)(i%4)*1242/4 && a.y1==65+(int)(i/4)*106);
        assert(lv_area_get_width(&a)==299 && lv_area_get_height(&a)==96);
        assert(a.x2<1280 && a.y2<328);
    }
    write_bmp("quick-controls-unavailable");
    device_info_remote_versions_t versions={.main_app={2,6,19},.image_app={1,4,8}};
    device_info_confirm_remote_versions(&versions);page_01_main_quick_refresh_data(UI_DATA_TOPIC_DEVICE_VERSION);
    render();write_bmp("quick-controls");
    bool original_popup=fault_popup_get_auto_enabled();
    assert(ui_notice_get_enabled() && !strcmp(lv_quick_controls_title(QUICK_POPUP),"Popup"));
    unsigned old_navigation=pushes,old_protocol=protocol_calls;
    quick_flip_test(true);write_bmp("quick-controls-page-2");
    lv_area_t top_bounds;lv_obj_get_coords(quick.controls.tiles[QUICK_TOP_POPUP],&top_bounds);
    assert(top_bounds.x1==25&&top_bounds.y1==65&&lv_area_get_width(&top_bounds)==299&&lv_area_get_height(&top_bounds)==96);
    assert(pushes==old_navigation&&protocol_calls==old_protocol);
    click_object(quick.controls.tiles[QUICK_TOP_POPUP]);
    assert(quick.active&&!ui_notice_get_enabled()&&fault_popup_get_auto_enabled()==original_popup);
    assert(lv_obj_get_x(quick.controls.knobs[QUICK_TOP_POPUP])==4);
    quick_flip_test(false);
    click_object(quick.controls.tiles[QUICK_POPUP]);
    assert(!ui_notice_get_enabled()&&fault_popup_get_auto_enabled()!=original_popup);
    click_object(quick.controls.tiles[QUICK_POPUP]);
    quick_flip_test(true);
    host_top_popup_save_fails=true;click_object(quick.controls.tiles[QUICK_TOP_POPUP]);
    assert(!ui_notice_get_enabled()&&fault_popup_get_auto_enabled()==original_popup);
    assert(strstr(lv_label_get_text(quick.message),"Could not save"));
    host_top_popup_save_fails=false;click_object(quick.controls.tiles[QUICK_TOP_POPUP]);
    assert(quick.active&&ui_notice_get_enabled()&&fault_popup_get_auto_enabled()==original_popup);
    assert(lv_obj_get_x(quick.controls.knobs[QUICK_TOP_POPUP])==24);
    quick_flip_test(false);
    click_object(quick.controls.tiles[QUICK_LAYOUT]);assert(!page_01_main_layout_is_enabled());
    click_object(quick.controls.tiles[QUICK_GESTURES]);assert(!host_gestures);
    assert(quick.active);write_bmp("quick-controls-off");
    host_gesture_save_fails=true;click_object(quick.controls.tiles[QUICK_GESTURES]);
    assert(!host_gestures && !gesture_guide_is_open() && strstr(lv_label_get_text(quick.message),"Could not save"));
    host_gesture_save_fails=false;click_object(quick.controls.tiles[QUICK_GESTURES]);assert(host_gestures);
    tick(180);assert(gesture_guide_is_open());
    for(unsigned i=0;i<16;i++){
        lv_obj_t *overlay=lv_obj_get_child(lv_layer_top(),-1),*panel=lv_obj_get_child(overlay,0);
        assert(!lv_obj_has_flag(overlay,LV_OBJ_FLAG_SCROLLABLE));
        assert(!lv_obj_has_flag(panel,LV_OBJ_FLAG_SCROLLABLE));
        assert(lv_obj_get_scrollbar_mode(overlay)==LV_SCROLLBAR_MODE_OFF);
        assert(lv_obj_get_scrollbar_mode(panel)==LV_SCROLLBAR_MODE_OFF);
        assert(lv_obj_get_scroll_y(overlay)==0);tick(20);
    }
    assert(!quick.active && gesture_guide_is_open());
    assert(lv_anim_count_running()>0);render();write_bmp("quick-gesture-guide");
    lv_obj_t *guide_panel=lv_obj_get_child(lv_obj_get_child(lv_layer_top(),-1),0);
    lv_obj_t *guide_viewport=lv_obj_get_child(lv_obj_get_child(guide_panel,1),0);
    pointer(975,180,true);pointer(720,180,true);pointer(465,180,true);pointer(320,180,true);pointer(320,180,false);
    tick(400);assert(lv_obj_get_scroll_x(guide_viewport)>500);
    tap(937,328);tick(220);assert(!gesture_guide_is_open()); /* original Got it button */
    quick_open_test();
    click_object(quick.controls.tiles[QUICK_LAYOUT]);assert(page_01_main_layout_is_enabled());
    unsigned clear=callbacks[CB_CLEAR];tap(1210,355);tick(200);
    assert(!quick.active && callbacks[CB_CLEAR]==clear); /* outside close cannot click through */
#ifndef HOST_SKIN_FALLBACK
    /* Reversal must keep both the sheet position and the clear backdrop. */
    tap(640,8);tick(40);int mid_y=quick.y;
    assert(mid_y>-328 && mid_y<0);quick_assert_clear_backdrop();
    assert(page_01_main_quick_request_back());assert(quick.y==mid_y);quick_assert_clear_backdrop();
    tick(40);assert(quick.y<=mid_y);quick_assert_clear_backdrop();
    tick(200);assert(!quick.active && quick.y==-328);quick_assert_clear_backdrop();
#endif
    host_gestures=false;quick_open_test();quick_close_test();host_gestures=true;
    /* Native top actions still receive a normal short tap. */
    ui_main_layout_t original;ui_main_layout_default(&original);page_01_apply_customer_layout(&original);
    unsigned menu=callbacks[CB_MENU];tap(1220,30);assert(callbacks[CB_MENU]==menu+1);
    layout_hold(1220,30);pointer(1220,30,false);layout_action(1);
    tap(1220,30);assert(callbacks[CB_MENU]==++menu+1);
    pointer(1220,20,true);pointer(1220,100,true);quick_assert_clear_backdrop();assert(quick.y==-248);
    pointer(1220,60,true);quick_assert_clear_backdrop();assert(quick.y==-288);
    pointer(1220,20,true);quick_assert_clear_backdrop();assert(quick.y==-328);
    pointer(1220,20,false);tick(200);quick_assert_clear_backdrop();
    assert(!quick.active && callbacks[CB_MENU]==menu+1);
    pointer(1220,20,true);pointer(1220,140,true);pointer(1220,140,false);tick(220);
    assert(quick.active && callbacks[CB_MENU]==menu+1);
    pointer(500,80,true);pointer(500,10,true);pointer(500,10,false);tick(200);assert(!quick.active);
    quick_open_test();start_busy=true;tick(100);assert(quick.active);
    quick_close_test();quick_open_test();quick_close_test();start_busy=false;
    quick_open_test();host_fault_pending=true;tick(100);assert(quick.active);
    quick_close_test();quick_open_test();
    host_fault_showing=true;tick(100);assert(!quick.active);
    host_fault_showing=false;host_fault_pending=false;
    /* Every page retains the same card geometry. Closing from page two uses
     * that live page's snapshot, then reopening resets pagination to page one. */
    quick_open_test();
    assert(!lv_obj_has_flag(quick.controls.viewport,LV_OBJ_FLAG_SCROLLABLE));
    quick_circular_motion_test();
    quick_flip_test(true);unsigned page_captures=host_quick_captures;
    quick_close_test();quick_open_test();
    assert(lv_quick_controls_page(&quick.controls)==0);
#ifndef HOST_SKIN_FALLBACK
    assert(host_quick_captures>page_captures);
#else
    (void)page_captures;
#endif
    click_object(quick.controls.device_button);
    assert(lv_obj_is_visible(quick.controls.version_panel));write_bmp("quick-device-info");
    click_object(quick.controls.device_button);assert(!lv_obj_is_visible(quick.controls.version_panel));
#ifndef HOST_SKIN_FALLBACK
    click_object(quick.controls.device_button);
    pointer(500,55,true);pointer(500,0,true);pointer(500,0,false);tick(220);
    assert(!quick.active && quick.dirty);
    unsigned version_captures=host_quick_captures;
    quick_open_test();
    assert(host_quick_captures>version_captures);
    assert(!lv_obj_is_visible(quick.controls.version_panel));
#endif
    uint8_t original_order[QUICK_CONTROL_COUNT];memcpy(original_order,host_quick_order,sizeof(original_order));
    click_object(quick.arrange);assert(quick.arranging);
    click_object(quick.controls.tiles[QUICK_LAYOUT]);assert(quick.selected==QUICK_LAYOUT);
    bool top_before_sort=ui_notice_get_enabled();quick_flip_test(true);
    assert(quick.selected==QUICK_LAYOUT);
    click_object(quick.controls.tiles[QUICK_TOP_POPUP]);
    assert(quick.order[0]==QUICK_TOP_POPUP&&quick.order[8]==QUICK_LAYOUT&&ui_notice_get_enabled()==top_before_sort);
    assert(!memcmp(host_quick_order,original_order,sizeof(original_order)));
    click_object(quick.cancel);assert(!quick.arranging);
    quick_flip_test(false);click_object(quick.arrange);
    click_object(quick.controls.tiles[QUICK_LAYOUT]);click_object(quick.controls.tiles[QUICK_STANDBY]);
    assert(quick.order[0]==QUICK_STANDBY && quick.order[7]==QUICK_LAYOUT);
    assert(!memcmp(host_quick_order,original_order,sizeof(original_order)));write_bmp("quick-arrange");
    click_object(quick.cancel);assert(!quick.arranging && !memcmp(host_quick_order,original_order,sizeof(original_order)));
    click_object(quick.arrange);
    pointer(150,110,true);tick(360);pointer(1100,220,true);pointer(1100,220,false);
    assert(quick.arranging && quick.order[0]==QUICK_STANDBY && !quick.moving);
    host_order_save_fails=true;click_object(quick.done);
    assert(quick.arranging && !memcmp(host_quick_order,original_order,sizeof(original_order)));
    assert(strstr(lv_label_get_text(quick.message),"Could not save"));
    host_order_save_fails=false;click_object(quick.done);
    assert(!quick.arranging && host_quick_order[0]==QUICK_STANDBY);
    quick_close_test();quick_open_test();assert(quick.controls.state.order[0]==QUICK_STANDBY);
    click_object(quick.arrange);click_object(quick.controls.tiles[QUICK_LAYOUT]);
    click_object(quick.controls.tiles[QUICK_STANDBY]);tap(1000,355);tick(220);
    assert(!quick.active && host_quick_order[0]==QUICK_STANDBY);
    memcpy(host_quick_order,original_order,sizeof(original_order));
    quick_open_test();quick_close_test();
#ifndef HOST_SKIN_FALLBACK
    /* A late safety change must cancel pending navigation/tutorial, rather
     * than launching it over a fault or after leaving the page. */
    quick_open_test();
    unsigned settings_nav=pushes;
    click_object(quick.controls.tiles[QUICK_STANDBY]);host_fault_pending=true;
    tick(420);assert(pushes==settings_nav && quick.active && quick.after_close==QC_POST_NONE);quick_close_test();
    host_fault_pending=false;
    quick_open_test();host_gestures=false;qc_refresh();
    click_object(quick.controls.tiles[QUICK_GESTURES]);assert(host_gestures);
    start_busy=true;tick(220);assert(!gesture_guide_is_open() && !quick.active);start_busy=false;
#endif
    quick_open_test();
    unsigned nav=pushes;host_standby_busy=true;
    click_object(quick.controls.tiles[QUICK_STANDBY]);assert(pushes==nav && quick.active);
    host_standby_busy=false;click_object(quick.controls.tiles[QUICK_STANDBY]);tick(200);
    assert(pushes==nav+1 && destination==UI_PAGE_STANDBY);
    assert(!quick.active && counting_data_current()->total_pcs==231);
    quick_open_test();page_01_main_suspend();tick(220);assert(!quick.active && !lv_obj_is_visible(quick.root));
    page_01_main_resume();quick_open_test();
    quick_close_test();
    lv_obj_t *pure_host=lv_obj_create(lv_scr_act());
    lv_obj_set_size(pure_host,1280,400);
    lv_obj_set_pos(pure_host,0,0);
    page_01_main_quick_suspend();lv_obj_add_flag(main_page,LV_OBJ_FLAG_HIDDEN);
    host_current_page=UI_PAGE_PURE;
    page_01_main_quick_attach(pure_host,UI_PAGE_PURE);
    host_pointer_policy=page_01_main_quick_pointer;
    assert(lv_obj_get_parent(quick.grab)==pure_host);
    host_quick_enabled=false; /* Pure must keep an exit even with Main Quick disabled. */
    lv_obj_update_layout(lv_scr_act());
    assert(lv_obj_is_visible(pure_host) && qc_safe());
    tap(640,8);tick(240);assert(quick.active);
    quick_flip_test(true);quick_flip_test(false);
    pointer(500,140,true);pointer(500,15,true);pointer(500,15,false);tick(220);assert(!quick.active);
    tap(640,8);tick(240);assert(quick.active&&lv_quick_controls_page(&quick.controls)==0);
    smart_island_set_pure_count_enabled(true);
    start_busy=true;host_fault_pending=true;
    click_object(quick.controls.tiles[QUICK_PURE]);tick(220);
    start_busy=false;host_fault_pending=false;
    assert(pure_exits==1 && !quick.active && host_current_page==UI_PAGE_MAIN);
    host_current_page=UI_PAGE_PURE;
    assert(!smart_island_pure_count_is_enabled());
    assert(lv_quick_controls_execute(QUICK_PURE));
    assert(pure_exits==2 && host_current_page==UI_PAGE_MAIN);
    host_quick_enabled=true;
    page_01_main_quick_attach(main_page,UI_PAGE_MAIN);
    host_pointer_policy=page_01_main_pointer;
    lv_obj_clear_flag(main_page,LV_OBJ_FLAG_HIDDEN);
    lv_obj_del(pure_host);
    assert(lv_obj_get_parent(quick.grab)==main_page);
    smart_island_set_pure_count_enabled(false);
    quick_open_test();
    quick_close_test();quick_test_recovery();quick_open_test();
    pointer(500,80,true);pointer(500,20,true);ui_main_destroy();pointer(500,20,false);tick(220);
    assert(!quick.main && !quick.root && !quick.timer && !host_pointer_policy);
    puts("PASS quick controls: 9 identical cards in 4x2 pages, horizontal paging without action/navigation, cross-page tap and held drag sort, cancel/save failure, Main/PURE vertical close, version disclosure, gesture guide, safety cancellation, page snapshots/reset, exposed Main pixels, click shield, standby guard and teardown");
}
