#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_serial_query.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define SLOT_GUARD UINT16_C(0xa55a)

static void expect_slots(const counting_sim_t *data, const counting_serial_query_t *query,
                         const uint16_t *expected, size_t count)
{
    uint16_t output[24];
    counting_sim_t before;
    counting_serial_query_t query_before;
    if (data != NULL) memcpy(&before, data, sizeof(before));
    if (query != NULL) memcpy(&query_before, query, sizeof(query_before));
    for (size_t i = 0; i < ARRAY_COUNT(output); ++i) output[i] = SLOT_GUARD;
    counting_serial_query_result_t result =
        counting_serial_query_build(data, query, output, ARRAY_COUNT(output));
    assert(result.valid_count == (uint32_t)counting_data_serial_valid_count(data));
    assert(result.matched_count == count && result.written_count == count);
    if (count > 0) assert(memcmp(output, expected, count * sizeof(*expected)) == 0);
    for (size_t i = count; i < ARRAY_COUNT(output); ++i) assert(output[i] == SLOT_GUARD);
    if (data != NULL) assert(memcmp(&before, data, sizeof(before)) == 0);
    if (query != NULL) assert(memcmp(&query_before, query, sizeof(query_before)) == 0);
}

static void sparse_fixture(counting_sim_t *data, char **slots)
{
    memset(data, 0, sizeof(*data));
    memset(slots, 0, 20 * sizeof(*slots));
    data->sn_str = slots;
    data->sn_capacity = 20;
    data->total_pcs = 1; /* A late count must not bound serial scanning. */
    slots[1] = "AbC01"; data->denom_mix[1] = 100;
    slots[3] = "xxabc01yy"; data->denom_mix[3] = 50;
    slots[4] = "AbC01"; data->denom_mix[4] = 20;
    slots[6] = ""; data->denom_mix[6] = 10;
    slots[8] = "A0I1"; data->denom_mix[8] = 100;
    slots[10] = "AOIL"; data->denom_mix[10] = 50;
    slots[11] = "INVALID"; data->denom_mix[11] = INT_MIN;
    slots[12] = "ABC02"; data->denom_mix[12] = 0;
    data->denom_mix[13] = 200;
    slots[15] = "ab c"; data->denom_mix[15] = 5;
    slots[16] = "\xe0"; data->denom_mix[16] = 20;
    slots[17] = "\xc0"; data->denom_mix[17] = 20;
    slots[19] = " NO "; data->denom_mix[19] = 10;
}

static void test_empty_and_invalid_inputs(void)
{
    counting_sim_t data = {0};
    counting_serial_query_t query = {0};
    char *one[] = {"A"};
    uint16_t sentinel = SLOT_GUARD;
    expect_slots(NULL, NULL, NULL, 0);
    expect_slots(NULL, &query, NULL, 0);
    expect_slots(&data, &query, NULL, 0);
    data.sn_capacity = 1;
    expect_slots(&data, NULL, NULL, 0); /* NULL slot array. */
    data.sn_str = one;
    data.denom_mix[0] = 1;
    const int invalid[] = {-1, INT_MIN, COUNTING_DATA_MAX_ITEMS + 1, INT_MAX};
    for (size_t i = 0; i < ARRAY_COUNT(invalid); ++i) {
        data.sn_capacity = invalid[i];
        expect_slots(&data, &query, NULL, 0);
    }
    data.sn_capacity = 1;
    counting_serial_query_result_t result = counting_serial_query_build(&data, NULL, NULL, SIZE_MAX);
    assert(result.valid_count == 1 && result.matched_count == 1 && result.written_count == 0);
    result = counting_serial_query_build(&data, NULL, &sentinel, 0);
    assert(result.matched_count == 1 && result.written_count == 0 && sentinel == SLOT_GUARD);
}

static void test_sparse_empty_and_duplicate_serials(void)
{
    counting_sim_t data;
    char *slots[20], *pointers_before[20];
    counting_serial_query_t query = {0};
    const uint16_t all[] = {1, 3, 4, 6, 8, 10, 15, 16, 17, 19};
    sparse_fixture(&data, slots);
    memcpy(pointers_before, slots, sizeof(slots));
    expect_slots(&data, NULL, all, ARRAY_COUNT(all));
    expect_slots(&data, &query, all, ARRAY_COUNT(all));
    query.match = COUNTING_SERIAL_MATCH_EXACT;
    query.exclude_text = query.exclude_denominations = true;
    expect_slots(&data, &query, all, ARRAY_COUNT(all));
    assert(memcmp(pointers_before, slots, sizeof(slots)) == 0);
    assert(strcmp(slots[1], "AbC01") == 0 && strcmp(slots[3], "xxabc01yy") == 0);
}

