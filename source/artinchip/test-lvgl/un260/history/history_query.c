#include "history_query.h"

#include <limits.h>
#include <string.h>

static bool copy_trim(char *out, size_t capacity, const char *source, size_t source_capacity)
{
    size_t length = 0;
    while (length < source_capacity && source[length]) ++length;
    if (length == source_capacity) return false;
    return counting_serial_text_trim(out, capacity, source, length);
}

static unsigned month_days(unsigned year, unsigned month)
{
    static const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (!year || year > 9999 || !month || month > 12) return 0;
    return days[month - 1] + (month == 2 && year % 4 == 0 &&
        (year % 100 != 0 || year % 400 == 0));
}

static bool decimal(const char **text, uint64_t maximum, uint64_t *out)
{
    uint64_t value = 0;
    if (**text < '0' || **text > '9') return false;
    while (**text >= '0' && **text <= '9') {
        unsigned digit = (unsigned)(*(*text)++ - '0');
        if (digit > maximum || value > (maximum - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    *out = value;
    return true;
}

static bool fixed_digits(const char *text, size_t length, unsigned *out)
{
    unsigned value = 0;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] < '0' || text[i] > '9') return false;
        value = value * 10U + (unsigned)(text[i] - '0');
    }
    *out = value;
    return true;
}

static bool parse_date(const char *text, uint32_t *first, uint32_t *last)
{
    size_t length = strlen(text);
    unsigned year = 0, month = 0, day = 0;
    bool separated = length == 7 || length == 10;
    if (length != 4 && length != 6 && length != 8 && !separated) return false;
    if (!fixed_digits(text, 4, &year) || !year || year > 9999) return false;
    if (length > 4) {
        if (separated && text[4] != '-') return false;
        if (!fixed_digits(text + (separated ? 5 : 4), 2, &month) ||
            !month || month > 12) return false;
    }
    if (length == 8 || length == 10) {
        if (separated && text[7] != '-') return false;
        if (!fixed_digits(text + (separated ? 8 : 6), 2, &day) ||
            !day || day > month_days(year, month)) return false;
    }
    *first = year * 10000U + (month ? month : 1U) * 100U + (day ? day : 1U);
    *last = year * 10000U + (month ? month : 12U) * 100U +
        (day ? day : month_days(year, month ? month : 12U));
    return true;
}

static bool parse_time(const char *text, uint32_t *first, uint32_t *last)
{
    size_t length = strlen(text);
    unsigned hour = 0, minute = 0, second = 0;
    bool separated = length == 5 || length == 8;
    if (length != 2 && length != 4 && length != 6 && !separated) return false;
    if (!fixed_digits(text, 2, &hour) || hour > 23) return false;
    if (length > 2) {
        if (separated && text[2] != ':') return false;
        if (!fixed_digits(text + (separated ? 3 : 2), 2, &minute) || minute > 59)
            return false;
    }
    if (length == 6 || length == 8) {
        if (separated && text[5] != ':') return false;
        if (!fixed_digits(text + (separated ? 6 : 4), 2, &second) || second > 59)
            return false;
    }
    *first = hour * 3600U + minute * 60U + second;
    *last = *first + (length == 2 ? 3599U : (length == 4 || length == 5 ? 59U : 0U));
    return true;
}

static bool parse_number(const char *text, history_query_number_range_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!*text) return true;
    out->enabled = true;
    out->max = UINT32_MAX;
    bool lower = text[0] == '>' && text[1] == '=';
    bool upper = text[0] == '<' && text[1] == '=';
    if (lower || upper) text += 2;
    uint64_t first;
    if (!decimal(&text, UINT32_MAX, &first)) return false;
    if (lower || upper) {
        if (*text) return false;
        if (lower) out->min = first;
        else out->max = first;
    } else {
        out->min = out->max = first;
        if (*text == '.' && text[1] == '.') {
            text += 2;
            if (!decimal(&text, UINT32_MAX, &out->max)) return false;
        }
        if (*text || out->min > out->max) return false;
    }
    return true;
}

static unsigned char fold(unsigned char ch)
{
    return ch >= 'a' && ch <= 'z' ? (unsigned char)(ch - 'a' + 'A') : ch;
}

static bool currency_code(const char *source, char *out)
{
    if (!source[0] || !source[1] || !source[2] || source[3]) return false;
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char ch = fold((unsigned char)source[i]);
        if (ch < 'A' || ch > 'Z') return false;
        out[i] = (char)ch;
    }
    out[3] = '\0';
    return true;
}

