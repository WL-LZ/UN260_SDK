#include "history_export_sn_parser.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    history_export_sn_entry_t *items;
    int count;
    int capacity;
} history_export_sn_builder_t;

static bool history_export_next_line(const char **cursor,
                                     char *line,
                                     size_t line_size)
{
    const char *start;
    size_t source_len = 0;
    size_t copy_len;

    if (cursor == NULL || *cursor == NULL || line == NULL || line_size == 0U) {
        return false;
    }

    while (**cursor == '\n' || **cursor == '\r') {
        (*cursor)++;
    }
    if (**cursor == '\0') {
        return false;
    }

    start = *cursor;
    while (start[source_len] != '\0' &&
           start[source_len] != '\n' && start[source_len] != '\r') {
        source_len++;
    }
    copy_len = source_len < line_size - 1U ? source_len : line_size - 1U;
    memcpy(line, start, copy_len);
    line[copy_len] = '\0';

    *cursor = start + source_len;
    while (**cursor == '\n' || **cursor == '\r') {
        (*cursor)++;
    }
    return true;
}

static bool history_export_parse_unsigned(const char *text, unsigned *value)
{
    char *end;
    unsigned long parsed;

    if (text == NULL || value == NULL) {
        return false;
    }

    while (isspace((unsigned char)*text)) {
        text++;
    }
    if (!isdigit((unsigned char)*text)) {
        return false;
    }

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (end == text || errno == ERANGE || parsed > UINT_MAX) {
        return false;
    }
    while (isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return false;
    }

    *value = (unsigned)parsed;
    return true;
}

static bool history_export_sn_append(history_export_sn_builder_t *builder,
                                     unsigned no,
                                     unsigned denom,
                                     const char *sn)
{
    history_export_sn_entry_t *next;
    int new_capacity;

    if (builder == NULL || sn == NULL || *sn == '\0') {
        return true;
    }
    if (builder->count >= HISTORY_EXPORT_SN_MAX_ENTRIES) {
        return true;
    }

    if (builder->count == builder->capacity) {
        new_capacity = builder->capacity > 0 ? builder->capacity * 2 : 16;
        if (new_capacity > HISTORY_EXPORT_SN_MAX_ENTRIES) {
            new_capacity = HISTORY_EXPORT_SN_MAX_ENTRIES;
        }
        next = realloc(builder->items,
                       sizeof(*next) * (size_t)new_capacity);
        if (next == NULL) {
            return false;
        }
        builder->items = next;
        builder->capacity = new_capacity;
    }

    builder->items[builder->count].no = no;
    builder->items[builder->count].denom = denom;
    snprintf(builder->items[builder->count].sn,
             sizeof(builder->items[builder->count].sn), "%s", sn);
    builder->count++;
    return true;
}

static bool history_export_parse_detail(const char *text,
                                        history_export_sn_builder_t *builder)
{
    const char *cursor = text;
    char line[256];

    while (history_export_next_line(&cursor, line, sizeof(line))) {
        char *field_two;
        char *field_three;
        char *tab;
        const char *sn;
        unsigned no;
        unsigned denom;

        tab = strchr(line, '\t');
        if (tab == NULL) {
            continue;
        }
        *tab++ = '\0';
        field_two = tab;
        tab = strchr(field_two, '\t');
        if (tab == NULL) {
            continue;
        }
        *tab++ = '\0';
        field_three = tab;

        if (!history_export_parse_unsigned(line, &no)) {
            no = (unsigned)builder->count + 1U;
        }
        if (history_export_parse_unsigned(field_two, &denom)) {
            sn = field_three; /* Current format: no, denomination, serial. */
        } else {
            sn = field_two;  /* Legacy format: no, serial, denomination. */
            if (!history_export_parse_unsigned(field_three, &denom)) {
                denom = 0;
            }
        }

        if (!history_export_sn_append(builder, no, denom, sn)) {
            return false;
        }
    }
    return true;
}

static int history_export_hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    return -1;
}

