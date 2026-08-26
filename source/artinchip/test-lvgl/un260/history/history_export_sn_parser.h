#ifndef HISTORY_EXPORT_SN_PARSER_H
#define HISTORY_EXPORT_SN_PARSER_H

#include <stdbool.h>

#define HISTORY_EXPORT_SN_TEXT_SIZE 64
#define HISTORY_EXPORT_SN_MAX_ENTRIES 256

typedef struct {
    unsigned no;
    char sn[HISTORY_EXPORT_SN_TEXT_SIZE];
    unsigned denom;
} history_export_sn_entry_t;

bool history_export_sn_parse(const char *detail_text,
                             const char *session_log,
                             const char *legacy_sn_text,
                             history_export_sn_entry_t **entries,
                             int *count);

#endif
