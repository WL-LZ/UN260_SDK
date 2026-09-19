#include "un260/lv_resources/ui_page_background.h"
#include "page_19_history_search.h"
#include "un260/lv_components/lv_popup_style.h"
#include "lv_port_indev.h"
#include "un260/lv_components/lv_alnum_keyboard.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/machine_time.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SEARCH_INK 0x17212A
#define SEARCH_BODY 0x4C606E
#define SEARCH_MUTED 0x7A8D9B
#define SEARCH_LINE 0xE7ECEF
#define SEARCH_MAX_CURRENCIES (UI_HISTORY_MAX_RECORDS * HISTORY_MULTI_CURRENCIES + 2U)
#define SEARCH_MAX_DENOMS HISTORY_DETAIL_MAX_DENOMS
enum { FIELD_FROM, FIELD_TO, FIELD_TIME, FIELD_AMOUNT, FIELD_PCS, FIELD_SERIAL, FIELD_COUNT };

struct page_19_history_search {
    lv_obj_t *root, *tabs[3], *panels[3], *fields[FIELD_COUNT], *error;
    lv_obj_t *currency, *rejects, *mode, *denom_title, *denom_empty, *denom_all;
    lv_obj_t *denom_buttons[SEARCH_MAX_DENOMS], *matches[4];
    lv_alnum_keyboard_t *keyboard;
    lv_obj_t *editor, *precision, *wheels[3], *wheel_titles[3], *year_button;
    lv_obj_t *number[2], *number_title[2], *editor_error;
    unsigned year_first, year_value, number_edit;
    char number_text[2][21];
    history_query_input_t input;
    const history_query_record_t *records;
    size_t record_count;
    char currencies[SEARCH_MAX_CURRENCIES][4];
    size_t currency_count;
    uint8_t reject_codes[256];
    size_t reject_count;
    uint32_t denominations[SEARCH_MAX_DENOMS];
    size_t denomination_count;
    bool denominations_limited;
    unsigned active_tab, editing_field;
    page_19_history_search_close_cb_t close;
    void *context;
};

static void refresh_controls(page_19_history_search_t *s);
static void open_editor(page_19_history_search_t *s, unsigned field);

static lv_obj_t *surface(lv_obj_t *parent, int x, int y, int width, int height,
                         uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    if (!o) return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, width, height);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int width, int height,
                       const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *o = lv_label_create(parent);
    if (!o) return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, width, height);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
    lv_label_set_text(o, text ? text : "");
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *button(lv_obj_t *parent, int x, int y, int width, int height,
                        const char *text, lv_event_cb_t callback, void *context)
{
    const lv_damped_button_style_t style = {
        0xF4F6F7, 0xE0E3E5, 0xF7F8F9, SEARCH_BODY, 0xAAB5BE, 8
    };
    lv_obj_t *o = lv_damped_button_create(parent, &style, text,
                                        &lv_font_instrument_sans_medium_18);
    if (!o) return NULL;
    if (!lv_damped_button_get_label(o) ||
        !lv_obj_add_event_cb(o, callback, LV_EVENT_CLICKED, context)) {
        lv_obj_del(o); return NULL;
    }
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, width, height);
    lv_obj_t *text_label = lv_damped_button_get_label(o);
    lv_obj_set_width(text_label, width - 20);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}

static void selected(lv_obj_t *o, bool active)
{
    uint32_t color = active ? 0xEAF2FF : 0xF4F6F7;
    if (active) lv_obj_add_state(o, LV_STATE_CHECKED);
    else lv_obj_clear_state(o, LV_STATE_CHECKED);
    lv_damped_button_set_palette(o, lv_color_hex(color), lv_color_hex(color));
    lv_obj_set_style_border_width(o, active ? 1 : 0, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0xB8D4FC), 0);
    lv_obj_set_style_border_opa(o, active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_t *text=lv_damped_button_get_label(o);
    if(text) lv_obj_set_style_text_color(text,lv_color_hex(active ? 0x0074F8 : SEARCH_BODY),0);
}

static char *field_value(page_19_history_search_t *s, unsigned field, size_t *size)
{
    switch (field) {
    case FIELD_FROM: *size = sizeof(s->input.date_from); return s->input.date_from;
    case FIELD_TO: *size = sizeof(s->input.date_to); return s->input.date_to;
    case FIELD_TIME: *size = sizeof(s->input.time); return s->input.time;
    case FIELD_AMOUNT: *size = sizeof(s->input.amount); return s->input.amount;
    case FIELD_PCS: *size = sizeof(s->input.pcs); return s->input.pcs;
    default: *size = sizeof(s->input.serial); return s->input.serial;
    }
}

static ui_text_id_t field_title(unsigned field)
{
    static const ui_text_id_t titles[] = {
        UI_TEXT_HISTORY_DATE_FROM, UI_TEXT_HISTORY_DATE_TO, UI_TEXT_HISTORY_TIME,
        UI_TEXT_HISTORY_AMOUNT, UI_TEXT_HISTORY_PCS, UI_TEXT_LIST_SERIAL_NUMBERS
    };
    return titles[field];
}

static int denomination_selected(const page_19_history_search_t *s, uint32_t value)
{
    for (size_t i = 0; i < s->input.denomination_count; ++i)
        if (s->input.denominations[i] == value) return (int)i;
    return -1;
}

static void offer_denomination(page_19_history_search_t *s, uint32_t value)
{
    if (!value) return;
    size_t position = 0;
    while (position < s->denomination_count && s->denominations[position] < value) ++position;
    if (position < s->denomination_count && s->denominations[position] == value) return;
    if (s->denomination_count == SEARCH_MAX_DENOMS) {
        s->denominations_limited = true; return;
    }
    memmove(s->denominations + position + 1, s->denominations + position,
            (s->denomination_count - position) * sizeof(s->denominations[0]));
    s->denominations[position] = value; ++s->denomination_count;
}

