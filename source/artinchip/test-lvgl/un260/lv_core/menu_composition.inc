/* Private Menu presentation helpers. No persistence or protocol parsing here. */
static lv_obj_t *icon(lv_obj_t *p,const char *src,int x,int y,uint32_t color)
{
    lv_obj_t *im=ui_icon_create(p,src);lv_obj_set_pos(im,x,y);
    lv_obj_set_style_img_recolor(im,lv_color_hex(color),0);
    lv_obj_set_style_img_recolor_opa(im,LV_OPA_COVER,0);return im;
}
static void segments(lv_obj_t *p,int x,int y,int w,int h,const char *const *names,unsigned count,unsigned value,unsigned action_base)
{
    lv_obj_t *base=box(p,x,y,w,h,HEADER_SURFACE,13);
    int inner=w-8-(count-1)*2;
    for(unsigned i=0;i<count;i++){
        int left=4+i*inner/count+i*2,right=4+(i+1)*inner/count+i*2;
        lv_obj_t *v=button(base,left,4,right-left,h-8,names[i],action_base+i,false);
        lv_damped_button_set_exact_palette(v,lv_color_hex(value==i?0xFFFFFF:HEADER_SURFACE),lv_color_hex(0xDCE4E8));
        lv_obj_set_style_radius(v,10,0);
        lv_obj_t *l=lv_obj_get_child(v,0);lv_obj_set_style_text_color(l,lv_color_hex(value==i?BLUE:MUTED),0);
        lv_obj_set_style_text_font(l,value==i?&lv_font_instrument_sans_semibold_16:&lv_font_instrument_sans_medium_16,0);
    }
}
static uint32_t business_day(void)
{
    if(menu.record_day)return menu.record_day;
    if(menu.tab==2&&cashbook_store_view_archived()){
        const cashbook_t *book=cashbook_store_view();if(book->close_count)return book->closes[book->close_count-1].day;
        if(book->group_count)return book->groups[book->group_count-1].day;
    }
    if(cashbook_store_get()->business_day)return cashbook_store_get()->business_day;
    machine_time_value_t now;machine_time_get(&now);
    uint32_t day=now.year*10000U+now.month*100U+now.day;
    return cashbook_day_valid(day)?day:cashbook_store_get()->business_day;
}
static void short_day(char *out,size_t size,uint32_t day)
{
    static const char *months[]={UI_N_("Jan"),UI_N_("Feb"),UI_N_("Mar"),UI_N_("Apr"),UI_N_("May"),UI_N_("Jun"),UI_N_("Jul"),UI_N_("Aug"),UI_N_("Sep"),UI_N_("Oct"),UI_N_("Nov"),UI_N_("Dec")};
    unsigned month=day/100%100;
    if(month>=1&&month<=12)snprintf(out,size,"%u %s",day%100,ui_tr(months[month-1]));
    else snprintf(out,size,"%s",ui_tr("Set business date"));
}
/* Ledger amounts are integer values; format without a lossy float conversion. */
static void money_text(char out[32],uint64_t value)
{
    char digits[24];snprintf(digits,sizeof(digits),"%llu",(unsigned long long)value);
    size_t length=strlen(digits),used=0;
    for(size_t i=0;i<length;i++){
        if(i&&(length-i)%3==0)out[used++]=',';
        out[used++]=digits[i];
    }
    memcpy(out+used,".00",4);
}
static void currency_totals(lv_obj_t *p,int x,int y,int w,int h,uint32_t day)
{
    cashbook_money_t totals[CASHBOOK_CURRENCIES];unsigned n,confirmed,pending;
    bool ok=cashbook_totals(menu.tab==2?cashbook_store_view():cashbook_store_get(),day,totals,&n,&confirmed,&pending);
    lv_obj_t *card=box(p,x,y,w,h,BLUE,18);
    label(card,16,16,w-52,ui_tr("Confirmed"),&lv_font_instrument_sans_medium_16,0xFFFFFF);icon(card,MICON("check",20),w-36,16,0xFFFFFF);
    if(!cashbook_store_ready()||!ok){label(card,16,58,w-32,cashbook_store_message(),&lv_font_instrument_sans_menumedium_14,0xFFFFFF);return;}
    if(!n){label(card,16,68,w-32,ui_tr("No totals yet"),&lv_font_instrument_sans_semibold_21,0xFFFFFF);return;}
    char current[4];currency_state_get_selected_code(current);
    for(unsigned i=1;i<n;i++)if(!strcmp(totals[i].code,current)){
        cashbook_money_t first=totals[i];memmove(totals+1,totals,i*sizeof(*totals));totals[0]=first;break;
    }
    lv_obj_t *list=scroll(card,16,46,w-32,h-98);
    for(unsigned i=0;i<n;i++){
        label(list,0,i*53,w-32,totals[i].code,&lv_font_instrument_sans_menumedium_14,0xFFFFFF);
        char s[32];snprintf(s,sizeof(s),ui_trn("%u notes", (totals[i].pcs)),totals[i].pcs);label(list,0,i*53+20,85,s,&lv_font_instrument_sans_medium_12,0xD9E7FB);
        money_text(s,totals[i].amount);
        lv_obj_t *l=label(list,70,i*53+3,w-102,s,strlen(s)>12?&lv_font_instrument_sans_semibold_18:&lv_font_instrument_sans_semibold_24,0xFFFFFF);
        lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_RIGHT,0);
        if(i+1<n){lv_obj_t *rule=box(list,0,i*53+43,w-32,1,0xFFFFFF,0);lv_obj_set_style_bg_opa(rule,LV_OPA_20,0);}
    }
}
