static print_config_value_t output_observed;
static bool output_waiting;
static void output_send(print_config_field_t field,const print_config_value_t *value)
{
    if(app_command_runtime_count_start_busy()||!print_config_request_field(field,value)){
        notify(UI_NOTICE_WARNING,UI_N_("Print setting not sent. Finish the current operation and try again."));return;
    }
    output_waiting=true;ui_notice_post_text(UI_NOTICE_PROGRESS,"settings.receipt",UI_N_("Receipt settings"),UI_N_("Applying settings..."));
}
static void output_input(const char *value,void *context)
{
    print_config_field_t field=(uintptr_t)context;print_config_value_t config;print_config_get(&config);
    if(field==PRINT_CONFIG_HEAD1||field==PRINT_CONFIG_HEAD2)
        snprintf(field==PRINT_CONFIG_HEAD1?config.head1:config.head2,sizeof(config.head1),"%s",value);
    else{char *end;long n=strtol(value,&end,10);if(!*value||*end||n<0||n>PRINT_SETTING_SPACE_MAX_LINES){explain(UI_N_("Enter 0-99 blank lines."));return;}
        if(field==PRINT_CONFIG_TOP)config.space_top=n;else config.space_bottom=n;}
    output_send(field,&config);
}
static bool output_layout;
static bool output_action(unsigned id)
{
    if(id>=820&&id<=827){
        unsigned row=(id-820)/4,value=(id-820)%4;
        if(value==3)id=row?806:805;
        else{print_config_value_t config;print_config_get(&config);
            if(row)config.space_bottom=value;else config.space_top=value;
            output_send(row?PRINT_CONFIG_BOTTOM:PRINT_CONFIG_TOP,&config);return true;}
    }
    if(id==807){output_layout=!output_layout;menu.dirty=true;return true;}
    if(id==810||id==811){menu.reject_all=id==811;menu.reject_code=0;menu.scroll_y[5][0]=0;menu.dirty=true;return true;}
    if(id==812){char details[512];snprintf(details,sizeof(details),ui_tr("Display: %s\nController: %s\nImage: %s\nFPGA: %s\nController boot: %s\nImage boot: %s"),device_info_display_app(),device_info_main_app(),device_info_image_app(),device_info_fpga(),device_info_main_boot(),device_info_image_boot());settings_detail_dialog_show(ui_tr("Software versions"),details,ui_tr("Close"),NULL,NULL,NULL,NULL);return true;}
    if(id<800||id>806)return false;
    print_config_value_t config;print_config_get(&config);
    if(id<=802){config.content=id-799;output_send(PRINT_CONFIG_CONTENT,&config);return true;}
    print_config_field_t field=id-802;char value[32];const char *title;
    if(field==PRINT_CONFIG_HEAD1){strcpy(value,config.head1);title=ui_tr("Receipt heading 1");}
    else if(field==PRINT_CONFIG_HEAD2){strcpy(value,config.head2);title=ui_tr("Receipt heading 2");}
    else{snprintf(value,sizeof(value),"%u",field==PRINT_CONFIG_TOP?config.space_top:config.space_bottom);title=field==PRINT_CONFIG_TOP?ui_tr("Top blank lines / 0-99"):ui_tr("Bottom blank lines / 0-99");}
    bool heading_field=field==PRINT_CONFIG_HEAD1||field==PRINT_CONFIG_HEAD2;
    settings_detail_keyboard_show(title,value,heading_field?PRINT_SETTING_HEAD_MAX_LEN:2,heading_field?SETTINGS_DETAIL_KEYBOARD_TEXT:SETTINGS_DETAIL_KEYBOARD_UINT,output_input,(void *)(uintptr_t)field);return true;
}
static void output_view(lv_obj_t *b)
{
    heading(b,menu.sub?ui_tr("QR export"):output_layout?ui_tr("Layout & spacing"):ui_tr("Print & receipt"));if(!output_layout||menu.sub)context(b,ui_tr("Current result"),menu.sub?132:183);
    const counting_sim_t *result=counting_data_current();char line[96],code[4];currency_state_get_effective_code(code);
    bool available=result&&result->total_pcs>0&&!app_command_runtime_count_start_busy();
    bool monetary=result&&!result->multi_currency_result&&counting_data_monetary_result_supported(result);
    if(menu.sub){
        lv_obj_t *card=box(b,0,48,684,248,PANEL,20);
        snprintf(line,sizeof(line),ui_tr("%s · counting summary"),currency_state_display_code(code));
        label(card,20,20,616,line,&lv_font_instrument_sans_semibold_21,INK);icon(card,MICON("qr",22),642,20,BLUE);
        if(monetary)money_text(line,(uint64_t)result->total_amount);else snprintf(line,sizeof(line),"%s",ui_tr("Separate currencies"));
        if(monetary)hero_amount(card,20,55,644,(uint64_t)result->total_amount,false);else label(card,20,55,644,line,&lv_font_instrument_sans_semibold_29,INK);
        snprintf(line,sizeof(line),ui_trn("%d notes · denomination summary", (result?result->total_pcs:0)),result?result->total_pcs:0);small(card,20,119,644,line);
        lv_obj_t *controls=box(card,20,184,644,44,PANEL,0);toggle(controls,0,644,ui_tr("Show after counting"),NULL,user()&&user()->qr_after_count,A_AUTO_QR);
        lv_obj_t *graphic=box(b,716,52,312,76,PANEL,18);icon(graphic,MICON("qr",38),137,19,BLUE);
        label(b,716,142,312,ui_tr("Take your result with you"),&lv_font_instrument_sans_semibold_21,INK);
        lv_obj_t *hint=small(b,716,182,312,ui_tr("Manual export works even when automatic QR is off."));lv_obj_set_style_text_line_space(hint,5,0);
        lv_obj_t *show=button(b,716,248,312,44,ui_tr("Preview QR export"),A_QR,true);
        char payload[3072];if(!available||!ui_qr_data_build_summary(payload,sizeof(payload)))lv_obj_add_state(show,LV_STATE_DISABLED);
        return;
    }
    print_config_value_t config;print_config_get(&config);output_observed=config;
    if(output_layout){
        button(b,867,0,161,36,ui_tr("Back to receipt"),807,false);
        lv_obj_t *fields=box(b,0,48,1028,248,PANEL,18);
        const char *titles[]={ui_tr("Second heading"),ui_tr("Top blank lines"),ui_tr("Bottom blank lines")};
        for(unsigned i=0;i<3;i++){
            text(fields,20,25+i*69,570,titles[i]);
            if(!i)white_button(fields,618,15,390,44,*config.head2?config.head2:ui_tr("Not set"),804);
            else{
                unsigned value=i==1?config.space_top:config.space_bottom;
                char custom[24];snprintf(custom,sizeof(custom),value>2?ui_tr("Custom: %u"):ui_tr("Custom"),value);
                const char *presets[]={"0","1","2",custom};
                segments(fields,570,15+i*69,438,44,presets,4,value>2?3:value,820+(i-1)*4);
            }
        }
        return;
    }
    lv_obj_t *card=box(b,0,48,304,248,PANEL,20),*paper=box(card,23,14,258,167,0xFFFFFF,4);
    lv_obj_t *h=label(paper,14,13,230,config.head1[0]?config.head1:"UN260",&lv_font_instrument_sans_semibold_21,INK);lv_obj_set_style_text_align(h,LV_TEXT_ALIGN_CENTER,0);
    h=label(paper,14,43,230,config.head2[0]?config.head2:ui_tr("COUNT REPORT"),&lv_font_instrument_sans_medium_13,INK);lv_obj_set_style_text_align(h,LV_TEXT_ALIGN_CENTER,0);
    snprintf(line,sizeof(line),ui_tr("%s / %d notes"),currency_state_display_code(code),result?result->total_pcs:0);small(paper,18,70,222,line);
    box(paper,18,95,222,1,LINE,0);small(paper,18,114,66,ui_tr("TOTAL"));
    if(monetary)money_text(line,(uint64_t)result->total_amount);else snprintf(line,sizeof(line),"%s",ui_tr("Separate"));
    h=label(paper,82,108,158,line,&lv_font_instrument_sans_semibold_26,INK);lv_obj_set_style_text_align(h,LV_TEXT_ALIGN_RIGHT,0);
    label(paper,18,147,222,config.content==1?ui_tr("Denomination summary"):config.content==2?ui_tr("Serial records"):ui_tr("Summary + serials"),&lv_font_instrument_sans_medium_13,MUTED);
    lv_obj_t *print=button(card,23,190,258,44,ui_tr("Print result"),A_PRINT,true);
    header_content(print,ui_tr("Print result"),MICON("print",22),0xFFFFFF);if(!available||print_config_pending())lv_obj_add_state(print,LV_STATE_DISABLED);
    lv_obj_t *fields=box(b,324,48,704,174,PANEL,20);
    text(fields,18,24,310,ui_tr("Report content"));const char *names[]={ui_tr("Summary"),ui_tr("Serial"),ui_tr("Both")};
    segments(fields,396,14,290,42,names,3,config.content?config.content-1:0,800);
    box(fields,18,61,668,1,LINE,0);text(fields,18,78,340,ui_tr("Receipt heading"));
    field_button(fields,396,68,290,*config.head1?config.head1:ui_tr("Not set"),803);
    box(fields,18,114,668,1,LINE,0);text(fields,18,131,330,ui_tr("Layout & spacing"));
    snprintf(line,sizeof(line),ui_tr("%u top / %u bottom"),config.space_top,config.space_bottom);link_button(fields,494,121,192,line,807);
    small(b,324,280,704,ui_tr("Serial reports need complete serial records."));
}
static void reject_pick(lv_event_t *event){menu.reject_code=(uintptr_t)lv_event_get_user_data(event);menu.dirty=true;}
static void support_export(void *unused)
{
    (void)unused;char report[8192];
    if(!support_report_build(report,sizeof(report))||!workspace_store_export_support(report))explain(UI_N_("The report could not be prepared. Wait for storage and try again."));
    else app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,UI_N_("Writing support report to USB. Keep the drive connected."));
}

