/* Private Menu callbacks; all decisions go through the durable ledger service. */
static void show_closed_day(unsigned id)
{
    const cashbook_t *book=cashbook_store_view();
    if(!id||id>book->close_count)return;
    menu.selected_close=id;menu.dirty=true;
}
static void closed_day_qr(void)
{
    const cashbook_t *book=cashbook_store_view();
    if(!menu.selected_close||menu.selected_close>book->close_count)return;
    const cashbook_close_t *c=&book->closes[menu.selected_close-1];char payload[2500];
    int n=snprintf(payload,sizeof(payload),"UN260 DAY CLOSE\nDATE=%08u\nVERSION=%u\nGROUPS=%u\nCURRENCY,NOTES,AMOUNT\n",c->day,c->revision,c->groups);
    for(unsigned i=0;i<c->currencies;i++){
        if(n<0||(size_t)n>=sizeof(payload)){explain("Use USB export for this saved day.");return;}
        int added=snprintf(payload+n,sizeof(payload)-n,"%s,%u,%llu\n",c->money[i].code,c->money[i].pcs,(unsigned long long)c->money[i].amount);
        if(added<0||(size_t)added>=sizeof(payload)-n){explain("Use USB export for this saved day.");return;}n+=added;
    }
    if(!lv_qr_popup_show(payload))explain("QR could not be displayed. Export the period to USB instead.");
    else menu.qr_generation=lv_qr_popup_generation();
}
static void verify_confirm(void *unused)
{
    (void)unused;
    if(cashbook_store_view_archived()||!counting_cashbook_arm_verify(menu.selected_group)){
        explain("Finish saves. Use an open group, ADD off and one currency.");return;
    }
    notify(UI_NOTICE_INFO,"Ready to verify. The next count will be linked for review.");
    confirmed_back(NULL);
}
static void archive_confirm(void *unused)
{
    (void)unused;
    const counting_sim_t *current=counting_data_current();
    if(current&&current->total_pcs&&(machine_state_add_enabled()||current->multi_currency_result)){
        explain("Clear the current ADD or MULTI result before archiving.");return;
    }
    if(app_command_runtime_count_start_busy()||!counting_history_can_archive()||counting_cashbook_verify_group()||!cashbook_store_archive())explain("Finish counting, verification and pending record reviews.");
    else{menu.selected_group=0;menu.record_day=0;menu.show_archives=false;menu.dirty=true;app_ui_runtime_notice_started(APP_UI_NOTICE_RECORD_STORE,"Archiving closed records...");}
}
static void export_records_confirm(void *unused)
{(void)unused;if(!cashbook_store_export())explain("Wait for record storage, then retry.");else app_ui_runtime_notice_started(APP_UI_NOTICE_RECORD_STORE,"Exporting records to USB. Keep the drive connected.");}
static bool record_action(unsigned id)
{
    if(id>=10000){
        if(cashbook_store_open_archive(id-10000)){menu.show_archives=false;menu.selected_group=0;menu.record_day=0;menu.sub=0;menu.dirty=true;}
        else explain("The archive could not be opened. Wait for storage and retry.");
        return true;
    }
    if(id==A_ARCHIVES){menu.show_archives=true;menu.selected_group=0;record_page=0;menu.dirty=true;cashbook_store_scan_archives();return true;}
    if(id==A_CURRENT_RECORDS){
        if(cashbook_store_busy()){explain("Wait for record storage to finish.");return true;}
        cashbook_store_close_view();menu.show_archives=false;menu.selected_group=0;menu.record_day=0;menu.dirty=true;return true;
    }
    if(id==A_ARCHIVE_PERIOD){settings_detail_dialog_show("Archive the closed period?","All groups must be confirmed or excluded, and every business day closed. Records stay on this device and remain readable in Archives. This frees the active period for new counts.","Archive","Cancel",archive_confirm,NULL,NULL);return true;}
    if(id==A_EXPORT_RECORDS){settings_detail_dialog_show("Export record summary?","Writes attempts and saved day-close totals to USB as CSV. Amounts are included; serials and operator names are excluded. Each attempt is marked counted or not counted.","Export","Cancel",export_records_confirm,NULL,NULL);return true;}
    const cashbook_t *book=cashbook_store_view();
    const cashbook_group_t *g=cashbook_group(book,menu.selected_group);
    const cashbook_run_t *r=cashbook_run(book,menu.selected_run);
    if(id==657||id==658){menu.record_pending=id==658;record_page=0;menu.dirty=true;return true;}
    if(id==659){menu.verify_pick=true;menu.dirty=true;return true;}
    if(id==660){menu.sub=0;menu.dirty=true;return true;}
    if(id==654){menu.selected_close=0;menu.dirty=true;return true;}
    if(id==655){closed_day_qr();return true;}
    if(id==656){record_request(CASHBOOK_CONFIRM_SINGLES,0,0,business_day(),"Confirm single counts?","Only complete, one-attempt groups without a recount suggestion are included. Suspected repeats, multiple attempts and incomplete results still require individual review. No cash is merged automatically.");return true;}
    if(id==650){menu.selected_group=0;record_page=0;menu.dirty=true;return true;}
    if(id==651||id==652){if(id==651){if(record_page)record_page--;}else record_page++;menu.dirty=true;return true;}
    if(id>700&&id<=700+CASHBOOK_CLOSES){show_closed_day(id-700);return true;}
    if(id==A_BUSINESS_DAY){char date[16];snprintf(date,sizeof(date),"%u",business_day());settings_detail_keyboard_show("Business date / YYYYMMDD",business_day()?date:"",8,SETTINGS_DETAIL_KEYBOARD_UINT,record_date_done,NULL);return true;}
    if(id==A_CLOSE_DAY){uint32_t day=business_day();bool closed=cashbook_closed(book,day);
        record_request(closed?CASHBOOK_REOPEN:CASHBOOK_CLOSE,0,0,day,closed?"Reopen this day?":"Close this day?",closed?"The saved version stays in Closed days. Later corrections create a new version.":"Only confirmed, selected results are counted. Every pending or changed group must be reviewed first. Totals stay separate by currency.");return true;}
    if(id==653){if(g)settings_detail_dialog_show("Verify this group?","The next non-empty single-currency count will join this group. The current selected result stays counted until you choose another. ADD must be off. No motor command is sent.","Continue","Cancel",verify_confirm,NULL,NULL);return true;}
    if(id!=A_CONFIRM_RUN&&id!=A_MERGE_RUN&&id!=A_SPLIT_RUN&&id!=A_EXCLUDE_RUN)return false;
    if(!g||!r||r->group!=g->id){explain("Select a record first.");return true;}
    if(id==A_CONFIRM_RUN)record_request(CASHBOOK_SELECT,g->id,r->id,0,"Count this result?","This group's previous selection will be replaced, not added. Other attempts remain available. If this is a suggested recount of another group, attach it first to avoid counting the same cash twice.");
    if(id==A_MERGE_RUN)record_request(CASHBOOK_MERGE,r->candidate,r->id,0,"Attach as a recount?","Serial overlap is a suggestion, not proof. Confirm these are the same physical cash. The destination's selected result stays counted until you review it.");
    if(id==A_SPLIT_RUN)record_request(CASHBOOK_SPLIT,g->id,r->id,0,"Separate this attempt?","Use this only for a different business transaction. The new group will need confirmation before it contributes to totals.");
    if(id==A_EXCLUDE_RUN)record_request(g->excluded?CASHBOOK_RESTORE:CASHBOOK_EXCLUDE,g->id,0,0,g->excluded?"Restore this group?":"Exclude this group?","Original attempts and Main results are preserved. Restored groups need confirmation before being counted again.");
    return true;
}
static void mode_confirm(void *context)
{
    /* Re-selecting the confirmed mode must not schedule a destructive result clear. */
    if((uintptr_t)context==machine_state_mode())return;
    if(app_command_runtime_count_start_busy()||!setting_service_request_mode((uintptr_t)context))notify(UI_NOTICE_WARNING,"Mode not sent. Finish counting and try again.");
    else clear_notice();
}
static void capacity_done(const char *value,void *unused)
{
    (void)unused;char *end;long v=strtol(value,&end,10);
    if(!*value||*end||v<REJECT_POCKET_MIN_CAPACITY||v>REJECT_POCKET_MAX_CAPACITY){explain("Enter a capacity from 30 to 100 notes.");return;}
    if(app_command_runtime_count_start_busy()||!setting_service_request_reject_pocket_max(v,machine_state_reject_pocket_max()))notify(UI_NOTICE_WARNING,"Capacity not sent. Finish the current operation and retry.");
    else clear_notice();
}
static void timeout_done(const char *value,void *unused)
{
    (void)unused;char *end;long v=strtol(value,&end,10);
    if(!*value||*end||v<0||v>60){explain("Enter 0 to disable automatic standby, or 1-60 minutes.");return;}
    standby_config_t config=*standby_config();config.minutes=v;
    if(!standby_store_save(&config))explain("Standby settings could not be saved. Try again shortly.");
    else ui_notice_post(UI_NOTICE_PROGRESS,"settings.standby","Standby settings","Saving...");
}
