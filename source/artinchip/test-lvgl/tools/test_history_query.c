#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/history/history_query.h"
#include "un260/protocol/protocol_frame.h"

static void append_frame(char *log, size_t capacity, uint8_t command,
                           const uint8_t *payload, size_t payload_length)
{
    uint8_t frame[255];
    char hex[768], line[800];
    int length = protocol_frame_build(frame, sizeof(frame), command, payload, (uint16_t)payload_length);
    assert(length > 0);
    protocol_frame_format_hex(frame, (size_t)length, hex, sizeof(hex));
    snprintf(line, sizeof(line), "0x%02X %s\n", command, hex);
    assert(strlen(log) + strlen(line) < capacity);
    strcat(log, line);
}

static void parser_cases(void)
{
    history_record_detail_t detail = {0};
    history_detail_input_t input = {
        .denom_text = "100 x 1\n20X1\n50 x 0",
        .sn_detail_text = "03\t100\tAb001\n08\t20\tXX999",
        .total_pcs = 2
    };
    assert(history_record_detail_build(&input, &detail));
    assert(detail.denom_count == 3 && detail.denomination_pcs == 2 && detail.denomination_amount == 120);
    assert(detail.denoms_complete && detail.serials_complete && !detail.rejects_complete);
    assert(detail.serial_count == 2 && detail.serials[0].no == 3 && detail.serials[1].denom == 20);
    assert(!strcmp(detail.serials[0].sn, "Ab001"));
    history_record_detail_release(&detail);
    history_record_detail_release(&detail);

    input.serials_truncated = true;
    assert(history_record_detail_build(&input, &detail));
    assert(detail.serial_count == 1 && !detail.serials_complete);
    history_record_detail_release(&detail);
    input.serials_truncated = false;
    input.sn_detail_text = "01\tLEGACY001\t100";
    input.total_pcs = 1;
    assert(history_record_detail_build(&input, &detail));
    assert(detail.serial_count == 1 && detail.serials[0].denom == 100);
    assert(!strcmp(detail.serials[0].sn, "LEGACY001"));
    history_record_detail_release(&detail);

    char log[4096] = "";
    uint8_t ignored[] = {0, 0};
    for (unsigned i = 0; i < 65; ++i) append_frame(log, sizeof(log), 0x0B, ignored, sizeof(ignored));
    uint8_t reject[] = {5, 7};
    append_frame(log, sizeof(log), 0x0C, reject, sizeof(reject));
    uint8_t serial[] = {1, '1', '0', '0', ' ', 'A', 'B', '0', '0', '1'};
    append_frame(log, sizeof(log), 0x0D, serial, sizeof(serial));
    input = (history_detail_input_t){.session_log=log, .total_pcs=1, .reject_log_complete=true};
    assert(history_record_detail_build(&input, &detail));
    assert(detail.reject_count == 1 && detail.rejects[0].code == 5 && detail.saved_reject_pcs == 7);
    assert(detail.serial_count == 1 && !strcmp(detail.serials[0].sn, "AB001"));
    assert(detail.rejects_complete && detail.serials_complete);
    history_record_detail_release(&detail);
    input.error_frame_text = "FD DF 07 0C 05 07 7F"; /* Receive trailer intentionally not 0A. */
    assert(history_record_detail_build(&input, &detail));
    assert(detail.reject_count == 1); /* Last-frame fallback must not double count. */
    history_record_detail_release(&detail);
    input.session_log = "0x0C FD DF 07 0C 03 04 00\n0x0C FD DF 07 0D 09 09 00\n0x0C FD DF 07 0C 01 02\n";
    input.error_frame_text = "";
    assert(history_record_detail_build(&input, &detail));
    assert(detail.reject_count == 1 && detail.malformed && !detail.rejects_complete);
    history_record_detail_release(&detail);
    input = (history_detail_input_t){.sn_detail_text="garbage", .sn_text="0000123\nAb0009", .total_pcs=2};
    assert(history_record_detail_build(&input, &detail));
    assert(detail.serial_count == 2 && !strcmp(detail.serials[0].sn, "0000123"));
    assert(detail.serials[0].denom == 0);
    history_record_detail_release(&detail);
    input = (history_detail_input_t){.denom_text="-1 x 2\n100 x 2garbage\n4294967296 x 1\n10 x 2", .total_pcs=2};
    assert(history_record_detail_build(&input, &detail));
    assert(detail.denom_count == 1 && detail.malformed && !detail.denoms_complete);
    history_record_detail_release(&detail);
    input = (history_detail_input_t){0};
    assert(history_record_detail_build(&input, &detail));
    assert(!detail.denoms_available && !detail.serials_available && !detail.rejects_available);
    assert(!detail.denoms_complete && !detail.serials_complete && !detail.rejects_complete);
    history_record_detail_release(&detail);
}

