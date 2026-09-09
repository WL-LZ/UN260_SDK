#ifndef COUNTING_SERIAL_QUERY_H
#define COUNTING_SERIAL_QUERY_H

#include <stddef.h>
#include "counting_data_types.h"
#include "counting_serial_text.h"

typedef enum {
    COUNTING_SERIAL_MATCH_CONTAINS = COUNTING_SERIAL_TEXT_CONTAINS,
    COUNTING_SERIAL_MATCH_EXACT = COUNTING_SERIAL_TEXT_EXACT,
    COUNTING_SERIAL_MATCH_PREFIX = COUNTING_SERIAL_TEXT_PREFIX,
    COUNTING_SERIAL_MATCH_SUFFIX = COUNTING_SERIAL_TEXT_SUFFIX,
} counting_serial_match_t;

typedef struct {
    char text[32];
    counting_serial_match_t match;
    bool exclude_text;
    int denominations[COUNTING_DENOM_MAX_ITEMS];
    uint8_t denomination_count;
    bool exclude_denominations;
    bool descending;
} counting_serial_query_t;

typedef struct {
    uint32_t valid_count;
    uint32_t matched_count;
    uint32_t written_count;
} counting_serial_query_result_t;

/* A valid record has a non-NULL serial and positive denomination, including an
 * empty serial string. NULL query selects all. Empty conditions never filter,
 * even when excluded. Conditions combine with AND; denominations within a set
 * combine with OR. Text matching folds ASCII letters only, without trimming.
 * A non-terminated text uses all 32 bytes; invalid match values use CONTAINS.
 * Output contains original zero-based slots, not renumbered results or physical
 * note positions. NULL output counts only. Counts include matches beyond the
 * output capacity. Inputs and output must not overlap; borrowed input strings
 * must remain valid for this synchronous call. No allocation or I/O occurs. */
counting_serial_query_result_t counting_serial_query_build(
    const counting_sim_t *data, const counting_serial_query_t *query,
    uint16_t *out_slots, size_t capacity);

/* Positive denominations from valid serial records and the current denomination
 * directory, unique and descending. Returns the distinct count when it fits;
 * otherwise capacity+1 means truncated (not an exact total). Writes the
 * largest capacity values, or none for NULL output. Does not
 * infer currency identity or catalogue completeness. Uses constant workspace
 * and at most capacity+1 scans, even with malformed distinct denominations. */
size_t counting_serial_query_denominations(const counting_sim_t *data,
                                         int *out_values, size_t capacity);

#endif