static void collect_denominations(page_19_history_search_t *s)
{
    s->denomination_count = 0;
    s->denominations_limited = false;
    if (!s->input.currency[0]) return;
    /* A retained filter remains visible even if its last saved record expires. */
    for (size_t i = 0; i < s->input.denomination_count; ++i)
        offer_denomination(s, s->input.denominations[i]);
    for (size_t record = 0; record < s->record_count; ++record) {
        const history_query_record_t *r = &s->records[record];
        if(r->valid && r->multi && r->multi->enabled) {
            for(unsigned i=0;i<r->multi->count;i++) {
                const history_multi_currency_t *g=&r->multi->currencies[i];
                if(strcmp(g->code,s->input.currency))continue;
                for(unsigned j=0;j<g->count;j++)if(g->denoms[j].pcs)offer_denomination(s,g->denoms[j].value);
            }
            continue;
        }
        if (!r->valid || !r->detail || strcmp(r->currency, s->input.currency)) continue;
        for (size_t d = 0; d < r->detail->denom_count; ++d) {
            uint32_t value = r->detail->denoms[d].value;
            if (!value || !r->detail->denoms[d].pcs) continue;
            offer_denomination(s, value);
        }
        /* Older snapshots may retain denomination only beside each serial.
         * Offer those known values too; zero still means unknown, not a note. */
        for (size_t note = 0; note < r->detail->serial_count; ++note)
            offer_denomination(s, r->detail->serials[note].denom);
    }
}

static void activate_tab(page_19_history_search_t *s, unsigned tab)
{
    if(s->mode)lv_dropdown_close(s->mode);
    if (s->currency) lv_dropdown_close(s->currency);
    if (s->rejects) lv_dropdown_close(s->rejects);
    s->active_tab = tab;
    for (unsigned i = 0; i < 3; ++i) {
        selected(s->tabs[i], i == tab);
        if (i == tab) lv_obj_clear_flag(s->panels[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s->panels[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void tab_event(lv_event_t *event)
{
    page_19_history_search_t *s = lv_event_get_user_data(event);
    for (unsigned i = 0; i < 3; ++i)
        if (lv_event_get_target(event) == s->tabs[i]) activate_tab(s, i);
}

static void stop_feedback(lv_obj_t *object)
{
    lv_anim_del(object, NULL);
    if (lv_obj_check_type(object, &lv_btn_class) && lv_damped_button_get_label(object))
        lv_damped_button_set_palette(object, lv_color_hex(0xF4F6F7), lv_color_hex(0xF4F6F7));
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(object); ++i)
        stop_feedback(lv_obj_get_child(object, (int32_t)i));
}

static void close_search(page_19_history_search_t *s, bool apply)
{
    if(s->mode)lv_dropdown_close(s->mode);
    history_query_input_t input = s->input;
    page_19_history_search_close_cb_t close = s->close;
    void *context = s->context;
    lv_dropdown_close(s->currency); lv_dropdown_close(s->rejects);
    lv_alnum_keyboard_hide(s->keyboard);
    lv_obj_add_flag(s->root, LV_OBJ_FLAG_HIDDEN);
    stop_feedback(s->root);
    close(apply ? &input : NULL, context);
}

static void cancel_event(lv_event_t *event)
{ close_search(lv_event_get_user_data(event), false); }

static void apply_event(lv_event_t *event)
{
    page_19_history_search_t *s = lv_event_get_user_data(event);
    history_query_t compiled;
    history_query_error_t error = history_query_compile(&s->input, &compiled);
    if (error == HISTORY_QUERY_OK) { close_search(s, true); return; }
    ui_text_id_t message = UI_TEXT_HISTORY_INPUT_ERROR;
    if (error >= HISTORY_QUERY_BAD_DATE_FROM && error <= HISTORY_QUERY_BAD_TIME) {
        message = UI_TEXT_HISTORY_DATE_ERROR; activate_tab(s, 0);
    } else if (error == HISTORY_QUERY_BAD_PCS || error == HISTORY_QUERY_BAD_AMOUNT) {
        message = UI_TEXT_HISTORY_NUMBER_ERROR; activate_tab(s, 1);
    } else if (error == HISTORY_QUERY_CURRENCY_REQUIRED) {
        message = UI_TEXT_HISTORY_CURRENCY_REQUIRED; activate_tab(s, 1);
    } else activate_tab(s, error == HISTORY_QUERY_BAD_SERIAL || error == HISTORY_QUERY_BAD_REJECT ? 2 : 1);
    lv_label_set_text(s->error, ui_text_get(message));
}

static void reset_event(lv_event_t *event)
{
    page_19_history_search_t *s = lv_event_get_user_data(event);
    lv_dropdown_close(s->currency); lv_dropdown_close(s->rejects);
    bool oldest_first = s->input.oldest_first;
    memset(&s->input, 0, sizeof(s->input));
    s->input.oldest_first = oldest_first;
    collect_denominations(s); refresh_controls(s);
}

static void input_submit(const char *text, void *context)
{
    page_19_history_search_t *s = context;
    size_t size;
    char *value = field_value(s, s->editing_field, &size);
    snprintf(value, size, "%s", text);
    refresh_controls(s);
}

static void input_event(lv_event_t *event)
{
    page_19_history_search_t *s = lv_event_get_user_data(event);
    unsigned field;
    for (field = 0; field < FIELD_COUNT; ++field)
        if (lv_event_get_target(event) == s->fields[field]) break;
    if (field == FIELD_COUNT || (field == FIELD_AMOUNT && !s->input.currency[0])) return;
    lv_dropdown_close(s->currency); lv_dropdown_close(s->rejects);
    if (field != FIELD_SERIAL) { open_editor(s, field); return; }
    if (s->keyboard) { lv_alnum_keyboard_destroy(s->keyboard); s->keyboard = NULL; }
    size_t size;
    char *value = field_value(s, field, &size);
    s->editing_field = field;
    const lv_alnum_keyboard_config_t config = {
        .title = ui_text_get(field_title(field)),
        .placeholder = "",
        .apply_text = ui_text_get(UI_TEXT_HISTORY_APPLY),
        .clear_text = ui_text_get(UI_TEXT_SERIAL_CLEAR),
        .cancel_text = ui_text_get(UI_TEXT_SERIAL_ESC),
        .max_length = (uint8_t)LV_MIN(size - 1, LV_ALNUM_KEYBOARD_MAX_TEXT),
        .submit = input_submit, .context = s, .symbols = true
    };
    s->keyboard = lv_alnum_keyboard_create(s->root, &config);
    if (!s->keyboard || !lv_alnum_keyboard_set_text(s->keyboard, value)) {
        lv_label_set_text(s->error, ui_text_get(UI_TEXT_HISTORY_INPUT_ERROR)); return;
    }
    lv_alnum_keyboard_show(s->keyboard);
}

static void filter_event(lv_event_t *event)
{
    page_19_history_search_t *s = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);
    if (target == s->denom_all) {
        s->input.denomination_count = 0; refresh_controls(s); return;
    }
    for (unsigned i = 0; i < 4; ++i) if (target == s->matches[i]) {
        s->input.serial_match = (history_text_match_t)i; refresh_controls(s); return;
    }
    for (size_t i = 0; i < s->denomination_count; ++i) if (target == s->denom_buttons[i]) {
        int index = denomination_selected(s, s->denominations[i]);
        if (index >= 0) {
            memmove(s->input.denominations + index, s->input.denominations + index + 1,
                    (s->input.denomination_count - (size_t)index - 1) * sizeof(uint32_t));
            --s->input.denomination_count;
        } else if (s->input.denomination_count < HISTORY_DETAIL_MAX_DENOMS)
            s->input.denominations[s->input.denomination_count++] = s->denominations[i];
        refresh_controls(s); return;
    }
}

static void dropdown_event(lv_event_t *event)
{
    page_19_history_search_t *s = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);
    unsigned index = lv_dropdown_get_selected(target);
    if(target==s->mode){s->input.mode=index;return;}
    if (target == s->currency && index < s->currency_count) {
        if (strcmp(s->input.currency, s->currencies[index])) {
            memcpy(s->input.currency, s->currencies[index], sizeof(s->input.currency));
            s->input.denomination_count = 0; s->input.amount[0] = '\0';
            collect_denominations(s);
        }
    } else if (target == s->rejects) {
        s->input.rejects = index == 0 ? HISTORY_REJECT_ALL :
                           index == 1 ? HISTORY_REJECT_SAVED_ANY : HISTORY_REJECT_CODE;
        s->input.reject_code = index >= 2 && index - 2 < s->reject_count ? s->reject_codes[index - 2] : 0;
    }
    refresh_controls(s);
}

static lv_obj_t *dropdown(lv_obj_t *parent, int x, int y, int width,
                          page_19_history_search_t *s)
{
    lv_obj_t *o = lv_dropdown_create(parent);
    if (!o) return NULL;
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, width, 42);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xF4F6F7), 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xDCE6EC), LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xE0E3E5), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(SEARCH_BODY), 0);
    lv_obj_set_style_text_font(o, &lv_font_instrument_sans_medium_18, 0);
    lv_obj_set_style_border_width(o, 0, 0); lv_obj_set_style_radius(o, 8, 0);
    lv_obj_set_style_pad_top(o, 9, 0); lv_obj_set_style_pad_left(o, 14, 0);
    lv_obj_set_style_pad_right(o, 32, 0);
    lv_dropdown_set_symbol(o, NULL);
    static const lv_point_t arrow_points[] = { {0, 0}, {5, 5}, {10, 0} };
    lv_obj_t *arrow = lv_line_create(o);
    if (!arrow) { lv_obj_del(o); return NULL; }
    lv_line_set_points(arrow, arrow_points, 3);
    lv_obj_set_style_line_color(arrow, lv_color_hex(SEARCH_MUTED), 0);
    lv_obj_set_style_line_width(arrow, 2, 0);
    lv_obj_set_style_line_rounded(arrow, true, 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 18, 0);
    lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *list = lv_dropdown_get_list(o);
    if (list) {
        lv_obj_set_style_text_font(list, &lv_font_instrument_sans_medium_18, 0);
        lv_obj_set_style_text_color(list, lv_color_hex(SEARCH_BODY), 0);
        lv_obj_set_style_bg_color(list, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(list, lv_color_hex(0xC7D3DB), 0);
        lv_obj_set_style_border_width(list, 1, 0);
        lv_obj_set_style_radius(list, 8, 0);
        lv_obj_set_style_pad_hor(list, 14, 0);
        lv_obj_set_style_max_height(list, 210, 0);
        lv_obj_set_style_max_width(list, width, 0);
        lv_obj_set_style_pad_ver(list, 8, 0);
        lv_obj_set_style_text_line_space(list, 16, 0);
        lv_obj_set_style_bg_color(list, lv_color_hex(0xDCE6EC), LV_PART_SELECTED);
        lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED);
        lv_obj_set_style_text_color(list, lv_color_hex(SEARCH_INK), LV_PART_SELECTED);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN_VER | LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
        lv_port_indev_set_drag_obj(list, true);
    }
    if (!lv_obj_add_event_cb(o, dropdown_event, LV_EVENT_VALUE_CHANGED, s)) {
        lv_obj_del(o); return NULL;
    }
    return o;
}

