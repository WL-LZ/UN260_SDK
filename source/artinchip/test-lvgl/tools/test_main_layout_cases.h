/* Included in the real-LVGL Main harness; uses production editor and widgets. */
static void layout_hold(int x,int y)
{
    pointer(x,y,true);tick(700);
    assert(editor.editing);
    for(unsigned i=0;i<7;++i)
        assert(!lv_obj_has_state(editor.items[i],LV_STATE_PRESSED));
}
static void layout_drag(int x,int y,int tx,int ty)
{ pointer(x,y,true);pointer(tx,ty,true);pointer(tx,ty,false);render(); }
static void layout_action(unsigned index) { click_object(editor.actions[index]); }
static void layout_done(void) { layout_action(2);assert(!editor.editing);render(); }
static void assert_skin_positions(void)
{
    for(unsigned i=0;i<6;++i) {
        lv_dma_static_skin_t *skin=i<3?&s_main_action_skins[i]:&s_main_detail_tab_skins[i-3];
        if(!skin->image) continue;
        lv_area_t a,b;lv_obj_get_coords(skin->source,&a);lv_obj_get_coords(skin->image,&b);
        assert(a.x1+a.x2==b.x1+b.x2 && a.y1+a.y2==b.y1+b.y2);
        assert(lv_obj_get_index(skin->image)<lv_obj_get_index(skin->source));
        assert(!lv_obj_has_flag(skin->image,LV_OBJ_FLAG_CLICKABLE));
    }
}

/* Compare actual rendered icon/label/fill pixels by object identity. The
 * translucent corners lie over a gradient, so compare the opaque interior. */