static void test_text_modes_and_ascii_identity(void)
{
    counting_sim_t data;
    char *slots[20];
    counting_serial_query_t query = {0};
    const uint16_t contains[] = {1, 3, 4};
    const uint16_t exact[] = {1, 4};
    const uint16_t excluded[] = {3, 6, 8, 10, 15, 16, 17, 19};
    sparse_fixture(&data, slots);
    strcpy(query.text, "aBc01");
    expect_slots(&data, &query, contains, ARRAY_COUNT(contains));
    query.match = COUNTING_SERIAL_MATCH_EXACT;
    expect_slots(&data, &query, exact, ARRAY_COUNT(exact));
    query.exclude_text = true;
    expect_slots(&data, &query, excluded, ARRAY_COUNT(excluded));
    query.exclude_text = false;
    strcpy(query.text, "a0i1");
    expect_slots(&data, &query, (uint16_t[]){8}, 1);
    strcpy(query.text, "AOIL");
    expect_slots(&data, &query, (uint16_t[]){10}, 1);
    strcpy(query.text, "A011");
    expect_slots(&data, &query, NULL, 0);
    strcpy(query.text, "\xe0");
    expect_slots(&data, &query, (uint16_t[]){16}, 1);
    strcpy(query.text, "\xc0");
    expect_slots(&data, &query, (uint16_t[]){17}, 1);
    strcpy(query.text, "NO");
    expect_slots(&data, &query, NULL, 0); /* No implicit whitespace trimming. */
    strcpy(query.text, " no ");
    expect_slots(&data, &query, (uint16_t[]){19}, 1);
    strcpy(query.text, "abc01");
    query.match = (counting_serial_match_t)99;
    expect_slots(&data, &query, contains, ARRAY_COUNT(contains));
}

static void test_exclusion_combination_and_descending(void)
{
    counting_sim_t data;
    char *slots[20];
    counting_serial_query_t query = {0};
    const uint16_t both[] = {1, 4}, both_descending[] = {4, 1};
    const uint16_t no_text[] = {8, 16, 17}, no_denom[] = {3};
    const uint16_t neither[] = {6, 10, 15, 19};
    sparse_fixture(&data, slots);
    strcpy(query.text, "abc01");
    query.denominations[0] = 100;
    query.denominations[1] = 20;
    query.denominations[2] = 100; /* A repeated selection does not duplicate rows. */
    query.denomination_count = 3;
    expect_slots(&data, &query, both, ARRAY_COUNT(both));
    query.descending = true;
    expect_slots(&data, &query, both_descending, ARRAY_COUNT(both_descending));
    query.descending = false;
    query.exclude_text = true;
    expect_slots(&data, &query, no_text, ARRAY_COUNT(no_text));
    query.exclude_text = false;
    query.exclude_denominations = true;
    expect_slots(&data, &query, no_denom, ARRAY_COUNT(no_denom));
    query.exclude_text = true;
    expect_slots(&data, &query, neither, ARRAY_COUNT(neither));
    query.text[0] = '\0';
    query.exclude_denominations = false;
    expect_slots(&data, &query, (uint16_t[]){1, 4, 8, 16, 17}, 5);
    memset(query.denominations, 0, sizeof(query.denominations));
    query.denominations[COUNTING_DENOM_MAX_ITEMS - 1] = 50;
    query.denomination_count = UINT8_MAX;
    expect_slots(&data, &query, (uint16_t[]){3, 10}, 2);
}

