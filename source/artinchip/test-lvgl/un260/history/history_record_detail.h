#ifndef HISTORY_RECORD_DETAIL_H
#define HISTORY_RECORD_DETAIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "history_export_sn_parser.h"

#define HISTORY_DETAIL_MAX_DENOMS 64
#define HISTORY_DETAIL_MAX_REJECTS 256

typedef struct {
    const char *denom_text;
    const char *sn_detail_text;
    const char *sn_text;
    const char *session_log;
    const char *error_frame_text;
    uint32_t total_pcs;
    bool denoms_truncated;
    /* Source-specific: these flags describe their named persisted fields,
     * regardless of whether that source eventually parses any serial rows. */
    bool serials_truncated;
    bool legacy_serials_truncated; /* sn_text; serials_truncated is sn_detail_text. */
    bool log_truncated;
    /* Only set when the producer proves all reject events were retained.
     * An old record with no saved reject rows does not prove no rejection. */
    bool reject_log_complete;
} history_detail_input_t;

typedef struct {
    uint32_t value;
    uint32_t pcs;
    uint64_t amount;
} history_detail_denom_t;

typedef struct {
    uint32_t no;
    uint32_t pcs;
    uint8_t code;
} history_detail_reject_t;

typedef struct {
    history_detail_denom_t denoms[HISTORY_DETAIL_MAX_DENOMS];
    size_t denom_count;
    history_export_sn_entry_t *serials;
    size_t serial_count;
    history_detail_reject_t rejects[HISTORY_DETAIL_MAX_REJECTS];
    size_t reject_count;
    uint64_t denomination_pcs;
    uint64_t denomination_amount;
    uint64_t saved_reject_pcs;
    bool denoms_available;
    bool serials_available;
    bool rejects_available;
    bool denoms_complete;
    bool serials_complete;
    bool rejects_complete;
    bool malformed;
} history_record_detail_t;

/* No LVGL, storage access or protocol side effects. Input strings must be NUL
 * terminated. Output must be zero-initialized or previously released. Build
 * owns at most one bounded serial allocation. On failure output is empty.
 * Release before rebuilding; release is safe on a zeroed/released object.
 * Complete is evidence-based; available only means some detail was retained.
 * Serial source precedence is parsed rows from detail, then session log, then
 * legacy text. Only the source actually used contributes truncation status. */
bool history_record_detail_build(const history_detail_input_t *input,
                                 history_record_detail_t *out);
void history_record_detail_release(history_record_detail_t *detail);

#endif
