#include "counting_serial_query.h"
#include "counting_data_store.h"

static bool denomination_matches(int value, const counting_serial_query_t *query,
                                 unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
        if (query->denominations[i] == value) return true;
    return false;
}

counting_serial_query_result_t counting_serial_query_build(
    const counting_sim_t *data, const counting_serial_query_t *query,
    uint16_t *out_slots, size_t capacity)
{
    counting_serial_query_result_t result = {0};
    int limit = counting_data_serial_scan_limit(data);
    size_t text_length = 0;
    unsigned denomination_count = 0;

    if (query != NULL) {
        while (text_length < sizeof(query->text) && query->text[text_length] != '\0')
            ++text_length;
        denomination_count = query->denomination_count;
        if (denomination_count > COUNTING_DENOM_MAX_ITEMS)
            denomination_count = COUNTING_DENOM_MAX_ITEMS;
    }
    for (int position = 0; position < limit; ++position) {
        int slot = query != NULL && query->descending ? limit - position - 1 : position;
        if (data->sn_str[slot] == NULL || data->denom_mix[slot] <= 0) continue;
        ++result.valid_count;
        if (text_length > 0 &&
            counting_serial_text_matches(data->sn_str[slot], query->text, text_length,
                (counting_serial_text_match_t)query->match) == query->exclude_text)
            continue;
        if (denomination_count > 0 &&
            denomination_matches(data->denom_mix[slot], query, denomination_count) ==
                query->exclude_denominations) continue;
        ++result.matched_count;
        if (out_slots != NULL && result.written_count < capacity)
            out_slots[result.written_count++] = (uint16_t)slot;
    }
    return result;
}

size_t counting_serial_query_denominations(const counting_sim_t *data,
                                         int *out_values, size_t capacity)
{
    int limit = counting_data_serial_scan_limit(data);
    unsigned directory_count;
    size_t total = 0;
    int previous = 0;

    if (data == NULL) return 0;
    directory_count = data->denom_number;
    if (directory_count > COUNTING_DENOM_MAX_ITEMS)
        directory_count = COUNTING_DENOM_MAX_ITEMS;
    for (;;) {
        int next = 0;
        for (unsigned i = 0; i < directory_count; ++i) {
            int value = data->denom[i].value;
            if (value > next && (total == 0 || value < previous)) next = value;
        }
        for (int i = 0; i < limit; ++i) {
            int value = data->denom_mix[i];
            if (data->sn_str[i] != NULL && value > next &&
                (total == 0 || value < previous)) next = value;
        }
        if (next == 0) return total;
        if (out_values != NULL && total < capacity) out_values[total] = next;
        ++total;
        /* One extra distinct value proves truncation. Do not enumerate an
         * unbounded set of malformed denominations on the UI thread. */
        if (total > capacity) return total;
        previous = next;
    }
}
