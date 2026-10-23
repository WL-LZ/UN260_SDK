#include "un260/lv_resources/ui_page_background.h"
#include "page_02_list.h"

#include "un260/font/ui_message_font.h"
#include "page_02_list_data.h"
#include "page_02_list_search.h"
#include "page_02_list_serials.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_components/ui_multi_detail.h"
#include "un260/gesture/gesture_service.h"
#include "lv_page_event.h"
#include "lv_page_manager.h"
#include "ui_frame_commit.h"
#include "lv_port_indev.h"
#include "un260/counting/counting_data_store.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_components/lv_recycled_list.h"
#include "un260/lv_components/lv_card_surface.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"
#include "aic_ui/perf_stats.h"
#include <stdio.h>
#include <string.h>

/* Page layout and semantic binding only. The common viewport owns bounded
 * recycling/navigation. No bitmap background or invisible paging buttons. */
#define ALL_SECTIONS ((1U << PAGE_02_SECTION_COUNT) - 1)
#define ROWS 7
#define ROW_HEIGHT 36
#define PANEL_Y 12
#define PANEL_HEIGHT 376
#define ACTION_GAP 8
#define ACTION_HEIGHT ((PANEL_HEIGHT - (LIST_ACTION_COUNT - 1) * ACTION_GAP) / LIST_ACTION_COUNT)
#define BODY_Y 76
#define HEADER_TOP_Y 48
#define HEADER_BOTTOM_Y (BODY_Y - 1)
#define FOOTER_Y (BODY_Y + ROWS * ROW_HEIGHT)
#define INK 0x17212A
#define BODY 0x4C606E
#define MUTED 0x7A8D9B
#define LINE 0xE7ECEF
#define AMBER 0xA67834
#define ICON_INK 0x657F90

enum {
    LIST_ACTION_HISTORY, LIST_ACTION_SEARCH, LIST_ACTION_PRINT, LIST_ACTION_MAIN,
    LIST_ACTION_COUNT
};
typedef struct {
    page_02_section_id_t id;
    lv_coord_t x, width;
    lv_coord_t col_x[3], col_w[3];
    lv_text_align_t align[3];
    ui_text_id_t title, columns[3];
    const char *title_icon, *empty_icon;
    uint32_t badge_color;
} section_layout_t;
typedef struct {
    const section_layout_t *layout;
    lv_obj_t *panel, *title, *headers[3], *range, *mode, *previous, *next, *empty, *empty_text, *details;
    lv_recycled_list_t *list;
} list_section_t;
typedef struct {
    lv_obj_t *page, *total_title, *pcs, *amount;
    lv_obj_t *actions[LIST_ACTION_COUNT];
    page_02_list_search_t *search;
    page_02_list_serials_t *serials;
    ui_multi_detail_t *multi;
    bool multi_visible;
    uint16_t located_slot;
    list_section_t section[PAGE_02_SECTION_COUNT];
    page_02_list_data_t data;
    language_t language;
} list_view_t;
static list_view_t *view;
static int requested_currency=-1,requested_tab;
void page_02_list_multi_open(int currency,int tab)
{requested_currency=currency;requested_tab=tab;}
static bool multi_gesture(gesture_action_t action)
{return view&&view->multi_visible&&(action==GESTURE_ACTION_EXIT_PAGE||action==GESTURE_ACTION_HOME)&&ui_multi_detail_back(view->multi);}
static uint32_t dirty = ALL_SECTIONS, reset_positions = ALL_SECTIONS;
static void commit(void *context,uint32_t flags);
static void search_open(lv_event_t *e);
static void serials_open(lv_event_t *e);
static void serials_discard(void);
static void base_view_visibility(void);
static bool list_monetary_result_supported(void)
{
    return !currency_state_multi_selected() &&
           counting_data_monetary_result_supported(counting_data_current());
}
static const section_layout_t layouts[PAGE_02_SECTION_COUNT] = {
    { PAGE_02_SECTION_A, 16, 360, {10,112,192}, {88,64,122},
      {LV_TEXT_ALIGN_LEFT,LV_TEXT_ALIGN_RIGHT,LV_TEXT_ALIGN_RIGHT},
      UI_TEXT_LIST_DENOMINATIONS, {UI_TEXT_PAGE01_DETAIL_COL_DENOM,
      UI_TEXT_PAGE01_DETAIL_COL_PCS,UI_TEXT_PAGE01_DETAIL_COL_AMOUNT},
      LVGL_DIR "list_icons/receipt_24.png", LVGL_DIR "list_icons/receipt_24.png", 0x2BD900 },
    { PAGE_02_SECTION_B, 388, 430, {10,58,308}, {42,242,76},
      {LV_TEXT_ALIGN_LEFT,LV_TEXT_ALIGN_LEFT,LV_TEXT_ALIGN_RIGHT},
      UI_TEXT_LIST_SERIAL_NUMBERS, {UI_TEXT_PAGE01_DETAIL_COL_NO,
      UI_TEXT_LIST_COL_SERIAL_NUMBER,UI_TEXT_PAGE01_DETAIL_COL_DENOM},
      LVGL_DIR "list_icons/barcode_24.png", LVGL_DIR "list_icons/barcode_36.png", 0x0074F8 },
    { PAGE_02_SECTION_C, 830, 326, {10,58,110}, {44,40,170},
      {LV_TEXT_ALIGN_LEFT,LV_TEXT_ALIGN_RIGHT,LV_TEXT_ALIGN_LEFT},
      UI_TEXT_LIST_REJECT_TITLE, {UI_TEXT_PAGE01_DETAIL_COL_NO,
      UI_TEXT_PAGE01_DETAIL_COL_PCS,UI_TEXT_LIST_COL_REASON},
      LVGL_DIR "list_icons/warning_circle_24.png", LVGL_DIR "list_icons/warning_circle_36.png", 0xF85820 },
};
static const char *const inv_names[] = {
    "LIST_DENOM_SCROLL", "LIST_SERIAL_SCROLL", "LIST_ERROR_SCROLL"
};