static bool history_export_hex_to_bytes(const char *text,
                                        uint8_t *buf,
                                        int buf_size,
                                        int *out_len)
{
    int hi = -1;
    int len = 0;

    if (text == NULL || buf == NULL || buf_size <= 0 || out_len == NULL) {
        return false;
    }

    for (const char *p = text; *p != '\0'; p++) {
        int value = history_export_hex_value(*p);

        if (value < 0) {
            continue;
        }
        if (hi < 0) {
            hi = value;
        } else {
            if (len >= buf_size) {
                return false;
            }
            buf[len++] = (uint8_t)((hi << 4) | value);
            hi = -1;
        }
    }

    *out_len = len;
    return len > 0 && hi < 0;
}

static bool history_export_parse_session(const char *text,
                                         history_export_sn_builder_t *builder)
{
    const char *cursor = text;
    char line[1024];

    while (history_export_next_line(&cursor, line, sizeof(line))) {
        uint8_t raw[256];
        char ascii[256];
        char *payload;
        char *end;
        const char *space;
        int raw_len;
        int ascii_len;
        unsigned denom;
        unsigned long parsed_denom;

        if (strncmp(line, "0x0D", 4) != 0 ||
            (line[4] != '\0' && !isspace((unsigned char)line[4]))) {
            continue;
        }
        space = strchr(line, ' ');
        if (space == NULL ||
            !history_export_hex_to_bytes(space + 1, raw,
                                         (int)sizeof(raw), &raw_len) ||
            raw_len < 8 || raw[0] != 0xFD || raw[1] != 0xDF ||
            raw[2] != (uint8_t)raw_len || raw[4] == 0x00 || raw[4] == 0xFF) {
            continue;
        }

        ascii_len = raw_len - 6;
        if (ascii_len <= 0 || ascii_len >= (int)sizeof(ascii)) {
            continue;
        }
        memcpy(ascii, &raw[5], (size_t)ascii_len);
        ascii[ascii_len] = '\0';
        while (ascii_len > 0 && isspace((unsigned char)ascii[ascii_len - 1])) {
            ascii[--ascii_len] = '\0';
        }

        payload = ascii;
        while (isspace((unsigned char)*payload)) {
            payload++;
        }
        if (!isdigit((unsigned char)*payload)) {
            continue;
        }
        errno = 0;
        parsed_denom = strtoul(payload, &end, 10);
        if (end == payload || errno == ERANGE || parsed_denom > UINT_MAX) {
            continue;
        }
        denom = (unsigned)parsed_denom;
        while (isspace((unsigned char)*end)) {
            end++;
        }
        if (*end == '\0') {
            continue;
        }

        if (!history_export_sn_append(builder,
                                      (unsigned)builder->count + 1U,
                                      denom, end)) {
            return false;
        }
    }
    return true;
}

static bool history_export_parse_legacy_text(
    const char *text,
    history_export_sn_builder_t *builder)
{
    const char *cursor = text;
    char line[HISTORY_EXPORT_SN_TEXT_SIZE];

    while (history_export_next_line(&cursor, line, sizeof(line))) {
        if (!history_export_sn_append(builder,
                                      (unsigned)builder->count + 1U,
                                      0, line)) {
            return false;
        }
    }
    return true;
}

bool history_export_sn_parse(const char *detail_text,
                             const char *session_log,
                             const char *legacy_sn_text,
                             history_export_sn_entry_t **entries,
                             int *count)
{
    history_export_sn_builder_t builder = {0};

    if (entries == NULL || count == NULL) {
        return false;
    }
    *entries = NULL;
    *count = 0;

    if (detail_text != NULL && *detail_text != '\0') {
        if (!history_export_parse_detail(detail_text, &builder)) {
            goto error;
        }
    }
    if (builder.count == 0 && session_log != NULL && *session_log != '\0') {
        if (!history_export_parse_session(session_log, &builder)) {
            goto error;
        }
    }
    if (builder.count == 0 && legacy_sn_text != NULL && *legacy_sn_text != '\0') {
        if (!history_export_parse_legacy_text(legacy_sn_text, &builder)) {
            goto error;
        }
    }

    *entries = builder.items;
    *count = builder.count;
    return true;

error:
    free(builder.items);
    return false;
}