static void editor_close(page_19_history_search_t *s)
{
    if (s->keyboard) { lv_alnum_keyboard_destroy(s->keyboard); s->keyboard = NULL; }
    lv_obj_t *editor = s->editor;
    s->editor = NULL;
    if (editor) {
        lv_obj_del(editor);
    }
    s->precision = s->year_button = s->editor_error = NULL;
    memset(s->wheels, 0, sizeof(s->wheels));
    memset(s->wheel_titles, 0, sizeof(s->wheel_titles));
    memset(s->number, 0, sizeof(s->number));
    memset(s->number_title, 0, sizeof(s->number_title));
}

static void editor_cancel(lv_event_t *e) { editor_close(lv_event_get_user_data(e)); }

static unsigned picker_days(unsigned year, unsigned month)
{
    static const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month-1] + (month == 2 && year%4 == 0 && (year%100 != 0 || year%400 == 0));
}

static bool wheel_options(lv_obj_t *wheel, unsigned first, unsigned last, unsigned value)
{
    size_t capacity = (last-first+1U)*6U+1U;
    char *options = lv_mem_alloc(capacity);
    if (!options) return false;
    size_t used = 0;
    for (unsigned n=first;n<=last;++n) {
        int written=snprintf(options+used,capacity-used,n==first ? "%02u" : "\n%02u",n);
        if (written<0 || (size_t)written>=capacity-used) {lv_mem_free(options);return false;}
        used+=(size_t)written;
    }
    lv_roller_set_options(wheel,options,LV_ROLLER_MODE_NORMAL);
    lv_mem_free(options);
    lv_roller_set_selected(wheel,(uint16_t)(LV_CLAMP(first,value,last)-first),LV_ANIM_OFF);
    return true;
}