static void test_prefix_suffix_and_input_normalization(void)
{
    counting_sim_t data;
    char *slots[20];
    counting_serial_query_t query = {.match = COUNTING_SERIAL_MATCH_PREFIX};
    sparse_fixture(&data, slots);
    strcpy(query.text, "ab");
    expect_slots(&data, &query, (uint16_t[]){1, 4, 15}, 3);
    query.match = COUNTING_SERIAL_MATCH_SUFFIX;
    expect_slots(&data, &query, NULL, 0);
    strcpy(query.text, "01");
    expect_slots(&data, &query, (uint16_t[]){1, 4}, 2);
    query.denominations[0] = 100;
    query.denomination_count = 1;
    expect_slots(&data, &query, (uint16_t[]){1}, 1);
    query.exclude_text = true;
    expect_slots(&data, &query, (uint16_t[]){8}, 1);
    query.text[0] = '\0';
    expect_slots(&data, &query, (uint16_t[]){1, 8}, 2);

    char text[32] = " \t00a B\r\n ";
    assert(counting_serial_text_trim(text, sizeof(text), text, strlen(text)));
    assert(!strcmp(text, "00a B"));
    char target[8] = "KEEP";
    assert(!counting_serial_text_trim(target, 3, "abcd", 4));
    assert(!strcmp(target, "KEEP"));
    assert(!counting_serial_text_trim(target, sizeof(target), NULL, 1));
    assert(counting_serial_text_trim(target, sizeof(target), NULL, 0) && !target[0]);
    assert(counting_serial_text_trim(text, sizeof(text), "\xa0" "00A" "\xa0", 5));
    assert(!strcmp(text, "\xa0" "00A" "\xa0")); /* No locale-dependent trimming. */
    assert(counting_serial_text_matches("abc", NULL, 0, COUNTING_SERIAL_TEXT_EXACT));
    assert(!counting_serial_text_matches(NULL, "", 0, COUNTING_SERIAL_TEXT_CONTAINS));
    assert(!counting_serial_text_matches("abc", NULL, 1, COUNTING_SERIAL_TEXT_PREFIX));
    const char unterminated[] = {'b', 'c'};
    assert(counting_serial_text_matches("ABC", unterminated, sizeof(unterminated), COUNTING_SERIAL_TEXT_SUFFIX));
    assert(!counting_serial_text_matches("B", unterminated, sizeof(unterminated), COUNTING_SERIAL_TEXT_PREFIX));
}

static void test_capacity_prefix_and_slot_replacement(void)
{
    counting_sim_t data;
    char *slots[20];
    counting_serial_query_t query = {.descending = true};
    struct { uint16_t before, values[2], after; } guarded =
        {SLOT_GUARD, {SLOT_GUARD, SLOT_GUARD}, SLOT_GUARD};
    sparse_fixture(&data, slots);
    counting_serial_query_result_t result = counting_serial_query_build(&data, &query, guarded.values, 1);
    assert(result.valid_count == 10 && result.matched_count == 10 && result.written_count == 1);
    assert(guarded.values[0] == 19 && guarded.values[1] == SLOT_GUARD);
    assert(guarded.before == SLOT_GUARD && guarded.after == SLOT_GUARD);
    strcpy(query.text, "ABC01");
    query.match = COUNTING_SERIAL_MATCH_EXACT;
    slots[4] = "CHANGED";
    expect_slots(&data, &query, (uint16_t[]){1}, 1);
    slots[4] = "ABC01";
    slots[1] = NULL;
    expect_slots(&data, &query, (uint16_t[]){4}, 1);
}

static void test_full_width_text_and_short_serial(void)
{
    counting_sim_t data = {0};
    counting_serial_query_t query = {0};
    char full[33], longer[34];
    char *slots[] = {"", "a", full, longer};
    memset(query.text, 'a', sizeof(query.text));
    memset(full, 'A', sizeof(full) - 1); full[32] = '\0';
    memset(longer, 'A', sizeof(longer) - 1); longer[33] = '\0';
    data.sn_str = slots; data.sn_capacity = 4;
    for (int i = 0; i < 4; ++i) data.denom_mix[i] = 1;
    expect_slots(&data, &query, (uint16_t[]){2, 3}, 2);
    query.match = COUNTING_SERIAL_MATCH_EXACT;
    expect_slots(&data, &query, (uint16_t[]){2}, 1);
    query.match = COUNTING_SERIAL_MATCH_PREFIX;
    expect_slots(&data, &query, (uint16_t[]){2, 3}, 2);
    query.match = COUNTING_SERIAL_MATCH_SUFFIX;
    expect_slots(&data, &query, (uint16_t[]){2, 3}, 2);
    query.match = COUNTING_SERIAL_MATCH_EXACT;
    query.text[31] = '\0';
    expect_slots(&data, &query, NULL, 0);
    full[31] = '\0';
    expect_slots(&data, &query, (uint16_t[]){2}, 1);
}