static history_query_t compile_ok(const history_query_input_t *input)
{
    history_query_t query;
    history_query_error_t error = history_query_compile(input, &query);
    if (error != HISTORY_QUERY_OK) fprintf(stderr, "compile failed: %s\n", history_query_error_text(error));
    assert(error == HISTORY_QUERY_OK);
    return query;
}

static void serial_source_truncation_cases(void)
{
    /* Reproduce a full legacy persisted field whose tail looks like a serial,
     * while a nonempty malformed preferred field forces the parser to fall back. */
    char legacy[640] = "";
    for (unsigned i = 0; i < 70; ++i) strcat(legacy, "AA000001\n");
    strcat(legacy, "PARTIAL99");
    assert(strlen(legacy) == sizeof(legacy) - 1U);
    history_detail_input_t input = {.sn_detail_text="garbage", .sn_text=legacy,
        .total_pcs=71, .legacy_serials_truncated=true};
    history_record_detail_t detail = {0};
    assert(history_record_detail_build(&input, &detail));
    assert(detail.serial_count == 70 && !detail.serials_complete);
    for (size_t i = 0; i < detail.serial_count; ++i)
        assert(strcmp(detail.serials[i].sn, "PARTIAL99"));
    history_query_record_t record = {.record_no=1, .valid=true, .pcs=71, .detail=&detail};
    history_query_input_t raw = {.serial="PARTIAL99", .serial_match=HISTORY_TEXT_EXACT};
    history_query_t query = compile_ok(&raw);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_UNKNOWN);
    history_query_result_t result = history_query_build(&record, 1, &query, NULL, 0);
    assert(result.matched_count == 0 && result.unknown_count == 1);
    history_record_detail_release(&detail);

    /* An unused truncated legacy backup cannot taint a complete preferred source. */
    input.sn_detail_text = "1\t100\tKNOWN001";
    input.total_pcs = 1;
    assert(history_record_detail_build(&input, &detail));
    assert(detail.serial_count == 1 && detail.serials_complete);
    assert(!strcmp(detail.serials[0].sn, "KNOWN001"));
    history_record_detail_release(&detail);

    char log[100] = "";
    const uint8_t payload[] = {1, '2', '0', ' ', 'L', 'O', 'G', '0', '0', '1'};
    append_frame(log, sizeof(log), 0x0D, payload, sizeof(payload));
    input.sn_detail_text = "garbage";
    input.serials_truncated = true;
    input.session_log = log;
    assert(history_record_detail_build(&input, &detail));
    assert(detail.serial_count == 1 && detail.serials_complete);
    assert(!strcmp(detail.serials[0].sn, "LOG001"));
    history_record_detail_release(&detail);
    input.log_truncated = true;
    assert(history_record_detail_build(&input, &detail));
    assert(detail.serial_count == 1 && !detail.serials_complete);
    history_record_detail_release(&detail);

    /* No session rows: a complete legacy source remains complete, even when
     * discarded preferred/log sources had truncation flags. */
    input.session_log = "0x0D FD DF";
    input.sn_text = "LEGACY001";
    input.legacy_serials_truncated = false;
    assert(history_record_detail_build(&input, &detail));
    assert(detail.serial_count == 1 && detail.serials_complete);
    assert(!strcmp(detail.serials[0].sn, "LEGACY001"));
    history_record_detail_release(&detail);
}

