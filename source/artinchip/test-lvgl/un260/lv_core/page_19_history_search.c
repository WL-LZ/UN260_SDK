#include "page_19_history_search.h"
#include "lv_port_indev.h"
#include "un260/lv_components/lv_alnum_keyboard.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_system/ui_text.h"
#include <stdio.h>
#include <string.h>

#define SEARCH_INK 0x17212A
#define SEARCH_BODY 0x4C606E
#define SEARCH_MUTED 0x7A8D9B
#define SEARCH_LINE 0xE7ECEF
#define SEARCH_MAX_CURRENCIES 22
#define SEARCH_MAX_DENOMS HISTORY_DETAIL_MAX_DENOMS
enum { FIELD_FROM, FIELD_TO, FIELD_TIME, FIELD_AMOUNT, FIELD_PCS, FIELD_SERIAL, FIELD_COUNT };

struct page_19_history_search {
    lv_obj_t *root, *tabs[3], *panels[3], *fields[FIELD_COUNT], *error;
    lv_obj_t *currency, *rejects, *denom_title, *denom_empty, *denom_all;
    lv_obj_t *denom_buttons[SEARCH_MAX_DENOMS], *matches[4];
    lv_alnum_keyboard_t *keyboard;
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
    uint32_t color = active ? 0xDCE6EC : 0xF4F6F7;
    if (active) lv_obj_add_state(o, LV_STATE_CHECKED);
    else lv_obj_clear_state(o, LV_STATE_CHECKED);
    lv_damped_button_set_palette(o, lv_color_hex(color), lv_color_hex(color));
    lv_obj_set_style_border_width(o, active ? 1 : 0, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0xA8BAC6), 0);
    lv_obj_set_style_border_opa(o, active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
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
        if (!r->valid || !r->detail || strcmp(r->currency, s->input.currency)) continue;
        for (size_t d = 0; d < r->detail->denom_count; ++d) {
            uint32_t value = r->detail->denoms[d].value;
            if (!value || !r->detail->denoms[d].pcs) continue;
            offer_denomination(s, value);
        }
    }
}