static void test_denomination_union_sort_and_capacities(void)
{
    counting_sim_t data, before;
    char *slots[20];
    int output[12];
    const int expected[] = {INT_MAX, 200, 100, 50, 20, 10, 5, 1};
    sparse_fixture(&data, slots);
    data.denom_number = 6;
    data.denom[0].value = 100;
    data.denom[1].value = 200;
    data.denom[2].value = INT_MAX;
    data.denom[3].value = -1;
    data.denom[4].value = 1;
    data.denom[5].value = 200;
    data.denom[6].value = 5000; /* Outside directory count. */
    memcpy(&before, &data, sizeof(before));
    for (size_t capacity = 0; capacity <= ARRAY_COUNT(output); ++capacity) {
        for (size_t i = 0; i < ARRAY_COUNT(output); ++i) output[i] = -12345;
        size_t total = counting_serial_query_denominations(&data, output, capacity);
        size_t written = capacity < ARRAY_COUNT(expected) ? capacity : ARRAY_COUNT(expected);
        assert(total == (capacity < ARRAY_COUNT(expected) ? capacity+1 : ARRAY_COUNT(expected)));
        assert(memcmp(output, expected, written * sizeof(*expected)) == 0);
        for (size_t i = written; i < ARRAY_COUNT(output); ++i) assert(output[i] == -12345);
    }
    assert(counting_serial_query_denominations(&data, NULL, SIZE_MAX) == ARRAY_COUNT(expected));
    assert(memcmp(&before, &data, sizeof(before)) == 0);
    assert(counting_serial_query_denominations(NULL, output, ARRAY_COUNT(output)) == 0);
    data.sn_str = NULL;
    assert(counting_serial_query_denominations(&data, output, ARRAY_COUNT(output)) == 4);
    assert(memcmp(output, (int[]){INT_MAX, 200, 100, 1}, 4 * sizeof(*output)) == 0);
    data.denom_number = UINT8_MAX;
    data.denom[COUNTING_DENOM_MAX_ITEMS - 1].value = 2;
    assert(counting_serial_query_denominations(&data, output, ARRAY_COUNT(output)) == 6);
    memset(&data, 0, sizeof(data));
    assert(counting_serial_query_denominations(&data, output, ARRAY_COUNT(output)) == 0);
}

static void test_ten_thousand_slots_and_denomination_truncation(void)
{
    static counting_sim_t data, before;
    static char *slots[COUNTING_DATA_MAX_ITEMS];
    static uint16_t output[COUNTING_DATA_MAX_ITEMS + 1];
    counting_serial_query_t query = {0};
    int denoms[COUNTING_DENOM_MAX_ITEMS + 1];
    memset(&data, 0, sizeof(data));
    data.sn_str = slots; data.sn_capacity = COUNTING_DATA_MAX_ITEMS;
    for (int i = 0; i < COUNTING_DATA_MAX_ITEMS; ++i) {
        slots[i] = "FULL";
        data.denom_mix[i] = (i % 15 + 1) * 5;
    }
    memcpy(&before, &data, sizeof(before));
    output[COUNTING_DATA_MAX_ITEMS] = SLOT_GUARD;
    counting_serial_query_result_t result = counting_serial_query_build(&data, &query, output, SIZE_MAX);
    assert(result.valid_count == 10000 && result.matched_count == 10000 && result.written_count == 10000);
    for (int i = 0; i < COUNTING_DATA_MAX_ITEMS; ++i) assert(output[i] == i);
    query.descending = true;
    result = counting_serial_query_build(&data, &query, output, 9999);
    assert(result.matched_count == 10000 && result.written_count == 9999);
    for (int i = 0; i < 9999; ++i) assert(output[i] == 9999 - i);
    assert(output[COUNTING_DATA_MAX_ITEMS] == SLOT_GUARD);
    assert(counting_serial_query_denominations(&data, denoms, ARRAY_COUNT(denoms)) == 15);
    for (int i = 0; i < 15; ++i) assert(denoms[i] == (15 - i) * 5);
    assert(memcmp(&before, &data, sizeof(before)) == 0);
    for (int i = 0; i < COUNTING_DATA_MAX_ITEMS; ++i) data.denom_mix[i] = i + 1;
    denoms[15] = -12345;
    assert(counting_serial_query_denominations(&data, denoms, 15) == 16);
    for (int i = 0; i < 15; ++i) assert(denoms[i] == COUNTING_DATA_MAX_ITEMS - i);
    assert(denoms[15] == -12345);
    data.sn_capacity = COUNTING_DATA_MAX_ITEMS + 1;
    expect_slots(&data, NULL, NULL, 0);
    assert(counting_serial_query_denominations(&data, denoms, 15) == 0);
}

int main(void)
{
    test_empty_and_invalid_inputs();
    test_sparse_empty_and_duplicate_serials();
    test_text_modes_and_ascii_identity();
    test_exclusion_combination_and_descending();
    test_prefix_suffix_and_input_normalization();
    test_capacity_prefix_and_slot_replacement();
    test_full_width_text_and_short_serial();
    test_denomination_union_sort_and_capacities();
    test_ten_thousand_slots_and_denomination_truncation();
    puts("serial query: 9 regression groups passed, including prefix/suffix, bounded text and 10000 original slots");
    return 0;
}
