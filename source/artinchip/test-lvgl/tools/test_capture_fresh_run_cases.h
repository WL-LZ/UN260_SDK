/* Gate/lifecycle regressions exercise the real page, modal and circular pager.
 * Count-handler publication is separately checked against the actual parsers. */
static void capture_denied(unsigned index,unsigned expected_sends){
 capture_click_source(index);assert(sends==expected_sends);
 assert(settings_detail_overlay_is_open());assert(label_find(lv_scr_act(),"Run one note on this page before capturing."));
 click("OK");assert(!settings_detail_overlay_is_open());
}
static void capture_new_count(uint32_t baseline,uint16_t issue,bool add){
 lv_tick_inc(1);ui_page_dynamic_capture_on_count_start(baseline,issue,add);
}
static void capture_finish_image_direct(uint8_t source){
 image_header(source,5,3);unsigned n=image_data_reply_scanline_count(source,5,3);
 for(unsigned line=1;line<=n;line++)image_scanline(source,line,5,3);
 image_end();assert(test_notice_kind==UI_NOTICE_SUCCESS);image_finish_feedback();
}
static void capture_fresh_run_regression(void){
 current_page=UI_PAGE_IMAGE_GET;lv_tick_inc(3001);uint64_t entry=lv_tick_get();ui_page_dynamic_capture_create(lv_scr_act());unsigned before=sends;
 test_notice_enabled=false;lv_obj_update_layout(lv_scr_act());lv_area_t first;lv_obj_get_coords(capture_source_button(0),&first);
 lv_point_t tap={(first.x1+first.x2)/2,(first.y1+first.y2)/2};
 assert(capture_pointer_policy(NULL,LV_EVENT_PRESSED,&tap,1));
 assert(capture_pointer_policy(NULL,LV_EVENT_RELEASED,&tap,0));assert(sends==before&&settings_detail_overlay_is_open());
 assert(test_notice_dialog_holds==1);unsigned roots=lv_obj_get_child_cnt(lv_scr_act());
 for(unsigned i=0;i<16;i++){capture_click_source(i);assert(sends==before&&lv_obj_get_child_cnt(lv_scr_act())==roots);}
 for(unsigned i=0;i<16;i++)assert(lv_obj_get_style_bg_color(capture_source_button(i),0).full==lv_color_hex(0xF4F6F7).full);
 snapshot("capture-before-run-popup");lv_point_t popup={600,200};
 assert(!capture_pointer_policy(NULL,LV_EVENT_PRESSED,&popup,1));click("OK");
 assert(!settings_detail_overlay_is_open()&&!test_notice_dialog_holds);test_notice_enabled=true;
 capture_selected_geometry=false;assert(lv_content_pager_set_active(capture_source_pager(),0,false));capture_pointer_swipe(1);assert(lv_content_pager_get_active(capture_source_pager())==2);
 capture_pointer_swipe(-1);assert(lv_content_pager_get_active(capture_source_pager())==0);
 /* Global history/old END and RX received before this page do not prepare it. */
 ++count_end_generation;ui_page_dynamic_capture_poll(lv_tick_get());ui_page_dynamic_capture_on_count_info(99,3,2);
 capture_denied(0,before);uint64_t old=entry-1;
 protocol_request_result_context_begin(old);ui_page_dynamic_capture_on_count_start(100,3,false);
 ui_page_dynamic_capture_on_count_info(101,3,1);ui_page_dynamic_capture_on_count_info(0,0,2);protocol_request_result_context_end();
 capture_denied(6,before);assert(label_find(lv_scr_act(),"Place one note in the hopper."));
 current_page=UI_PAGE_MAIN;capture_new_count(0,0,false);ui_page_dynamic_capture_on_count_info(1,0,1);ui_page_dynamic_capture_on_count_info(0,0,2);
 current_page=UI_PAGE_IMAGE_GET;capture_denied(0,before);
 gate=false;capture_new_count(0,0,false);gate=true;ui_page_dynamic_capture_on_count_info(1,0,2);capture_denied(0,before);
 capture_new_count(99,4,false);ui_page_dynamic_capture_on_count_info(0,0,0);ui_page_dynamic_capture_on_count_info(0,0,1);
 ui_page_dynamic_capture_on_count_info(0,0,2);capture_denied(0,before);
 /* Unchanged ADD totals remain unprepared even after both zero status0 and
  * zero status1 frames. Reject counters are independent of accepted counts. */
 capture_new_count(100,2,true);ui_page_dynamic_capture_on_count_info(0,0,0);ui_page_dynamic_capture_on_count_info(0,0,1);
 ui_page_dynamic_capture_on_count_info(100,2,1);ui_page_dynamic_capture_on_count_info(0,0,2);capture_denied(0,before);
 capture_new_count(100,2,true);ui_page_dynamic_capture_on_count_info(0,0,0);ui_page_dynamic_capture_on_count_info(0,0,1);
 ui_page_dynamic_capture_on_count_info(0,0,2);capture_denied(0,before);
 capture_new_count(100,2,true);ui_page_dynamic_capture_on_count_info(101,2,1);
 lv_event_send(capture_source_button(0),LV_EVENT_CLICKED,NULL);assert(sends==before); /* RUN still active */
 ui_page_dynamic_capture_on_count_info(0,0,2);assert(!label_find(lv_scr_act(),"Place one note in the hopper."));
 capture_click_source(0);assert(sends==++before&&sent_cmd==0x47);capture_finish_image_direct(1);
 capture_click_source(6);assert(sends==++before&&sent_cmd==0x48);wave_wire_channel(1,120);wave_end();image_finish_feedback();
 capture_click_source(0);capture_click_source(6);capture_click_source(0);assert(sends==before);image_native_base(1);
 /* A true new RUN invalidates complete caches once, rather than serving the
  * previous note. A duplicate accepted START during that RUN cannot erase it. */
 capture_new_count(101,2,true);ui_page_dynamic_capture_on_count_info(102,2,1);
 ui_page_dynamic_capture_on_count_start(102,2,true);ui_page_dynamic_capture_on_count_info(0,0,2);
 capture_click_source(0);assert(sends==++before);capture_finish_image_direct(1);
 capture_click_source(1);assert(sends==++before); /* no first header */
 lv_tick_inc(5001);assert(ui_page_dynamic_capture_poll(lv_tick_get()));
 assert(!ui_page_dynamic_capture_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 capture_selected_geometry=false;lv_tick_inc(3001);ui_page_dynamic_capture_poll(lv_tick_get());
 capture_click_source(0);assert(sends==before);image_native_base(1); /* completed same-run cache survives failure */
 capture_denied(1,before);capture_new_count(102,2,true);ui_page_dynamic_capture_on_count_info(103,2,1);ui_page_dynamic_capture_on_count_info(0,0,2);
 capture_click_source(1);assert(sends==++before);capture_finish_image_direct(2);
 /* Reject-only run and an actual positive reset both prove a new note. */
 capture_new_count(103,2,true);ui_page_dynamic_capture_on_count_info(103,3,1);ui_page_dynamic_capture_on_count_info(0,0,2);
 capture_click_source(6);assert(sends==++before);wave_wire_channel(1,40);wave_end();image_finish_feedback();
 capture_new_count(103,3,true);ui_page_dynamic_capture_on_count_info(1,0,1);ui_page_dynamic_capture_on_count_info(0,0,2);
 capture_click_source(0);assert(sends==++before);image_header(1,5,3);image_scanline(1,1,5,3);
 /* Stream stall preserves navigation until its exact FF, even past 120s.
  * Wrong command/end and a new RUN cannot release or free its owned buffer. */
 lv_tick_inc(5001);assert(ui_page_dynamic_capture_poll(lv_tick_get()));assert(ui_page_dynamic_capture_is_capturing()&&capture_nav_locks&&capture_gesture_locks);
 assert(label_find(lv_scr_act(),"Transfer interrupted. Waiting for the end frame."));capture_assert_only_progress(-1);
 unsigned pop_before=pops;click("Back");assert(pops==pop_before);capture_assert_sources_locked(true);
 uint8_t end[]={0xff};ui_page_dynamic_capture_on_frame(0x48,end,1);assert(ui_page_dynamic_capture_is_capturing());
 capture_new_count(1,0,false);ui_page_dynamic_capture_on_count_info(1,0,1);ui_page_dynamic_capture_on_count_info(0,0,2);
 lv_tick_inc(120001);assert(!ui_page_dynamic_capture_poll(lv_tick_get()));assert(ui_page_dynamic_capture_is_capturing());
 image_end();assert(!ui_page_dynamic_capture_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 assert(!label_find(lv_scr_act(),"Place one note in the hopper."));capture_click_source(0);assert(sends==++before);capture_finish_image_direct(1);
 capture_click_source(7);assert(sends==++before);wave_wire_channel(2,99);
 capture_new_count(1,0,false);ui_page_dynamic_capture_on_count_info(1,0,1);ui_page_dynamic_capture_on_count_info(0,0,2);
 wave_end();assert(!capture_nav_locks&&!capture_gesture_locks);capture_click_source(7);assert(sends==++before);
 wave_wire_channel(2,199);wave_end();image_finish_feedback();wave_expect_constant(244,199);
 /* A started stream reaching the absolute 120s ceiling still waits for FF,
  * even when a queued timely packet postpones the rolling inactivity timeout. */
 capture_click_source(1);assert(sends==++before);image_header(2,5,3);
 protocol_request_set_pending_reply_probe(image_pending_probe);image_probe_pending=true;
 lv_tick_inc(120001);assert(ui_page_dynamic_capture_poll(lv_tick_get())&&capture_nav_locks&&capture_gesture_locks);
 image_probe_pending=false;protocol_request_set_pending_reply_probe(NULL);image_end();assert(!capture_nav_locks&&!capture_gesture_locks);
 capture_new_count(0,0,false);ui_page_dynamic_capture_on_count_info(1,0,1);ui_page_dynamic_capture_on_count_info(0,0,2);
 /* Dimension errors drain; late FF releases once. Force destroy is safe. */
 capture_click_source(2);assert(sends==++before);image_header(3,5,3);image_header(3,6,3);
 assert(ui_page_dynamic_capture_is_capturing()&&capture_nav_locks);lv_tick_inc(5001);image_end();assert(!capture_nav_locks&&!capture_gesture_locks);
 capture_denied(2,before);capture_new_count(0,0,false);ui_page_dynamic_capture_on_count_info(1,0,1);ui_page_dynamic_capture_on_count_info(0,0,2);
 capture_click_source(2);assert(sends==++before);image_header(3,5,3);ui_page_dynamic_capture_destroy();
 assert(!capture_nav_locks&&!capture_gesture_locks&&!capture_pointer_policy&&!settings_detail_overlay_is_open());
 ui_page_dynamic_capture_create(lv_scr_act());lv_tick_inc(3001);ui_page_dynamic_capture_poll(lv_tick_get());
 image_end();assert(!ui_page_dynamic_capture_is_capturing());capture_denied(0,before);
 /* Only the owning modal is cleared at destroy. A later foreign modal stays. */
 capture_click_source(0);static int foreign;assert(settings_detail_dialog_show("Foreign","Message","OK",NULL,NULL,NULL,&foreign));
 ui_page_dynamic_capture_destroy();assert(settings_detail_dialog_is_owned_by(&foreign));settings_detail_dialog_hide();
 assert(!settings_detail_dialog_is_owned_by(NULL)&&!test_notice_dialog_holds);
 capture_selected_geometry=false;
 puts("PASS fresh page START/LIVE/END gate, old RX/page/END rejection, zero and ADD/reject/reset cases, same-run cache/new-run invalidation, no-response recovery, late FF lock/drain/new-run ownership, real modal ownership and destruction");
}
