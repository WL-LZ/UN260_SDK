#ifndef COUNTING_SERIAL_TEXT_H
#define COUNTING_SERIAL_TEXT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    COUNTING_SERIAL_TEXT_CONTAINS = 0,
    COUNTING_SERIAL_TEXT_EXACT,
    COUNTING_SERIAL_TEXT_PREFIX,
    COUNTING_SERIAL_TEXT_SUFFIX
} counting_serial_text_match_t;

/* Match a NUL-terminated serial against exactly text_length query bytes.
 * Query need not be NUL-terminated. ASCII letters fold case; other bytes match
 * literally. Empty text matches any non-NULL serial. Invalid mode uses
 * CONTAINS. No trimming, allocation or I/O. NULL text is valid only at length 0. */
bool counting_serial_text_matches(const char *serial, const char *text,
    size_t text_length, counting_serial_text_match_t mode);

/* Copy a byte span, stripping outer ASCII whitespace only and adding NUL.
 * Interior bytes/leading zeroes remain unchanged. Overlap is allowed. Failure
 * (NULL input with nonzero length, or insufficient capacity) leaves output
 * unchanged. Intended for normalization at input boundaries, not per note. */
bool counting_serial_text_trim(char *out, size_t capacity,
                                const char *text, size_t length);

#endif
