/* Menu-private geometry, matching Studio revision 04; all values come from services. */
static void context(lv_obj_t *p,const char *s,int x)
{
    /* Each content header starts with heading(). Reserve its actual translated
       width instead of overlapping two broad, independently RTL-aligned boxes. */
    lv_obj_t *title=lv_obj_get_child(p,0);
    lv_obj_update_layout(p);
    if(title&&lv_obj_check_type(title,&lv_label_class))x=LV_MAX(x,lv_obj_get_x(title)+lv_obj_get_width(title)+20);
    lv_obj_t *caption=small(p,x,14,LV_MAX(1,LV_MIN(420,850-x)),s);
    lv_label_set_long_mode(caption,LV_LABEL_LONG_DOT);
    lv_obj_set_width(caption,LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(caption,LV_MAX(1,LV_MIN(420,850-x)),0);
}
static void palette(lv_obj_t *b,uint32_t color)
{focus_style(b);lv_damped_button_set_exact_palette(b,lv_color_hex(color),lv_damped_button_pressed_color(lv_color_hex(color)));}
static lv_obj_t *link_button(lv_obj_t *p,int x,int y,int w,const char *s,int id)
{
    lv_obj_t *v=button(p,x,y,w,40,s,id,false);palette(v,lv_obj_get_style_bg_color(p,0).full&0xFFFFFF);
    lv_obj_set_style_bg_opa(v,LV_OPA_TRANSP,0);
    lv_obj_t *l=lv_obj_get_child(v,0);lv_obj_set_style_text_font(l,ui_message_font(&lv_font_instrument_sans_medium_16),0);
    lv_obj_set_style_text_color(l,lv_color_hex(BLUE),0);lv_label_set_long_mode(l,LV_LABEL_LONG_CLIP);
    lv_obj_set_width(l,w-30);lv_obj_align(l,LV_ALIGN_LEFT_MID,3,0);
    icon(v,MICON("next",18),w-21,11,BLUE);return v;
}
static lv_obj_t *white_button(lv_obj_t *p,int x,int y,int w,int h,const char *s,int id)
{lv_obj_t *v=button(p,x,y,w,h,s,id,false);palette(v,0xFFFFFF);return v;}
static void button_icon(lv_obj_t *v,const char *src,uint32_t color)
{
    lv_obj_t *l=lv_obj_get_child(v,0);lv_obj_set_width(l,LV_SIZE_CONTENT);
    lv_obj_t *im=ui_icon_create(v,src);lv_obj_set_style_img_recolor(im,lv_color_hex(color),0);lv_obj_set_style_img_recolor_opa(im,LV_OPA_COVER,0);
    lv_obj_update_layout(v);int iw=lv_obj_get_width(im),tw=lv_obj_get_width(l),x=(lv_obj_get_content_width(v)-iw-8-tw)/2;
    lv_obj_align(im,LV_ALIGN_LEFT_MID,x,0);lv_obj_align(l,LV_ALIGN_LEFT_MID,x+iw+8,0);
}
static lv_obj_t *field_button(lv_obj_t *p,int x,int y,int w,const char *s,int id)
{
    lv_obj_t *v=white_button(p,x,y,w,40,s,id);
    lv_obj_t *l=lv_obj_get_child(v,0);lv_obj_set_width(l,w-52);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_align(l,LV_ALIGN_LEFT_MID,13,0);
    icon(v,MICON("edit",18),w-31,11,MUTED);return v;
}
static void hero_amount(lv_obj_t *p,int x,int y,int w,uint64_t amount,bool inverse)
{
    char s[32];money_text(s,amount);char *dot=strchr(s,'.');if(dot)*dot=0;
    const lv_font_t *f=strlen(s)>10?&lv_font_instrument_sans_semibold_37:inverse?&lv_font_instrument_sans_semibold_52:&lv_font_instrument_sans_semibold_50;
    lv_obj_t *n=label(p,x,y,w,s,f,inverse?0xFFFFFF:INK);lv_obj_set_width(n,LV_SIZE_CONTENT);lv_obj_set_style_text_letter_space(n,-2,0);lv_obj_update_layout(n);
    const lv_font_t *fraction=inverse?&lv_font_instrument_sans_semibold_28:&lv_font_instrument_sans_medium_16;
    label(p,x+lv_obj_get_width(n)+1,y+f->line_height-f->base_line-fraction->line_height+fraction->base_line,62,".00",fraction,inverse?0xFFFFFF:MUTED);
}
static void avatar_size(lv_obj_t *p,int x,int y,int size,const workspace_avatar_t *a,const char *name,lv_img_dsc_t *d)
{
    lv_obj_t *frame=box(p,x,y,size,size,INK,size==72?20:15);lv_obj_set_style_clip_corner(frame,true,0);
    if(a&&a->present){
        *d=(lv_img_dsc_t){.header={.cf=LV_IMG_CF_TRUE_COLOR,.w=64,.h=64},.data_size=WORKSPACE_AVATAR_BYTES,.data=a->pixels};
        lv_obj_t *im=lv_img_create(frame);lv_img_set_src(im,d);lv_img_set_pivot(im,0,0);lv_img_set_zoom(im,size*256/64);
    }else{
        char initial[5]="U";
        if(name&&*name){unsigned n=_lv_txt_encoded_size(name);if(n&&n<sizeof(initial)){memcpy(initial,name,n);initial[n]=0;}}
        lv_obj_t *l=label(frame,0,0,size,initial,size==72?&lv_font_instrument_sans_medium_32:&lv_font_instrument_sans_medium_24,0xFFFFFF);lv_obj_center(l);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
    }
}
static void flag_icon(lv_obj_t *p,int x,int y,const char *code)
{
    /* Only display an installed flag; unsupported currencies retain their code. */
    const char *known[]={"CNY","USD","EUR"};
    for(unsigned i=0;i<3;i++)if(!strcmp(code,known[i])){
        char path[128];snprintf(path,sizeof(path),LVGL_DIR "menu_icons/flag_%s.png",code);
        lv_obj_t *im=lv_img_create(p);lv_img_set_src(im,path);lv_obj_set_pos(im,x,y);
        lv_img_header_t info;if(lv_img_decoder_get_info(path,&info)==LV_RES_OK&&info.w){lv_img_set_pivot(im,0,0);lv_img_set_zoom(im,30*256/info.w);}
        return;
    }
}
static void step_card(lv_obj_t *p,int x,int y,int w,int h,const char *n,const char *title,const char *detail,const char *src)
{
    lv_obj_t *v=box(p,x,y,w,h,PANEL,16);
    label(v,16,16,50,n,&lv_font_instrument_sans_semibold_16,BLUE);icon(v,src,w-38,14,BLUE);
    lv_obj_t *t=label(v,16,49,w-32,title,&lv_font_instrument_sans_semibold_19,INK);
    lv_obj_update_layout(t);int dy=49+lv_obj_get_height(t)+13;
    lv_obj_t *d=small(v,16,dy,w-32,detail);lv_obj_set_style_text_line_space(d,5,0);
}
static void overview(void)
{
    lv_obj_t *b=menu.body;char line[120],code[4];
    lv_obj_t *count=box(b,0,0,482,296,PANEL,20);
    icon(count,MICON("note",22),20,29,ORANGE);
    label(count,52,29,274,ui_tr("Ready to count"),&lv_font_instrument_sans_semibold_20,INK);
    link_button(count,365,20,97,ui_tr("Options"),303);
    lv_obj_t *batch=white_button(count,20,76,160,132,"",301);
    small(batch,16,14,126,ui_tr("Batch"));
    snprintf(line,sizeof(line),machine_state_batch_enabled()?"%u":ui_tr("OFF"),machine_state_batch_num());
    label(batch,16,38,133,line,&lv_font_instrument_sans_semibold_55,INK);
    small(batch,16,105,130,machine_state_batch_enabled()?ui_tr("notes per batch"):ui_tr("No batch limit"));icon(batch,MICON("next",18),130,99,ORANGE);
    currency_state_get_selected_code(code);flag_icon(count,202,77,code);
    label(count,241,77,207,currency_state_display_code(code),&lv_font_instrument_sans_semibold_28,INK);
    text(count,202,115,260,machine_state_mode()==MODE_CNT?ui_tr("Piece counting"):machine_state_mode()==MODE_SDC?ui_tr("Single denomination"):ui_tr("Mixed denominations"));
    snprintf(line,sizeof(line),ui_tr("%s start · ADD %s"),machine_state_work_mode()?ui_tr("Manual"):ui_tr("Auto"),machine_state_add_enabled()?ui_tr("on"):ui_tr("off"));small(count,202,145,260,line);
    link_button(count,202,169,181,ui_tr("Counting profiles"),302);
    text(count,20,246,65,ui_tr("Speed"));const char *speeds[]={ui_tr("Low"),ui_tr("Standard"),ui_tr("High")};segments(count,88,230,374,46,speeds,3,machine_state_speed(),400);
    lv_obj_t *today=box(b,498,0,452,296,BLUE,20);
    label(today,20,20,376,ui_tr("Today's confirmed total"),&lv_font_instrument_sans_medium_19,0xFFFFFF);icon(today,MICON("history",22),410,20,0xFFFFFF);
    uint32_t day=business_day();char date[24];short_day(date,sizeof(date),day);snprintf(line,sizeof(line),ui_tr("%s · currencies kept separate"),date);
    label(today,20,48,412,line,&lv_font_instrument_sans_menumedium_14,0xD9E7FB);
    cashbook_money_t totals[CASHBOOK_CURRENCIES];unsigned n=0,confirmed=0,pending=0;cashbook_totals(cashbook_store_get(),day,totals,&n,&confirmed,&pending);
    const cashbook_money_t *primary=NULL,*secondary=NULL;
    for(unsigned i=0;i<n;i++)if(!strcmp(totals[i].code,code))primary=&totals[i];
    if(!primary&&n)primary=&totals[0];
    for(unsigned i=0;i<n;i++)if(&totals[i]!=primary){secondary=&totals[i];break;}
    const char *primary_code=primary?primary->code:code;
    flag_icon(today,20,84,primary_code);label(today,58,84,190,primary_code,&lv_font_instrument_sans_medium_16,0xFFFFFF);
    icon(today,MICON("check",16),322,85,0xFFFFFF);label(today,344,84,88,ui_tr("Confirmed"),&lv_font_instrument_sans_menumedium_14,0xFFFFFF);
    hero_amount(today,20,111,412,primary?primary->amount:0,true);
    lv_obj_t *rule=box(today,20,184,412,1,0xFFFFFF,0);lv_obj_set_style_bg_opa(rule,LV_OPA_20,0);
    label(today,20,198,120,secondary?secondary->code:"",&lv_font_instrument_sans_medium_16,0xFFFFFF);
    if(secondary){money_text(line,secondary->amount);lv_obj_t *v=label(today,154,195,278,line,&lv_font_instrument_sans_semibold_20,0xFFFFFF);lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_RIGHT,0);}
    snprintf(line,sizeof(line),pending?ui_trn("Review %u pending counts", (pending)):ui_tr("View daily totals"),pending);
    lv_obj_t *review=white_button(today,20,232,412,44,line,306);lv_obj_set_style_text_color(review,lv_color_hex(BLUE),0);
    lv_obj_align(lv_obj_get_child(review,0),LV_ALIGN_LEFT_MID,17,0);icon(review,MICON("next",22),373,11,BLUE);
    lv_obj_t *person=box(b,966,0,266,82,PANEL,20);const workspace_user_t *u=user();
    avatar_size(person,16,17,48,u?&u->avatar:NULL,u?u->name:NULL,&menu.avatar_desc);
    lv_obj_t *name=label(person,76,21,174,u?u->name:ui_tr("Local operator"),&lv_font_instrument_sans_semibold_18,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    label(person,76,46,174,ui_tr("Current operator"),&lv_font_instrument_sans_medium_13,MUTED);
    const char *titles[]={ui_tr("Calculator"),ui_tr("Cash journal")},*icons[]={MICON("layers",19),MICON("history",19)};
    const counting_sim_t *result=counting_data_current();snprintf(line,sizeof(line),ui_trn("%u notes to check", (result?result->err_expected:0)),result?result->err_expected:0);
    for(unsigned i=0;i<2;i++){
        lv_obj_t *v=button(b,966,94+i*107,266,95,"",i?1901:1900,false);palette(v,PANEL);lv_obj_set_style_radius(v,18,0);
        icon(v,icons[i],17,35,i?ORANGE:BLUE);label(v,55,26,179,titles[i],&lv_font_instrument_sans_semibold_18,INK);
        label(v,55,53,184,ui_tr(i?UI_N_("Cash receipts and payments"):UI_N_("Ordinary and cash")),&lv_font_instrument_sans_medium_13,MUTED);
        icon(v,MICON("next",18),233,39,MUTED);
    }
}
static void batch_view(lv_obj_t *b)
{
    if(!menu.batch_dirty&&menu.owner!=(user()?user()->id:0))reset_batch();
    heading(b,ui_tr("Batch presets"));char line[64];snprintf(line,sizeof(line),ui_tr("%u / 10 slots"),menu.batch_count);context(b,line,181);
    if(menu.batch_dirty){button(b,689,0,92,36,ui_tr("Cancel"),500,false);button(b,789,0,92,36,ui_tr("Save"),A_SAVE_BATCH,true);}
    int add_width=LV_MAX(112,menu_text_width(ui_tr("Add slot"))+46);
    lv_obj_t *add=button(b,1028-add_width,0,add_width,36,ui_tr("Add slot"),A_ADD_BATCH,false);button_icon(add,MICON("plus",22),INK);if(menu.batch_count>=WORKSPACE_BATCHES)lv_obj_add_state(add,LV_STATE_DISABLED);
    for(unsigned j=0;j<5;j++){
        unsigned i=menu.batch_page*5+j;if(i>=menu.batch_count)break;
        lv_obj_t *v=lv_settings_button(b,j*145,77,j==4?134:135,128,"",false,pick_event,(void *)(uintptr_t)i);
        bool selected=i==menu.selected_batch;palette(v,selected?0xFFFFFF:PANEL);lv_obj_set_style_radius(v,14,0);lv_obj_set_style_border_width(v,selected?2:0,0);lv_obj_set_style_border_color(v,lv_color_hex(BLUE),0);
        label(v,14,14,107,i?ui_tr("Notes"):ui_tr("Fixed slot"),&lv_font_instrument_sans_medium_13,selected?BLUE:MUTED);
        snprintf(line,sizeof(line),i?"%u":ui_tr("OFF"),menu.batches[i]);label(v,14,43,107,line,&lv_font_instrument_sans_semibold_37,INK);
        bool active=machine_state_batch_enabled()?i&&menu.batches[i]==machine_state_batch_num():i==0;
        if(active){label(v,14,100,90,ui_tr("In use"),&lv_font_instrument_sans_medium_13,selected?BLUE:MUTED);icon(v,MICON("check",16),107,100,BLUE);}
    }
    small(b,0,231,470,ui_tr("1–199 notes · OFF is fixed"));
    lv_obj_t *prev=button(b,612,219,46,36,"",510,false),*next=button(b,668,219,46,36,"",511,false);
    icon(prev,MICON("back",22),12,7,MUTED);icon(next,MICON("next",22),12,7,MUTED);
    if(!menu.batch_page)lv_obj_add_state(prev,LV_STATE_DISABLED);
    if((menu.batch_page+1)*5>=menu.batch_count)lv_obj_add_state(next,LV_STATE_DISABLED);
    lv_obj_t *d=box(b,734,48,294,248,PANEL,20);
    label(d,20,18,222,menu.selected_batch?ui_tr("Selected preset"):ui_tr("No batch limit"),&lv_font_instrument_sans_semibold_21,INK);icon(d,MICON("layers",22),252,19,ORANGE);
    snprintf(line,sizeof(line),menu.selected_batch?"%u":ui_tr("OFF"),menu.batches[menu.selected_batch]);lv_obj_t *value=label(d,20,58,254,line,&lv_font_instrument_sans_semibold_50,INK);lv_obj_set_style_text_letter_space(value,-1,0);lv_obj_set_width(value,LV_SIZE_CONTENT);lv_obj_update_layout(value);
    if(menu.selected_batch)label(d,26+lv_obj_get_width(value),58+lv_font_instrument_sans_semibold_50.line_height-lv_font_instrument_sans_semibold_50.base_line-lv_font_instrument_sans_medium_16.line_height+lv_font_instrument_sans_medium_16.base_line,90,ui_tr("notes"),&lv_font_instrument_sans_medium_16,MUTED);
    small(d,20,122,254,menu.selected_batch?ui_tr("Stops when the batch is complete."):ui_tr("Count continuously without a preset."));
    if(menu.selected_batch){int edit_width=LV_MAX(83,menu_text_width(ui_tr("Edit"))+46);lv_obj_t *edit=white_button(d,20,146,edit_width,36,ui_tr("Edit"),A_EDIT_BATCH);button_icon(edit,MICON("edit",22),INK);lv_obj_t *remove=button(d,28+edit_width,146,137,36,ui_tr("Remove"),A_DELETE_BATCH,false);lv_obj_set_style_bg_opa(remove,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(remove,lv_color_hex(0xB33535),0);icon(remove,MICON("trash",22),9,7,0xB33535);lv_obj_align(lv_obj_get_child(remove,0),LV_ALIGN_LEFT_MID,40,0);if(menu.batch_count<=2)lv_obj_add_state(remove,LV_STATE_DISABLED);}
    bool active=!menu.batch_dirty&&(machine_state_batch_enabled()?menu.selected_batch&&menu.batches[menu.selected_batch]==machine_state_batch_num():!menu.selected_batch);
    lv_obj_t *apply=button(d,20,186,254,44,active?ui_tr("In use"):ui_tr("Use this preset"),A_APPLY_BATCH,true);if(active)button_icon(apply,MICON("check",22),0x64717C);if(active)lv_obj_add_state(apply,LV_STATE_DISABLED);
    lv_obj_t *main_batch=button(b,0,256,470,40,
        ui_tr(ui_state_main_batch_action_get()==UI_MAIN_BATCH_CYCLE?UI_N_("Main Batch: Cycle"):UI_N_("Main Batch: Settings")),
        A_BATCH_BEHAVIOR,true);
    lv_label_set_long_mode(lv_obj_get_child(main_batch,0),LV_LABEL_LONG_CLIP);
}
static bool profile_matches(const workspace_profile_t *p)
{
    workspace_profile_t c=current_profile();
    return p->speed==c.speed&&p->work==c.work&&p->add==c.add&&p->sort==c.sort&&p->beep==c.beep&&(!p->mode||p->mode==c.mode)&&p->batch_enabled==c.batch_enabled&&(!p->batch_enabled||p->batch==c.batch);
}
static const char *profile_display_name(const workspace_profile_t *profile)
{
    return workspace_profile_has_default_name(profile) ? ui_tr("Everyday count") : profile->name;
}
static void profiles_view(lv_obj_t *b)
{
    heading(b,ui_tr("Counting profiles"));int add_width=LV_MAX(141,menu_text_width(ui_tr("Save current"))+46);lv_obj_t *add=button(b,1028-add_width,0,add_width,36,ui_tr("Save current"),A_NEW_PROFILE,false);button_icon(add,MICON("plus",22),INK);
    const workspace_user_t *u=user();if(!u)return;if(menu.selected_profile>=u->profile_count)menu.selected_profile=u->profile_count-1;
    lv_obj_t *list=scroll(b,0,48,586,248);menu.task_scroll=list;
    int item_width=ui_scrollbar_content_width(list)-3;
    for(unsigned i=0;i<u->profile_count;i++){
        const workspace_profile_t *p=&u->profiles[i];bool selected=i==menu.selected_profile;
        lv_obj_t *v=lv_settings_button(list,3,2+i*80,item_width,72,"",false,pick_event,(void *)(uintptr_t)i);palette(v,selected?HEADER_SURFACE:PANEL);lv_obj_set_style_border_width(v,0,0);lv_obj_set_style_radius(v,14,0);
        if(selected)box(v,0,9,3,54,BLUE,2);
        icon(v,p->batch_enabled?MICON("layers",22):p->work&&p->speed==0?MICON("shield",22):MICON("note",22),16,25,MUTED);
        lv_obj_t *name=label(v,52,13,item_width-95,profile_display_name(p),&lv_font_instrument_sans_semibold_18,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
        char hint[96];snprintf(hint,sizeof(hint),"%s / %s",p->mode==2?ui_tr("Single denomination"):p->mode==3?ui_tr("Piece counting"):ui_tr("Mixed notes"),p->batch_enabled?ui_tr("Batch counting"):p->work?ui_tr("Manual start"):ui_tr("Continuous counting"));
        small(v,52,41,item_width-95,hint);icon(v,MICON("next",22),item_width-38,25,BLUE);
    }
    const workspace_profile_t *p=&u->profiles[menu.selected_profile];lv_obj_t *d=box(b,606,48,422,248,PANEL,20);
    lv_obj_t *name=label(d,20,18,382,profile_display_name(p),&lv_font_instrument_sans_semibold_21,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    profile_summary(d,p,61);
    int delete_width=LV_MAX(100,menu_text_width(ui_tr("Delete"))+53);
    lv_obj_t *del=button(d,20,194,delete_width,36,ui_tr("Delete"),A_DELETE_PROFILE,false);lv_obj_set_style_bg_opa(del,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(del,lv_color_hex(0xB33535),0);icon(del,MICON("trash",22),8,7,0xB33535);lv_obj_set_width(lv_obj_get_child(del,0),delete_width-45);lv_obj_align(lv_obj_get_child(del,0),LV_ALIGN_LEFT_MID,37,0);if(u->profile_count<=1)lv_obj_add_state(del,LV_STATE_DISABLED);
    bool active=profile_matches(p);lv_obj_t *apply=button(d,258,186,144,44,active?ui_tr("In use"):ui_tr("Apply profile"),A_APPLY_PROFILE,true);
    menu_button_fit_right(apply,402,144,24);lv_obj_center(lv_obj_get_child(apply,0));
    if(active)lv_obj_add_state(apply,LV_STATE_DISABLED);
}
static void option_label(lv_obj_t *p,unsigned row,const char *s)
{
    lv_obj_t *title=text(p,18,23+row*59,180,s);
    lv_label_set_long_mode(title,LV_LABEL_LONG_CLIP);
    lv_obj_set_width(title,LV_MAX(180,menu_label_width(title,NULL)));
    if(row)box(p,18,4+row*59,468,1,LINE,0);
}
static void options_view(lv_obj_t *b)
{
    heading(b,ui_tr("Counting options"));char value[64],code[4];currency_state_get_selected_code(code);snprintf(value,sizeof(value),ui_tr("%s · current configuration"),currency_state_display_code(code));context(b,value,225);
    lv_obj_t *left=box(b,0,48,504,248,PANEL,18),*right=box(b,524,48,504,248,PANEL,18);
    const char *modes[]={ui_tr("Mixed"),ui_tr("Single"),ui_tr("Count")},*speeds[]={ui_tr("Low"),ui_tr("Standard"),ui_tr("High")},*work[]={ui_tr("Auto"),ui_tr("Manual")},*sort[]={ui_tr("Off"),ui_tr("Face"),ui_tr("Orient."),ui_tr("Both")};
    option_label(left,0,ui_tr("Count mode"));segments(left,210,12,276,42,modes,3,machine_state_mode()==MODE_SDC?1:machine_state_mode()==MODE_CNT?2:0,450);
    option_label(left,1,ui_tr("Speed"));segments(left,210,71,276,42,speeds,3,machine_state_speed(),400);
    option_label(left,2,ui_tr("Start"));segments(left,210,130,276,42,work,2,machine_state_work_mode(),410);
    box(left,18,181,468,1,LINE,0);lv_obj_t *acc=box(left,18,191,468,46,PANEL,0);toggle(acc,0,468,ui_tr("ADD accumulation"),NULL,machine_state_add_enabled(),420+!machine_state_add_enabled());
    option_label(right,0,ui_tr("Sorting"));segments(right,210,12,276,42,sort,4,machine_state_fo_mode(),430);
    option_label(right,1,ui_tr("Reject pocket"));snprintf(value,sizeof(value),ui_trn("%u notes", (machine_state_reject_pocket_max())),machine_state_reject_pocket_max());
    lv_obj_t *capacity=field_button(right,346,72,140,value,A_CAPACITY);
    int capacity_width=menu_button_fit_right(capacity,486,140,52);
    lv_obj_align(lv_obj_get_child(capacity,0),LV_ALIGN_LEFT_MID,13,0);lv_obj_set_x(lv_obj_get_child(capacity,1),capacity_width-31);
    option_label(right,2,ui_tr("Current batch"));snprintf(value,sizeof(value),machine_state_batch_enabled()?ui_trn("%u notes", (machine_state_batch_num())):ui_tr("Off"),machine_state_batch_num());
    lv_obj_t *batch=link_button(right,366,131,120,value,301);
    int batch_width=menu_button_fit_right(batch,486,120,30);
    lv_obj_align(lv_obj_get_child(batch,0),LV_ALIGN_LEFT_MID,3,0);lv_obj_set_x(lv_obj_get_child(batch,1),batch_width-21);
    box(right,18,181,468,1,LINE,0);small(right,18,195,409,ui_tr("Mode changes clear the current result after confirmation. Saved records stay unchanged."));icon(right,MICON("shield",22),464,199,TEAL);
}

static void interaction_view(lv_obj_t *b)
{
    lv_obj_t *title=label(b,0,2,LV_SIZE_CONTENT,ui_tr("Interaction"),&lv_font_instrument_sans_semibold_26,INK);
    lv_label_set_long_mode(title,LV_LABEL_LONG_CLIP);lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_LEFT,0);
    lv_obj_update_layout(title);
    int controls_x=lv_obj_get_width(title)+24;
    int controls_width=LV_MAX(160,menu_text_width(ui_tr("Controls"))+48);
    int visibility_width=LV_MAX(238,menu_text_width(ui_tr("Pull-down shortcuts"))+48);
    lv_obj_t *tray=box(b,controls_x,0,controls_width+visibility_width+10,48,PANEL,14);
    lv_obj_t *controls=button(tray,4,4,controls_width,40,ui_tr("Controls"),A_INTERACTION_CONTROLS,false);
    lv_obj_t *visibility=button(tray,controls_width+6,4,visibility_width,40,ui_tr("Pull-down shortcuts"),A_SHORTCUT_VISIBILITY,false);
    header_surface(controls);header_surface(visibility);
    choose(controls,!menu.shortcut_visibility);choose(visibility,menu.shortcut_visibility);
    lv_obj_set_style_text_color(lv_obj_get_child(controls,0),lv_color_hex(menu.shortcut_visibility?MUTED:BLUE),0);
    lv_obj_set_style_text_color(lv_obj_get_child(visibility,0),lv_color_hex(menu.shortcut_visibility?BLUE:MUTED),0);
    if(!menu.shortcut_visibility){
        if(!lv_quick_controls_create_all(&menu.interaction_controls,b,0,60,1028,quick_tile)){
            small(b,0,72,1028,ui_tr("Quick action is unavailable right now."));return;
        }
        menu.interaction_created=true;
        lv_quick_controls_refresh(&menu.interaction_controls,lv_quick_controls_current_state());
        lv_content_pager_set_active(menu.interaction_controls.pager,menu.interaction_page,false);
        gesture_service_set_pointer_policy(UI_PAGE_MENU,menu_quick_pointer);
        return;
    }
    lv_obj_t *left=box(b,0,60,504,236,PANEL,20),*rows=box(left,20,8,464,120,PANEL,0);
    toggle(rows,0,464,ui_tr("Pull-down controls"),ui_tr("Swipe down from Main"),workspace_service_quick_enabled(),A_QUICK_TOGGLE);
    toggle(rows,61,464,ui_tr("QR after counting"),ui_tr("Show after a completed count"),user()&&user()->qr_after_count,A_AUTO_QR);
    box(rows,0,58,464,1,LINE,0);
    small(left,20,151,464,ui_tr("Hiding a shortcut keeps its function available in Controls."));
    lv_obj_t *right=box(b,524,60,504,236,PANEL,20);
    label(right,16,9,472,ui_tr("Pull-down shortcuts"),&lv_font_instrument_sans_semibold_17,INK);
    small(right,16,32,472,ui_tr("Tap to show or hide in pull-down controls."));
    lv_obj_t *shortcuts=box(right,16,64,472,156,PANEL,0);
    lv_obj_add_flag(shortcuts,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_USER_4);lv_obj_set_scroll_dir(shortcuts,LV_DIR_VER);lv_port_indev_set_drag_obj(shortcuts,true);
    menu.task_scroll=shortcuts;
    int item_width=(ui_scrollbar_content_width(shortcuts)-8)/2;
    uint8_t order[QUICK_CONTROL_COUNT];ui_state_quick_order_get(order);
    uint16_t mask=ui_state_quick_visible_get();
    unsigned shown=0;
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++){
        if(order[i]==QUICK_CASH_JOURNAL)continue;
        bool selected=(mask&(1U<<order[i]))!=0;
        lv_obj_t *item=button(shortcuts,(shown%2)*(item_width+8),(shown/2)*38,item_width,34,
            lv_quick_controls_title(order[i]),A_QUICK_ORDER_BASE+order[i],false);
        shown++;
        lv_obj_set_style_radius(item,9,0);
        palette(item,selected?0xE1EEFC:0xFFFFFF);
        lv_obj_t *title=lv_obj_get_child(item,0);
        lv_obj_set_width(title,item_width-48);lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_LEFT,0);lv_obj_align(title,LV_ALIGN_LEFT_MID,40,0);
        lv_obj_t *badge=box(item,9,8,18,18,selected?BLUE:0xFFFFFF,5);
        border(badge,selected?BLUE:LINE);
        if(selected)icon(badge,MICON("check",16),1,1,0xFFFFFF);
    }
}
static void display_view(lv_obj_t *b)
{
    heading(b,ui_tr("Display & sound"));
    lv_obj_t *left=box(b,0,48,504,248,PANEL,20);
    icon(left,MICON("volume",22),20,20,BLUE);
    label(left,52,18,400,ui_tr("Sound"),&lv_font_instrument_sans_semibold_21,INK);
    box(left,20,64,464,1,LINE,0);
    lv_obj_t *machine=box(left,20,77,464,68,0xFFFFFF,12);
    lv_obj_add_flag(machine,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(machine,action,LV_EVENT_CLICKED,(void *)(uintptr_t)(440+!machine_state_buzzer_enabled()));
    lv_obj_t *machine_row=box(machine,14,4,436,60,0xFFFFFF,0);
    toggle(machine_row,0,436,ui_tr("Machine sound"),ui_tr("Machine alerts"),machine_state_buzzer_enabled(),440+!machine_state_buzzer_enabled());
    lv_obj_t *touch=box(left,20,157,464,68,0xFFFFFF,12);
    lv_obj_add_flag(touch,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch,action,LV_EVENT_CLICKED,(void *)(uintptr_t)A_TOUCH_SOUND);
    lv_obj_t *touch_row=box(touch,14,4,436,60,0xFFFFFF,0);
    toggle(touch_row,0,436,ui_tr("Touch sound"),ui_tr("Screen taps"),user_cfg_touch_sound_enabled(),A_TOUCH_SOUND);
    lv_obj_t *right=box(b,524,48,504,248,PANEL,20);
    /* Full-screen display outline, without a desktop stand. */
    lv_obj_t *screen=box(right,20,22,25,18,PURPLE,4);
    lv_obj_t *glass=box(screen,2,2,21,14,PANEL,2);
    box(glass,2,10,17,2,0xD9CCF1,1);
    label(right,52,18,390,ui_tr("Display"),&lv_font_instrument_sans_semibold_21,INK);
    box(right,20,64,464,1,LINE,0);
    text(right,20,90,285,ui_tr("Interface language"));
    label(right,373,89,110,ui_lang_current()->name,&lv_font_instrument_sans_semibold_18,INK);
    box(right,20,123,464,1,LINE,0);
    text(right,20,144,300,ui_tr("Standby delay"));
    char duration[32];unsigned minutes=standby_config()->minutes;
    snprintf(duration,sizeof(duration),minutes?ui_tr("%u min"):ui_tr("Never"),minutes);
    field_button(right,325,132,159,duration,A_TIMEOUT);
    white_button(right,20,187,464,40,ui_tr("Enter standby"),A_STANDBY);
}

static void operators_summary_view(lv_obj_t *b)
{
    heading(b,ui_tr("Operators"));
    const workspace_model_t *m=workspace_store_get();const workspace_user_t *active=workspace_active(m);
    lv_obj_t *left=box(b,0,48,360,248,PANEL,20),*right=box(b,380,48,648,248,PANEL,20);
    small(left,20,18,320,ui_tr("Quick view"));text(left,20,51,320,active&&active->id?active->name:ui_tr("Device workspace"));
    small(left,20,93,320,ui_tr(m&&m->operators_enabled?UI_N_("Operators enabled"):UI_N_("Operators disabled")));
    small(left,20,126,320,ui_tr("Names label records; balances are shared."));
    small(left,20,151,320,ui_tr("Manage people and permissions in full view."));
    button(left,20,192,320,40,ui_tr("Manage operators"),1902,true);
    if(!m)return;
    if(menu.selected_user>=m->user_count)menu.selected_user=0;
    small(right,20,15,600,ui_tr("Select a person, then switch operator."));
    lv_obj_t *people=box(right,20,43,608,150,PANEL,0);
    lv_obj_add_flag(people,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_USER_4);lv_obj_set_scroll_dir(people,LV_DIR_VER);lv_port_indev_set_drag_obj(people,true);
    for(unsigned i=0;i<m->user_count;i++){
        const workspace_user_t *u=&m->users[i];bool current=m->operators_enabled&&u->id==m->active_id;
        lv_obj_t *item=lv_settings_button(people,(i%2)*296,(i/2)*76,284,68,"",false,pick_event,(void *)(uintptr_t)i);
        lv_damped_button_set_exact_palette(item,lv_color_hex(menu.selected_user==i?0xE1EEFC:0xFFFFFF),lv_color_hex(0xDCE4E8));
        lv_obj_set_style_border_width(item,0,0);lv_obj_set_style_radius(item,14,0);
        lv_obj_t *avatar=box(item,12,14,40,40,0xEBE5F7,12);char initial[5];operator_initial(u->name,initial);
        lv_obj_t *letter=text(avatar,0,0,40,initial);lv_obj_set_style_text_color(letter,lv_color_hex(PURPLE),0);lv_obj_set_style_text_align(letter,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(letter);
        lv_obj_t *name=text(item,64,12,208,u->name);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);small(item,64,39,208,ui_tr(m->disabled[i]?UI_N_("Disabled"):current?UI_N_("Current operator"):UI_N_("Available")));
        if(m->disabled[i])lv_obj_add_state(item,LV_STATE_DISABLED);
    }
    if(m->operators_enabled&&menu.selected_user<m->user_count&&!m->disabled[menu.selected_user]){
        small(right,20,213,374,m->users[menu.selected_user].name);
        lv_obj_t *switch_button=button(right,408,202,220,32,ui_tr(m->users[menu.selected_user].id==m->active_id?UI_N_("Current operator"):UI_N_("Switch operator")),A_USER_SWITCH,false);
        if(m->users[menu.selected_user].id==m->active_id)lv_obj_add_state(switch_button,LV_STATE_DISABLED);
    }else small(right,20,210,608,ui_tr(m->operators_enabled?UI_N_("This operator is disabled. Restore them in Manage operators."):UI_N_("Enable operators in Manage operators.")));
}
