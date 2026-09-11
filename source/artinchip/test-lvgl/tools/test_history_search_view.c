#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "test_history_view_support.h"
#include "un260/lv_system/ui_lang.h"
/* Separate translation unit keeps production page/search private helper names
 * independent while testing real search objects without firmware accessors. */
#include "un260/lv_core/page_19_history_search.c"

typedef struct {
    page_19_history_search_t *owner;
    unsigned closed, applied;
    history_query_input_t input;
} search_test_context_t;

static void search_test_closed(const history_query_input_t *input, void *context)
{
    search_test_context_t *test = context;
    assert(test->owner && !lv_obj_is_visible(test->owner->root));
    ++test->closed;
    if (input) { ++test->applied; test->input = *input; }
    page_19_history_search_destroy(test->owner);
    test->owner = NULL;
}

static void search_test_input(page_19_history_search_t *s, unsigned field, const char *text)
{
    if(field!=FIELD_SERIAL) {
        /* Legacy syntax/invalid-input defence belongs to the query layer;
         * picker pointer interaction is exercised separately below. */
        size_t size;char *value=field_value(s,field,&size);
        snprintf(value,size,"%s",text);refresh_controls(s);return;
    }
    history_test_click(s->fields[field]);
    assert(s->keyboard && lv_alnum_keyboard_is_visible(s->keyboard));
    assert(lv_alnum_keyboard_set_text(s->keyboard, text));
    history_test_click(history_test_button(s->root, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!lv_alnum_keyboard_is_visible(s->keyboard));
}

static void search_test_dropdown(lv_obj_t *object, unsigned index)
{
    lv_dropdown_set_selected(object, (uint16_t)index);
    lv_event_send(object, LV_EVENT_VALUE_CHANGED, NULL);
    history_test_tick(40);
}

static void search_test_currency(page_19_history_search_t *s, const char *currency)
{
    for (size_t i = 0; i < s->currency_count; ++i) {
        if (!strcmp(s->currencies[i], currency)) {
            search_test_dropdown(s->currency, (unsigned)i); return;
        }
    }
    assert(!"Expected stored currency missing from options");
}

void history_test_search_apply(page_19_history_search_t *search,
                                const history_query_input_t *input)
{
    assert(search && input && lv_obj_is_visible(search->root));
    /* Control-by-control editing is covered below. This bridge exercises the
     * real validation, Apply click and page callback for integration cases. */
    search->input = *input;
    collect_denominations(search);
    refresh_controls(search);
    history_test_click(history_test_button(search->root, ui_text_get(UI_TEXT_HISTORY_APPLY)));
}

static void search_test_editors(void)
{
    char caption[128];
    assert(!strcmp(number_caption(">=100",caption,sizeof(caption)),"At least 100"));
    assert(!strcmp(number_caption("<=100",caption,sizeof(caption)),"At most 100"));
    assert(!strcmp(number_caption("4..10",caption,sizeof(caption)),"4 - 10"));
    assert(!strcmp(number_caption("100",caption,sizeof(caption)),"100"));
    search_test_context_t test={0};history_query_input_t input={0};
    strcpy(input.currency,"USD");
    test.owner=page_19_history_search_create(lv_scr_act(),&input,NULL,0,search_test_closed,&test);
    page_19_history_search_t *s=test.owner;assert(s);
    history_test_click(s->fields[FIELD_FROM]);assert(s->editor && !s->keyboard);
    assert(s->year_value==2026);
    for(unsigned i=0;i<3;++i) {
        assert(lv_obj_get_style_text_font(s->wheels[i],LV_PART_MAIN)==
            lv_obj_get_style_text_font(s->wheels[i],LV_PART_SELECTED));
        assert(lv_obj_get_style_text_line_space(s->wheels[i],LV_PART_MAIN)==
            lv_obj_get_style_text_line_space(s->wheels[i],LV_PART_SELECTED));
    }
    lv_roller_set_selected(s->wheels[0],(uint16_t)(2024-s->year_first),LV_ANIM_OFF);
    lv_roller_set_selected(s->wheels[1],1,LV_ANIM_OFF);
    lv_event_send(s->wheels[1],LV_EVENT_VALUE_CHANGED,NULL);
    assert(lv_roller_get_option_cnt(s->wheels[2])==29);
    lv_roller_set_selected(s->wheels[2],28,LV_ANIM_OFF);
    lv_obj_update_layout(s->editor);
    for(unsigned i=0;i<3;++i) {
        lv_obj_t *l=lv_obj_get_child(s->wheels[i],0);
        unsigned line=lv_obj_get_style_text_font(l,0)->line_height;
        unsigned space=lv_obj_get_style_text_line_space(l,0);
        assert(lv_obj_get_height(l)==(int)(lv_roller_get_option_cnt(s->wheels[i])*(line+space)-space));
    }
    history_test_bmp("history-date-rollers");
    history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!s->editor && !strcmp(s->input.date_from,"2024-02-29"));
    history_test_click(s->fields[FIELD_FROM]);
    lv_roller_set_selected(s->wheels[0],(uint16_t)(2025-s->year_first),LV_ANIM_OFF);
    lv_event_send(s->wheels[0],LV_EVENT_VALUE_CHANGED,NULL);
    assert(lv_roller_get_option_cnt(s->wheels[2])==28 && lv_roller_get_selected(s->wheels[2])==27);
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    assert(!s->editor && !strcmp(s->input.date_from,"2024-02-29"));
    history_test_click(s->fields[FIELD_FROM]);
    search_test_dropdown(s->precision,0);
    assert(!lv_obj_is_visible(s->wheels[1]));
    history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!strcmp(s->input.date_from,"2024"));
    const char *years[]={"1","9999","2024"};
    const char *expected_years[]={"0001","9999","2024"};
    for(unsigned i=0;i<3;++i) {
        history_test_click(s->fields[FIELD_FROM]);
        history_test_click(s->year_button);assert(s->keyboard);
        assert(lv_alnum_keyboard_set_text(s->keyboard,years[i]));
        history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
        assert(s->year_value==(unsigned)atoi(years[i]));
        assert(lv_roller_get_option_cnt(s->wheels[0])<=101);
        history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
        assert(!s->editor && !strcmp(s->input.date_from,expected_years[i]));
    }
    history_test_click(s->fields[FIELD_TIME]);
    search_test_dropdown(s->precision,1);
    lv_roller_set_selected(s->wheels[0],23,LV_ANIM_OFF);
    lv_roller_set_selected(s->wheels[1],59,LV_ANIM_OFF);
    history_test_bmp("history-time-rollers");
    history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!strcmp(s->input.time,"23:59"));
    history_test_click(s->tabs[1]);history_test_click(s->fields[FIELD_PCS]);
    search_test_dropdown(s->precision,3);
    for(unsigned i=0;i<2;++i) {
        history_test_click(s->number[i]);assert(s->keyboard);
        assert(lv_alnum_keyboard_set_text(s->keyboard,i ? "10" : "20"));
        history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
    }
    history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(s->editor && lv_label_get_text(s->editor_error)[0] && !s->input.pcs[0]);
    history_test_click(s->number[1]);assert(lv_alnum_keyboard_set_text(s->keyboard,"100"));
    history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
    history_test_bmp("history-number-range");
    history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!s->editor && !strcmp(s->input.pcs,"20..100"));
    history_test_click(s->fields[FIELD_PCS]);
    history_test_click(history_test_button(s->editor,ui_text_get(UI_TEXT_SERIAL_ALL)));
    assert(!s->input.pcs[0]);
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    assert(!test.owner);
    puts("PASS date/time rollers, leap clamping, precision, cancel, numeric ranges and clear");
}