static void editor_visibility(page_19_history_search_t *s)
{
    unsigned mode=lv_dropdown_get_selected(s->precision);
    if (s->editing_field<=FIELD_TIME) {
        for (unsigned i=1;i<3;++i) {
            if (mode>=i) {
                lv_obj_clear_flag(s->wheels[i],LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(s->wheel_titles[i],LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s->wheels[i],LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(s->wheel_titles[i],LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        if(mode==3) lv_obj_clear_flag(s->number[1],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s->number[1],LV_OBJ_FLAG_HIDDEN);
        for(unsigned i=0;i<2;++i) {
            if(mode==3) lv_obj_clear_flag(s->number_title[i],LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(s->number_title[i],LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void editor_changed(lv_event_t *e)
{
    page_19_history_search_t *s=lv_event_get_user_data(e);
    if(s->editing_field<=FIELD_TO && lv_event_get_target(e)!=s->precision) {
        s->year_value=s->year_first+lv_roller_get_selected(s->wheels[0]);
        unsigned month=lv_roller_get_selected(s->wheels[1])+1;
        unsigned day=lv_roller_get_selected(s->wheels[2])+1;
        if(!wheel_options(s->wheels[2],1,picker_days(s->year_value,month),day))
            lv_label_set_text(s->editor_error,ui_text_get(UI_TEXT_SERIAL_UNAVAILABLE));
        char text[16];snprintf(text,sizeof(text),"%04u",s->year_value);
        lv_damped_button_set_text(s->year_button,text);
    }
    editor_visibility(s);
}

static void editor_numeric_submit(const char *text, void *context)
{
    page_19_history_search_t *s=context;
    if(!s->editor) return;
    char *end=NULL;
    bool digits=text && text[0];
    for(const char *p=text;digits && *p;++p) if(*p<'0'||*p>'9') digits=false;
    if(!digits) {lv_label_set_text(s->editor_error,ui_text_get(UI_TEXT_HISTORY_NUMBER_ERROR));return;}
    if(s->editing_field<=FIELD_TO) {
        unsigned long value=strtoul(text,&end,10);
        if(!end || *end || value<1 || value>9999) {
            lv_label_set_text(s->editor_error,ui_text_get(UI_TEXT_HISTORY_DATE_ERROR));return;
        }
        s->year_first=value>50 ? (unsigned)value-50 : 1;
        if(!wheel_options(s->wheels[0],s->year_first,LV_MIN(9999U,s->year_first+100U),(unsigned)value)) return;
        lv_event_send(s->wheels[0],LV_EVENT_VALUE_CHANGED,NULL);
    } else {
        snprintf(s->number_text[s->number_edit],sizeof(s->number_text[0]),"%s",text);
        lv_damped_button_set_text(s->number[s->number_edit],text);
    }
    lv_label_set_text(s->editor_error,"");
}

static void editor_numeric_open(lv_event_t *e)
{
    page_19_history_search_t *s=lv_event_get_user_data(e);
    s->number_edit=lv_event_get_target(e)==s->number[1];
    if(s->keyboard) {lv_alnum_keyboard_destroy(s->keyboard);s->keyboard=NULL;}
    const lv_alnum_keyboard_config_t config={
        .title=ui_text_get(s->editing_field<=FIELD_TO ? UI_TEXT_HISTORY_YEAR : field_title(s->editing_field)),
        .placeholder="",.apply_text=ui_text_get(UI_TEXT_HISTORY_APPLY),
        .clear_text=ui_text_get(UI_TEXT_SERIAL_CLEAR),.cancel_text=ui_text_get(UI_TEXT_SERIAL_ESC),
        .max_length=s->editing_field<=FIELD_TO ? 4 : 20,.submit=editor_numeric_submit,.context=s
    };
    s->keyboard=lv_alnum_keyboard_create(s->editor,&config);
    char year[16];snprintf(year,sizeof(year),"%04u",s->year_value);
    if(s->keyboard) {
        lv_alnum_keyboard_set_text(s->keyboard,s->editing_field<=FIELD_TO ? year : s->number_text[s->number_edit]);
        lv_alnum_keyboard_show(s->keyboard);
    } else lv_label_set_text(s->editor_error,ui_text_get(UI_TEXT_SERIAL_UNAVAILABLE));
}

static void editor_apply(lv_event_t *e)
{
    page_19_history_search_t *s=lv_event_get_user_data(e);
    unsigned mode=lv_dropdown_get_selected(s->precision);
    char text[48];
    if(s->editing_field<=FIELD_TO) {
        unsigned year=s->year_first+lv_roller_get_selected(s->wheels[0]);
        unsigned month=lv_roller_get_selected(s->wheels[1])+1;
        unsigned day=lv_roller_get_selected(s->wheels[2])+1;
        if(mode==0) snprintf(text,sizeof(text),"%04u",year);
        else if(mode==1) snprintf(text,sizeof(text),"%04u-%02u",year,month);
        else snprintf(text,sizeof(text),"%04u-%02u-%02u",year,month,day);
    } else if(s->editing_field==FIELD_TIME) {
        unsigned hour=lv_roller_get_selected(s->wheels[0]);
        unsigned minute=lv_roller_get_selected(s->wheels[1]);
        unsigned second=lv_roller_get_selected(s->wheels[2]);
        if(mode==0) snprintf(text,sizeof(text),"%02u",hour);
        else if(mode==1) snprintf(text,sizeof(text),"%02u:%02u",hour,minute);
        else snprintf(text,sizeof(text),"%02u:%02u:%02u",hour,minute,second);
    } else {
        if(mode==3) snprintf(text,sizeof(text),"%s..%s",s->number_text[0],s->number_text[1]);
        else snprintf(text,sizeof(text),"%s%s",mode==1 ? ">=" : mode==2 ? "<=" : "",s->number_text[0]);
    }
    history_query_input_t candidate=s->input;
    char *target=s->editing_field==FIELD_FROM ? candidate.date_from :
        s->editing_field==FIELD_TO ? candidate.date_to : s->editing_field==FIELD_TIME ? candidate.time :
        s->editing_field==FIELD_AMOUNT ? candidate.amount : candidate.pcs;
    size_t capacity=s->editing_field<=FIELD_TIME ? sizeof(candidate.time) : sizeof(candidate.amount);
    if(strlen(text)>=capacity) {lv_label_set_text(s->editor_error,ui_text_get(UI_TEXT_HISTORY_NUMBER_ERROR));return;}
    memcpy(target,text,strlen(text)+1);
    history_query_t query;
    history_query_error_t error=history_query_compile(&candidate,&query);
    if(error!=HISTORY_QUERY_OK) {
        lv_label_set_text(s->editor_error,ui_text_get(s->editing_field<=FIELD_TIME ?
            UI_TEXT_HISTORY_DATE_ERROR : UI_TEXT_HISTORY_NUMBER_ERROR));return;
    }
    s->input=candidate;
    editor_close(s);refresh_controls(s);
}

static void editor_clear(lv_event_t *e)
{
    page_19_history_search_t *s=lv_event_get_user_data(e);
    size_t size;field_value(s,s->editing_field,&size)[0]='\0';
    editor_close(s);refresh_controls(s);
}

static void open_editor(page_19_history_search_t *s, unsigned field)
{
    editor_close(s);s->editing_field=field;
    s->editor=surface(s->root,0,0,1280,400,0xD8E2E8,0);
    if(!s->editor) return;
    lv_obj_add_flag(s->editor,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *card=surface(s->editor,160,16,960,368,0xFFFFFF,18);
    if(card) lv_popup_style(s->editor,card);
    lv_obj_add_event_cb(s->editor,editor_cancel,LV_EVENT_CLICKED,s);
    if(!card || !label(card,28,18,420,32,&lv_font_instrument_sans_semibold_22,SEARCH_INK,ui_text_get(field_title(field)))) goto failed;
    s->precision=lv_dropdown_create(card);
    if(!s->precision) goto failed;
    lv_obj_set_pos(s->precision,620,16);lv_obj_set_size(s->precision,310,44);
    lv_obj_set_style_text_font(s->precision,&lv_font_instrument_sans_medium_18,0);
    lv_obj_set_style_bg_color(s->precision,lv_color_hex(0xEAF2FF),0);
    lv_obj_set_style_bg_opa(s->precision,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(s->precision,lv_color_hex(0x0074F8),0);
    lv_obj_set_style_radius(s->precision,12,0);
    lv_obj_set_style_pad_left(s->precision,16,0);
    lv_obj_set_style_pad_right(s->precision,36,0);
    lv_obj_set_style_pad_top(s->precision,10,0);
    lv_dropdown_set_symbol(s->precision,NULL);
    static const lv_point_t arrow_points[]={{0,0},{5,5},{10,0}};
    lv_obj_t *arrow=lv_line_create(s->precision);
    if(!arrow) goto failed;
    lv_line_set_points(arrow,arrow_points,3);
    lv_obj_set_style_line_color(arrow,lv_color_hex(0x0074F8),0);
    lv_obj_set_style_line_width(arrow,2,0);
    lv_obj_align(arrow,LV_ALIGN_RIGHT_MID,18,0);
    lv_obj_clear_flag(arrow,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(lv_dropdown_get_list(s->precision),&lv_font_instrument_sans_medium_18,0);
    lv_obj_set_style_bg_opa(lv_dropdown_get_list(s->precision),LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(lv_dropdown_get_list(s->precision),lv_color_hex(0xFFFFFF),0);
    lv_dropdown_set_options(s->precision,ui_text_get(field<=FIELD_TO ? UI_TEXT_HISTORY_PRECISION_DATE :
        field==FIELD_TIME ? UI_TEXT_HISTORY_PRECISION_TIME : UI_TEXT_HISTORY_NUMBER_MODES));
    lv_obj_add_event_cb(s->precision,editor_changed,LV_EVENT_VALUE_CHANGED,s);
    size_t size;const char *value=field_value(s,field,&size);
    unsigned precision=2;
    if(field<=FIELD_TIME) {
        machine_time_value_t now;machine_time_get(&now);
        unsigned values[3]={now.year ? now.year : 2026,now.month ? now.month : 1,now.day ? now.day : 1};
        if(field==FIELD_TIME) {values[0]=now.hour;values[1]=now.minute;values[2]=now.second;}
        char digits[16];unsigned n=0;
        for(const char *p=value;*p && n<sizeof(digits)-1;++p) if(*p>='0' && *p<='9') digits[n++]=*p;
        digits[n]='\0';
        if(n) {
            unsigned first=field==FIELD_TIME ? 2 : 4;
            char part[5]={0};memcpy(part,digits,first);values[0]=(unsigned)atoi(part);
            precision=(n-first)/2;
            for(unsigned i=1;i<=precision && i<3;++i) {
                memcpy(part,digits+first+(i-1)*2,2);part[2]=0;values[i]=(unsigned)atoi(part);
            }
        }
        s->year_value=LV_CLAMP(1U,values[0],9999U);
        /* The content-size limit is 8191 in this LVGL build. Keep the full
         * options label below it; direct year entry retains the 1..9999 range. */
        s->year_first=s->year_value>50 ? s->year_value-50 : 1;
        for(unsigned i=0;i<3;++i) {
            ui_text_id_t title=(ui_text_id_t)((field==FIELD_TIME ? UI_TEXT_HISTORY_HOUR : UI_TEXT_HISTORY_YEAR)+i);
            s->wheel_titles[i]=label(card,100+i*260,76,220,26,&lv_font_instrument_sans_medium_18,SEARCH_BODY,ui_text_get(title));
            if(!s->wheel_titles[i]) goto failed;
            s->wheels[i]=lv_roller_create(card);if(!s->wheels[i]) goto failed;
            lv_obj_set_style_text_font(s->wheels[i],&lv_font_instrument_sans_medium_22,0);
            /* Match both text layers: different metrics can shift the selected
             * overlay away from the actual option on long year wheels. */
            lv_obj_set_style_text_font(s->wheels[i],&lv_font_instrument_sans_medium_22,LV_PART_SELECTED);
            lv_obj_set_style_bg_color(s->wheels[i],lv_color_hex(0xF4F6F7),0);
            lv_obj_set_style_bg_opa(s->wheels[i],LV_OPA_COVER,0);
            lv_obj_set_style_text_color(s->wheels[i],lv_color_hex(SEARCH_MUTED),0);
            lv_obj_set_style_text_align(s->wheels[i],LV_TEXT_ALIGN_CENTER,0);
            lv_obj_set_style_text_align(s->wheels[i],LV_TEXT_ALIGN_CENTER,LV_PART_SELECTED);
            lv_obj_set_style_text_line_space(s->wheels[i],18,0);
            lv_obj_set_style_text_line_space(s->wheels[i],18,LV_PART_SELECTED);
            lv_obj_set_style_radius(s->wheels[i],12,0);
            lv_obj_set_style_bg_color(s->wheels[i],lv_color_hex(0xEAF2FF),LV_PART_SELECTED);
            lv_obj_set_style_bg_opa(s->wheels[i],LV_OPA_COVER,LV_PART_SELECTED);
            lv_obj_set_style_text_color(s->wheels[i],lv_color_hex(0x0074F8),LV_PART_SELECTED);
            lv_roller_set_visible_row_count(s->wheels[i],3);
            lv_obj_set_pos(s->wheels[i],100+i*260,112);lv_obj_set_width(s->wheels[i],220);
            unsigned first=field==FIELD_TIME ? 0 : i==0 ? s->year_first : 1;
            unsigned last=field==FIELD_TIME ? (i==0 ? 23 : 59) : i==0 ? LV_MIN(9999U,s->year_first+100U) : i==1 ? 12 : picker_days(s->year_value,LV_CLAMP(1U,values[1],12U));
            if(!wheel_options(s->wheels[i],first,last,values[i])) goto failed;
            if(field<=FIELD_TO && i<2) lv_obj_add_event_cb(s->wheels[i],editor_changed,LV_EVENT_VALUE_CHANGED,s);
        }
        if(field<=FIELD_TO) {
            char year[16];snprintf(year,sizeof(year),"%04u",s->year_value);
            s->year_button=button(card,100,72,220,34,year,editor_numeric_open,s);
            if(!s->year_button) goto failed;
        }
    } else {
        history_query_t query;history_query_number_range_t range={0};
        if(history_query_compile(&s->input,&query)==HISTORY_QUERY_OK) range=field==FIELD_AMOUNT ? query.amount : query.pcs;
        precision=strstr(value,"..") ? 3 : !strncmp(value,">=",2) ? 1 : !strncmp(value,"<=",2) ? 2 : 0;
        snprintf(s->number_text[0],sizeof(s->number_text[0]),"%llu",(unsigned long long)(precision==2 ? range.max : range.min));
        snprintf(s->number_text[1],sizeof(s->number_text[1]),"%llu",(unsigned long long)(precision==3 ? range.max : 0));
        for(unsigned i=0;i<2;++i) {
            s->number_title[i]=label(card,100+i*400,96,360,30,&lv_font_instrument_sans_medium_18,
                SEARCH_BODY,ui_text_get(i ? UI_TEXT_HISTORY_MAXIMUM : UI_TEXT_HISTORY_MINIMUM));
            s->number[i]=button(card,100+i*400,140,360,68,s->number_text[i],editor_numeric_open,s);
            if(!s->number[i] || !s->number_title[i]) goto failed;
            lv_obj_set_style_text_font(lv_damped_button_get_label(s->number[i]),&lv_font_instrument_sans_semibold_22,0);
        }
    }
    lv_dropdown_set_selected(s->precision,(uint16_t)precision);
    s->editor_error=label(card,28,250,904,32,&lv_font_instrument_sans_medium_16,0xF85820,"");
    if(!s->editor_error || !lv_nav_button_create(card,28,306,180,44,editor_cancel,s) ||
        !button(card,226,306,180,44,ui_text_get(UI_TEXT_SERIAL_ALL),editor_clear,s)) goto failed;
    lv_obj_t *apply=button(card,680,306,250,44,ui_text_get(UI_TEXT_HISTORY_APPLY),editor_apply,s);
    if(!apply) goto failed;
    lv_damped_button_set_palette(apply,lv_color_hex(0x0074F8),lv_color_hex(0x005BCD));
    lv_obj_set_style_text_color(lv_damped_button_get_label(apply),lv_color_hex(0xFFFFFF),0);
    editor_visibility(s);return;
failed:
    editor_close(s);lv_label_set_text(s->error,ui_text_get(UI_TEXT_SERIAL_UNAVAILABLE));
}

static bool create_options(page_19_history_search_t *s)
{
    bool reject_seen[256] = { false };
    s->currency_count = 1;
    for (size_t r = 0; r < s->record_count; ++r) {
        const history_query_record_t *record = &s->records[r];
        if (!record->valid) continue;
        if(record->multi && record->multi->enabled) {
            for(unsigned g=0;g<record->multi->count;g++) {
                const char *code=record->multi->currencies[g].code;
                size_t i=1;
                while(i<s->currency_count && strcmp(s->currencies[i],code))i++;
                if(i==s->currency_count && i<SEARCH_MAX_CURRENCIES) {
                    memcpy(s->currencies[i],code,4);s->currency_count++;
                }
            }
        }
        if (!record->multi && record->currency[0] >= 'A' && record->currency[0] <= 'Z' &&
            record->currency[1] >= 'A' && record->currency[1] <= 'Z' &&
            record->currency[2] >= 'A' && record->currency[2] <= 'Z' && !record->currency[3]) {
            size_t i = 1;
            while (i < s->currency_count && strcmp(s->currencies[i], record->currency)) ++i;
            if (i == s->currency_count && i < SEARCH_MAX_CURRENCIES) {
                memcpy(s->currencies[i], record->currency, 4); ++s->currency_count;
            }
        }
        if (record->detail) for (size_t i = 0; i < record->detail->reject_count; ++i)
            reject_seen[record->detail->rejects[i].code] = true;
    }
    if (s->input.currency[0]) {
        size_t i = 1;
        while (i < s->currency_count && strcmp(s->currencies[i], s->input.currency)) ++i;
        if (i == s->currency_count && i < SEARCH_MAX_CURRENCIES) {
            memcpy(s->currencies[i], s->input.currency, 4); ++s->currency_count;
        }
    }
    for (size_t i = 1; i < s->currency_count; ++i)
        for (size_t j = i + 1; j < s->currency_count; ++j)
            if (strcmp(s->currencies[i], s->currencies[j]) > 0) {
                char swap[4]; memcpy(swap, s->currencies[i], 4);
                memcpy(s->currencies[i], s->currencies[j], 4); memcpy(s->currencies[j], swap, 4);
            }
    if (s->input.rejects == HISTORY_REJECT_CODE) reject_seen[s->input.reject_code] = true;
    const char *all = ui_text_get(UI_TEXT_SERIAL_ALL);
    size_t capacity = strlen(all) + 1U + (s->currency_count - 1U) * 4U;
    char *currency_options = lv_mem_alloc(capacity);
    if (!currency_options) return false;
    int written = snprintf(currency_options, capacity, "%s", all);
    if (written < 0 || (size_t)written >= capacity) { lv_mem_free(currency_options); return false; }
    size_t used = (size_t)written;
    for (size_t i = 1; i < s->currency_count; ++i) {
        written = snprintf(currency_options + used, capacity - used, "\n%s", s->currencies[i]);
        if (written < 0 || (size_t)written >= capacity - used) {
            lv_mem_free(currency_options); return false;
        }
        used += (size_t)written;
    }
    lv_dropdown_set_options(s->currency, currency_options);
    lv_mem_free(currency_options);
    for (unsigned code = 1; code < 255; ++code)
        if (reject_seen[code]) s->reject_codes[s->reject_count++] = (uint8_t)code;
    capacity = 512 + s->reject_count * 256;
    char *options = lv_mem_alloc(capacity);
    if (!options) return false;
    written = snprintf(options, capacity, "%s\n%s", all, ui_text_get(UI_TEXT_HISTORY_REJECT_ANY));
    if (written < 0 || (size_t)written >= capacity) { lv_mem_free(options); return false; }
    used = (size_t)written;
    for (size_t i = 0; i < s->reject_count; ++i) {
        uint8_t code = s->reject_codes[i];
        written = snprintf(options + used, capacity - used, "\n%02X  %s", code, ui_text_counting_reject_reason(code));
        if (written < 0 || (size_t)written >= capacity - used) { lv_mem_free(options); return false; }
        used += (size_t)written;
    }
    lv_dropdown_set_options(s->rejects, options); lv_mem_free(options);
    return true;
}

static const char *number_caption(const char *value, char *text, size_t size)
{
    const char *range=strstr(value,"..");
    if(range) {
        snprintf(text,size,"%.*s - %s",(int)(range-value),value,range+2);
        return text;
    }
    unsigned mode=!strncmp(value,">=",2) ? 1 : !strncmp(value,"<=",2) ? 2 : 0;
    if(!mode) return value;
    const char *title=ui_text_get(UI_TEXT_HISTORY_NUMBER_MODES);
    for(unsigned i=0;i<mode;++i) {
        const char *next=strchr(title,'\n');
        if(!next) return value;
        title=next+1;
    }
    const char *end=strchr(title,'\n');
    snprintf(text,size,"%.*s %s",(int)(end ? (size_t)(end-title) : strlen(title)),title,value+2);
    return text;
}

static void refresh_controls(page_19_history_search_t *s)
{
    lv_label_set_text(s->error, "");
    for (unsigned field = 0; field < FIELD_COUNT; ++field) {
        size_t size;
        const char *value = field_value(s, field, &size);
        (void)size;
        char caption[128];
        const char *display=(field==FIELD_AMOUNT || field==FIELD_PCS) ?
            number_caption(value,caption,sizeof(caption)) : value;
        lv_damped_button_set_text(s->fields[field], value[0] ? display : ui_text_get(UI_TEXT_SERIAL_ALL));
        selected(s->fields[field],value[0]!=0);
    }
    lv_damped_button_set_enabled(s->fields[FIELD_AMOUNT], s->input.currency[0] != '\0');
    if (!s->input.currency[0]) lv_damped_button_set_text(s->fields[FIELD_AMOUNT], ui_text_get(UI_TEXT_HISTORY_CURRENCY_REQUIRED));
    unsigned currency_index = 0;
    for (size_t i = 1; i < s->currency_count; ++i)
        if (!strcmp(s->input.currency, s->currencies[i])) currency_index = (unsigned)i;
    lv_dropdown_set_selected(s->currency, (uint16_t)currency_index);
    if(s->mode)lv_dropdown_set_selected(s->mode,s->input.mode);
    unsigned reject_index = s->input.rejects == HISTORY_REJECT_SAVED_ANY ? 1 : 0;
    if (s->input.rejects == HISTORY_REJECT_CODE) for (size_t i = 0; i < s->reject_count; ++i)
        if (s->reject_codes[i] == s->input.reject_code) reject_index = (unsigned)i + 2;
    lv_dropdown_set_selected(s->rejects, (uint16_t)reject_index);
    for (unsigned i = 0; i < 4; ++i) selected(s->matches[i], s->input.serial_match == (history_text_match_t)i);
    selected(s->denom_all, s->input.denomination_count == 0);
    for (size_t i = 0; i < SEARCH_MAX_DENOMS; ++i) {
        if (i >= s->denomination_count) { lv_obj_add_flag(s->denom_buttons[i], LV_OBJ_FLAG_HIDDEN); continue; }
        char text[24]; snprintf(text, sizeof(text), "%u", (unsigned)s->denominations[i]);
        lv_damped_button_set_text(s->denom_buttons[i], text);
        selected(s->denom_buttons[i], denomination_selected(s, s->denominations[i]) >= 0);
        lv_obj_clear_flag(s->denom_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (s->denomination_count) lv_obj_add_flag(s->denom_empty, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(s->denom_empty, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s->denom_empty, ui_text_get(s->input.currency[0] ? UI_TEXT_HISTORY_DENOM_EMPTY : UI_TEXT_HISTORY_CURRENCY_REQUIRED));
    if (s->denominations_limited) {
        char text[96]; snprintf(text, sizeof(text), ui_text_get(UI_TEXT_SERIAL_DENOM_LIMIT), (unsigned)SEARCH_MAX_DENOMS);
        lv_label_set_text(s->denom_title, text);
    } else lv_label_set_text(s->denom_title, ui_text_get(UI_TEXT_HISTORY_DENOMS));
}

static void deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event))
        lv_mem_free(lv_event_get_user_data(event));
}

page_19_history_search_t *page_19_history_search_create(lv_obj_t *parent,
    const history_query_input_t *initial, const history_query_record_t *records,
    size_t count, page_19_history_search_close_cb_t close, void *context)
{
    history_query_t validated;
    if (!parent || !close || (count && !records) || count > UI_HISTORY_MAX_RECORDS ||
        (initial && history_query_compile(initial, &validated) != HISTORY_QUERY_OK)) return NULL;
    page_19_history_search_t *s = lv_mem_alloc(sizeof(*s));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    if (initial) s->input = *initial;
    s->records = records; s->record_count = count; s->close = close; s->context = context;
    s->root = surface(parent, 0, 0, 1280, 400, 0xD8E2E8, 0);
    ui_page_background_apply(s->root, UI_BACKGROUND_USER);
    if (!s->root) { lv_mem_free(s); return NULL; }
    if (!lv_obj_add_event_cb(s->root, deleted, LV_EVENT_DELETE, s)) {
        lv_obj_del(s->root); lv_mem_free(s); return NULL;
    }
    lv_obj_add_flag(s->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s->root, LV_OBJ_FLAG_GESTURE_BUBBLE);
    if (!label(s->root, 22, 17, 800, 30, &lv_font_instrument_sans_semibold_22,
               SEARCH_INK, ui_text_get(UI_TEXT_HISTORY_SEARCH_TITLE))) goto failed;
    lv_obj_t *esc = lv_nav_button_create(s->root, 1168, 12, 96, 36, cancel_event, s);
    if (!esc) goto failed;
    lv_obj_set_style_text_font(lv_damped_button_get_label(esc),&lv_font_instrument_sans_medium_18,0);
    lv_damped_button_set_text(esc, ui_text_get(UI_TEXT_SERIAL_ESC));
    s->mode=dropdown(s->root,840,12,304,s);
    if(!s->mode)goto failed;
    lv_dropdown_set_options(s->mode,ui_text_get(UI_TEXT_HISTORY_MODE_OPTIONS));
    if(!label(s->root,24,329,888,20,&lv_font_instrument_sans_medium_12,SEARCH_MUTED,
        ui_text_get(UI_TEXT_HISTORY_MULTI_SCOPE)))goto failed;
    static const ui_text_id_t tab_names[] = { UI_TEXT_HISTORY_DATE, UI_TEXT_HISTORY_VALUES, UI_TEXT_HISTORY_SERIALS };
    for (unsigned i = 0; i < 3; ++i) {
        s->tabs[i] = button(s->root, 16 + i * 420, 60, 408, 38, ui_text_get(tab_names[i]), tab_event, s);
        s->panels[i] = surface(s->root, 16, 108, 1248, 222, 0xFFFFFF, 15);
        if (!s->tabs[i] || !s->panels[i]) goto failed;
    }
    for (unsigned i = 0; i < 3; ++i) {
        int x = 24 + i * 408;
        if (!label(s->panels[0], x, 46, 384, 28, &lv_font_instrument_sans_medium_18,
                   SEARCH_BODY, ui_text_get(field_title(i)))) goto failed;
        s->fields[i] = button(s->panels[0], x, 90, 384, 72, "", input_event, s);
        if (!s->fields[i]) goto failed;
    }
    s->currency = dropdown(s->panels[1], 20, 43, 278, s);
    if (!s->currency || !label(s->panels[1], 20, 15, 278, 24,
            &lv_font_instrument_sans_semibold_14, SEARCH_BODY, ui_text_get(UI_TEXT_HISTORY_CURRENCY))) goto failed;
    for (unsigned i = 0; i < 2; ++i) {
        unsigned field = i == 0 ? FIELD_AMOUNT : FIELD_PCS;
        int x = 318 + i * 454;
        if (!label(s->panels[1], x, 15, 436, 24, &lv_font_instrument_sans_semibold_14,
                   SEARCH_BODY, ui_text_get(field_title(field)))) goto failed;
        s->fields[field] = button(s->panels[1], x, 43, 436, 42, "", input_event, s);
        if (!s->fields[field]) goto failed;
    }
    s->denom_title = label(s->panels[1], 20, 99, 168, 24, &lv_font_instrument_sans_semibold_14, SEARCH_BODY, "");
    s->denom_all = button(s->panels[1], 194, 94, 104, 30, ui_text_get(UI_TEXT_SERIAL_ALL), filter_event, s);
    if (!s->denom_title || !s->denom_all) goto failed;
    lv_obj_t *grid = surface(s->panels[1], 20, 132, 1208, 78, 0xFFFFFF, 0);
    if (!grid) goto failed;
    lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER); lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLL_CHAIN_VER | LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_port_indev_set_drag_obj(grid, true);
    for (size_t i = 0; i < SEARCH_MAX_DENOMS; ++i) {
        s->denom_buttons[i] = button(grid, (i % 12) * 100, (i / 12) * 39, 92, 33, "", filter_event, s);
        if (!s->denom_buttons[i]) goto failed;
    }
    s->denom_empty = label(grid, 4, 18, 1180, 42, &lv_font_instrument_sans_medium_18, SEARCH_MUTED, "");
    if (!s->denom_empty) goto failed;
    if (!label(s->panels[2], 24, 15, 786, 24, &lv_font_instrument_sans_semibold_14,
               SEARCH_BODY, ui_text_get(UI_TEXT_LIST_SERIAL_NUMBERS)) ||
        !label(s->panels[2], 854, 15, 368, 24, &lv_font_instrument_sans_semibold_14,
               SEARCH_BODY, ui_text_get(UI_TEXT_HISTORY_REJECTS))) goto failed;
    s->fields[FIELD_SERIAL] = button(s->panels[2], 24, 43, 804, 44, "", input_event, s);
    s->rejects = dropdown(s->panels[2], 854, 43, 370, s);
    if (!s->fields[FIELD_SERIAL] || !s->rejects) goto failed;
    static const ui_text_id_t modes[] = { UI_TEXT_SERIAL_CONTAINS, UI_TEXT_SERIAL_EXACT, UI_TEXT_SERIAL_STARTS, UI_TEXT_SERIAL_ENDS };
    for (unsigned i = 0; i < 4; ++i) {
        s->matches[i] = button(s->panels[2], 24 + i * 204, 104, 192, 38,
                               ui_text_get(modes[i]), filter_event, s);
        if (!s->matches[i]) goto failed;
    }
    s->error = label(s->root, 24, 348, 892, 43, &lv_font_instrument_sans_medium_14, 0xAC4C3D, "");
    if (!s->error || !button(s->root, 936, 346, 146, 42, ui_text_get(UI_TEXT_SERIAL_RESET), reset_event, s)) goto failed;
    lv_obj_t *apply=button(s->root, 1096, 346, 168, 42, ui_text_get(UI_TEXT_HISTORY_APPLY), apply_event, s);
    if(!apply) goto failed;
    lv_damped_button_set_palette(apply,lv_color_hex(0x0074F8),lv_color_hex(0x005BCD));
    lv_obj_set_style_text_color(lv_damped_button_get_label(apply),lv_color_hex(0xFFFFFF),0);
    if (!create_options(s)) goto failed;
    collect_denominations(s); refresh_controls(s); activate_tab(s, 0);
    lv_obj_move_foreground(s->root);
    return s;
failed:
    lv_obj_del(s->root); return NULL;
}

void page_19_history_search_destroy(page_19_history_search_t *s)
{ if (s) lv_obj_del(s->root); }