static void compiler_cases(void)
{
    history_query_input_t input = {0};
    history_query_t query = compile_ok(&input);
    assert(!query.date_enabled && !query.time_enabled && !query.pcs.enabled);
    strcpy(input.date_from, "2024-02");
    strcpy(input.date_to, "202402");
    strcpy(input.time, "23:59");
    query = compile_ok(&input);
    assert(query.date_min == 20240201 && query.date_max == 20240229);
    assert(query.time_min == 86340 && query.time_max == 86399);
    strcpy(input.date_from, "2023");
    strcpy(input.date_to, "2023-02");
    strcpy(input.time, "00");
    query = compile_ok(&input);
    assert(query.date_min == 20230101 && query.date_max == 20230228 && query.time_max == 3599);
    strcpy(input.date_from, "2000-02-29");
    strcpy(input.date_to, "2000");
    strcpy(input.time, "123456");
    query = compile_ok(&input);
    assert(query.time_min == 45296 && query.time_max == 45296);
    strcpy(input.date_from, "1900-02-29");
    assert(history_query_compile(&input, &query) == HISTORY_QUERY_BAD_DATE_FROM);
    strcpy(input.date_from, "2024-04-31");
    assert(history_query_compile(&input, &query) == HISTORY_QUERY_BAD_DATE_FROM);
    strcpy(input.date_from, "2024");
    assert(history_query_compile(&input, &query) == HISTORY_QUERY_REVERSED_DATES);
    memset(&input, 0, sizeof(input));
    const char *bad_times[] = {"24", "1260", "12:01:60", "1", "12:3", "-12"};
    for (size_t i = 0; i < sizeof(bad_times)/sizeof(*bad_times); ++i) {
        strcpy(input.time, bad_times[i]);
        assert(history_query_compile(&input, &query) == HISTORY_QUERY_BAD_TIME);
    }
    memset(&input, 0, sizeof(input));
    strcpy(input.pcs, " 100..200 ");
    query = compile_ok(&input);
    assert(query.pcs.min == 100 && query.pcs.max == 200);
    strcpy(input.pcs, ">=0");
    query = compile_ok(&input);
    assert(query.pcs.min == 0 && query.pcs.max == UINT32_MAX);
    strcpy(input.pcs, "<=4294967295");
    query = compile_ok(&input);
    const char *bad_numbers[] = {"-1", "1.5", "2..1", "1,000", "4294967296", ">1", "100abc", "100..", "1.. 2"};
    for (size_t i = 0; i < sizeof(bad_numbers)/sizeof(*bad_numbers); ++i) {
        strcpy(input.pcs, bad_numbers[i]);
        history_query_t previous = query;
        assert(history_query_compile(&input, &query) == HISTORY_QUERY_BAD_PCS);
        assert(!memcmp(&previous, &query, sizeof(query)));
    }
    memset(&input, 0, sizeof(input));
    strcpy(input.amount, "100");
    assert(history_query_compile(&input, &query) == HISTORY_QUERY_CURRENCY_REQUIRED);
    strcpy(input.currency, "usd");
    strcpy(input.serial, "  00Ab  ");
    query = compile_ok(&input);
    assert(!strcmp(query.currency, "USD") && !strcmp(query.serial, "00Ab"));
    memset(input.date_from, '9', sizeof(input.date_from));
    assert(history_query_compile(&input, &query) == HISTORY_QUERY_BAD_DATE_FROM);
    input.date_from[0] = 0;
    input.rejects = HISTORY_REJECT_CODE;
    assert(history_query_compile(&input, &query) == HISTORY_QUERY_BAD_REJECT);
    input.reject_code = 255;
    assert(history_query_compile(&input, &query) == HISTORY_QUERY_BAD_REJECT);
}