static void search_test_capacity(void)
{
    history_query_record_t records[100] = {0};
    for (unsigned i = 0; i < 100; ++i) {
        records[i].valid = true;
        records[i].record_no = 2000U + i;
        records[i].currency[0] = 'A';
        records[i].currency[1] = (char)('A' + i / 26U);
        records[i].currency[2] = (char)('A' + i % 26U);
    }
    const size_t counts[] = {21, 100};
    language_t previous = ui_lang_get();
    const language_t languages[] = {LANGUAGE_EN, LANGUAGE_CN, LANGUAGE_KR};
    unsigned baseline = history_test_timers();
    for (unsigned language = 0; language < 3; ++language) {
        ui_lang_set(languages[language]);
        for (unsigned size = 0; size < 2; ++size) {
            history_query_input_t initial = {.currency="ZZZ"};
            search_test_context_t test = {0};
            test.owner = page_19_history_search_create(lv_scr_act(), &initial, records,
                counts[size], search_test_closed, &test);
            assert(test.owner); /* More than twenty records must still open Search. */
            page_19_history_search_t *s = test.owner;
            assert(s->currency_count == counts[size] + 2U); /* All + records + retained filter. */
            assert(lv_dropdown_get_option_cnt(s->currency) == s->currency_count);
            const char *options = lv_dropdown_get_options(s->currency);
            const char *all = ui_text_get(UI_TEXT_SERIAL_ALL);
            assert(strlen(options) == strlen(all) + 4U * (counts[size] + 1U));
            assert(!strncmp(options, all, strlen(all)) && options[strlen(all)] == '\n');
            for (size_t i = 0; i < counts[size]; ++i) search_test_currency(s, records[i].currency);
            search_test_currency(s, "ZZZ");
            assert(!strcmp(s->input.currency, "ZZZ"));
            assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
            assert(!test.owner && test.closed == 1 && test.applied == 0);
            history_test_tick(300);
            assert(history_test_timers() == baseline);
        }
    }
    ui_lang_set(previous);
    puts("PASS: Search opens 21/100 records; 100 distinct currencies plus retained filter and translated All fit exactly");
}

