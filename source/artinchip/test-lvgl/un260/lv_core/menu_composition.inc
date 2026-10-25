/* Private Menu presentation helpers. No persistence or protocol parsing here. */
static lv_obj_t *icon(lv_obj_t *p,const char *src,int x,int y,uint32_t color)
{
    lv_obj_t *im=ui_icon_create(p,src);lv_obj_set_pos(im,x,y);
    lv_obj_set_style_img_recolor(im,lv_color_hex(color),0);
    lv_obj_set_style_img_recolor_opa(im,LV_OPA_COVER,0);return im;
}
static lv_obj_t *segments(lv_obj_t *p,int x,int y,int w,int h,const char *const *names,unsigned count,unsigned value,unsigned action_base)
{
    if(!count)return NULL;
    lv_obj_t *base=box(p,x,y,w,h,HEADER_SURFACE,13);
    int inner=w-8-(count-1)*2,needed=0;
    const lv_font_t *normal=ui_message_font(&lv_font_instrument_sans_medium_16);
    const lv_font_t *bold=ui_message_font(&lv_font_instrument_sans_semibold_16);
    for(unsigned i=0;i<count;i++){
        lv_obj_t *v=button(base,4,4,inner/count,h-8,names[i],action_base+i,false);
        lv_damped_button_set_exact_palette(v,lv_color_hex(value==i?0xFFFFFF:HEADER_SURFACE),lv_color_hex(0xDCE4E8));
        lv_obj_set_style_radius(v,10,0);
        lv_obj_t *l=lv_obj_get_child(v,0);lv_obj_set_style_text_color(l,lv_color_hex(value==i?BLUE:MUTED),0);
        lv_obj_set_style_text_font(l,value==i?bold:normal,0);
        needed+=LV_MAX(menu_label_width(l,normal),menu_label_width(l,bold));
    }
    int padding=LV_MIN(12,LV_MAX(0,(inner-needed)/(int)count));
    needed+=padding*count;
    int spare=LV_MAX(0,inner-needed),left=4,prefix=0;
    for(unsigned i=0;i<count;i++){
        lv_obj_t *v=lv_obj_get_child(base,i),*l=lv_obj_get_child(v,0);
        int minimum=LV_MAX(menu_label_width(l,normal),menu_label_width(l,bold))+padding;
        /* Keep an unexpectedly wide resource bounded. Supported locales are
           checked against the full natural caption width in the Menu test. */
        int width=needed<=inner?minimum+(i+1)*spare/count-i*spare/count:
            (prefix+minimum)*inner/needed-prefix*inner/needed;
        prefix+=minimum;lv_obj_set_x(v,left);lv_obj_set_width(v,width);
        lv_label_set_long_mode(l,LV_LABEL_LONG_CLIP);lv_obj_set_width(l,LV_MAX(1,width-padding));
        lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);
        left+=width+2;
    }
    return base;
}
/* Format integer counting amounts without a lossy float conversion. */
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