static void sorter_badge(lv_obj_t *parent,int x,int y)
{
    lv_obj_t *body=box(parent,x,y,30,34,0x244B65,4);
    box(body,4,3,22,4,0x89B1C7,1);
    box(body,4,9,22,10,0xB9D7E4,2);
    box(body,4,22,10,8,0xE9F0F1,1);
    box(body,16,22,10,8,0xE9F0F1,1);
    box(body,6,20,18,2,0x4E7E96,0);
}
static void help_view(lv_obj_t *b)
{
    const char *titles[]={ui_tr("Reject guide"),ui_tr("Care"),ui_tr("Device & support")};heading(b,titles[menu.sub]);
    if(menu.sub==0){
        const counting_sim_t *sim=counting_data_current();unsigned counts[256]={0};
        int count=counting_data_error_detail_count(sim);
        if(sim&&sim->err_code&&sim->err_pcs)for(int i=0;i<count;i++)counts[sim->err_code[i]]+=sim->err_pcs[i];
        const char *filter[]={ui_tr("Current"),ui_tr("All codes")};segments(b,818,0,210,38,filter,2,menu.reject_all,810);
        context(b,ui_tr("Code / cause / action"),180);
        lv_obj_t *list=scroll(b,0,48,330,248);menu.task_scroll=list;
        unsigned row=0,first=256;
        for(unsigned code=0;code<256;code++){
            if(menu.reject_all?code>=0x32:!counts[code])continue;
            if(first==256)first=code;
        }
        if((!menu.reject_all&&!counts[menu.reject_code])||(menu.reject_all&&menu.reject_code>=0x32))menu.reject_code=first==256?0:first;
        for(unsigned code=0;code<256;code++){
            if(menu.reject_all?code>=0x32:!counts[code])continue;
            bool selected=menu.reject_code==code;
            lv_obj_t *v=lv_settings_button(list,0,row++*58,322,52,"",false,reject_pick,(void *)(uintptr_t)code);
            palette(v,selected?HEADER_SURFACE:PANEL);lv_obj_set_style_border_width(v,0,0);lv_obj_set_style_radius(v,12,0);
            if(selected)box(v,0,10,3,32,BLUE,2);
            char line[100];snprintf(line,sizeof(line),"%02X  %s",code,ui_tr(counting_reject_guide_get(code)->title));
            lv_obj_t *l=label(v,12,9,298,line,&lv_font_instrument_sans_semibold_16,selected?BLUE:INK);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);
            snprintf(line,sizeof(line),counts[code]?ui_trn("%s / %u notes", (counts[code])):"%s",ui_text_counting_reject_reason(code),counts[code]);label(v,12,31,298,line,&lv_font_instrument_sans_medium_13,MUTED);
        }
        if(!row){label(list,14,28,294,ui_tr("No current rejects"),&lv_font_instrument_sans_semibold_21,INK);small(list,14,70,294,ui_tr("Select All codes to browse the guide."));}
        lv_obj_t *detail=box(b,350,48,678,248,PANEL,20);
        const counting_reject_guide_t *g=counting_reject_guide_get(menu.reject_code);
        char line[100];snprintf(line,sizeof(line),"%02X / %s",menu.reject_code,ui_tr(g->title));
        label(detail,20,16,616,line,&lv_font_instrument_sans_semibold_21,INK);icon(detail,menu.reject_code?MICON("warning",22):MICON("check",22),636,18,menu.reject_code?ORANGE:TEAL);
        label(detail,20,51,638,ui_tr(g->meaning),&lv_font_instrument_sans_medium_15,INK);
        label(detail,20,98,638,ui_tr("Possible causes"),&lv_font_instrument_sans_semibold_16,ORANGE);
        label(detail,20,122,638,ui_tr(g->causes),&lv_font_instrument_sans_menumedium_14,MUTED);
        label(detail,20,164,638,ui_tr("What to do"),&lv_font_instrument_sans_semibold_16,BLUE);
        label(detail,20,188,638,ui_tr(g->action),&lv_font_instrument_sans_menumedium_14,INK);
    }else if(menu.sub==1){
        context(b,ui_tr("Routine maintenance"),68);small(b,0,48,680,ui_tr("Keep the feed path and sensors clean."));
        label(b,809,48,219,ui_tr("Power off before cleaning"),&lv_font_instrument_sans_menumedium_14,ORANGE);
        icon(b,MICON("warning",22),783,44,ORANGE);
        step_card(b,0,82,337,186,"01",ui_tr("Power off"),ui_tr("Disconnect power before opening accessible covers."),MICON("power",22));
        step_card(b,349,82,330,186,"02",ui_tr("Clean gently"),ui_tr("Use the approved brush. Keep liquids away from the note path."),MICON("brush",22));
        step_card(b,691,82,337,186,"03",ui_tr("Check the path"),ui_tr("Remove loose dust, close covers, then perform a test count."),MICON("note",22));
    }else{
        lv_obj_t *d=box(b,0,48,650,248,PANEL,20);sorter_badge(d,22,26);
        label(d,62,20,380,"UN260",&lv_font_instrument_sans_semibold_37,INK);button(d,519,28,109,30,ui_tr("Versions"),812,false);
        small(d,22,88,270,ui_tr("UI version"));text(d,22,111,270,device_info_display_app());
        small(d,336,88,292,ui_tr("Controller"));text(d,336,111,292,device_info_is_valid()?device_info_main_app():ui_tr("Not connected"));
        small(d,22,154,270,ui_tr("Display"));text(d,22,177,270,"1280 x 400");
        small(d,336,154,292,ui_tr("Service access"));link_button(d,336,173,275,ui_tr("Protected settings"),A_SETTINGS);
        lv_obj_t *report=box(b,670,48,358,248,PANEL,20);icon(report,MICON("download",25),20,23,TEAL);
        label(report,55,21,283,ui_tr("Support report"),&lv_font_instrument_sans_semibold_21,INK);
        small(report,20,63,318,ui_tr("Versions, configuration and current fault codes in one report."));
        small(report,20,122,318,ui_tr("Serials, note images and operator photos are excluded."));
        white_button(report,20,187,318,44,ui_tr("Review report contents"),A_DIAGNOSTICS);
    }
}
