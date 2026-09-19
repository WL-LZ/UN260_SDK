/* Real LVGL/footer source plus sparse raw input; no replacement UI widgets. */
static lv_color_t footer_expected[2][450*44];
static void footer_pixels(bool capture)
{
    lv_obj_t *groups[]={s_bottom_area_a,s_bottom_area_c};
    render();
    for(unsigned i=0;i<2;++i) {
        lv_area_t a;lv_obj_get_coords(groups[i],&a);
        assert(lv_area_get_width(&a)==(i?450:435) && lv_area_get_height(&a)==44);
        int w=lv_area_get_width(&a)-48;
        for(int y=0;y<36;++y) {
            lv_color_t *actual=framebuffer+(a.y1+4+y)*1280+a.x1+24;
            if(capture) memcpy(footer_expected[i]+y*w,actual,w*sizeof(lv_color_t));
            else assert(!memcmp(footer_expected[i]+y*w,actual,w*sizeof(lv_color_t)));
        }
    }
}
static void assert_footer_positions(bool swapped)
{
    int a=swapped?814:10,c=swapped?10:799;
    assert(lv_obj_get_x(s_bottom_area_a)==a && lv_obj_get_x(s_bottom_area_c)==c);
    assert(lv_obj_get_x(s_bottom_a_btn_mode)==a && lv_obj_get_x(s_bottom_a_btn_add)==a+91);
    assert(lv_obj_get_x(s_bottom_a_btn_work)==a+195 && lv_obj_get_x(s_bottom_a_btn_fo)==a+314);
    assert(lv_obj_get_x(s_bottom_c_btn_batch)==c && lv_obj_get_x(s_bottom_c_btn_speed)==c+216);
    assert(lv_obj_get_x(s_bottom_c_box_cfd)==c+335);
    assert(lv_obj_get_x(s_bottom_area_b)==492 && lv_obj_get_width(s_bottom_area_b)==261);
    assert((swapped?c+450:a+435)<492 && (swapped?a:c)>753);
}
static void assert_selected(unsigned source,int target)
{
    assert(editor.source==(int)source && editor.target==target);
    assert(lv_obj_get_style_border_color(editor.marks[source],0).full==lv_color_hex(0x23864B).full);
    assert(lv_obj_get_style_border_width(editor.marks[source],0)==3);
    for(unsigned i=0;i<group_count(editor.group);++i) if(i!=source) {
        assert(lv_obj_get_style_border_color(editor.marks[i],0).full==lv_color_hex((int)i==target?0x125FCB:0x6D97C6).full);
        assert(lv_obj_get_style_border_width(editor.marks[i],0)==((int)i==target?4:2));
    }
}
static void test_main_footer_layout(void)
{
    ui_main_layout_default(&host_saved_layout);page_01_main_layout_set_enabled(true);
    assert(currency_state_confirm_active_code("EUR"));
    assert(currency_state_leave_special_selection());
    counting_data_reset_result_scope(counting_data_mutable());fixture(true);
    start_busy=false;ui_main_create(lv_scr_act());smart_island_restore_idle();tick(500);render();
    unsigned clicks[16];memcpy(clicks,callbacks,sizeof(clicks));unsigned saves=host_layout_saves;
    footer_pixels(true);assert_footer_positions(false);

    /* Long-hold ADD selects the whole footer, never toggles ADD. */
    layout_hold(150,370);assert(editor.group==3);assert_selected(0,-1);
    assert(!lv_obj_has_state(s_bottom_a_btn_add,LV_STATE_PRESSED));
    pointer(150,370,false);tick(300);assert_selected(0,-1);
    lv_area_t toolbar;lv_obj_get_coords(editor.bar,&toolbar);
    assert(toolbar.y2<348 && toolbar.x1>108 && toolbar.x2<1168);
    assert(lv_obj_get_style_border_width(editor.bar,0)==2);
    write_bmp("footer-selected-green");
    tap(950,370);assert_selected(1,-1); /* Tap selects, never swaps. */
    assert(!editor.draft.footer_swapped);
    tap(700,200);assert(editor.group==2);assert_selected(1,-1);
    tap(150,370);assert(editor.group==3);assert_selected(0,-1);
    tap(470,340);assert_selected(0,-1); /* Blank keeps selection. */
    pointer(150,370,true);pointer(950,370,true);assert_selected(0,1);
    write_bmp("footer-target-blue");
    pointer(620,370,false);assert_selected(0,-1); /* Outside lift removes hover. */
    assert(!editor.draft.footer_swapped);
    layout_drag(150,370,1200,280);assert(!editor.draft.footer_swapped); /* Other group. */
    layout_drag(150,370,950,370);assert(editor.draft.footer_swapped);
    assert_selected(1,-1);assert_footer_positions(true);
    write_bmp("footer-swapped-draft");
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0 && host_layout_saves==saves);
    layout_action(1);assert_footer_positions(false);footer_pixels(false);

    layout_hold(150,370);pointer(950,370,true);pointer(950,370,false);
    layout_done();tick(300);assert(host_saved_layout.footer_swapped && host_layout_saves==saves+1);
    assert_footer_positions(true);footer_pixels(false);write_bmp("footer-swapped-saved");
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    /* Every moved function still sends its own callback; CFD stays read-only. */
    lv_obj_t *buttons[]={s_bottom_a_btn_mode,s_bottom_a_btn_add,s_bottom_a_btn_work,
        s_bottom_a_btn_fo,s_bottom_c_btn_batch,s_bottom_c_btn_speed};
    const unsigned actions[]={CB_BOTTOM_MODE,CB_ADD,CB_WORK,CB_FO,CB_BATCH,CB_SPEED};
    for(unsigned i=0;i<6;++i) { click_object(buttons[i]);assert(callbacks[actions[i]]==++clicks[actions[i]]); }
    click_object(s_bottom_c_box_cfd);assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    tick(300);footer_pixels(false);

    /* Disabling rolls back only the draft, retaining the saved footer swap. */
    layout_hold(950,370);pointer(150,370,true);pointer(150,370,false);
    assert(!editor.draft.footer_swapped);
    pointer(150,370,true);page_01_main_layout_set_enabled(false);
    assert(!page_01_main_layout_is_enabled() && !editor.editing && lv_obj_has_flag(editor.overlay,LV_OBJ_FLAG_HIDDEN));
    pointer(950,370,true);pointer(950,370,false);assert_footer_positions(true);
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0 && host_layout_saves==saves+1);
    pointer(1210,280,true);tick(700);assert(!editor.editing);pointer(1210,280,false);
    assert(callbacks[CB_CLEAR]==++clicks[CB_CLEAR]); /* Normal click is not disabled. */
    page_01_main_suspend();assert(page_01_main_resume());render();
    assert(!page_01_main_layout_is_enabled());assert_footer_positions(true);
    ui_main_destroy();ui_main_create(lv_scr_act());tick(400);render();
    assert(!page_01_main_layout_is_enabled());assert_footer_positions(true);footer_pixels(false);

    /* OFF/ON during a candidate press cannot turn the held contact into a
     * new editor entry or leak its original command on release. */
    page_01_main_layout_set_enabled(true);
    pointer(950,370,true);tick(200);page_01_main_layout_set_enabled(false);
    assert(!editor.editing);page_01_main_layout_set_enabled(true);tick(800);
    assert(!editor.editing);pointer(950,370,false);
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    layout_hold(950,370);pointer(950,370,false);layout_action(1);
    assert_footer_positions(true);

    /* Footer editing also works in MUL, where the blue panels are absent. */
    assert(currency_state_confirm_multi_selection());ui_refresh_main_page();render();
    layout_hold(100,370);pointer(950,370,true);pointer(950,370,false);
    assert(!editor.draft.footer_swapped);layout_action(1);assert_footer_positions(true);
    /* A fault while dragging restores the saved positions, then drains lift. */
    layout_hold(100,370);pointer(950,370,true);host_fault_pending=true;tick(80);
    assert(!editor.editing);host_fault_pending=false;pointer(950,370,false);assert_footer_positions(true);
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    layout_hold(100,370);pointer(100,370,false);layout_action(0);
    assert(!editor.draft.footer_swapped);layout_action(1);assert_footer_positions(true);
    layout_hold(100,370);pointer(100,370,false);layout_action(0);layout_done();
    assert(!host_saved_layout.footer_swapped);assert_footer_positions(false);
    ui_main_destroy();tick(300);page_01_main_layout_set_enabled(false);
    assert(!page_01_main_layout_is_enabled());page_01_main_layout_set_enabled(true);
    puts("PASS footer group: native dimensions/pixels, stationary hold, selection/hover/drop, cancel/save/reset, callbacks, MUL/faults and feature OFF/ON lifetime");
}