history_query_error_t history_query_compile(const history_query_input_t *input,
                                            history_query_t *out)
{
    history_query_t query = {0};
    char value[64];
    uint32_t unused;
    if (!input || !out) return HISTORY_QUERY_BAD_DATE_FROM;
    query.date_max = UINT32_MAX;
    query.time_max = 86399;
    if (!copy_trim(value, sizeof(value), input->date_from, sizeof(input->date_from)))
        return HISTORY_QUERY_BAD_DATE_FROM;
    if (*value) {
        if (!parse_date(value, &query.date_min, &unused)) return HISTORY_QUERY_BAD_DATE_FROM;
        query.date_enabled = true;
    }
    if (!copy_trim(value, sizeof(value), input->date_to, sizeof(input->date_to)))
        return HISTORY_QUERY_BAD_DATE_TO;
    if (*value) {
        if (!parse_date(value, &unused, &query.date_max)) return HISTORY_QUERY_BAD_DATE_TO;
        query.date_enabled = true;
    }
    if (query.date_min > query.date_max) return HISTORY_QUERY_REVERSED_DATES;
    if (!copy_trim(value, sizeof(value), input->time, sizeof(input->time)))
        return HISTORY_QUERY_BAD_TIME;
    if (*value) {
        if (!parse_time(value, &query.time_min, &query.time_max)) return HISTORY_QUERY_BAD_TIME;
        query.time_enabled = true;
    }
    if (!copy_trim(value, sizeof(value), input->currency, sizeof(input->currency)))
        return HISTORY_QUERY_BAD_CURRENCY;
    if (*value && !currency_code(value, query.currency)) return HISTORY_QUERY_BAD_CURRENCY;
    if (input->denomination_count > HISTORY_DETAIL_MAX_DENOMS) return HISTORY_QUERY_BAD_DENOM;
    query.denomination_count = input->denomination_count;
    for (size_t i = 0; i < query.denomination_count; ++i) {
        if (!input->denominations[i]) return HISTORY_QUERY_BAD_DENOM;
        query.denominations[i] = input->denominations[i];
    }
    if (!copy_trim(value, sizeof(value), input->pcs, sizeof(input->pcs)) ||
        !parse_number(value, &query.pcs)) return HISTORY_QUERY_BAD_PCS;
    if (!copy_trim(value, sizeof(value), input->amount, sizeof(input->amount)) ||
        !parse_number(value, &query.amount)) return HISTORY_QUERY_BAD_AMOUNT;
    if ((query.denomination_count || query.amount.enabled) && !query.currency[0])
        return HISTORY_QUERY_CURRENCY_REQUIRED;
    if (!copy_trim(query.serial, sizeof(query.serial), input->serial, sizeof(input->serial)) ||
        input->serial_match < HISTORY_TEXT_CONTAINS || input->serial_match > HISTORY_TEXT_SUFFIX)
        return HISTORY_QUERY_BAD_SERIAL;
    for (size_t i = 0; query.serial[i]; ++i)
        if ((unsigned char)query.serial[i] < 32 || (unsigned char)query.serial[i] > 126)
            return HISTORY_QUERY_BAD_SERIAL;
    query.serial_match = input->serial_match;
    if (input->rejects < HISTORY_REJECT_ALL || input->rejects > HISTORY_REJECT_CODE ||
        (input->rejects == HISTORY_REJECT_CODE && (!input->reject_code || input->reject_code == 0xFF)))
        return HISTORY_QUERY_BAD_REJECT;
    query.rejects = input->rejects;
    query.reject_code = input->reject_code;
    query.oldest_first = input->oldest_first;
    *out = query;
    return HISTORY_QUERY_OK;
}

const char *history_query_error_text(history_query_error_t error)
{
    static const char *const messages[] = {
        "", "Invalid start date: YYYY, YYYY-MM or YYYY-MM-DD",
        "Invalid end date: YYYY, YYYY-MM or YYYY-MM-DD",
        "Start date must not be after end date",
        "Invalid time: HH, HH:MM or HH:MM:SS",
        "Select a three-letter currency", "Select a currency for amount or denomination",
        "Invalid PCS: N, N..M, >=N or <=N",
        "Invalid amount: whole units, N, N..M, >=N or <=N",
        "Invalid serial text or matching mode", "Invalid denomination selection",
        "Invalid saved reject code"
    };
    return (unsigned)error < sizeof(messages) / sizeof(messages[0]) ? messages[error] : "Invalid search";
}

static bool denomination_matches(uint32_t value, const history_query_t *query)
{
    for (size_t i = 0; i < query->denomination_count; ++i)
        if (query->denominations[i] == value) return true;
    return false;
}

static history_query_match_t note_condition(const history_query_t *query,
                                            const history_record_detail_t *detail)
{
    if (!query->serial[0] && !query->denomination_count) return HISTORY_QUERY_MATCH;
    if (!detail) return HISTORY_QUERY_UNKNOWN;
    if (query->serial[0]) {
        bool unknown_denom = false;
        for (size_t i = 0; i < detail->serial_count; ++i) {
            const history_export_sn_entry_t *row = &detail->serials[i];
            if (!counting_serial_text_matches(row->sn, query->serial, strlen(query->serial),
                (counting_serial_text_match_t)query->serial_match)) continue;
            if (!query->denomination_count || denomination_matches(row->denom, query))
                return HISTORY_QUERY_MATCH;
            if (!row->denom) unknown_denom = true;
        }
        return detail->serials_complete && !unknown_denom ? HISTORY_QUERY_NO_MATCH : HISTORY_QUERY_UNKNOWN;
    }
    for (size_t i = 0; i < detail->denom_count; ++i)
        if (detail->denoms[i].pcs && denomination_matches(detail->denoms[i].value, query))
            return HISTORY_QUERY_MATCH;
    for (size_t i = 0; i < detail->serial_count; ++i)
        if (denomination_matches(detail->serials[i].denom, query)) return HISTORY_QUERY_MATCH;
    if (detail->denoms_complete) return HISTORY_QUERY_NO_MATCH;
    bool complete = detail->serials_complete;
    for (size_t i = 0; i < detail->serial_count; ++i)
        if (!detail->serials[i].denom) complete = false;
    return complete ? HISTORY_QUERY_NO_MATCH : HISTORY_QUERY_UNKNOWN;
}

