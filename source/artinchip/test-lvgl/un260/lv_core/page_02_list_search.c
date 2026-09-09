#include "page_02_list_search.h"
#include "ui_frame_commit.h"
#include "lv_port_indev.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_serial_query.h"
#include "un260/lv_components/lv_alnum_keyboard.h"
#include "un260/lv_components/lv_card_surface.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_recycled_list.h"
#include "un260/lv_system/ui_text.h"
#include <stdio.h>
#include <string.h>

#define SEARCH_INK 0x17212A
#define SEARCH_BODY 0x4C606E
#define SEARCH_MUTED 0x7A8D9B
#define SEARCH_LINE 0xE7ECEF
#define SEARCH_OPTIONS 30
#define SEARCH_ROWS 6
#define SEARCH_ROW_HEIGHT 32

struct page_02_list_search {
    lv_obj_t *root, *scope, *input, *summary, *empty, *sort, *denom_title;
    lv_obj_t *contains, *exact, *starts, *ends, *exclude, *only, *except, *all;
    lv_obj_t *denom_buttons[SEARCH_OPTIONS];
    lv_recycled_list_t *list;
    lv_alnum_keyboard_t *keyboard;
    counting_serial_query_t query;
    counting_serial_query_result_t result;
    uint16_t slots[COUNTING_DATA_MAX_ITEMS];
    int denominations[SEARCH_OPTIONS];
    size_t denomination_count;
    uint32_t revision, pressed_revision;
    uint16_t pressed_slot;
    lv_point_t press;
    bool tapping, reset_position, data_dirty;
    page_02_list_search_close_fn close;
    void *context;
};