static void matching_cases(void)
{
    history_record_detail_t detail = {0};
    history_detail_input_t saved = {.denom_text="100 x 1\n20 x 1", .sn_detail_text="1\t100\tAB001\n2\t20\tXX999", .total_pcs=2};
    assert(history_record_detail_build(&saved, &detail));
    history_query_record_t record = {.record_no=15, .pcs=2, .amount=120, .currency="USD", .year=2024,
        .month=2, .day=29, .hour=12, .minute=34, .second=56, .valid=true, .detail=&detail};
    history_query_input_t input = {.currency="usd", .denominations={20}, .denomination_count=1, .serial="ab001"};
    history_query_t query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_NO_MATCH); /* Same note required. */
    input.denominations[0] = 100;
    query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_MATCH);
    strcpy(input.serial, "AB");
    input.serial_match = HISTORY_TEXT_PREFIX;
    query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_MATCH);
    input.serial_match = HISTORY_TEXT_EXACT;
    query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_NO_MATCH);
    strcpy(input.serial, "001");
    input.serial_match = HISTORY_TEXT_SUFFIX;
    query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_MATCH);
    strcpy(input.serial, "9999");
    query = compile_ok(&input);
    detail.serials_complete = false;
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_UNKNOWN);
    strcpy(input.currency, "EUR");
    query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_NO_MATCH); /* False overrides unknown. */
    memset(&input, 0, sizeof(input));
    input.rejects = HISTORY_REJECT_SAVED_ANY;
    query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_UNKNOWN);
    detail.rejects[0] = (history_detail_reject_t){.no=1, .pcs=3, .code=5};
    detail.reject_count = 1;
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_MATCH);
    input.rejects = HISTORY_REJECT_CODE;
    input.reject_code = 6;
    query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_UNKNOWN);
    detail.rejects_complete = true;
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_NO_MATCH);
    memset(&input, 0, sizeof(input));
    strcpy(input.date_from, "202402"); strcpy(input.date_to, "202402"); strcpy(input.time, "12:34");
    strcpy(input.pcs, "2..3");
    query = compile_ok(&input);
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_MATCH);
    record.day = 30;
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_UNKNOWN);
    record.day = 29;
    record.minute = 35;
    assert(history_query_match_record(&query, &record) == HISTORY_QUERY_NO_MATCH);
    history_record_detail_release(&detail);
}

static void result_cases(void)
{
    history_query_record_t records[] = {
        {.record_no=8,.pcs=2,.amount=100,.currency="USD",.valid=true},
        {.record_no=20,.pcs=3,.amount=200,.currency="usd",.valid=true},
        {.record_no=12,.pcs=4,.amount=300,.currency="USD",.valid=true},
        {.record_no=20,.pcs=999,.amount=999,.currency="USD",.valid=true},
        {.record_no=0,.valid=true}, {.record_no=99,.valid=false}
    };
    uint32_t ids[2] = {0};
    history_query_result_t result = history_query_build(records, 6, NULL, ids, 2);
    assert(result.valid_count == 3 && result.matched_count == 3 && result.written_count == 2);
    assert(ids[0] == 20 && ids[1] == 12 && result.matched_pcs == 9 && result.matched_amount == 600);
    assert(result.amount_comparable && !strcmp(result.currency, "USD"));
    history_query_input_t input = {.oldest_first=true};
    history_query_t query = compile_ok(&input);
    result = history_query_build(records, 6, &query, ids, 2);
    assert(ids[0] == 8 && ids[1] == 12);
    strcpy(records[2].currency, "EUR");
    result = history_query_build(records, 6, &query, NULL, 0);
    assert(result.matched_count == 3 && !result.written_count && !result.amount_comparable && !result.matched_amount);
    strcpy(input.serial, "A");
    query = compile_ok(&input);
    result = history_query_build(records, 6, &query, ids, 2);
    assert(!result.matched_count && result.unknown_count == 3);
    result = history_query_build(NULL, 100, NULL, ids, 2);
    assert(!result.valid_count && !result.matched_count);
}

int main(void)
{
    parser_cases(); serial_source_truncation_cases(); compiler_cases(); matching_cases(); result_cases();
    puts("PASS: history detail/query, full-log rejects, source-specific fallback/tail safety, date/numeric validation, same-note AND, unknowns, stable IDs and currency totals");
    return 0;
}