static void activate_tab(page_19_history_search_t *s, unsigned tab)
{
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
    if (s->keyboard) { lv_alnum_keyboard_destroy(s->keyboard); s->keyboard = NULL; }
    size_t size;
    char *value = field_value(s, field, &size);
    s->editing_field = field;
    const lv_alnum_keyboard_config_t config = {
        .title = ui_text_get(field_title(field)),
        .placeholder = ui_text_get(field == FIELD_SERIAL ? UI_TEXT_SERIAL_INPUT_HINT :
            (field <= FIELD_TO ? UI_TEXT_HISTORY_DATE_HINT :
             field == FIELD_TIME ? UI_TEXT_HISTORY_TIME_HINT : UI_TEXT_HISTORY_NUMBER_HINT)),
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

static bool create_options(page_19_history_search_t *s)
{
    char currency_options[128];
    bool reject_seen[256] = { false };
    s->currency_count = 1;
    for (size_t r = 0; r < s->record_count; ++r) {
        const history_query_record_t *record = &s->records[r];
        if (!record->valid) continue;
        if (record->currency[0] >= 'A' && record->currency[0] <= 'Z' &&
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
    size_t used = (size_t)snprintf(currency_options, sizeof(currency_options), "%s", ui_text_get(UI_TEXT_SERIAL_ALL));
    for (size_t i = 1; i < s->currency_count; ++i)
        used += (size_t)snprintf(currency_options + used, sizeof(currency_options) - used, "\n%s", s->currencies[i]);
    lv_dropdown_set_options(s->currency, currency_options);
    for (unsigned code = 1; code < 255; ++code)
        if (reject_seen[code]) s->reject_codes[s->reject_count++] = (uint8_t)code;
    size_t capacity = 512 + s->reject_count * 256;
    char *options = lv_mem_alloc(capacity);
    if (!options) return false;
    int written = snprintf(options, capacity, "%s\n%s", ui_text_get(UI_TEXT_SERIAL_ALL), ui_text_get(UI_TEXT_HISTORY_REJECT_ANY));
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

static void refresh_controls(page_19_history_search_t *s)
{
    lv_label_set_text(s->error, "");
    for (unsigned field = 0; field < FIELD_COUNT; ++field) {
        size_t size;
        const char *value = field_value(s, field, &size);
        (void)size;
        lv_damped_button_set_text(s->fields[field], value[0] ? value : ui_text_get(UI_TEXT_SERIAL_ALL));
    }
    lv_damped_button_set_enabled(s->fields[FIELD_AMOUNT], s->input.currency[0] != '\0');
    if (!s->input.currency[0]) lv_damped_button_set_text(s->fields[FIELD_AMOUNT], ui_text_get(UI_TEXT_HISTORY_CURRENCY_REQUIRED));
    unsigned currency_index = 0;
    for (size_t i = 1; i < s->currency_count; ++i)
        if (!strcmp(s->input.currency, s->currencies[i])) currency_index = (unsigned)i;
    lv_dropdown_set_selected(s->currency, (uint16_t)currency_index);
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
    if (!parent || !close || (count && !records) || count > SEARCH_MAX_CURRENCIES - 2 ||
        (initial && history_query_compile(initial, &validated) != HISTORY_QUERY_OK)) return NULL;
    page_19_history_search_t *s = lv_mem_alloc(sizeof(*s));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    if (initial) s->input = *initial;
    s->records = records; s->record_count = count; s->close = close; s->context = context;
    s->root = surface(parent, 0, 0, 1280, 400, 0xD8E2E8, 0);
    if (!s->root) { lv_mem_free(s); return NULL; }
    if (!lv_obj_add_event_cb(s->root, deleted, LV_EVENT_DELETE, s)) {
        lv_obj_del(s->root); lv_mem_free(s); return NULL;
    }
    lv_obj_add_flag(s->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s->root, LV_OBJ_FLAG_GESTURE_BUBBLE);
    if (!label(s->root, 22, 17, 330, 30, &lv_font_instrument_sans_semibold_22,
               SEARCH_INK, ui_text_get(UI_TEXT_HISTORY_SEARCH_TITLE)) ||
        !label(s->root, 365, 20, 783, 30, &lv_font_instrument_sans_medium_14,
               SEARCH_MUTED, ui_text_get(UI_TEXT_HISTORY_FILTER_SCOPE))) goto failed;
    lv_obj_t *esc = lv_nav_button_create(s->root, 1168, 12, 96, 36, cancel_event, s);
    if (!esc) goto failed;
    lv_damped_button_set_text(esc, ui_text_get(UI_TEXT_SERIAL_ESC));
    static const ui_text_id_t tab_names[] = { UI_TEXT_HISTORY_DATE, UI_TEXT_HISTORY_VALUES, UI_TEXT_HISTORY_SERIALS };
    for (unsigned i = 0; i < 3; ++i) {
        s->tabs[i] = button(s->root, 16 + i * 420, 60, 408, 38, ui_text_get(tab_names[i]), tab_event, s);
        s->panels[i] = surface(s->root, 16, 108, 1248, 222, 0xFFFFFF, 15);
        if (!s->tabs[i] || !s->panels[i]) goto failed;
    }
    for (unsigned i = 0; i < 3; ++i) {
        int x = 24 + i * 408;
        if (!label(s->panels[0], x, 17, 384, 24, &lv_font_instrument_sans_semibold_14,
                   SEARCH_BODY, ui_text_get(field_title(i)))) goto failed;
        s->fields[i] = button(s->panels[0], x, 48, 384, 44, "", input_event, s);
        if (!s->fields[i]) goto failed;
    }
    if (!label(s->panels[0], 24, 116, 788, 86, &lv_font_instrument_sans_medium_18,
               SEARCH_MUTED, ui_text_get(UI_TEXT_HISTORY_DATE_HINT)) ||
        !label(s->panels[0], 840, 116, 384, 86, &lv_font_instrument_sans_medium_18,
               SEARCH_MUTED, ui_text_get(UI_TEXT_HISTORY_TIME_HINT))) goto failed;
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
    if (!s->denom_title || !s->denom_all || !label(s->panels[1], 318, 97, 888, 30,
            &lv_font_instrument_sans_medium_14, SEARCH_MUTED, ui_text_get(UI_TEXT_HISTORY_NUMBER_HINT))) goto failed;
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
    if (!label(s->panels[2], 24, 166, 1200, 45, &lv_font_instrument_sans_medium_14,
               SEARCH_MUTED, ui_text_get(UI_TEXT_HISTORY_LIMITED))) goto failed;
    s->error = label(s->root, 24, 348, 892, 43, &lv_font_instrument_sans_medium_14, 0xAC4C3D, "");
    if (!s->error || !button(s->root, 936, 346, 146, 42, ui_text_get(UI_TEXT_SERIAL_RESET), reset_event, s) ||
        !button(s->root, 1096, 346, 168, 42, ui_text_get(UI_TEXT_HISTORY_APPLY), apply_event, s)) goto failed;
    if (!create_options(s)) goto failed;
    collect_denominations(s); refresh_controls(s); activate_tab(s, 0);
    lv_obj_move_foreground(s->root);
    return s;
failed:
    lv_obj_del(s->root); return NULL;
}

void page_19_history_search_destroy(page_19_history_search_t *s)
{ if (s) lv_obj_del(s->root); }