static void text_set(lv_obj_t *label, const char *text)
{
    if (!text) text="";
    if (label && strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
}
static void number_set(lv_obj_t *label, double value, const lv_font_t *preferred)
{
    if (!label || !preferred) return;
    char text[40];
    snprintf(text, sizeof(text), "%.0f", value);
    const lv_font_t *font=preferred;
    const lv_font_t *fallback[]={&lv_font_instrument_sans_medium_18,
        &lv_font_instrument_sans_medium_16,&lv_font_instrument_sans_medium_14};
    /* These numeric cells have fixed pixel widths. Before the first layout,
     * lv_obj_get_width() can still be zero (or stale after resizing), which
     * incorrectly selects the smallest font until the next page refresh. */
    const lv_coord_t available_width=lv_obj_get_style_width(label,0);
    lv_point_t size;
    lv_txt_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    for (unsigned i=0;i<sizeof(fallback)/sizeof(fallback[0]) && size.x>available_width;++i) {
        if (fallback[i]->line_height>=font->line_height) continue;
        font=fallback[i];
        lv_txt_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    }
    if (lv_obj_get_style_text_font(label,0)!=font) lv_obj_set_style_text_font(label,font,0);
    text_set(label, text);
}
static void reject_reason_set(lv_obj_t *label, const char *text)
{
    if (!label) return;
    const lv_font_t *const sizes[]={&lv_font_instrument_sans_medium_18,
        &lv_font_instrument_sans_medium_16,&lv_font_instrument_sans_medium_14};
    const lv_coord_t available_width=lv_obj_get_style_width(label,0);
    const lv_font_t *font=NULL;
    lv_point_t size;
    /* Protocol names keep English metrics and punctuation in every locale. */
    for (unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
        font=ui_message_font_for(sizes[i],LANGUAGE_EN);
        lv_txt_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if (size.x<=available_width) break;
    }
    if (lv_obj_get_style_text_font(label,0)!=font) lv_obj_set_style_text_font(label,font,0);
    const lv_coord_t height=lv_font_get_line_height(font);
    lv_obj_set_height(label,height);
    lv_obj_set_y(label,(ROW_HEIGHT-height)/2);
    text_set(label,text);
}
static lv_obj_t *label_create(lv_obj_t *parent, int x, int y, int w, int h,
                              const lv_font_t *font, uint32_t color,
                              lv_text_align_t align)
{
    if (!parent || !font) return NULL;
    lv_obj_t *o = lv_label_create(parent);
    if (!o) return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_text_font(o, ui_message_font(font), 0);
    lv_obj_set_style_base_dir(o, LV_BASE_DIR_AUTO, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(o, align, 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_label_set_text(o, "");
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *surface(lv_obj_t *parent, int x, int y, int w, int h,
                         int radius, uint32_t color)
{
    if (!parent) return NULL;
    lv_obj_t *o = lv_obj_create(parent);
    if (!o) return NULL;
    lv_obj_remove_style_all(o); lv_obj_set_pos(o, x, y);
    lv_card_surface_style_t skin = {w,h,radius,color,LINE,0};
    lv_card_surface_apply(o, &skin);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* Section images use the existing compiled-asset registry and lazy DMA cache.
 * Separate native sizes avoid scaling the light strokes at draw time. */
static bool section_image_create(lv_obj_t *parent,int x,int y,const char *source)
{
    if (!parent || !source) return false;
    lv_obj_t *o=lv_img_create(parent);
    if (!o) return false;
    lv_img_set_src(o,source);
    lv_obj_set_pos(o,x,y);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return true;
}

/* Page-local action glyphs share the existing line weight and palette. */
enum { ICON_HISTORY, ICON_SEARCH, ICON_PRINT, ICON_HOME, ICON_PREV, ICON_NEXT, ICON_TOGGLE };
static const struct {
    ui_text_id_t title;
    int icon;
    lv_event_cb_t clicked;
} action_layouts[LIST_ACTION_COUNT] = {
    [LIST_ACTION_HISTORY] = {UI_TEXT_LIST_HISTORY, ICON_HISTORY, page_02_history_btn_event_cb},
    [LIST_ACTION_SEARCH] = {UI_TEXT_SERIAL_SEARCH, ICON_SEARCH, search_open},
    [LIST_ACTION_PRINT] = {UI_TEXT_LIST_PRINT, ICON_PRINT, page_01_print_btn_event_cb},
    [LIST_ACTION_MAIN] = {UI_TEXT_LIST_MAIN, ICON_HOME, page_01_back_btn_event_cb},
};
static void icon_line(lv_draw_ctx_t *ctx, lv_color_t color, lv_coord_t x, lv_coord_t y,
                       int x1, int y1, int x2, int y2)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.color=color; d.width=2; d.round_start=d.round_end=1;
    lv_point_t a={x+x1,y+y1}, b={x+x2,y+y2};
    lv_draw_line(ctx,&d,&a,&b);
}
static void icon_draw(lv_event_t *e)
{
    lv_obj_t *o=lv_event_get_target(e);
    lv_area_t area; lv_obj_get_coords(o,&area);
    lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(e);
    lv_color_t color=lv_color_hex(ICON_INK);
    int kind=(int)(uintptr_t)lv_event_get_user_data(e), x=area.x1, y=area.y1;
#define L(a,b,c,d) icon_line(ctx,color,x,y,a,b,c,d)
    if (kind==ICON_PRINT) {
        L(7,8,7,2); L(7,2,20,2); L(20,2,20,8); L(3,8,24,8);
        L(3,8,3,19); L(24,8,24,19); L(3,19,7,19); L(20,19,24,19);
        L(7,15,20,15); L(7,15,7,25); L(7,25,20,25); L(20,25,20,15); L(20,11,21,11);
    } else if (kind==ICON_HISTORY) {
        lv_draw_arc_dsc_t d; lv_draw_arc_dsc_init(&d); d.color=color; d.width=2;
        lv_point_t p={x+13,y+13}; lv_draw_arc(ctx,&d,&p,11,210,180);
        L(2,4,2,10); L(2,10,8,10); L(13,6,13,13); L(13,13,18,16);
    } else if (kind==ICON_SEARCH) {
        lv_draw_arc_dsc_t d; lv_draw_arc_dsc_init(&d); d.color=color; d.width=2;
        lv_point_t p={x+11,y+11}; lv_draw_arc(ctx,&d,&p,8,0,360);
        L(17,17,24,24);
    } else if (kind==ICON_PREV) { L(15,7,9,13); L(9,13,15,19); }
    else if (kind==ICON_TOGGLE) { L(8,10,13,15); L(13,15,18,10); }
    else { L(10,7,16,13); L(16,13,10,19); }
#undef L
}
static bool icon_create(lv_obj_t *parent,int x,int y,int kind)
{
    if (kind==ICON_HOME) {
        lv_obj_t *home=lv_nav_home_icon_create(parent);
        if (!home) return false;
        lv_obj_set_pos(home,x,y);
        return true;
    }
    lv_obj_t *o=surface(parent,x,y,27,27,0,0xFFFFFF);
    if (!o) return false;
    lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);
    if (!lv_obj_add_event_cb(o,icon_draw,LV_EVENT_DRAW_MAIN,(void *)(uintptr_t)kind)) {
        lv_obj_del(o);
        return false;
    }
    return true;
}
static lv_obj_t *button_create(lv_obj_t *parent,int x,int y,int w,int h,
                               const lv_font_t *font,lv_event_cb_t cb,void *data)
{
    if (!parent || !font || !cb) return NULL;
    const lv_damped_button_style_t style={0xFFFFFF,0xEBEBEB,0xF6F7F8,BODY,0xAAB5BE,12};
    lv_obj_t *o=lv_damped_button_create(parent,&style,"",font);
    if (!o) return NULL;
    if (!lv_damped_button_get_label(o)) {
        lv_obj_del(o);
        return NULL;
    }
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_border_width(o,1,0);
    lv_obj_set_style_border_color(o,lv_color_hex(LINE),0);
    if (!lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,data)) {
        lv_obj_del(o);
        return NULL;
    }
    return o;
}
static lv_obj_t *row_create(lv_obj_t *parent,lv_coord_t width,void *context)
{
    list_section_t *s=context;
    if (!parent || !s || !s->layout) return NULL;
    lv_obj_t *row=surface(parent,0,0,width,ROW_HEIGHT,5,0xFFFFFF);
    if (!row) return NULL;
    for (unsigned col=0;col<3;++col) {
        uint32_t color=col==0 && s->layout->id!=PAGE_02_SECTION_A ? MUTED : BODY;
        if (s->layout->id==PAGE_02_SECTION_C && col==1) color=AMBER;
        const lv_font_t *font=&lv_font_instrument_sans_medium_20;
        bool small=col==0 && s->layout->id!=PAGE_02_SECTION_A;
        if (small) font=&lv_font_instrument_sans_medium_14;
        if (s->layout->id==PAGE_02_SECTION_C && col==2) font=&lv_font_instrument_sans_medium_18;
        if (!label_create(row,s->layout->col_x[col],small ? 9 : 5,s->layout->col_w[col],28,
                          font,color,s->layout->align[col])) {
            lv_obj_del(row);
            return NULL;
        }
    }
    return row;
}
static void row_bind(lv_obj_t *row,uint32_t index,void *context)
{
    list_section_t *s=context;
    const counting_sim_t *data=counting_data_current();
    lv_obj_t *a=lv_obj_get_child(row,0),*b=lv_obj_get_child(row,1),*c=lv_obj_get_child(row,2);
    lv_obj_set_style_bg_color(row,lv_color_hex(index%2 ? 0xF4F6F7 : 0xFFFFFF),0);
    if (s->layout->id==PAGE_02_SECTION_A) {
        if (!list_monetary_result_supported()) {
            text_set(a,"--");
            number_set(b,data->total_pcs,&lv_font_instrument_sans_medium_20);
            text_set(c,"--");
            return;
        }
        const denom_t zero={0};
        const denom_t *d=index<view->data.denom_count ? &data->denom[view->data.denom[index]] : &zero;
        number_set(a,d->value,&lv_font_instrument_sans_medium_20);
        number_set(b,d->pcs,&lv_font_instrument_sans_medium_20);
        number_set(c,d->amount,&lv_font_instrument_sans_medium_20);
    } else if (s->layout->id==PAGE_02_SECTION_B) {
        int slot=index<view->data.serial_count ? view->data.serial[index] : -1;
        number_set(a,slot>=0 ? slot+1 : 0,&lv_font_instrument_sans_medium_14);
        bool valid=slot>=0 && slot<counting_data_serial_scan_limit(data) && data->sn_str[slot];
        if (valid && slot==view->located_slot)
            lv_obj_set_style_bg_color(row,lv_color_hex(0xDCE6EC),0);
        text_set(b,valid ? data->sn_str[slot] : "");
        if (list_monetary_result_supported())
            number_set(c,valid ? data->denom_mix[slot] : 0,&lv_font_instrument_sans_medium_20);
        else text_set(c,"--");
    } else {
        bool valid=index<(uint32_t)counting_data_error_detail_count(data);
        number_set(a,index+1,&lv_font_instrument_sans_medium_14);
        number_set(b,valid ? data->err_pcs[index] : 0,&lv_font_instrument_sans_medium_20);
        reject_reason_set(c,ui_text_counting_reject_reason(valid ? data->err_code[index] : UINT8_MAX));
    }
}
static void range_changed(const ui_list_window_t *w,void *context)
{
    list_section_t *s=context;
    if (s->empty) {
        if (w->count) lv_obj_add_flag(s->empty,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s->empty,LV_OBJ_FLAG_HIDDEN);
    }
    if (!s->mode) return;
    char text[64];
    if (w->paged) snprintf(text,sizeof(text),ui_text_get(UI_TEXT_LIST_PAGE_FMT),
                           (unsigned)ui_list_window_page_number(w),(unsigned)ui_list_window_pages(w));
    else snprintf(text,sizeof(text),ui_text_get(UI_TEXT_LIST_RANGE_FMT),
                  w->count ? (unsigned)w->first+1 : 0,(unsigned)ui_list_window_last(w),(unsigned)w->count);
    text_set(s->range,text);
    lv_damped_button_set_text(s->mode,ui_text_get(w->paged ? UI_TEXT_LIST_PAGES : UI_TEXT_LIST_SCROLL));
    if (w->paged) {
        lv_obj_clear_flag(s->previous,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(s->next,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(s->range,52); lv_obj_set_width(s->range,s->layout->width-212);
    } else {
        lv_obj_add_flag(s->previous,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s->next,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(s->range,20); lv_obj_set_width(s->range,s->layout->width-125);
    }
    lv_damped_button_set_enabled(s->previous,w->count && w->first>0);
    lv_damped_button_set_enabled(s->next,ui_list_window_page_number(w)<ui_list_window_pages(w));
}
static void mode_clicked(lv_event_t *e)
{
    list_section_t *s=lv_event_get_user_data(e);
    const ui_list_window_t *w=lv_recycled_list_window(s->list);
    if (w) lv_recycled_list_set_paged(s->list,!w->paged);
}
static void page_clicked(lv_event_t *e)
{
    list_section_t *s=lv_event_get_user_data(e);
    lv_recycled_list_page_step(s->list,lv_event_get_target(e)==s->previous ? -1 : 1);
}
static void serials_entry_layout(list_section_t *section)
{
    if(!section->details)return;
    const char *caption=ui_text_get(UI_TEXT_MULTI_DETAILS_ACTION);
    lv_obj_t *label=lv_damped_button_get_label(section->details);
    lv_damped_button_set_text(section->details,caption);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    const lv_font_t *font=ui_message_font(&lv_font_instrument_sans_medium_14);
    lv_obj_set_style_text_font(label,font,0);
    int width=LV_MAX(96,lv_txt_get_width(caption,strlen(caption),font,0,LV_TEXT_FLAG_NONE)+24);
    int x=section->layout->width-12-width;
    lv_obj_set_pos(section->details,x,(HEADER_TOP_Y-36)/2);lv_obj_set_size(section->details,width,36);
    lv_obj_set_size(label,width-20,lv_font_get_line_height(font));
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(label);
    lv_obj_set_width(section->title,x-64);
    lv_obj_set_height(section->title,LV_MAX(28,
        lv_font_get_line_height(lv_obj_get_style_text_font(section->title,0))));
}
static bool section_create(list_section_t *s,const section_layout_t *layout)
{
    if (!view || !view->page || !s || !layout) return false;
    s->layout=layout;
    s->panel=surface(view->page,layout->x,PANEL_Y,layout->width,PANEL_HEIGHT,15,0xFFFFFF);
    if (!s->panel) return false;
    lv_obj_set_style_border_width(s->panel,1,0);
    lv_obj_set_style_border_color(s->panel,lv_color_hex(0xECF0F3),0);
    lv_obj_t *badge=surface(s->panel,17,15,26,26,7,layout->badge_color);
    if (!badge) return false;
    lv_obj_t *letter=label_create(badge,0,0,26,LV_SIZE_CONTENT,&lv_font_instrument_sans_bold_14,0xFFFFFF,LV_TEXT_ALIGN_CENTER);
    if (!letter) return false;
    text_set(letter,ui_text_get((ui_text_id_t)(UI_TEXT_PAGE01_DETAIL_BTN_A+layout->id)));
    lv_obj_center(letter);
    if (layout->id!=PAGE_02_SECTION_B &&
        !section_image_create(s->panel,layout->width-42,16,layout->title_icon)) return false;
    s->title=label_create(s->panel,52,16,layout->width-104,28,&lv_font_instrument_sans_semibold_20,INK,LV_TEXT_ALIGN_LEFT);
    if (!s->title) return false;
    if(layout->id==PAGE_02_SECTION_B){
        s->details=button_create(s->panel,0,10,96,36,
            &lv_font_instrument_sans_medium_14,serials_open,NULL);
        if(!s->details)return false;
        lv_damped_button_set_exact_palette(s->details,lv_color_hex(0xF5F6F7),lv_color_hex(0xE8ECEF));
        lv_obj_set_style_border_width(s->details,0,0);
        lv_obj_set_style_text_color(lv_damped_button_get_label(s->details),lv_color_hex(BODY),0);
        serials_entry_layout(s);
    }
    const lv_font_t *header_font=&lv_font_instrument_sans_medium_12;
    lv_coord_t header_height=lv_font_get_line_height(header_font);
    /* Center the text's line box, not the former oversized label rectangle.
     * Keep the existing body/row capacity and a gap below the section title. */
    lv_coord_t header_y=HEADER_TOP_Y+1+(HEADER_BOTTOM_Y-HEADER_TOP_Y-1-header_height)/2;
    for (int col=0;col<3;++col) {
        s->headers[col]=label_create(s->panel,12+layout->col_x[col],header_y,layout->col_w[col],header_height,
                                    header_font,MUTED,layout->align[col]);
        if (!s->headers[col]) return false;
    }
    if (!surface(s->panel,12,HEADER_TOP_Y,layout->width-36,1,0,LINE) ||
        !surface(s->panel,12,HEADER_BOTTOM_Y,layout->width-36,1,0,LINE) ||
        !surface(s->panel,0,FOOTER_Y,layout->width,1,0,LINE)) return false;
    if (layout->id==PAGE_02_SECTION_A) {
        view->total_title=label_create(s->panel,22,FOOTER_Y+15,86,26,&lv_font_instrument_sans_semibold_14,BODY,LV_TEXT_ALIGN_LEFT);
        view->pcs=label_create(s->panel,119,FOOTER_Y+10,74,32,&lv_font_instrument_sans_semibold_22,BODY,LV_TEXT_ALIGN_RIGHT);
        view->amount=label_create(s->panel,209,FOOTER_Y+10,117,32,&lv_font_instrument_sans_semibold_22,BODY,LV_TEXT_ALIGN_RIGHT);
        if (!view->total_title || !view->pcs || !view->amount) return false;
    }
    {
        /* One visibility owner keeps the icon and text in step with the actual
         * row count. No reject details does not mean zero rejected notes. */
        s->empty=surface(s->panel,22,BODY_Y,layout->width-58,ROWS*ROW_HEIGHT,0,0xFFFFFF);
        if (!s->empty) return false;
        lv_obj_set_style_bg_opa(s->empty,LV_OPA_TRANSP,0);
        lv_obj_set_flex_flow(s->empty,LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(s->empty,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(s->empty,12,0);
        if (!section_image_create(s->empty,0,0,layout->empty_icon)) return false;
        s->empty_text=label_create(s->empty,0,0,layout->width-58,24,&lv_font_instrument_sans_medium_16,MUTED,LV_TEXT_ALIGN_CENTER);
        if (!s->empty_text) return false;
    }
    if (layout->id!=PAGE_02_SECTION_A) {
        s->range=label_create(s->panel,20,FOOTER_Y+16,layout->width-125,24,&lv_font_instrument_sans_medium_14,MUTED,LV_TEXT_ALIGN_LEFT);
        if (!s->empty_text || !s->range) return false;
        s->mode=button_create(s->panel,layout->width-102,FOOTER_Y+5,90,38,
                              &lv_font_instrument_sans_semibold_12,mode_clicked,s);
        if (!s->mode) return false;
        lv_obj_t *mode_label=lv_damped_button_get_label(s->mode);
        lv_obj_set_width(mode_label,62);
        lv_obj_set_style_text_align(mode_label,LV_TEXT_ALIGN_CENTER,0);
        lv_obj_align(mode_label,LV_ALIGN_LEFT_MID,1,0);
        if (!icon_create(s->mode,62,5,ICON_TOGGLE)) return false;
        s->previous=button_create(s->panel,10,FOOTER_Y+5,36,38,&lv_font_instrument_sans_medium_14,page_clicked,s);
        if (!s->previous) return false;
        s->next=button_create(s->panel,layout->width-150,FOOTER_Y+5,36,38,&lv_font_instrument_sans_medium_14,page_clicked,s);
        if (!s->next || !icon_create(s->previous,4,5,ICON_PREV) ||
            !icon_create(s->next,4,5,ICON_NEXT)) return false;
    }
    const lv_recycled_list_config_t cfg={12,BODY_Y,layout->width-12,ROWS,ROW_HEIGHT,
                                         row_create,row_bind,range_changed,s};
    s->list=lv_recycled_list_create(s->panel,&cfg);
    if (!s->list) return false;
    /* Device input ownership is a platform registration, not a page rule in
     * the reusable list. Raw edge/two-finger capture may still cancel it. */
    lv_port_indev_set_drag_obj(lv_recycled_list_object(s->list),true);
    if (s->empty) lv_obj_move_foreground(s->empty);
    perf_profile_watch_invalidation(lv_recycled_list_object(s->list),inv_names[layout->id]);
    return true;
}
static bool visible(void)
{ return view && view->page && !lv_obj_has_flag(view->page,LV_OBJ_FLAG_HIDDEN); }
static void translate(void)
{
    view->language=ui_lang_get();
    text_set(view->total_title,ui_text_get(UI_TEXT_LIST_TOTAL));
    for (int i=0;i<LIST_ACTION_COUNT;++i)
        lv_damped_button_set_text(view->actions[i],ui_text_get(action_layouts[i].title));
    for (int i=0;i<PAGE_02_SECTION_COUNT;++i) {
        list_section_t *s=&view->section[i];
        text_set(s->title,ui_text_get(s->layout->title));
        serials_entry_layout(s);
        for (int c=0;c<3;++c) text_set(s->headers[c],ui_text_get(s->layout->columns[c]));
        if (s->empty_text) text_set(s->empty_text,ui_text_get(i==PAGE_02_SECTION_A ? UI_TEXT_LIST_NO_COUNTING_DATA : i==PAGE_02_SECTION_B ? UI_TEXT_LIST_NO_SERIAL_NUMBERS : UI_TEXT_LIST_NO_REJECT_DETAILS));
    }
}
static void commit(void *context,uint32_t flags)
{
    (void)context; (void)flags;
    if (!visible() || view->search) return;
    const counting_sim_t *data=counting_data_current();
    const bool multi=currency_state_multi_selected();
    if(multi && view->serials)serials_discard();
    if(multi&&!view->multi){
        view->multi=ui_multi_detail_create(view->page,true,NULL,NULL);
        ui_multi_detail_visible(view->multi,true);
    }
    base_view_visibility();
    if(view->multi_visible!=multi){
        view->multi_visible=multi;ui_multi_detail_visible(view->multi,multi);
        if(multi)gesture_service_set_page_policy(UI_PAGE_LIST,NULL,multi_gesture);
        else gesture_service_clear_page_policy(UI_PAGE_LIST);
    }
    if(multi){ui_multi_detail_refresh(view->multi);return;}
    if(view->serials){
        if(dirty&(1U<<PAGE_02_SECTION_B)){
            page_02_list_data_serials(&view->data,data);
            page_02_list_serials_refresh(view->serials,false,list_monetary_result_supported());
            dirty&=~(1U<<PAGE_02_SECTION_B);
        }
        return;
    }
    if (view->language!=ui_lang_get()) { translate(); dirty|=ALL_SECTIONS; }
    uint32_t pending=dirty; dirty=0;
    for (int i=0;i<PAGE_02_SECTION_COUNT;++i) {
        if (!(pending&(1U<<i))) continue;
        uint32_t count;
        if (i==PAGE_02_SECTION_A) {
            page_02_list_data_denoms(&view->data,data);
            if (!list_monetary_result_supported()) view->data.denom_count=0;
            count=view->data.denom_count ? view->data.denom_count : 1;
            if (currency_state_auto_selected() && data->total_pcs == 0) count=0;
            number_set(view->pcs,data->total_pcs,&lv_font_instrument_sans_semibold_22);
            if (list_monetary_result_supported())
                number_set(view->amount,data->total_amount,&lv_font_instrument_sans_semibold_22);
            else text_set(view->amount,"--");
        } else if (i==PAGE_02_SECTION_B) {
            page_02_list_data_serials(&view->data,data); count=view->data.serial_count;
        } else {
            count=counting_data_error_detail_count(data);
        }
        lv_recycled_list_refresh(view->section[i].list,count,(reset_positions&(1U<<i))!=0);
        reset_positions&=~(1U<<i);
    }
}
static void base_view_visibility(void)
{
    bool covered=view->serials!=NULL;
    bool multi=currency_state_multi_selected();
    for(unsigned i=0;i<PAGE_02_SECTION_COUNT;i++) {
        if(covered||multi)lv_obj_add_flag(view->section[i].panel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(view->section[i].panel,LV_OBJ_FLAG_HIDDEN);
    }
    for(unsigned i=0;i<LIST_ACTION_COUNT;i++) {
        if(covered)lv_obj_add_flag(view->actions[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(view->actions[i],LV_OBJ_FLAG_HIDDEN);
    }
}
static void serials_discard(void)
{
    if(!view||!view->serials)return;
    page_02_list_serials_t *serials=view->serials;
    view->serials=NULL;
    page_02_list_serials_destroy(serials);
    base_view_visibility();
}
static void serials_main_selected(void *context)
{
    (void)context;
    if(view)lv_event_send(view->actions[LIST_ACTION_MAIN],LV_EVENT_CLICKED,NULL);
    if(ui_manager_get_current_page()==UI_PAGE_MAIN)serials_discard();
}
static void serials_closed(void *context)
{
    (void)context;
    serials_discard();
    if(!visible())return;
    /* The ABC view keeps its own reading position and mode throughout the
     * subview. Refresh current data without importing the viewer's position. */
    dirty|=ALL_SECTIONS;commit(NULL,dirty);
}
static void serials_open(lv_event_t *event)
{
    (void)event;
    if(!visible()||view->search||view->serials||view->multi_visible)return;
    ui_frame_commit_cancel(commit,NULL);
    commit(NULL,dirty);
    const ui_list_window_t *window=lv_recycled_list_window(view->section[PAGE_02_SECTION_B].list);
    unsigned first=window?window->first:0;
    for(unsigned i=0;i<PAGE_02_SECTION_COUNT;i++)
        lv_recycled_list_stop(view->section[i].list);
    view->serials=page_02_list_serials_create(view->page,&view->data,first,
        list_monetary_result_supported(),serials_closed,serials_main_selected,NULL);
    if(!view->serials)
        ui_notice_post_text(UI_NOTICE_WARNING,"list.serials",
            ui_text_msgid(UI_TEXT_LIST_SERIAL_NUMBERS),ui_text_msgid(UI_TEXT_SERIAL_UNAVAILABLE));
    else {
        base_view_visibility();
        lv_obj_invalidate(view->page);
    }
}
static void search_discard(void)
{
    if (!view || !view->search) return;
    page_02_list_search_t *search=view->search;
    view->search=NULL;
    page_02_list_search_destroy(search);
}
static void search_closed(uint16_t slot,void *context)
{
    (void)context;
    search_discard();
    if (!visible()) return;
    view->located_slot=slot;
    dirty|=ALL_SECTIONS;commit(NULL,dirty);
    /* Resolve the source slot against the current, unfiltered projection.
     * Never use a filtered rank or serial text (duplicates are legitimate). */
    for (unsigned i=0;slot!=UINT16_MAX && i<view->data.serial_count;++i)
        if (view->data.serial[i]==slot) {
            lv_recycled_list_scroll_to_index(view->section[PAGE_02_SECTION_B].list,i);
            break;
        }
}
static void search_open(lv_event_t *e)
{
    (void)e;
    if (!visible() || view->search || view->serials) return;
    if(view->multi_visible){ui_multi_detail_search(view->multi);return;}
    ui_frame_commit_cancel(commit,NULL);
    for (unsigned i=0;i<PAGE_02_SECTION_COUNT;++i)
        lv_recycled_list_stop(view->section[i].list);
    view->search=page_02_list_search_create(view->page,search_closed,NULL);
    lv_damped_button_set_text(view->actions[LIST_ACTION_SEARCH],ui_text_get(view->search ?
        UI_TEXT_SERIAL_SEARCH : UI_TEXT_SERIAL_UNAVAILABLE));
}
void page_02_list_section_mark_dirty(page_02_section_id_t id)
{
    if ((unsigned)id>=PAGE_02_SECTION_COUNT) return;
    dirty|=1U<<id;
    if (view && view->search) {
        if (id==PAGE_02_SECTION_A || id==PAGE_02_SECTION_B)
            page_02_list_search_data_changed(view->search);
        return;
    }
    if (visible() && !ui_frame_commit_defer(commit,NULL,dirty)) commit(NULL,dirty);
}
void page_02_list_section_data_ready(page_02_section_id_t id)
{ page_02_list_section_mark_dirty(id); }
void page_02_list_report_reset(void)
{
    search_discard();
    if (view) {
        view->located_slot=UINT16_MAX;
        if(view->serials){
            view->data.serial_count=0;
            page_02_list_serials_refresh(view->serials,true,list_monetary_result_supported());
        }
    }
    dirty=reset_positions=ALL_SECTIONS;
    if (visible() && !ui_frame_commit_defer(commit,NULL,dirty)) commit(NULL,dirty);
}
void ui_page_02_list_create(lv_obj_t *parent)
{
    if (view) return;
    view=lv_mem_alloc(sizeof(*view));
    if (!view) return;
    memset(view,0,sizeof(*view));
    view->located_slot=UINT16_MAX;
    view->page=surface(parent ? parent : lv_scr_act(),0,0,1280,400,0,0xD8E2E8);
    ui_page_background_apply(view->page, UI_BACKGROUND_USER);
    if (!view->page) goto creation_failed;
    for (int i=0;i<PAGE_02_SECTION_COUNT;++i)
        if (!section_create(&view->section[i],&layouts[i])) goto creation_failed;
    for (int i=0;i<LIST_ACTION_COUNT;++i) {
        view->actions[i]=button_create(view->page,1168,PANEL_Y+i*(ACTION_HEIGHT+ACTION_GAP),96,ACTION_HEIGHT,
            &lv_font_instrument_sans_semibold_12,action_layouts[i].clicked,NULL);
        if (!view->actions[i]) goto creation_failed;
        if(i==LIST_ACTION_HISTORY || i==LIST_ACTION_SEARCH) {
            lv_obj_set_style_border_width(view->actions[i],0,0);
            lv_obj_set_style_border_width(view->actions[i],0,LV_STATE_PRESSED);
            lv_obj_set_style_border_width(view->actions[i],0,LV_STATE_DISABLED);
        }
        lv_obj_t *label=lv_damped_button_get_label(view->actions[i]);
        if (!label) goto creation_failed;
        lv_obj_set_size(label,90,23); lv_obj_align(label,LV_ALIGN_BOTTOM_MID,0,-6);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
        if (!icon_create(view->actions[i],35,17,action_layouts[i].icon)) goto creation_failed;
    }
    translate(); page_02_list_report_reset();
    return;

creation_failed:
    ui_page_02_list_destroy();
}
bool ui_page_02_list_resume(void)
{
    if (!view || !view->page) return false;
    lv_obj_clear_flag(view->page,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(view->page);
    /* Reentry also covers prewarm before runtime notifications existed. Keep
     * anchors/modes unless a real result reset has requested otherwise. */
    page_02_list_serials_set_visible(view->serials,true);
    dirty|=ALL_SECTIONS; commit(NULL,dirty);
    if(view->multi_visible&&view->multi){
        ui_multi_detail_visible(view->multi,true);
        if(requested_currency>=0){ui_multi_detail_select(view->multi,requested_currency,requested_tab);requested_currency=-1;}
        gesture_service_set_page_policy(UI_PAGE_LIST,NULL,multi_gesture);
    }
    return true;
}
void ui_page_02_list_suspend(void)
{
    if (!view) return;
    ui_frame_commit_cancel(commit,NULL);
    ui_multi_detail_visible(view->multi,false);
    page_02_list_serials_set_visible(view->serials,false);
    gesture_service_clear_page_policy(UI_PAGE_LIST);
    search_discard();
    for (int i=0;i<PAGE_02_SECTION_COUNT;++i) lv_recycled_list_stop(view->section[i].list);
    if (view->page) lv_obj_add_flag(view->page,LV_OBJ_FLAG_HIDDEN);
}
void ui_page_02_list_destroy(void)
{
    if (!view) return;
    ui_page_02_list_suspend();
    serials_discard();
    ui_multi_detail_destroy(view->multi);view->multi=NULL;
    for (int i=0;i<PAGE_02_SECTION_COUNT;++i)
        perf_profile_unwatch_invalidation(lv_recycled_list_object(view->section[i].list));
    if (view->page) lv_obj_del(view->page);
    lv_mem_free(view); view=NULL;
    dirty=reset_positions=ALL_SECTIONS;
}
