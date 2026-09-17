#ifndef HISTORY_QUERY_H
#define HISTORY_QUERY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "history_record_detail.h"
#include "history_multi.h"
#include "un260/counting/counting_serial_text.h"

typedef enum {
    HISTORY_TEXT_CONTAINS = COUNTING_SERIAL_TEXT_CONTAINS,
    HISTORY_TEXT_EXACT = COUNTING_SERIAL_TEXT_EXACT,
    HISTORY_TEXT_PREFIX = COUNTING_SERIAL_TEXT_PREFIX,
    HISTORY_TEXT_SUFFIX = COUNTING_SERIAL_TEXT_SUFFIX
} history_text_match_t;

typedef enum {
    HISTORY_REJECT_ALL = 0,
    HISTORY_REJECT_SAVED_ANY,
    HISTORY_REJECT_CODE
} history_reject_filter_t;

typedef enum {
    HISTORY_QUERY_OK = 0,
    HISTORY_QUERY_BAD_DATE_FROM,
    HISTORY_QUERY_BAD_DATE_TO,
    HISTORY_QUERY_REVERSED_DATES,
    HISTORY_QUERY_BAD_TIME,
    HISTORY_QUERY_BAD_CURRENCY,
    HISTORY_QUERY_CURRENCY_REQUIRED,
    HISTORY_QUERY_BAD_PCS,
    HISTORY_QUERY_BAD_AMOUNT,
    HISTORY_QUERY_BAD_SERIAL,
    HISTORY_QUERY_BAD_DENOM,
    HISTORY_QUERY_BAD_REJECT
} history_query_error_t;

typedef struct {
    char date_from[16]; /* YYYY, YYYY-MM, YYYY-MM-DD; compact forms also accepted. */
    char date_to[16];   /* A partial upper date includes its entire month/year. */
    char time[16];      /* HH, HH:MM, HH:MM:SS or compact HHMM / HHMMSS. */
    char currency[4];   /* Empty means all currencies. Otherwise exactly A-Z x 3. */
    uint32_t denominations[HISTORY_DETAIL_MAX_DENOMS];
    size_t denomination_count;
    char pcs[32];       /* Empty, N, N..M, >=N, <=N; unsigned whole numbers. */
    char amount[32];    /* Same syntax, stored whole currency units, not cents. */
    char serial[64];
    history_text_match_t serial_match;
    history_reject_filter_t rejects;
    uint8_t reject_code; /* Specific 0x01..0xFE; 0/FF are not reject codes. */
    bool oldest_first;  /* Default false = newest record_no first. */
    uint8_t mode;       /* 0 all, 1 MULTI, 2 other/legacy. */
} history_query_input_t;

typedef struct {
    bool enabled;
    uint64_t min;
    uint64_t max;
} history_query_number_range_t;

typedef struct {
    uint32_t date_min, date_max; /* Inclusive YYYYMMDD, 0 / UINT32_MAX unbounded. */
    uint32_t time_min, time_max; /* Inclusive seconds since local midnight. */
    bool date_enabled, time_enabled;
    char currency[4];
    uint32_t denominations[HISTORY_DETAIL_MAX_DENOMS];
    size_t denomination_count;
    history_query_number_range_t pcs, amount;
    char serial[64];
    history_text_match_t serial_match;
    history_reject_filter_t rejects;
    uint8_t reject_code;
    bool oldest_first;
    uint8_t mode;
} history_query_t;

typedef struct {
    uint32_t record_no;
    uint32_t pcs;
    uint32_t amount;
    char currency[4];
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    bool valid;
    /* Borrowed immutable detail, or NULL when not yet parsed/unavailable. */
    const history_record_detail_t *detail;
    const history_multi_t *multi;
} history_query_record_t;

typedef enum {
    HISTORY_QUERY_NO_MATCH = 0,
    HISTORY_QUERY_MATCH,
    HISTORY_QUERY_UNKNOWN
} history_query_match_t;

typedef struct {
    size_t valid_count;
    size_t matched_count;
    size_t unknown_count;
    size_t written_count;
    uint64_t matched_pcs;
    uint64_t matched_amount;
    /* Never sum unlike currencies into a misleading money total. */
    bool amount_comparable;
    char currency[4];
} history_query_result_t;

/* Compile is atomic: invalid/unterminated input leaves *out unchanged. Empty
 * criteria disable filters. Date/time use stored wall clock, no timezone I/O.
 * Serial trims outer ASCII whitespace, preserves leading zeroes and interior
 * characters, and folds ASCII letters only. No wildcard/regex syntax. */
history_query_error_t history_query_compile(const history_query_input_t *input,
                                            history_query_t *out);
const char *history_query_error_text(history_query_error_t error);
history_query_match_t history_query_match_record(const history_query_t *query,
                                                 const history_query_record_t *record);
/* Fields combine AND; denominations combine OR. Serial+denomination must
 * occur on the same note. Unknown detail is counted separately, never silently
 * treated as confirmed absence. Output contains stable IDs, one per input
 * record, sorted by ID. NULL output/capacity 0 counts only. No allocation/I/O. */
history_query_result_t history_query_build(const history_query_record_t *records,
    size_t count, const history_query_t *query, uint32_t *out_record_nos, size_t capacity);

#endif