static void search_commit(void *context, uint32_t flags);
static lv_obj_t *search_surface(lv_obj_t *parent, int x, int y, int w, int h,
                                int radius, uint32_t color)
{
    lv_obj_t *o=lv_obj_create(parent);
    if (!o) return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);
    const lv_card_surface_style_t style={w,h,radius,color,SEARCH_LINE,0};
    lv_card_surface_apply(o,&style);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *search_label(lv_obj_t *parent,int x,int y,int w,int h,
                               const lv_font_t *font,uint32_t color,const char *text)
{
    lv_obj_t *o=lv_label_create(parent);
    if (!o) return NULL;
    lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,text ? text : "");
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *search_button(lv_obj_t *parent,int x,int y,int w,int h,
                                const char *text,lv_event_cb_t cb,void *context)
{
    const lv_damped_button_style_t style={0xF4F6F7,0xE0E3E5,0xF7F8F9,
                                         SEARCH_BODY,0xAAB5BE,8};
    lv_obj_t *o=lv_damped_button_create(parent,&style,text,&lv_font_instrument_sans_medium_14);
    if (!o) return NULL;
    if (!lv_damped_button_get_label(o)) { lv_obj_del(o);return NULL; }
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    if (!lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,context)) { lv_obj_del(o);return NULL; }
    return o;
}
static void search_selected(lv_obj_t *button,bool selected)
{
    if (lv_obj_has_state(button,LV_STATE_CHECKED)==selected) return;
    if (selected) lv_obj_add_state(button,LV_STATE_CHECKED);
    else lv_obj_clear_state(button,LV_STATE_CHECKED);
    uint32_t background=selected ? 0xDCE6EC : 0xF4F6F7;
    lv_damped_button_set_palette(button,lv_color_hex(background),lv_color_hex(background));
    lv_obj_set_style_text_color(lv_damped_button_get_label(button),
        lv_color_hex(selected ? SEARCH_INK : SEARCH_BODY),0);
    lv_obj_set_style_border_color(button,lv_color_hex(selected ? 0xA8BAC6 : SEARCH_LINE),0);
    lv_obj_set_style_border_width(button,selected ? 1 : 0,0);
}
static int selected_denom(const page_02_list_search_t *s,int denomination)
{
    for (unsigned i=0;i<s->query.denomination_count;++i)
        if (s->query.denominations[i]==denomination) return (int)i;
    return -1;
}
static void search_request(page_02_list_search_t *s,bool reset)
{
    ++s->revision;s->tapping=false;
    s->reset_position|=reset;
    if (!ui_frame_commit_defer(search_commit,s,1)) search_commit(s,1);
}
static void search_close_event(lv_event_t *e)
{
    page_02_list_search_t *s=lv_event_get_user_data(e);
    s->close(UINT16_MAX,s->context); /* May synchronously delete the workspace. */
}
static void search_controls(page_02_list_search_t *s)
{
    search_selected(s->contains,s->query.match==COUNTING_SERIAL_MATCH_CONTAINS);
    search_selected(s->exact,s->query.match==COUNTING_SERIAL_MATCH_EXACT);
    search_selected(s->starts,s->query.match==COUNTING_SERIAL_MATCH_PREFIX);
    search_selected(s->ends,s->query.match==COUNTING_SERIAL_MATCH_SUFFIX);
    search_selected(s->exclude,s->query.exclude_text);
    lv_damped_button_set_text(s->exclude,ui_text_get(s->query.exclude_text ?
        UI_TEXT_SERIAL_EXCLUDE_ON : UI_TEXT_SERIAL_EXCLUDE_OFF));
    search_selected(s->only,!s->query.exclude_denominations);
    search_selected(s->except,s->query.exclude_denominations);
    search_selected(s->all,s->query.denomination_count==0);
    for (size_t i=0;i<s->denomination_count;++i) {
        bool selected=selected_denom(s,s->denominations[i])>=0;
        search_selected(s->denom_buttons[i],selected);
        lv_damped_button_set_enabled(s->denom_buttons[i],selected ||
            s->query.denomination_count<COUNTING_DENOM_MAX_ITEMS);
    }
    lv_damped_button_set_text(s->sort,ui_text_get(s->query.descending ?
        UI_TEXT_SERIAL_DESCENDING : UI_TEXT_SERIAL_ASCENDING));
    lv_damped_button_set_text(s->input,s->query.text[0] ? s->query.text :
        ui_text_get(UI_TEXT_SERIAL_INPUT_HINT));
}
static void search_filter_event(lv_event_t *e)
{
    page_02_list_search_t *s=lv_event_get_user_data(e);
    lv_obj_t *target=lv_event_get_target(e);
    if (target==s->contains) s->query.match=COUNTING_SERIAL_MATCH_CONTAINS;
    else if (target==s->exact) s->query.match=COUNTING_SERIAL_MATCH_EXACT;
    else if (target==s->starts) s->query.match=COUNTING_SERIAL_MATCH_PREFIX;
    else if (target==s->ends) s->query.match=COUNTING_SERIAL_MATCH_SUFFIX;
    else if (target==s->exclude) s->query.exclude_text=!s->query.exclude_text;
    else if (target==s->only) s->query.exclude_denominations=false;
    else if (target==s->except) s->query.exclude_denominations=true;
    else if (target==s->all) s->query.denomination_count=0;
    else if (target==s->sort) s->query.descending=!s->query.descending;
    else {
        for (size_t i=0;i<s->denomination_count;++i) if (target==s->denom_buttons[i]) {
            int selected=selected_denom(s,s->denominations[i]);
            if (selected>=0) {
                for (unsigned j=(unsigned)selected+1;j<s->query.denomination_count;++j)
                    s->query.denominations[j-1]=s->query.denominations[j];
                --s->query.denomination_count;
            } else if (s->query.denomination_count<COUNTING_DENOM_MAX_ITEMS) {
                s->query.denominations[s->query.denomination_count++]=s->denominations[i];
            }
            break;
        }
    }
    search_request(s,true);
}
static void search_reset_event(lv_event_t *e)
{
    page_02_list_search_t *s=lv_event_get_user_data(e);
    memset(&s->query,0,sizeof(s->query));search_request(s,true);
}
static void search_input_submit(const char *text,void *context)
{
    page_02_list_search_t *s=context;
    snprintf(s->query.text,sizeof(s->query.text),"%s",text ? text : "");
    counting_serial_text_trim(s->query.text,sizeof(s->query.text),
                              s->query.text,strlen(s->query.text));
    search_request(s,true);
}
static void search_input_cancel(void *context)
{ search_request(context,false); }
static void search_input_event(lv_event_t *e)
{
    page_02_list_search_t *s=lv_event_get_user_data(e);
    if (!s->keyboard) {
        const lv_alnum_keyboard_config_t config={
            .title=ui_text_get(UI_TEXT_SERIAL_KEYBOARD_TITLE),
            .placeholder=ui_text_get(UI_TEXT_SERIAL_INPUT_HINT),
            .apply_text=ui_text_get(UI_TEXT_SERIAL_SEARCH),
            .clear_text=ui_text_get(UI_TEXT_SERIAL_CLEAR),
            .cancel_text=ui_text_get(UI_TEXT_SERIAL_ESC),
            .max_length=sizeof(s->query.text)-1,
            .submit=search_input_submit,.cancel=search_input_cancel,.context=s};
        s->keyboard=lv_alnum_keyboard_create(s->root,&config);
        if (!s->keyboard) {
            lv_label_set_text(s->scope,ui_text_get(UI_TEXT_SERIAL_INPUT_ERROR));
            return;
        }
    }
    lv_recycled_list_stop(s->list);s->tapping=false;
    lv_alnum_keyboard_set_text(s->keyboard,s->query.text);
    lv_alnum_keyboard_show(s->keyboard);
}
static lv_obj_t *search_row_create(lv_obj_t *parent,lv_coord_t width,void *context)
{
    (void)context;
    lv_obj_t *row=search_surface(parent,0,0,width,SEARCH_ROW_HEIGHT,5,0xFFFFFF);
    if (!row) return NULL;
    if (!search_label(row,10,7,74,22,&lv_font_instrument_sans_medium_14,SEARCH_MUTED,"") ||
        !search_label(row,102,4,486,26,&lv_font_instrument_sans_medium_20,SEARCH_INK,"") ||
        !search_label(row,592,5,112,26,&lv_font_instrument_sans_medium_18,SEARCH_BODY,"") ||
        !search_label(row,718,7,64,22,&lv_font_instrument_sans_medium_12,SEARCH_MUTED,
                      ui_text_get(UI_TEXT_SERIAL_LOCATE))) {
        lv_obj_del(row);return NULL;
    }
    lv_obj_set_style_text_align(lv_obj_get_child(row,2),LV_TEXT_ALIGN_RIGHT,0);
    return row;
}
static void search_row_bind(lv_obj_t *row,uint32_t index,void *context)
{
    page_02_list_search_t *s=context;
    const counting_sim_t *data=counting_data_current();
    int slot=index<s->result.written_count ? s->slots[index] : -1;
    bool valid=slot>=0 && slot<counting_data_serial_scan_limit(data) && data->sn_str[slot];
    char text[24];
    lv_obj_set_style_bg_color(row,lv_color_hex(index%2 ? 0xF4F6F7 : 0xFFFFFF),0);
    snprintf(text,sizeof(text),"%u",valid ? (unsigned)slot+1 : 0);
    lv_label_set_text(lv_obj_get_child(row,0),text);
    lv_label_set_text(lv_obj_get_child(row,1),valid ? data->sn_str[slot] : "");
    snprintf(text,sizeof(text),"%d",valid ? data->denom_mix[slot] : 0);
    lv_label_set_text(lv_obj_get_child(row,2),text);
}
static void search_pointer(lv_event_t *e)
{
    lv_event_code_t code=lv_event_get_code(e);
    if (code!=LV_EVENT_PRESSED && code!=LV_EVENT_PRESSING &&
        code!=LV_EVENT_RELEASED && code!=LV_EVENT_PRESS_LOST) return;
    page_02_list_search_t *s=lv_event_get_user_data(e);
    lv_indev_t *input=lv_event_get_indev(e);
    if (code==LV_EVENT_PRESS_LOST || !input) { s->tapping=false;return; }
    lv_point_t p;lv_indev_get_point(input,&p);
    if (code==LV_EVENT_PRESSED) {
        uint32_t index=UINT32_MAX;
        s->tapping=lv_recycled_list_index_at_point(s->list,&p,&index) &&
            index<s->result.written_count && !lv_alnum_keyboard_is_visible(s->keyboard);
        s->press=p;s->pressed_revision=s->revision;
        s->pressed_slot=s->tapping ? s->slots[index] : UINT16_MAX;
    } else {
        if (LV_ABS(p.x-s->press.x)>=6 || LV_ABS(p.y-s->press.y)>=6) s->tapping=false;
        if (code==LV_EVENT_RELEASED) {
            bool locate=s->tapping && s->pressed_revision==s->revision;
            s->tapping=false;
            if (locate) s->close(s->pressed_slot,s->context);
        }
    }
}
static void search_commit(void *context,uint32_t flags)
{
    (void)flags;
    page_02_list_search_t *s=context;
    if (lv_alnum_keyboard_is_visible(s->keyboard)) return;
    const counting_sim_t *data=counting_data_current();
    if (s->data_dirty) {
        size_t total=counting_serial_query_denominations(data,s->denominations,SEARCH_OPTIONS);
        s->denomination_count=LV_MIN(total,SEARCH_OPTIONS);
        for (size_t i=0;i<SEARCH_OPTIONS;++i) {
            if (i>=s->denomination_count) { lv_obj_add_flag(s->denom_buttons[i],LV_OBJ_FLAG_HIDDEN);continue; }
            char text[24];snprintf(text,sizeof(text),"%d",s->denominations[i]);
            lv_damped_button_set_text(s->denom_buttons[i],text);
            lv_obj_clear_flag(s->denom_buttons[i],LV_OBJ_FLAG_HIDDEN);
        }
        char text[96];
        if (total>SEARCH_OPTIONS) snprintf(text,sizeof(text),ui_text_get(UI_TEXT_SERIAL_DENOM_LIMIT),
                                          SEARCH_OPTIONS);
        else snprintf(text,sizeof(text),"%s",ui_text_get(UI_TEXT_SERIAL_DENOMINATIONS));
        lv_label_set_text(s->denom_title,text);
        s->data_dirty=false;
    }
    s->result=counting_serial_query_build(data,&s->query,s->slots,COUNTING_DATA_MAX_ITEMS);
    search_controls(s);
    lv_recycled_list_refresh(s->list,s->result.written_count,s->reset_position);
    s->reset_position=false;
    char text[96];snprintf(text,sizeof(text),ui_text_get(UI_TEXT_SERIAL_MATCH_COUNT),
                          (unsigned)s->result.matched_count,(unsigned)s->result.valid_count);
    lv_label_set_text(s->summary,text);
    if (s->result.written_count) lv_obj_add_flag(s->empty,LV_OBJ_FLAG_HIDDEN);
    else {
        lv_label_set_text(s->empty,ui_text_get(s->result.valid_count ?
            UI_TEXT_SERIAL_NO_MATCH : UI_TEXT_SERIAL_NO_RECORDS));
        lv_obj_clear_flag(s->empty,LV_OBJ_FLAG_HIDDEN);
    }
}
static void search_deleted(lv_event_t *e)
{
    if (lv_event_get_target(e)!=lv_event_get_current_target(e)) return;
    page_02_list_search_t *s=lv_event_get_user_data(e);
    ui_frame_commit_cancel(search_commit,s);
    lv_mem_free(s);
}
page_02_list_search_t *page_02_list_search_create(lv_obj_t *parent,
    page_02_list_search_close_fn close,void *context)
{
    if (!parent || !close) return NULL;
    page_02_list_search_t *s=lv_mem_alloc(sizeof(*s));
    if (!s) return NULL;
    memset(s,0,sizeof(*s));s->close=close;s->context=context;
    s->root=search_surface(parent,0,0,1280,400,0,0xD8E2E8);
    if (!s->root) { lv_mem_free(s);return NULL; }
    if (!lv_obj_add_event_cb(s->root,search_deleted,LV_EVENT_DELETE,s)) {
        lv_obj_del(s->root);lv_mem_free(s);return NULL;
    }
    lv_obj_add_flag(s->root,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s->root,LV_OBJ_FLAG_GESTURE_BUBBLE);
    if (!search_label(s->root,22,17,236,30,&lv_font_instrument_sans_semibold_22,
                      SEARCH_INK,ui_text_get(UI_TEXT_SERIAL_TITLE))) goto failed;
    s->scope=search_label(s->root,280,22,866,26,&lv_font_instrument_sans_medium_14,
                          SEARCH_MUTED,ui_text_get(UI_TEXT_SERIAL_SCOPE));
    lv_obj_t *back=lv_nav_button_create(s->root,1168,12,96,36,search_close_event,s);
    if (!s->scope || !back) goto failed;
    lv_damped_button_set_text(back,ui_text_get(UI_TEXT_SERIAL_ESC));
    lv_obj_t *filters=search_surface(s->root,16,60,410,328,15,0xFFFFFF);
    lv_obj_t *results=search_surface(s->root,438,60,826,328,15,0xFFFFFF);
    if (!filters || !results) goto failed;
    if (!search_label(filters,18,12,374,24,&lv_font_instrument_sans_semibold_14,
                      SEARCH_BODY,ui_text_get(UI_TEXT_SERIAL_MATCH_MODE))) goto failed;
#define FILTER(field,x,y,w,h,id) \
    s->field=search_button(filters,x,y,w,h,ui_text_get(id),search_filter_event,s); \
    if (!s->field) goto failed
    FILTER(contains,18,40,89,36,UI_TEXT_SERIAL_CONTAINS);
    FILTER(exact,113,40,89,36,UI_TEXT_SERIAL_EXACT);
    FILTER(starts,208,40,89,36,UI_TEXT_SERIAL_STARTS);
    FILTER(ends,303,40,89,36,UI_TEXT_SERIAL_ENDS);
    FILTER(exclude,18,84,374,34,UI_TEXT_SERIAL_EXCLUDE_OFF);
    if (!search_surface(filters,18,130,374,1,0,SEARCH_LINE)) goto failed;
    s->denom_title=search_label(filters,18,141,264,24,&lv_font_instrument_sans_semibold_14,
                               SEARCH_BODY,ui_text_get(UI_TEXT_SERIAL_DENOMINATIONS));
    if (!s->denom_title) goto failed;
    FILTER(all,290,135,102,30,UI_TEXT_SERIAL_ALL);
    FILTER(only,18,170,182,32,UI_TEXT_SERIAL_ONLY);
    FILTER(except,210,170,182,32,UI_TEXT_SERIAL_EXCEPT);
#undef FILTER
    lv_obj_t *grid=search_surface(filters,18,210,374,108,0,0xFFFFFF);
    if (!grid) goto failed;
    lv_obj_add_flag(grid,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid,LV_DIR_VER);lv_obj_set_scrollbar_mode(grid,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(grid,LV_OBJ_FLAG_SCROLL_CHAIN_VER | LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_port_indev_set_drag_obj(grid,true);
    for (unsigned i=0;i<SEARCH_OPTIONS;++i) {
        s->denom_buttons[i]=search_button(grid,(i%5)*76,(i/5)*40,68,34,"",search_filter_event,s);
        if (!s->denom_buttons[i]) goto failed;
    }
    s->input=search_button(results,16,14,674,40,ui_text_get(UI_TEXT_SERIAL_INPUT_HINT),search_input_event,s);
    if (!s->input || !search_button(results,702,14,108,40,ui_text_get(UI_TEXT_SERIAL_RESET),search_reset_event,s)) goto failed;
    lv_obj_t *input_text=lv_damped_button_get_label(s->input);
    lv_obj_set_width(input_text,642);lv_obj_set_style_text_align(input_text,LV_TEXT_ALIGN_LEFT,0);
    if (!search_label(results,26,72,74,22,&lv_font_instrument_sans_medium_12,
                      SEARCH_MUTED,ui_text_get(UI_TEXT_SERIAL_ORIGINAL_NO)) ||
        !search_label(results,118,72,486,22,&lv_font_instrument_sans_medium_12,
                      SEARCH_MUTED,ui_text_get(UI_TEXT_LIST_COL_SERIAL_NUMBER)) ||
        !search_label(results,648,72,102,22,&lv_font_instrument_sans_medium_12,
                      SEARCH_MUTED,ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_DENOM)) ||
        !search_surface(results,16,94,786,1,0,SEARCH_LINE)) goto failed;
    const lv_recycled_list_config_t config={16,96,810,SEARCH_ROWS,SEARCH_ROW_HEIGHT,
        search_row_create,search_row_bind,NULL,s};
    s->list=lv_recycled_list_create(results,&config);
    if (!s->list) goto failed;
    lv_obj_t *viewport=lv_recycled_list_object(s->list);
    lv_port_indev_set_drag_obj(viewport,true);
    if (!lv_obj_add_event_cb(viewport,search_pointer,LV_EVENT_ALL,s)) goto failed;
    s->empty=search_label(results,40,164,746,74,&lv_font_instrument_sans_medium_18,SEARCH_MUTED,"");
    s->summary=search_label(results,26,299,584,26,&lv_font_instrument_sans_medium_14,SEARCH_BODY,"");
    s->sort=search_button(results,618,292,192,32,ui_text_get(UI_TEXT_SERIAL_ASCENDING),search_filter_event,s);
    if (!s->empty || !s->summary || !s->sort) goto failed;
    lv_label_set_long_mode(s->empty,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s->empty,LV_TEXT_ALIGN_CENTER,0);
    s->data_dirty=s->reset_position=true;
    search_commit(s,1);lv_obj_move_foreground(s->root);
    return s;
failed:
    lv_obj_del(s->root);return NULL;
}
void page_02_list_search_data_changed(page_02_list_search_t *s)
{
    if (!s) return;
    s->data_dirty=true;search_request(s,false);
}
void page_02_list_search_destroy(page_02_list_search_t *s)
{
    if (s) lv_obj_del(s->root);
}
