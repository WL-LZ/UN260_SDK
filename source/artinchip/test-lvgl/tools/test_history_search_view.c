#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "test_history_view_support.h"
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

void history_test_search_module(void)
{
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
    history_test_click(s->fields[FIELD_FROM]);
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