static void search_test_serial_denominations(void)
{
    history_record_detail_t details[4] = {0};
    history_query_record_t records[4] = {0};
    const char *serials[] = {"1\t100\tMATCH001\n2\t50\tOTHER001",
        "1\t50\tMATCH001\n2\t100\tOTHER001", "1\t999\tMATCH001", ""};
    for (unsigned i = 0; i < 4; ++i) {
        history_detail_input_t saved = {.sn_detail_text=serials[i],
            .sn_text=i == 3 ? "MATCH001" : "", .total_pcs=2,
            .session_log="0x0C FD DF 07 0C 15 02 0A\n"};
        assert(history_record_detail_build(&saved, &details[i]));
        records[i] = (history_query_record_t){.valid=true, .record_no=1000U+i,
            .pcs=2, .amount=150, .year=2026, .month=9, .day=10,
            .hour=12, .minute=34, .second=56, .detail=&details[i]};
        memcpy(records[i].currency, i == 2 ? "EUR" : "USD", 4);
    }
    history_query_input_t initial = {.currency="USD", .date_from="202609",
        .date_to="2026-09", .time="12:34", .pcs="2..2", .amount=">=100",
        .serial="MATCH", .serial_match=HISTORY_TEXT_PREFIX,
        .rejects=HISTORY_REJECT_CODE, .reject_code=0x15};
    search_test_context_t test = {0};
    test.owner = page_19_history_search_create(lv_scr_act(), &initial, records, 4,
        search_test_closed, &test);
    assert(test.owner);
    page_19_history_search_t *s = test.owner;
    assert(s->denomination_count == 2 && s->denominations[0] == 50 && s->denominations[1] == 100);
    history_test_click(s->tabs[1]);
    history_test_click(s->denom_buttons[1]);
    assert(s->input.denomination_count == 1 && s->input.denominations[0] == 100);
    history_test_click(history_test_button(s->root, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!test.owner && test.applied == 1);
    history_query_t query;
    assert(history_query_compile(&test.input, &query) == HISTORY_QUERY_OK);
    uint32_t ids[4] = {0};
    history_query_result_t result = history_query_build(records, 4, &query, ids, 4);
    assert(result.matched_count == 1 && result.written_count == 1 && ids[0] == 1000);
    assert(result.unknown_count == 1); /* Missing denomination is still unknown. */
    assert(result.matched_pcs == 2 && result.matched_amount == 150);
    assert(history_query_match_record(&query, &records[1]) == HISTORY_QUERY_NO_MATCH); /* Same note required. */
    for (unsigned i = 0; i < 4; ++i) history_record_detail_release(&details[i]);
    history_test_tick(300);
    puts("PASS: SN-only denominations, same-currency deduplication and date/time/PCS/amount/serial/reject combination preserve unknowns");
}

void history_test_search_module(void)
{
    search_test_editors();
    search_test_capacity();
    search_test_serial_denominations();
    history_record_detail_t details[2] = {0};
    const history_detail_input_t inputs[] = {
        {.denom_text = "100 x 2\n50 x 2\n", .sn_detail_text = "1\t100\tSHARED0001\n",
         .session_log = "0x0C FD DF 07 0C 15 02 0A\n", .total_pcs = 4},
        {.denom_text = "20 x 10\n", .sn_detail_text = "1\t20\tSHARED0002\n", .total_pcs = 10}
    };
    history_query_record_t records[] = {
        {.record_no = 501, .pcs = 4, .amount = 300, .currency = "USD", .year = 2026, .month = 9, .day = 9, .valid = true, .detail = &details[0]},
        {.record_no = 500, .pcs = 10, .amount = 200, .currency = "EUR", .year = 2026, .month = 8, .day = 8, .valid = true, .detail = &details[1]}
    };
    assert(history_record_detail_build(&inputs[0], &details[0]));
    assert(history_record_detail_build(&inputs[1], &details[1]));
    history_query_record_t before[2]; memcpy(before, records, sizeof(before));
    unsigned baseline = history_test_timers();
    search_test_context_t test = {0};
    history_query_input_t initial = {0};
    test.owner = page_19_history_search_create(lv_scr_act(), &initial, records, 2, search_test_closed, &test);
    assert(test.owner);
    page_19_history_search_t *s = test.owner;
    assert(s->currency_count == 3 && s->active_tab == 0);
    history_test_bmp("history-search-date");
    search_test_input(s, FIELD_FROM, "20260230");
    history_test_click(history_test_button(s->root, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(test.closed == 0 && test.owner == s && lv_obj_is_visible(s->root));
    assert(lv_label_get_text(s->error)[0]);
    history_test_bmp("history-search-date-error");
    search_test_input(s, FIELD_FROM, "2026");
    search_test_input(s, FIELD_TO, "202609");
    search_test_input(s, FIELD_TIME, "12");
    history_test_click(s->tabs[1]);
    assert(!lv_damped_button_is_enabled(s->fields[FIELD_AMOUNT]));
    assert(s->denomination_count == 0);
    search_test_currency(s, "USD");
    assert(!strcmp(s->input.currency, "USD") && lv_damped_button_is_enabled(s->fields[FIELD_AMOUNT]));
    assert(s->denomination_count == 2);
    history_test_click(s->denom_buttons[0]);
    assert(s->input.denomination_count == 1);
    search_test_input(s, FIELD_AMOUNT, ">=100");
    search_test_input(s, FIELD_PCS, "300..100");
    history_test_click(history_test_button(s->root, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(test.closed == 0 && s->active_tab == 1 && lv_label_get_text(s->error)[0]);
    search_test_input(s, FIELD_PCS, "4..10");
    history_test_bmp("history-search-values");
    search_test_currency(s, "EUR");
    assert(!strcmp(s->input.currency, "EUR"));
    assert(!s->input.amount[0] && !s->input.denomination_count);
    assert(!strcmp(s->input.pcs, "4..10"));
    assert(s->denomination_count == 1 && s->denominations[0] == 20);
    search_test_dropdown(s->currency, 0);
    assert(!s->input.currency[0] && !lv_damped_button_is_enabled(s->fields[FIELD_AMOUNT]));
    history_test_click(s->tabs[2]);
    search_test_input(s, FIELD_SERIAL, "shared");
    for (unsigned i = 0; i < 4; ++i) {
        history_test_click(s->matches[i]);
        assert(s->input.serial_match == (history_text_match_t)i);
    }
    history_test_click(s->matches[0]);
    search_test_dropdown(s->rejects, 2);
    assert(s->input.rejects == HISTORY_REJECT_CODE && s->input.reject_code == 0x15);
    history_test_bmp("history-search-serials");
    history_test_click(history_test_button(s->root, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!test.owner && test.closed == 1 && test.applied == 1);
    assert(!strcmp(test.input.serial, "shared") && !strcmp(test.input.date_from, "2026"));
    assert(test.input.reject_code == 0x15 && !test.input.currency[0]);
    assert(!memcmp(before, records, sizeof(before)));
    history_test_tick(300);
    assert(history_test_timers() == baseline);

    test.owner = page_19_history_search_create(lv_scr_act(), &initial, records, 2, search_test_closed, &test);
    assert(test.owner); s = test.owner;
    history_test_click(s->tabs[2]);
    history_test_click(s->fields[FIELD_SERIAL]);
    assert(lv_alnum_keyboard_is_visible(s->keyboard));
    history_test_click(history_test_button(s->root, "#+="));
    history_test_click(history_test_button(s->root, ">"));
    history_test_click(history_test_button(s->root, "="));
    history_test_click(history_test_button(s->root, "1"));
    assert(!strcmp(lv_alnum_keyboard_get_text(s->keyboard), ">=1"));
    history_test_bmp("history-search-symbol-keyboard");
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(!lv_alnum_keyboard_is_visible(s->keyboard) && lv_obj_is_visible(s->root));
    assert(test.closed == 1 && !s->input.date_from[0]);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(!test.owner && test.closed == 2 && test.applied == 1);
    history_test_tick(300);
    assert(history_test_timers() == baseline);

    strcpy(initial.currency, "USD"); initial.denominations[0] = 999; initial.denomination_count = 1;
    test.owner = page_19_history_search_create(lv_scr_act(), &initial, records, 2, search_test_closed, &test);
    assert(test.owner); s = test.owner;
    history_test_click(s->tabs[1]);
    size_t retained = 0;
    while (retained < s->denomination_count && s->denominations[retained] != 999) ++retained;
    assert(retained < s->denomination_count);
    assert(lv_obj_is_visible(s->denom_buttons[retained]));
    assert(lv_obj_has_state(s->denom_buttons[retained], LV_STATE_CHECKED));
    history_test_bmp("history-search-retained-denomination");
    history_test_click(s->denom_buttons[retained]);
    assert(!s->input.denomination_count);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(!test.owner && test.closed == 3 && test.applied == 1);
    history_test_tick(300);
    assert(history_test_timers() == baseline);
    history_record_detail_release(&details[0]); history_record_detail_release(&details[1]);
    puts("PASS: History search real LVGL controls, dependent filters, invalid dates/ranges, four serial modes, symbols and layered ESC/destroy callback");
}