static void test_layout_identity_pixels(void)
{
    static lv_color_t expected[7][80*90];
    extern unsigned host_skin_captures;
    unsigned captures=host_skin_captures;
    for(unsigned i=0;i<7;++i) {
        lv_area_t a;lv_obj_get_coords(editor.items[i],&a);
        int w=lv_area_get_width(&a)-32,h=lv_area_get_height(&a)-32;
        for(int y=0;y<h;++y)
            memcpy(expected[i]+y*w,framebuffer+(a.y1+16+y)*1280+a.x1+16,w*sizeof(lv_color_t));
    }
    unsigned cases=0;
    for(int a=0;a<4;++a) for(int b=0;b<4;++b) for(int c=0;c<4;++c) for(int d=0;d<4;++d) {
        if(a==b || a==c || a==d || b==c || b==d || c==d)continue;
        for(int mirror=0;mirror<2;++mirror) for(int footer=0;footer<2;++footer) {
            ui_main_layout_t layout=editor.saved;
            layout.left[0]=a;layout.left[1]=b;layout.left[2]=c;layout.left[3]=d;
            static const uint8_t right[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
            memcpy(layout.right,right[cases%6],3);layout.mirrored=mirror;
            layout.footer_swapped=footer;
            page_01_apply_customer_layout(&layout);render();assert_skin_positions();
            for(unsigned i=0;i<7;++i) {
                lv_area_t area;lv_obj_get_coords(editor.items[i],&area);
                int w=lv_area_get_width(&area)-32,h=lv_area_get_height(&area)-32;
                for(int y=0;y<h;++y)
                    assert(!memcmp(expected[i]+y*w,framebuffer+(area.y1+16+y)*1280+area.x1+16,w*sizeof(lv_color_t)));
            }
            ++cases;
        }
    }
    assert(cases==96 && captures==host_skin_captures);
    page_01_apply_customer_layout(&editor.saved);render();
    puts("PASS 96 layouts: every left/right permutation, both panel/footer positions, identical action pixels and no snapshot allocation during moves");
}
static void test_main_layout(void)
{
    page_01_main_layout_set_enabled(true);
    ui_main_layout_default(&host_saved_layout);
    assert(currency_state_confirm_active_code("EUR"));
    counting_data_reset_result_scope(counting_data_mutable());fixture(true);
    start_busy=false;ui_main_create(lv_scr_act());render();smart_island_restore_idle();tick(500);
    unsigned clicks[16];memcpy(clicks,callbacks,sizeof(clicks));unsigned ps=pushes;
    write_bmp("layout-default");
    render();test_layout_identity_pixels();
    /* Reproduce the customer's exact sequence with sparse hardware frames. */
    layout_hold(1210,280);pointer(1210,280,false);
    for(unsigned repeat=0;repeat<3;++repeat) tap(1210,280);
    write_bmp("layout-clear-tapped");
    assert(!lv_obj_has_state(page_01_main_find_obj("esc_btn"),LV_STATE_PRESSED));
    layout_action(1);assert(!editor.editing);
    layout_hold(1210,280);pointer(1210,175,true);pointer(1210,175,false);
    layout_done();tick(300);render();write_bmp("layout-clear-start-swapped");
    assert_skin_positions();
    for(unsigned i=0;i<4;++i) {
        layout_hold(50,44+82*i);pointer(50,44+82*i,false);layout_action(1);
    }
    layout_hold(400,180);pointer(400,180,false);layout_action(1);
    layout_hold(1210,280);pointer(1210,280,false);layout_action(0);layout_done();
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    /* Short taps remain business actions; no storage, no edit. */
    tap(1200,60);assert(callbacks[CB_MENU]==++clicks[CB_MENU] && !editor.editing);
    /* Long-press START must never count, even when released without movement. */
    layout_hold(1210,175);pointer(1210,175,false);
    assert(callbacks[CB_START]==clicks[CB_START]);
    write_bmp("layout-edit-right");
    layout_drag(1210,175,1210,60);
    assert(editor.draft.right[0]==1 && lv_obj_get_y(page_01_main_find_obj("start_btn"))==12);
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    /* Cross-group drop does not swap or invoke anything. */
    ui_main_layout_t draft=editor.draft;
    layout_drag(1210,65,50,45);assert(ui_main_layout_equal(&draft,&editor.draft));
    pointer(1210,65,true);pointer(1210,175,true);pointer(800,370,false);
    assert(ui_main_layout_equal(&draft,&editor.draft)); /* Lift outside last target. */
    layout_action(1);assert(!editor.editing);
    assert(lv_obj_get_y(page_01_main_find_obj("start_btn"))==123);
    /* Blue preserves intrinsic dimensions and moves the whole detail tree. */
    layout_hold(400,180);pointer(820,180,true);write_bmp("layout-edit-blue-target");
    pointer(820,180,false);render();
    assert(editor.draft.mirrored && lv_obj_get_x(s_summary_card)==674);
    assert(lv_obj_get_x(s_detail_card)==108 && lv_obj_get_width(s_detail_card)==554);
    assert(lv_obj_get_width(s_summary_card)==482 && lv_obj_get_x(detail_view->root)==126);
    assert_skin_positions();
    assert(lv_obj_get_x(s_detail_btn_a)==126 && lv_obj_get_x(s_detail_btn_c)==472);
    assert_labels(main_page);
    write_bmp("layout-edit-blue-swapped");
    /* All left and right positions can swap; no geometry changes. */
    layout_drag(50,44,50,290);
    layout_drag(1200,175,1200,60);
    assert(editor.draft.left[0]==3 && editor.draft.right[0]==1);
    unsigned saves=host_layout_saves;
    host_save_fails=true;layout_action(2);
    assert(editor.editing && host_layout_saves==saves+1);
    assert(strstr(lv_label_get_text(editor.hint),"Could not save"));
    host_save_fails=false;layout_done();
    assert(host_layout_saves==saves+2 && host_saved_layout.mirrored);
    write_bmp("layout-custom-saved");
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0 && pushes==ps);
    /* Transparent pull-down hotzone must forward the MOVED top action. */
    tap(1200,28);assert(callbacks[CB_START]==++clicks[CB_START]);
    tap(50,45);assert(callbacks[CB_PRINT]==++clicks[CB_PRINT]);
    click_object(s_detail_btn_b);assert(s_detail_section==PAGE_01_DETAIL_SECTION_B);
    /* Rebuild and resume retain the chosen layout. */
    ui_main_destroy();ui_main_create(lv_scr_act());render();
    assert(lv_obj_get_x(s_summary_card)==674 && lv_obj_get_y(page_01_main_find_obj("start_btn"))==12);
    static const char *const left_names[]={"mode_btn","setting_btn","list_btn","print_btn"};
    static const char *const right_names[]={"menu_btn","start_btn","esc_btn"};
    for (unsigned i=0;i<4;++i) { click_object(page_01_main_find_obj(left_names[i]));assert(callbacks[i]==++clicks[i]); }
    for (unsigned i=0;i<3;++i) { click_object(page_01_main_find_obj(right_names[i]));assert(callbacks[CB_MENU+i]==++clicks[CB_MENU+i]); }
    page_01_main_suspend();assert(!editor.enabled && !host_pointer_policy);
    assert(page_01_main_resume());render();assert(editor.enabled);
    /* Second contact consumes the drag and rolls back unsaved changes. */
    layout_hold(50,44);pointer(50,130,true);pointer(50,130,false);
    pointer(50,130,true);
    lv_point_t two={55,140};assert(host_pointer_policy(lv_indev_get_next(NULL),LV_EVENT_PRESSING,&two,2));
    assert(!editor.editing && ui_main_layout_equal(&editor.draft,&editor.saved));
    pointer(50,130,false);assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    /* Hardware count beginning while held cancels safely. */
    layout_hold(50,44);start_busy=true;tick(80);
    assert(!editor.editing);pointer(50,44,false);start_busy=false;
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    /* A modal blocks long-press-through. */
    lv_obj_t *modal=lv_obj_create(lv_layer_top());lv_obj_set_pos(modal,0,0);lv_obj_set_size(modal,1280,400);
    render();pointer(50,44,true);tick(700);assert(!editor.editing);pointer(50,44,false);lv_obj_del(modal);
    /* Faults/transitions while dragging cancel the draft and drain the same
     * physical contact. The following new press must still work. */
    bool *blocked[]={&host_fault_pending,&host_fault_showing,&host_transitioning};
    for(unsigned i=0;i<3;++i) {
        layout_hold(50,44);pointer(50,130,true);
        *blocked[i]=true;tick(80);assert(!editor.editing);
        *blocked[i]=false;pointer(50,130,false);
        assert(ui_main_layout_equal(&editor.draft,&editor.saved));
        layout_hold(50,44);pointer(50,44,false);layout_action(1);
    }
    assert(memcmp(clicks,callbacks,sizeof(clicks))==0);
    /* Early movement remains a normal detail scroll, not rearrangement. */
    pointer(360,250,true);pointer(360,220,true);tick(700);assert(!editor.editing);pointer(360,220,false);
    /* No blue targets in MULTI; side buttons still editable. */
    assert(currency_state_confirm_multi_selection());ui_refresh_main_page();render();
    pointer(400,180,true);tick(700);assert(!editor.editing);pointer(400,180,false);
    layout_hold(50,45);pointer(50,45,false);assert(editor.editing);
    layout_action(1);assert(!editor.editing);
    assert(currency_state_confirm_active_code("EUR"));counting_data_reset_result_scope(counting_data_mutable());
    ui_refresh_main_page();render();assert(lv_obj_get_x(s_summary_card)==674);
    /* Reset is a draft; Cancel returns to saved customization. */
    layout_hold(50,45);pointer(50,45,false);layout_action(0);assert(!editor.draft.mirrored);
    layout_action(1);assert(editor.draft.mirrored && !editor.editing);
    layout_hold(50,45);pointer(50,45,false);layout_action(0);layout_done();
    assert(!host_saved_layout.mirrored && lv_obj_get_y(page_01_main_find_obj("print_btn"))==258);
    /* Destroy while editing: no callback/timer retains deleted objects. */
    layout_hold(50,45);ui_main_destroy();pointer(50,45,false);tick(100);
    assert(!editor.root && !host_pointer_policy);
    puts("PASS Main layout: long-press capture, same-group swap, native geometry, moved hit targets, draft/save failure, modal/count/multi-touch cancellation and lifetime");
}
