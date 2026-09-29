/* Menu-only record presentation. Commands are validated and persisted by Cashbook. */
static cashbook_command_t record_command;
static uint32_t record_command_revision;
static unsigned record_page;
static void record_commit(void *unused)
{
    (void)unused;
    if(cashbook_store_view_archived()){explain("Archived records are read only.");return;}
    if(record_command_revision!=cashbook_store_view()->sequence){
        explain("Record changed. Review the current values and try again.");return;
    }
    if(!cashbook_store_submit(&record_command))explain("Records are loading or being saved. Try again shortly.");
    else app_ui_runtime_notice_started(APP_UI_NOTICE_RECORD_STORE,"Saving the record decision...");
}
static void record_request(cashbook_operation_t operation,uint32_t group,uint32_t run,uint32_t day,const char *title,const char *body)
{
    if(cashbook_store_view_archived()){explain("Archive is read only. Edit current records instead.");return;}
    record_command=(cashbook_command_t){.operation=operation,.group=group,.run=run,.day=day,.operator_id=user()?user()->id:0};
    record_command_revision=cashbook_store_view()->sequence;
    settings_detail_dialog_show(title,body,"Confirm","Cancel",record_commit,NULL,NULL);
}
static void record_date_done(const char *value,void *unused)
{
    (void)unused;char *end;unsigned long day=strtoul(value,&end,10);
    if(strlen(value)!=8||*end||day>UINT32_MAX||!cashbook_day_valid(day)){explain("Enter a valid business date as YYYYMMDD, for example 20260923.");return;}
    menu.record_day=(uint32_t)day;
    if(cashbook_store_view_archived()){menu.dirty=true;return;}
    record_request(CASHBOOK_SET_DAY,0,0,(uint32_t)day,"Set business date?",
        "Undated, unconfirmed counts will be assigned to this date. Original count timestamps stay unchanged. Device time is not changed.");
}
static void record_pick(lv_event_t *e)
{
    const cashbook_t *book=cashbook_store_view();
    menu.selected_group=(uintptr_t)lv_event_get_user_data(e);
    const cashbook_group_t *g=cashbook_group(book,menu.selected_group);
    menu.selected_run=g?g->selected:0;record_page=0;menu.dirty=true;
}
static void attempt_pick(lv_event_t *e){menu.selected_run=(uintptr_t)lv_event_get_user_data(e);menu.dirty=true;}
static void record_pages(lv_obj_t *b,unsigned total,unsigned per)
{
    char range[48];snprintf(range,sizeof(range),"%u-%u / %u",total?record_page*per+1:0,(record_page+1)*per<total?(record_page+1)*per:total,total);
    small(b,278,272,160,range);
    lv_obj_t *previous=button(b,934,262,42,34,"",651,false),*next=button(b,986,262,42,34,"",652,false);
    icon(previous,MICON("back",20),11,7,MUTED);icon(next,MICON("next",20),11,7,MUTED);
    if(!record_page)lv_obj_add_state(previous,LV_STATE_DISABLED);
    if((record_page+1)*per>=total)lv_obj_add_state(next,LV_STATE_DISABLED);
}
static void closed_day_view(lv_obj_t *b)
{
    const cashbook_t *book=cashbook_store_view();
    if(!menu.selected_close||menu.selected_close>book->close_count){menu.selected_close=0;return;}
    const cashbook_close_t *c=&book->closes[menu.selected_close-1];
    heading(b,"Saved day close");button(b,869,0,159,39,"Back to list",654,false);
    lv_obj_t *summary=box(b,0,55,301,224,PANEL,14);border(summary,LINE);
    char line[80];snprintf(line,sizeof(line),"%04u-%02u-%02u",c->day/10000,c->day/100%100,c->day%100);
    label(summary,18,20,265,line,&lv_font_instrument_sans_semibold_28,INK);
    snprintf(line,sizeof(line),"Version %u / %u groups",c->revision,c->groups);text(summary,18,61,265,line);
    small(summary,18,100,265,"A fixed saved snapshot. Later corrections do not replace this version.");
    small(summary,18,174,265,c->current?"Current closed version":"Retained after reopening");
    lv_obj_t *list=scroll(b,328,55,700,170);menu.task_scroll=list;
    for(unsigned i=0;i<c->currencies;i++){
        text(list,12,i*52+8,80,c->money[i].code);
        snprintf(line,sizeof(line),"%u notes",c->money[i].pcs);small(list,103,i*52+12,170,line);
        money_text(line,c->money[i].amount);
        lv_obj_t *value=label(list,271,i*52+3,393,line,&lv_font_instrument_sans_semibold_28,INK);
        lv_obj_set_style_text_align(value,LV_TEXT_ALIGN_RIGHT,0);box(list,0,i*52+48,680,1,LINE,0);
    }
    button(b,328,238,337,41,"Show saved totals QR",655,true);
    button(b,677,238,351,41,"Export period to USB",A_EXPORT_RECORDS,false);
}
static void records_detail(lv_obj_t *b)
{
    (void)b;const cashbook_t *book=cashbook_store_view();const cashbook_group_t *g=cashbook_group(book,menu.selected_group);
    if(!g){menu.selected_group=0;return;}
    const cashbook_run_t *chosen=cashbook_run(book,menu.selected_run);
    if(!chosen||chosen->group!=g->id){menu.selected_run=g->selected;chosen=cashbook_run(book,g->selected);}
    menu.record_sheet=box(menu.root,0,0,1280,400,INK,0);lv_obj_set_style_bg_opa(menu.record_sheet,LV_OPA_30,0);lv_obj_add_flag(menu.record_sheet,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *sheet=box(menu.record_sheet,220,28,840,344,0xFFFFFF,22);lv_obj_add_flag(sheet,LV_OBJ_FLAG_CLICKABLE);
    label(sheet,24,20,630,"Choose the counted result",&lv_font_instrument_sans_semibold_25,INK);
    lv_obj_t *close=button(sheet,778,18,38,38,"",650,false);icon(close,MICON("close",22),8,8,INK);lv_nav_button_mark_back(close);
    unsigned total=cashbook_attempts(book,g->id),skip=record_page*3,row=0;if(skip>=total){record_page=0;skip=0;}
    for(unsigned i=0,seen=0;i<book->run_count;i++)if(book->runs[i].group==g->id){
        const cashbook_run_t *r=&book->runs[i];if(seen++<skip||row==3)continue;
        bool selected=r->id==menu.selected_run;lv_obj_t *v=lv_settings_button(sheet,24+row++*264,72,252,122,"",false,attempt_pick,(void *)(uintptr_t)r->id);
        palette(v,selected?0xFFFFFF:PANEL);lv_obj_set_style_radius(v,14,0);lv_obj_set_style_border_width(v,selected?2:0,0);lv_obj_set_style_border_color(v,lv_color_hex(BLUE),0);
        char line[100];snprintf(line,sizeof(line),"#%u / %02u:%02u",r->result.source,r->result.hour,r->result.minute);text(v,16,14,185,line);
        icon(v,selected?MICON("check",19):MICON("note",19),217,15,selected?BLUE:MUTED);
        if(r->result.currencies==1){money_text(line,r->result.money[0].amount);label(v,16,43,220,line,strlen(line)>12?&lv_font_instrument_sans_semibold_20:&lv_font_instrument_sans_semibold_28,INK);}
        else{snprintf(line,sizeof(line),"%u currencies",r->result.currencies);label(v,16,43,220,line,&lv_font_instrument_sans_semibold_28,INK);}
        snprintf(line,sizeof(line),"%u notes%s",r->result.pcs,r->id==g->selected&&g->confirmed?" / Counted":"");label(v,16,88,220,line,&lv_font_instrument_sans_menumedium_14,selected?BLUE:MUTED);
    }
    if(chosen){
        lv_obj_t *values=scroll(sheet,24,207,600,52);menu.task_scroll=values;
        for(unsigned i=0;i<chosen->result.currencies;i++){char amount[32],line[80];money_text(amount,chosen->result.money[i].amount);snprintf(line,sizeof(line),"%s  %s / %u notes",chosen->result.money[i].code,amount,chosen->result.money[i].pcs);small(values,0,i*21,580,line);}
        if(!chosen->result.complete)label(sheet,24,261,760,"Incomplete result: review before counting.",&lv_font_instrument_sans_medium_13,ORANGE);
        lv_obj_t *confirm=button(sheet,24,282,184,38,"Count this result",A_CONFIRM_RUN,true);
        if(!chosen->result.complete||cashbook_closed(book,g->day)||g->excluded)lv_obj_add_state(confirm,LV_STATE_DISABLED);
        button(sheet,220,282,172,38,chosen->candidate?"Attach as recount":"Verify this group",chosen->candidate?A_MERGE_RUN:653,false);
        lv_obj_t *split=button(sheet,404,282,112,38,"Split out",A_SPLIT_RUN,false);if(total<2||cashbook_closed(book,g->day))lv_obj_add_state(split,LV_STATE_DISABLED);
        button(sheet,528,282,112,38,g->excluded?"Restore":"Exclude",A_EXCLUDE_RUN,false);
        button(sheet,652,282,164,38,"Back to list",650,false);
        if(total>3){
            lv_obj_t *prev=button(sheet,724,213,40,34,"",651,false),*next=button(sheet,776,213,40,34,"",652,false);
            icon(prev,MICON("back",20),10,7,MUTED);icon(next,MICON("next",20),10,7,MUTED);
            if(!record_page)lv_obj_add_state(prev,LV_STATE_DISABLED);
            if((record_page+1)*3>=total)lv_obj_add_state(next,LV_STATE_DISABLED);
        }
    }
}
static void records_view(lv_obj_t *b)
{
    if(menu.selected_close){closed_day_view(b);return;}
    if(menu.show_archives){
        heading(b,"Archived periods");button(b,818,0,210,39,"Current records",A_CURRENT_RECORDS,false);
        const cashbook_archive_t *items;unsigned total=cashbook_store_archives(&items);
        if(record_page*3>=total)record_page=0;
        for(unsigned row=0;row<3&&record_page*3+row<total;row++){
            const cashbook_archive_t *item=&items[record_page*3+row];char line[100];
            snprintf(line,sizeof(line),"Saved period #%u / Open read-only",item->id);
            button(b,0,55+row*59,1028,53,line,10000+item->id,false);
        }
        if(!total)text(b,16,100,950,"No archived periods yet.");
        button(b,0,241,290,38,"Archive closed period",A_ARCHIVE_PERIOD,false);
        record_pages(b,total,3);return;
    }
    if(menu.selected_group){uint32_t selected=menu.selected_group;menu.selected_group=0;records_view(b);menu.selected_group=selected;records_detail(b);return;}
    const cashbook_t *book=cashbook_store_view();uint32_t day=business_day();
    const char *title=menu.sub==0?"Daily totals":menu.sub==1?"Recount groups":menu.sub==2?"Count history":menu.sub==3?"Verify a count":"Day closes";
    heading(b,title);
    if(menu.sub==3&&!menu.verify_pick){
        lv_obj_t *intro=box(b,0,48,304,248,PANEL,20);
        label(intro,20,20,264,"Compare, don't add.",&lv_font_instrument_sans_semibold_21,INK);
        small(intro,20,62,264,"Compare with a saved count. Verification does not add to daily totals.");
        icon(intro,MICON("shield",22),20,111,TEAL);
        button(intro,20,187,264,44,"Choose original",659,true);
        step_card(b,324,48,227,248,"01","Choose original","Select a saved count as your reference.",MICON("history",22));
        step_card(b,563,48,226,248,"02","Count again","Compare amount and available serials.",MICON("repeat",22));
        step_card(b,801,48,227,248,"03","Review changes","Confirm a result only after checking differences.",MICON("check",22));
        if(counting_cashbook_verify_group())button(b,812,0,216,36,"Cancel verification",A_CANCEL_VERIFY,false);
        return;
    }
    char date[40];short_day(date,sizeof(date),day);
    if(menu.sub==4&&cashbook_day_valid(day)){size_t used=strlen(date);snprintf(date+used,sizeof(date)-used," %u",day/10000);}
    lv_point_t title_size;lv_txt_get_size(&title_size,title,&lv_font_instrument_sans_semibold_26,0,0,650,LV_TEXT_FLAG_NONE);
    lv_obj_t *date_button=button(b,title_size.x+12,3,132,34,date,A_BUSINESS_DAY,false);lv_obj_set_style_bg_opa(date_button,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(date_button,lv_color_hex(MUTED),0);
    lv_obj_t *date_label=lv_obj_get_child(date_button,0);lv_obj_set_style_text_font(date_label,&lv_font_instrument_sans_menumedium_14,0);lv_obj_align(date_label,LV_ALIGN_LEFT_MID,0,0);
    if(menu.sub==4){
        unsigned total=book->close_count;if(record_page*3>=total)record_page=0;
        button(b,882,0,146,36,"Archives",A_ARCHIVES,false);
        if(!total){
            lv_obj_t *empty=box(b,0,48,1028,248,PANEL,20);icon(empty,MICON("receipt",22),24,28,TEAL);
            label(empty,24,64,800,"No closed periods yet",&lv_font_instrument_sans_semibold_26,INK);
            lv_obj_t *description=label(empty,24,112,480,"Review pending counts, then close today to keep a dated summary. Your current count will not be cleared.",&lv_font_instrument_sans_medium_16,MUTED);lv_obj_set_style_text_line_space(description,8,0);
            button(empty,24,182,140,44,"Review today",660,true);
        }else{
            for(unsigned row=0;row<3&&record_page*3+row<total;row++){
                const cashbook_close_t *c=&book->closes[total-1-record_page*3-row];char line[120];
                snprintf(line,sizeof(line),"%04u-%02u-%02u / Version %u",c->day/10000,c->day/100%100,c->day%100,c->revision);
                lv_obj_t *v=button(b,0,48+row*62,1028,56,"",700+c->id,false);palette(v,row%2?0xFFFFFF:PANEL);
                label(v,16,8,560,line,&lv_font_instrument_sans_semibold_19,INK);
                snprintf(line,sizeof(line),"%u groups / %u currencies / %s",c->groups,c->currencies,c->current?"Closed":"Reopened");small(v,584,19,410,line);
                icon(v,MICON("next",18),996,19,MUTED);
            }
            button(b,0,262,213,34,"Export period to USB",A_EXPORT_RECORDS,false);record_pages(b,total,3);
        }return;
    }
    cashbook_money_t daily[CASHBOOK_CURRENCIES];unsigned currencies=0,confirmed=0,pending=0;
    cashbook_totals(book,day,daily,&currencies,&confirmed,&pending);
    const char *filters[]={"All","Pending"};segments(b,menu.sub==0||menu.sub==2?766:864,0,164,38,filters,2,menu.record_pending,657);
    if(menu.sub==0){
        lv_obj_t *close=button(b,940,2,88,34,cashbook_store_view_archived()?"Current":cashbook_closed(book,day)?"Reopen":"Close day",cashbook_store_view_archived()?A_CURRENT_RECORDS:A_CLOSE_DAY,false);
        if(!confirmed&&!pending&&!cashbook_closed(book,day)&&!cashbook_store_view_archived())lv_obj_add_state(close,LV_STATE_DISABLED);
    }else if(menu.sub==2)button(b,940,2,88,34,"Find",A_HISTORY,false);
    currency_totals(b,0,48,258,248,day);
    small(b,290,48,350,"Count / time");lv_obj_t *l=small(b,692,48,152,"Amount");lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_RIGHT,0);
    l=small(b,854,48,130,"Status");lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_RIGHT,0);
    unsigned total=0,undated=0;
    for(unsigned i=0;i<book->group_count;i++){
        const cashbook_group_t *g=&book->groups[i];unsigned attempts=cashbook_attempts(book,g->id);
        if(!g->day&&attempts&&!g->excluded)undated++;
        if(g->day==day&&attempts&&(menu.sub!=1||attempts>1||g->review)&&(!menu.record_pending||(!g->excluded&&(!g->confirmed||g->review))))total++;
    }
    if(record_page*3>=total)record_page=0;
    unsigned row=0,seen=0;
    for(unsigned i=book->group_count;i>0;i--){
        const cashbook_group_t *g=&book->groups[i-1];unsigned attempts=cashbook_attempts(book,g->id);
        if(g->day!=day||!attempts||(menu.sub==1&&attempts==1&&!g->review)||(menu.record_pending&&(g->excluded||(g->confirmed&&!g->review))))continue;
        if(seen++<record_page*3||row==3)continue;
        const cashbook_run_t *r=cashbook_run(book,g->selected);if(!r)continue;
        lv_obj_t *v=lv_settings_button(b,278,72+row*60,750,60,"",false,record_pick,(void *)(uintptr_t)g->id);
        palette(v,row%2?0xFFFFFF:PANEL);lv_obj_set_style_border_width(v,0,0);lv_obj_set_style_radius(v,11,0);row++;
        char line[120];snprintf(line,sizeof(line),"%s",*r->result.operator_name?r->result.operator_name:"Saved count");
        lv_obj_t *name=label(v,12,9,340,line,&lv_font_instrument_sans_semibold_17,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
        snprintf(line,sizeof(line),"B%03u · %02u:%02u",g->id,r->result.hour,r->result.minute);
        if(attempts>1){size_t used=strlen(line);snprintf(line+used,sizeof(line)-used," · %u runs",attempts);}small(v,12,33,340,line);
        l=label(v,414,7,152,r->result.currencies==1?r->result.money[0].code:"MULTI",&lv_font_instrument_sans_medium_13,MUTED);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_RIGHT,0);
        if(r->result.currencies==1)money_text(line,r->result.money[0].amount);else snprintf(line,sizeof(line),"%u notes",r->result.pcs);
        l=label(v,414,22,152,line,&lv_font_instrument_sans_semibold_20,INK);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_RIGHT,0);
        l=label(v,576,22,130,g->excluded?"Excluded":g->review?"Review recount":g->confirmed?"Confirmed":"To confirm",&lv_font_instrument_sans_menumedium_14,g->excluded||(g->confirmed&&!g->review)?MUTED:ORANGE);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_RIGHT,0);
        icon(v,MICON("next",18),719,20,MUTED);
    }
    if(!total){label(b,290,133,704,menu.sub==1?"No recounts":"No counts to show",&lv_font_instrument_sans_semibold_24,INK);}
    record_pages(b,total,3);
    if(menu.sub==0&&pending&&!cashbook_store_view_archived()){lv_obj_t *confirm=button(b,543,262,190,34,"Confirm singles",656,false);lv_obj_set_style_bg_opa(confirm,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(confirm,lv_color_hex(MUTED),0);}
    else if(menu.sub==1){icon(b,MICON("warning",22),535,267,ORANGE);label(b,561,271,307,"Review each group",&lv_font_instrument_sans_menumedium_14,ORANGE);}
    if(undated){char status[96];snprintf(status,sizeof(status),"%u undated groups: set the business date.",undated);small(b,278,247,600,status);}
}
