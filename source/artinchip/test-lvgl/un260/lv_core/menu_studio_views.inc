/* Menu-private geometry, matching Studio revision 04; all values come from services. */
static void context(lv_obj_t *p,const char *s,int x)
{small(p,x,14,420,s);}
static void palette(lv_obj_t *b,uint32_t color)
{focus_style(b);lv_damped_button_set_exact_palette(b,lv_color_hex(color),lv_damped_button_pressed_color(lv_color_hex(color)));}
static lv_obj_t *link_button(lv_obj_t *p,int x,int y,int w,const char *s,int id)
{
    lv_obj_t *v=button(p,x,y,w,40,s,id,false);palette(v,lv_obj_get_style_bg_color(p,0).full&0xFFFFFF);
    lv_obj_set_style_bg_opa(v,LV_OPA_TRANSP,0);
    lv_obj_t *l=lv_obj_get_child(v,0);lv_obj_set_style_text_font(l,&lv_font_instrument_sans_medium_16,0);
    lv_obj_set_style_text_color(l,lv_color_hex(BLUE),0);lv_obj_align(l,LV_ALIGN_LEFT_MID,3,0);
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
        char initial[2]={name&&*name?*name:'U',0};
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
    label(count,52,29,274,"Ready to count",&lv_font_instrument_sans_semibold_20,INK);
    link_button(count,365,20,97,"Options",303);
    lv_obj_t *batch=white_button(count,20,76,160,132,"",301);
    small(batch,16,14,126,"Batch");
    snprintf(line,sizeof(line),machine_state_batch_enabled()?"%u":"OFF",machine_state_batch_num());
    label(batch,16,38,133,line,&lv_font_instrument_sans_semibold_55,INK);
    small(batch,16,105,130,machine_state_batch_enabled()?"notes per batch":"No batch limit");icon(batch,MICON("next",18),130,99,ORANGE);
    currency_state_get_selected_code(code);flag_icon(count,202,77,code);
    label(count,241,77,207,currency_state_display_code(code),&lv_font_instrument_sans_semibold_28,INK);
    text(count,202,115,260,machine_state_mode()==MODE_CNT?"Piece counting":machine_state_mode()==MODE_SDC?"Single denomination":"Mixed denominations");
    snprintf(line,sizeof(line),"%s start · ADD %s",machine_state_work_mode()?"Manual":"Auto",machine_state_add_enabled()?"on":"off");small(count,202,145,260,line);
    link_button(count,202,169,181,"Counting profiles",302);
    text(count,20,246,65,"Speed");const char *speeds[]={"Low","Standard","High"};segments(count,88,230,374,46,speeds,3,machine_state_speed(),400);
    lv_obj_t *today=box(b,498,0,452,296,BLUE,20);
    label(today,20,20,376,"Today's confirmed total",&lv_font_instrument_sans_medium_19,0xFFFFFF);icon(today,MICON("history",22),410,20,0xFFFFFF);
    uint32_t day=business_day();char date[24];short_day(date,sizeof(date),day);snprintf(line,sizeof(line),"%s · currencies kept separate",date);
    label(today,20,48,412,line,&lv_font_instrument_sans_menumedium_14,0xD9E7FB);
    cashbook_money_t totals[CASHBOOK_CURRENCIES];unsigned n=0,confirmed=0,pending=0;cashbook_totals(cashbook_store_get(),day,totals,&n,&confirmed,&pending);
    const cashbook_money_t *primary=NULL,*secondary=NULL;
    for(unsigned i=0;i<n;i++)if(!strcmp(totals[i].code,code))primary=&totals[i];
    if(!primary&&n)primary=&totals[0];
    for(unsigned i=0;i<n;i++)if(&totals[i]!=primary){secondary=&totals[i];break;}
    const char *primary_code=primary?primary->code:code;
    flag_icon(today,20,84,primary_code);label(today,58,84,190,primary_code,&lv_font_instrument_sans_medium_16,0xFFFFFF);
    icon(today,MICON("check",16),322,85,0xFFFFFF);label(today,344,84,88,"Confirmed",&lv_font_instrument_sans_menumedium_14,0xFFFFFF);
    hero_amount(today,20,111,412,primary?primary->amount:0,true);
    lv_obj_t *rule=box(today,20,184,412,1,0xFFFFFF,0);lv_obj_set_style_bg_opa(rule,LV_OPA_20,0);
    label(today,20,198,120,secondary?secondary->code:"",&lv_font_instrument_sans_medium_16,0xFFFFFF);
    if(secondary){money_text(line,secondary->amount);lv_obj_t *v=label(today,154,195,278,line,&lv_font_instrument_sans_semibold_20,0xFFFFFF);lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_RIGHT,0);}
    snprintf(line,sizeof(line),pending?"Review %u pending counts":"View daily totals",pending);
    lv_obj_t *review=white_button(today,20,232,412,44,line,306);lv_obj_set_style_text_color(review,lv_color_hex(BLUE),0);
    lv_obj_align(lv_obj_get_child(review,0),LV_ALIGN_LEFT_MID,17,0);icon(review,MICON("next",22),373,11,BLUE);
    lv_obj_t *person=box(b,966,0,266,82,PANEL,20);const workspace_user_t *u=user();
    avatar_size(person,16,17,48,u?&u->avatar:NULL,u?u->name:NULL,&menu.avatar_desc);
    lv_obj_t *name=label(person,76,21,174,u?u->name:"Local operator",&lv_font_instrument_sans_semibold_18,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    label(person,76,46,174,"Current operator",&lv_font_instrument_sans_medium_13,MUTED);
    const char *titles[]={"Print & export","Reject guide"},*hints[]={"Take a result with you",""},*icons[]={MICON("output",25),MICON("reject",25)};
    const counting_sim_t *result=counting_data_current();snprintf(line,sizeof(line),"%u notes to check",result?result->err_expected:0);
    for(unsigned i=0;i<2;i++){
        lv_obj_t *v=button(b,966,94+i*107,266,95,"",i?308:307,false);palette(v,PANEL);lv_obj_set_style_radius(v,18,0);
        icon(v,icons[i],17,35,i?ORANGE:BLUE);label(v,55,26,179,titles[i],&lv_font_instrument_sans_semibold_18,INK);
        label(v,55,53,184,i?line:hints[i],&lv_font_instrument_sans_medium_13,MUTED);icon(v,MICON("next",18),233,39,MUTED);
    }
}
static void batch_view(lv_obj_t *b)
{
    if(!menu.batch_dirty&&menu.owner!=(user()?user()->id:0))reset_batch();
    heading(b,"Batch presets");char line[64];snprintf(line,sizeof(line),"%u / 10 slots",menu.batch_count);context(b,line,181);
    if(menu.batch_dirty){button(b,689,0,92,36,"Cancel",500,false);button(b,789,0,92,36,"Save",A_SAVE_BATCH,true);}
    lv_obj_t *add=button(b,916,0,112,36,"Add slot",A_ADD_BATCH,false);button_icon(add,MICON("plus",22),INK);if(menu.batch_count>=WORKSPACE_BATCHES)lv_obj_add_state(add,LV_STATE_DISABLED);
    small(b,0,48,714,"Select a slot. Apply it when you are ready.");
    for(unsigned j=0;j<5;j++){
        unsigned i=menu.batch_page*5+j;if(i>=menu.batch_count)break;
        lv_obj_t *v=lv_settings_button(b,j*145,77,j==4?134:135,128,"",false,pick_event,(void *)(uintptr_t)i);
        bool selected=i==menu.selected_batch;palette(v,selected?0xFFFFFF:PANEL);lv_obj_set_style_radius(v,14,0);lv_obj_set_style_border_width(v,selected?2:0,0);lv_obj_set_style_border_color(v,lv_color_hex(BLUE),0);
        label(v,14,14,107,i?"Notes":"Fixed slot",&lv_font_instrument_sans_medium_13,selected?BLUE:MUTED);
        snprintf(line,sizeof(line),i?"%u":"OFF",menu.batches[i]);label(v,14,43,107,line,&lv_font_instrument_sans_semibold_37,INK);
        bool active=machine_state_batch_enabled()?i&&menu.batches[i]==machine_state_batch_num():i==0;
        if(active){label(v,14,100,90,"In use",&lv_font_instrument_sans_medium_13,selected?BLUE:MUTED);icon(v,MICON("check",16),107,100,BLUE);}
    }
    small(b,0,231,470,"1–199 notes · OFF is fixed");
    lv_obj_t *prev=button(b,612,219,46,36,"",510,false),*next=button(b,668,219,46,36,"",511,false);
    icon(prev,MICON("back",22),12,7,MUTED);icon(next,MICON("next",22),12,7,MUTED);
    if(!menu.batch_page)lv_obj_add_state(prev,LV_STATE_DISABLED);
    if((menu.batch_page+1)*5>=menu.batch_count)lv_obj_add_state(next,LV_STATE_DISABLED);
    lv_obj_t *d=box(b,734,48,294,248,PANEL,20);
    label(d,20,18,222,menu.selected_batch?"Selected preset":"No batch limit",&lv_font_instrument_sans_semibold_21,INK);icon(d,MICON("layers",22),252,19,ORANGE);
    snprintf(line,sizeof(line),menu.selected_batch?"%u":"OFF",menu.batches[menu.selected_batch]);lv_obj_t *value=label(d,20,58,254,line,&lv_font_instrument_sans_semibold_50,INK);lv_obj_set_style_text_letter_space(value,-1,0);lv_obj_set_width(value,LV_SIZE_CONTENT);lv_obj_update_layout(value);
    if(menu.selected_batch)label(d,26+lv_obj_get_width(value),58+lv_font_instrument_sans_semibold_50.line_height-lv_font_instrument_sans_semibold_50.base_line-lv_font_instrument_sans_medium_16.line_height+lv_font_instrument_sans_medium_16.base_line,90,"notes",&lv_font_instrument_sans_medium_16,MUTED);
    small(d,20,122,254,menu.selected_batch?"Stops when the batch is complete.":"Count continuously without a preset.");
    if(menu.selected_batch){lv_obj_t *edit=white_button(d,20,146,83,36,"Edit",A_EDIT_BATCH);button_icon(edit,MICON("edit",22),INK);lv_obj_t *remove=button(d,111,146,137,36,"Remove",A_DELETE_BATCH,false);lv_obj_set_style_bg_opa(remove,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(remove,lv_color_hex(0xB33535),0);icon(remove,MICON("trash",22),9,7,0xB33535);lv_obj_align(lv_obj_get_child(remove,0),LV_ALIGN_LEFT_MID,40,0);if(menu.batch_count<=2)lv_obj_add_state(remove,LV_STATE_DISABLED);}
    bool active=!menu.batch_dirty&&(machine_state_batch_enabled()?menu.selected_batch&&menu.batches[menu.selected_batch]==machine_state_batch_num():!menu.selected_batch);
    lv_obj_t *apply=button(d,20,186,254,44,active?"In use":"Use this preset",A_APPLY_BATCH,true);if(active)button_icon(apply,MICON("check",22),0x64717C);if(menu.batch_dirty||active)lv_obj_add_state(apply,LV_STATE_DISABLED);
}
static bool profile_matches(const workspace_profile_t *p)
{
    workspace_profile_t c=current_profile();
    return p->speed==c.speed&&p->work==c.work&&p->add==c.add&&p->sort==c.sort&&p->mode==c.mode&&p->batch_enabled==c.batch_enabled&&(!p->batch_enabled||p->batch==c.batch);
}
static void profiles_view(lv_obj_t *b)
{
    heading(b,"Counting profiles");lv_obj_t *add=button(b,887,0,141,36,"Save current",A_NEW_PROFILE,false);button_icon(add,MICON("plus",22),INK);
    const workspace_user_t *u=user();if(!u)return;if(menu.selected_profile>=u->profile_count)menu.selected_profile=u->profile_count-1;
    lv_obj_t *list=scroll(b,0,48,586,248);menu.task_scroll=list;
    for(unsigned i=0;i<u->profile_count;i++){
        const workspace_profile_t *p=&u->profiles[i];bool selected=i==menu.selected_profile;
        lv_obj_t *v=lv_settings_button(list,3,2+i*80,580,72,"",false,pick_event,(void *)(uintptr_t)i);palette(v,selected?HEADER_SURFACE:PANEL);lv_obj_set_style_border_width(v,0,0);lv_obj_set_style_radius(v,14,0);
        if(selected)box(v,0,9,3,54,BLUE,2);
        icon(v,p->batch_enabled?MICON("layers",22):p->work&&p->speed==0?MICON("shield",22):MICON("note",22),16,25,MUTED);
        lv_obj_t *name=label(v,52,13,485,p->name,&lv_font_instrument_sans_semibold_18,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
        char hint[96];snprintf(hint,sizeof(hint),"%s / %s",p->mode==2?"Single denomination":p->mode==3?"Piece counting":"Mixed notes",p->batch_enabled?"Batch counting":p->work?"Manual start":"Continuous counting");
        small(v,52,41,485,hint);icon(v,MICON("next",22),542,25,BLUE);
    }
    const workspace_profile_t *p=&u->profiles[menu.selected_profile];lv_obj_t *d=box(b,606,48,422,248,PANEL,20);
    lv_obj_t *name=label(d,20,18,382,p->name,&lv_font_instrument_sans_semibold_21,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    profile_summary(d,p,61);
    lv_obj_t *del=button(d,20,194,100,36,"Delete",A_DELETE_PROFILE,false);lv_obj_set_style_bg_opa(del,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(del,lv_color_hex(0xB33535),0);icon(del,MICON("trash",22),8,7,0xB33535);lv_obj_align(lv_obj_get_child(del,0),LV_ALIGN_LEFT_MID,37,0);if(u->profile_count<=1)lv_obj_add_state(del,LV_STATE_DISABLED);
    bool active=profile_matches(p);lv_obj_t *apply=button(d,258,186,144,44,active?"In use":"Apply profile",A_APPLY_PROFILE,true);if(active)lv_obj_add_state(apply,LV_STATE_DISABLED);
}
static void option_label(lv_obj_t *p,unsigned row,const char *s)
{text(p,18,23+row*59,180,s);if(row)box(p,18,4+row*59,468,1,LINE,0);}
static void options_view(lv_obj_t *b)
{
    heading(b,"Counting options");char value[64],code[4];currency_state_get_selected_code(code);snprintf(value,sizeof(value),"%s · current configuration",currency_state_display_code(code));context(b,value,225);
    lv_obj_t *left=box(b,0,48,504,248,PANEL,18),*right=box(b,524,48,504,248,PANEL,18);
    const char *modes[]={"Mixed","Single","Count"},*speeds[]={"Low","Standard","High"},*work[]={"Auto","Manual"},*sort[]={"Off","Face","Orient.","Both"};
    option_label(left,0,"Count mode");segments(left,210,12,276,42,modes,3,machine_state_mode()==MODE_SDC?1:machine_state_mode()==MODE_CNT?2:0,450);
    option_label(left,1,"Speed");segments(left,210,71,276,42,speeds,3,machine_state_speed(),400);
    option_label(left,2,"Start");segments(left,210,130,276,42,work,2,machine_state_work_mode(),410);
    box(left,18,181,468,1,LINE,0);lv_obj_t *acc=box(left,18,191,468,46,PANEL,0);toggle(acc,0,468,"ADD accumulation",NULL,machine_state_add_enabled(),420+!machine_state_add_enabled());
    option_label(right,0,"Sorting");segments(right,210,12,276,42,sort,4,machine_state_fo_mode(),430);
    option_label(right,1,"Reject pocket");snprintf(value,sizeof(value),"%u notes",machine_state_reject_pocket_max());field_button(right,346,72,140,value,A_CAPACITY);
    option_label(right,2,"Current batch");snprintf(value,sizeof(value),machine_state_batch_enabled()?"%u notes":"Off",machine_state_batch_num());link_button(right,366,131,120,value,301);
    box(right,18,181,468,1,LINE,0);small(right,18,205,409,"Changes never start the motor.");icon(right,MICON("shield",22),464,199,TEAL);
}
static void operator_view(lv_obj_t *b)
{
    heading(b,"Operators");const workspace_model_t *m=workspace_store_get();
    lv_obj_t *add=button(b,854,0,174,36,"New operator",A_NEW_USER,false);button_icon(add,MICON("plus",22),INK);
    lv_obj_t *list=scroll(b,0,48,340,248);menu.task_scroll=list;
    for(unsigned i=0;i<m->user_count;i++){
        const workspace_user_t *u=&m->users[i];bool selected=i==menu.selected_user,active=u->id==m->active_id;
        lv_obj_t *v=lv_settings_button(list,3,2+i*80,334,72,"",false,pick_event,(void *)(uintptr_t)i);palette(v,selected?HEADER_SURFACE:PANEL);lv_obj_set_style_border_width(v,0,0);lv_obj_set_style_radius(v,14,0);if(selected)box(v,0,9,3,54,BLUE,2);
        /* The list uses initials; selected USB photo is shown at full size in the detail. */
        avatar_size(v,16,15,42,NULL,u->name,NULL);
        lv_obj_t *name=label(v,72,13,220,u->name,&lv_font_instrument_sans_semibold_18,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
        small(v,72,41,220,active?"Current operator":u->employee_id);icon(v,active?MICON("check",22):MICON("next",22),296,25,BLUE);
    }
    if(menu.selected_user>=m->user_count)menu.selected_user=0;
    const workspace_user_t *u=&m->users[menu.selected_user];
    lv_obj_t *d=box(b,360,48,668,248,PANEL,18);avatar_size(d,20,20,72,&u->avatar,u->name,&menu.avatar_desc);
    lv_obj_t *name=label(d,112,22,536,u->name,&lv_font_instrument_sans_semibold_25,INK);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    if(u->id==m->active_id){icon(d,MICON("check",16),112,64,TEAL);label(d,135,63,470,"Active operator",&lv_font_instrument_sans_menumedium_14,TEAL);}
    small(d,112,94,260,"Employee ID");text(d,112,116,260,*u->employee_id?u->employee_id:"Not added");
    small(d,389,94,259,"Branch / team");text(d,389,116,259,*u->team?u->team:"Not added");
    lv_obj_t *edit=white_button(d,112,184,151,44,"Edit details",A_EDIT_USER);header_content(edit,"Edit details",MICON("edit",22),INK);
    bool active=u->id==m->active_id;lv_obj_t *sw=button(d,275,184,active?110:190,44,active?"In use":"Switch operator",A_USER_SWITCH,true);if(active){button_icon(sw,MICON("check",22),0x64717C);lv_obj_add_state(sw,LV_STATE_DISABLED);}
    if(menu.user_edit){
        menu.user_sheet=box(menu.root,0,0,1280,400,INK,0);lv_obj_set_style_bg_opa(menu.user_sheet,LV_OPA_30,0);lv_obj_add_flag(menu.user_sheet,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *sheet=box(menu.user_sheet,180,28,920,344,0xFFFFFF,22);lv_obj_add_flag(sheet,LV_OBJ_FLAG_CLICKABLE);
        label(sheet,22,22,812,menu.editing_user?"Edit operator":"New operator",&lv_font_instrument_sans_semibold_26,INK);
        lv_obj_t *close=button(sheet,860,20,38,38,"",A_USER_CANCEL,false);icon(close,MICON("close",22),8,8,INK);lv_nav_button_mark_back(close);
        lv_obj_t *photo=box(sheet,22,74,208,190,PANEL,16);avatar_size(photo,12,12,50,&menu.avatar,menu.user_name,&menu.form_avatar_desc);
        label(photo,72,20,112,"Photo",&lv_font_instrument_sans_medium_15,INK);label(photo,72,43,112,menu.avatar.present?"Imported":"Optional",&lv_font_instrument_sans_medium_12,MUTED);
        white_button(photo,12,74,184,38,"Import USB photo",A_USB_SCAN);
        label(photo,12,124,184,"avatar.png or\nun260_avatar_01-20.png\nPNG / up to 2 MB / 16-2048 px",&lv_font_instrument_sans_medium_12,MUTED);
        if(menu.avatar.present){lv_obj_t *remove=white_button(photo,168,8,28,28,"",A_REMOVE_PHOTO);icon(remove,MICON("close",18),5,5,MUTED);}
        small(sheet,254,77,644,"Operator name");
        white_button(sheet,254,100,644,44,*menu.user_name?menu.user_name:"Enter a name",A_EDIT_NAME);border(lv_obj_get_child(sheet,-1),LINE);
        small(sheet,254,159,312,"Employee ID / optional");small(sheet,586,159,312,"Branch / team / optional");
        lv_obj_t *id=white_button(sheet,254,182,312,44,*menu.employee_id?menu.employee_id:"Add employee ID",A_EMPLOYEE_ID);border(id,LINE);
        lv_obj_t *team=white_button(sheet,586,182,312,44,*menu.team?menu.team:"Add branch or team",A_TEAM);border(team,LINE);
        button(sheet,604,278,120,44,"Cancel",A_USER_CANCEL,false);button(sheet,738,278,160,44,menu.editing_user?"Save details":"Create operator",A_USER_SAVE,true);
    }
}
static void interaction_view(lv_obj_t *b)
{
    heading(b,"Interaction");context(b,"Your working preferences",147);
    lv_obj_t *left=box(b,0,48,504,248,PANEL,20),*rows=box(left,20,4,464,232,PANEL,0);
    toggle(rows,12,464,"Pull-down controls","Can always be reopened from Menu.",workspace_service_quick_enabled(),A_QUICK_TOGGLE);
    toggle(rows,88,464,"Side / two-finger gestures","Back buttons remain available.",gesture_service_enabled(),A_GESTURES);
    toggle(rows,164,464,"QR after counting","Manual export is always available.",user()&&user()->qr_after_count,A_AUTO_QR);
    box(rows,0,74,464,1,LINE,0);box(rows,0,150,464,1,LINE,0);
    lv_obj_t *graphic=box(b,544,52,476,68,PANEL,18);
    icon(graphic,MICON("screen",38),149,15,PURPLE);icon(graphic,MICON("down",38),217,15,PURPLE);icon(graphic,MICON("hand",38),285,15,PURPLE);
    label(b,544,132,476,"Quick access. Less interruption.",&lv_font_instrument_sans_semibold_22,INK);
    lv_obj_t *hint=label(b,544,171,476,"Adjust common controls without leaving the page you are working on.",&lv_font_instrument_sans_medium_15,MUTED);lv_obj_set_style_text_line_space(hint,6,0);
    lv_obj_t *try_quick=button(b,544,248,476,44,"Try Quick controls",A_QUICK,false);header_content(try_quick,"Try Quick controls",MICON("quick",22),INK);
}
static void display_view(lv_obj_t *b)
{
    heading(b,"Display & sound");brightness(b,0,48,504,110);
    lv_obj_t *left=box(b,6,172,492,53,0xFFFFFF,0);toggle(left,0,492,"Machine sound",NULL,machine_state_buzzer_enabled(),440+!machine_state_buzzer_enabled());
    box(b,0,217,504,1,LINE,0);text(b,6,241,260,"Standby delay");
    char duration[32];unsigned minutes=standby_config()->minutes;snprintf(duration,sizeof(duration),minutes?"%u min":"Never",minutes);field_button(b,359,228,139,duration,A_TIMEOUT);
    lv_obj_t *right=box(b,524,48,504,248,PANEL,20);
    text(right,20,38,320,"Interface language");label(right,403,38,81,"English",&lv_font_instrument_sans_semibold_18,INK);
    small(right,20,88,464,"Manage installed languages in protected Settings.");
    text(right,20,157,412,"Taking a break?");icon(right,MICON("power",22),462,155,PURPLE);
    white_button(right,20,186,464,44,"Enter standby",A_STANDBY);
}
