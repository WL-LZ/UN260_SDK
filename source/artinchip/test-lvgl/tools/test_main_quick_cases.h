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
    static lv_color_t underlay[1280*120];
    render();memcpy(underlay,framebuffer+280*1280,sizeof(underlay));
    lv_obj_clear_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
#ifndef HOST_SKIN_FALLBACK
    extern unsigned host_quick_captures;
    unsigned captures=host_quick_captures;
    lv_obj_clear_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);
    for(int i=0;i<=4;++i) {
        qc_position(-280+70*i);quick_assert_clear_backdrop();render();
        assert(!memcmp(underlay,framebuffer+280*1280,sizeof(underlay)));
        /* Removing dimming must not fade the sheet's text or surface. */
        /* Inner border is visible at the edge; sample untouched sheet fill. */
        if(i)assert(framebuffer[10*1280+10].full==lv_color_hex(0xF6F8FA).full);
        char name[40];snprintf(name,sizeof(name),"quick-progress-%03d",25*i);write_bmp(name);
    }
    for(int i=4;i>=0;--i) {
        qc_position(-280+70*i);quick_assert_clear_backdrop();render();
        assert(!memcmp(underlay,framebuffer+280*1280,sizeof(underlay)));
    }
    qc_position(-400);assert(quick.y==-280);quick_assert_clear_backdrop();
    qc_position(50);assert(quick.y==0);quick_assert_clear_backdrop();
    assert(host_quick_captures==captures);
    lv_obj_add_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);