static history_query_match_t reject_condition(const history_query_t *query,
                                               const history_record_detail_t *detail)
{
    if (query->rejects == HISTORY_REJECT_ALL) return HISTORY_QUERY_MATCH;
    if (!detail) return HISTORY_QUERY_UNKNOWN;
    for (size_t i = 0; i < detail->reject_count; ++i)
        if (query->rejects == HISTORY_REJECT_SAVED_ANY || detail->rejects[i].code == query->reject_code)
            return HISTORY_QUERY_MATCH;
    return detail->rejects_complete ? HISTORY_QUERY_NO_MATCH : HISTORY_QUERY_UNKNOWN;
}

history_query_match_t history_query_match_record(const history_query_t *query,
                                                 const history_query_record_t *record)
{
    if (!record || !record->valid || !record->record_no) return HISTORY_QUERY_NO_MATCH;
    if (!query) return HISTORY_QUERY_MATCH;
    bool unknown = false;
    if (query->date_enabled) {
        unsigned days = month_days(record->year, record->month);
        if (!days || !record->day || record->day > days) unknown = true;
        else {
            uint32_t date = record->year * 10000U + record->month * 100U + record->day;
            if (date < query->date_min || date > query->date_max) return HISTORY_QUERY_NO_MATCH;
        }
    }
    if (query->time_enabled) {
        if (record->hour > 23 || record->minute > 59 || record->second > 59) unknown = true;
        else {
            uint32_t time = record->hour * 3600U + record->minute * 60U + record->second;
            if (time < query->time_min || time > query->time_max) return HISTORY_QUERY_NO_MATCH;
        }
    }
    if (query->currency[0]) {
        char code[4];
        if (!currency_code(record->currency, code)) unknown = true;
        else if (strcmp(code, query->currency)) return HISTORY_QUERY_NO_MATCH;
    }
    if (query->pcs.enabled && (record->pcs < query->pcs.min || record->pcs > query->pcs.max))
        return HISTORY_QUERY_NO_MATCH;
    if (query->amount.enabled && (record->amount < query->amount.min || record->amount > query->amount.max))
        return HISTORY_QUERY_NO_MATCH;
    history_query_match_t notes = note_condition(query, record->detail);
    if (notes == HISTORY_QUERY_NO_MATCH) return HISTORY_QUERY_NO_MATCH;
    history_query_match_t rejects = reject_condition(query, record->detail);
    if (rejects == HISTORY_QUERY_NO_MATCH) return HISTORY_QUERY_NO_MATCH;
    return unknown || notes == HISTORY_QUERY_UNKNOWN || rejects == HISTORY_QUERY_UNKNOWN
        ? HISTORY_QUERY_UNKNOWN : HISTORY_QUERY_MATCH;
}

history_query_result_t history_query_build(const history_query_record_t *records,
    size_t count, const history_query_t *query, uint32_t *out_record_nos, size_t capacity)
{
    history_query_result_t result = {0};
    if (!records) return result;
    for (size_t i = 0; i < count; ++i) {
        const history_query_record_t *record = &records[i];
        if (!record->valid || !record->record_no) continue;
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j)
            if (records[j].valid && records[j].record_no == record->record_no) duplicate = true;
        if (duplicate) continue;
        ++result.valid_count;
        history_query_match_t match = history_query_match_record(query, record);
        if (match == HISTORY_QUERY_UNKNOWN) { ++result.unknown_count; continue; }
        if (match != HISTORY_QUERY_MATCH) continue;
        char code[4];
        bool code_valid = currency_code(record->currency, code);
        if (!result.matched_count) {
            result.amount_comparable = code_valid;
            if (code_valid) memcpy(result.currency, code, sizeof(code));
        } else if (!code_valid || strcmp(code, result.currency)) result.amount_comparable = false;
        ++result.matched_count;
        result.matched_pcs += record->pcs;
        if (result.amount_comparable) result.matched_amount += record->amount;
        else { result.matched_amount = 0; result.currency[0] = '\0'; }
        if (!out_record_nos || !capacity) continue;
        size_t position = 0;
        bool ascending = query && query->oldest_first;
        while (position < result.written_count &&
            (ascending ? out_record_nos[position] < record->record_no
                       : out_record_nos[position] > record->record_no)) ++position;
        if (position >= capacity) continue;
        size_t used = result.written_count < capacity ? result.written_count + 1 : capacity;
        for (size_t j = used - 1; j > position; --j) out_record_nos[j] = out_record_nos[j - 1];
        out_record_nos[position] = record->record_no;
        result.written_count = used;
    }
    return result;
}