#endif
    lv_obj_clear_flag(quick.sheet,LV_OBJ_FLAG_HIDDEN);render();
    assert(!memcmp(underlay,framebuffer+280*1280,sizeof(underlay)));
    qc_close_now();render();
    assert(!memcmp(underlay,framebuffer+280*1280,sizeof(underlay)));
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
    quick_assert_clear_backdrop();assert(quick.y==-280);
}
static void quick_test_recovery(void)
{
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
    quick_test_clear_backdrop_raster();
    unsigned captures=host_quick_captures, sent=protocol_calls;
    tap(640,8);
#ifndef HOST_SKIN_FALLBACK
    assert(quick.moving && lv_obj_has_flag(quick.sheet,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_is_visible(quick.surface.image));
    tick(40);
    assert(quick.y>-280 && quick.y<0);quick_assert_clear_backdrop();
#endif
    tick(240);render();assert(quick.active && protocol_calls==sent+1);
    quick_assert_clear_backdrop();assert(quick.y==0);
    assert(host_quick_captures==captures); /* warmed sheet reused, no per-frame capture */
    assert(!strcmp(lv_label_get_text(quick.versions[0]),"Not received"));
    assert(!strcmp(lv_label_get_text(quick.versions[2]),"1.0.0"));
    assert(lv_obj_get_style_border_width(quick.sheet,0)==0);assert_labels(quick.sheet);
    lv_obj_t *inner_outline=lv_obj_get_child(quick.sheet,-1);
    assert(lv_obj_get_style_border_width(inner_outline,0)==2);
    assert(lv_obj_get_style_bg_opa(inner_outline,0)==LV_OPA_TRANSP);
    assert(!lv_obj_has_flag(inner_outline,LV_OBJ_FLAG_CLICKABLE));
    assert(lv_obj_get_width(quick.sheet)==1280 && lv_obj_get_x(quick.sheet)==0 && lv_obj_get_y(quick.sheet)==0);
    assert(framebuffer[0].full==lv_color_hex(0xF6F8FA).full && framebuffer[1279].full==lv_color_hex(0xF6F8FA).full);
    write_bmp("quick-controls-unavailable");
    device_info_remote_versions_t versions={.main_app={2,6,19},.image_app={1,4,8}};
    device_info_confirm_remote_versions(&versions);page_01_main_quick_refresh_data(UI_DATA_TOPIC_DEVICE_VERSION);
    assert(!strcmp(lv_label_get_text(quick.versions[0]),"2.6.19"));
    assert(!strcmp(lv_label_get_text(quick.versions[1]),"1.4.8"));
    render();write_bmp("quick-controls");
    click_object(lv_obj_get_parent(quick.switches[0]));assert(!page_01_main_layout_is_enabled());
    click_object(lv_obj_get_parent(quick.switches[1]));assert(!host_gestures);
    assert(quick.active);write_bmp("quick-controls-off");
    host_gesture_save_fails=true;click_object(lv_obj_get_parent(quick.switches[1]));
    assert(!host_gestures && !gesture_guide_is_open() && strstr(lv_label_get_text(quick.message),"Could not save"));
    host_gesture_save_fails=false;click_object(lv_obj_get_parent(quick.switches[1]));assert(host_gestures);
    tick(440);assert(!quick.active && gesture_guide_is_open());
    assert(lv_anim_count_running()>0);render();write_bmp("quick-gesture-guide");
    lv_obj_t *guide_panel=lv_obj_get_child(lv_obj_get_child(lv_layer_top(),-1),0);
    lv_obj_t *guide_viewport=lv_obj_get_child(lv_obj_get_child(guide_panel,1),0);
    pointer(975,180,true);pointer(720,180,true);pointer(465,180,true);pointer(320,180,true);pointer(320,180,false);
    tick(400);assert(lv_obj_get_scroll_x(guide_viewport)>500);
    tap(937,328);tick(220);assert(!gesture_guide_is_open()); /* original Got it button */
    quick_open_test();
    click_object(lv_obj_get_parent(quick.switches[0]));assert(page_01_main_layout_is_enabled());
    unsigned clear=callbacks[CB_CLEAR];tap(1210,316);tick(200);
    assert(!quick.active && callbacks[CB_CLEAR]==clear); /* outside close cannot click through */
#ifndef HOST_SKIN_FALLBACK
    /* Reversal must keep both the sheet position and the clear backdrop. */
    tap(640,8);tick(40);int mid_y=quick.y;
    assert(mid_y>-280 && mid_y<0);quick_assert_clear_backdrop();
    assert(page_01_main_quick_request_back());assert(quick.y==mid_y);quick_assert_clear_backdrop();
    tick(40);assert(quick.y<=mid_y);quick_assert_clear_backdrop();
    tick(200);assert(!quick.active && quick.y==-280);quick_assert_clear_backdrop();
#endif
    host_gestures=false;quick_open_test();quick_close_test();host_gestures=true;
    /* Native top actions still receive a normal short tap. */
    ui_main_layout_t original;ui_main_layout_default(&original);page_01_apply_customer_layout(&original);
    unsigned menu=callbacks[CB_MENU];tap(1220,30);assert(callbacks[CB_MENU]==menu+1);
    layout_hold(1220,30);pointer(1220,30,false);layout_action(1);
    tap(1220,30);assert(callbacks[CB_MENU]==++menu+1);
    pointer(1220,20,true);pointer(1220,100,true);quick_assert_clear_backdrop();assert(quick.y==-200);
    pointer(1220,60,true);quick_assert_clear_backdrop();assert(quick.y==-240);
    pointer(1220,20,true);quick_assert_clear_backdrop();assert(quick.y==-280);
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
    /* Gear opens the existing settings page, never the standby preview. */
    quick_open_test();unsigned settings_nav=pushes;
    host_standby_busy=true;tap(838,137);assert(quick.active && pushes==settings_nav);
    host_standby_busy=false;
    pointer(838,137,true);render();
    lv_point_t settings_point={838,137};
    lv_obj_t *settings_button=lv_indev_search_obj(quick.root,&settings_point);
    assert(settings_button && lv_obj_has_state(settings_button,LV_STATE_PRESSED));
    assert(lv_obj_get_style_bg_color(settings_button,0).full==lv_color_hex(0xD7E5F6).full);
    write_bmp("quick-settings-pressed");pointer(838,137,false);
    tick(220);assert(!quick.active && pushes==settings_nav+1 && destination==UI_PAGE_STANDBY_SETTING);
    assert(quick.after_close==QC_POST_NONE);
#ifndef HOST_SKIN_FALLBACK
    /* A late safety change must cancel pending navigation/tutorial, rather
     * than launching it over a fault or after leaving the page. */
    quick_open_test();settings_nav=pushes;tap(838,137);host_fault_pending=true;
    tick(220);assert(pushes==settings_nav && !quick.active && quick.after_close==QC_POST_NONE);
    host_fault_pending=false;
    quick_open_test();host_gestures=false;qc_refresh();
    click_object(lv_obj_get_parent(quick.switches[1]));assert(host_gestures);
    start_busy=true;tick(220);assert(!gesture_guide_is_open() && !quick.active);start_busy=false;
#endif
    quick_open_test();unsigned nav=pushes;host_standby_busy=true;tap(750,196);assert(pushes==nav && quick.active);
    host_standby_busy=false;tap(750,196);tick(200);assert(pushes==nav+1 && destination==UI_PAGE_STANDBY);
    assert(!quick.active && counting_data_current()->total_pcs==231);
    quick_open_test();page_01_main_suspend();tick(220);assert(!quick.active && !lv_obj_is_visible(quick.root));
    page_01_main_resume();quick_open_test();
    quick_close_test();quick_test_recovery();quick_open_test();
    pointer(500,80,true);pointer(500,20,true);ui_main_destroy();pointer(500,20,false);tick(220);
    assert(!quick.main && !quick.root && !quick.timer && !host_pointer_policy);
    puts("PASS quick controls: standby-settings shortcut/press feedback/safety cancellation, original animated gesture guide on successful enable only, unchanged exposed Main pixels throughout open/reverse/close, transparent click shield, opaque sheet, clamped endpoints, whole-sheet snapshots, warm reuse, native taps, reverse/up/outside close, no click-through, switches/failure, real versions, standby guards, faults/count and teardown");
}
