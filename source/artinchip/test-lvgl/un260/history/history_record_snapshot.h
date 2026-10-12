#ifndef HISTORY_RECORD_SNAPSHOT_H
#define HISTORY_RECORD_SNAPSHOT_H
#include "un260/counting/counting_data_types.h"
#include "history_record_detail.h"

/* Serialized little-endian bytes; never a dump of host pointers or structs.
 * The producer owns bytes until release. Storage jobs copy them before return. */
#define HISTORY_SNAPSHOT_MAX_BYTES (32U + COUNTING_DENOM_MAX_ITEMS*8U + COUNTING_DATA_MAX_ITEMS*80U)
typedef struct { unsigned char *bytes; size_t size; uint64_t hash; uint8_t reject_codes[32]; uint64_t amount; bool amount_complete; denom_t denoms[COUNTING_DENOM_MAX_ITEMS]; unsigned denom_count; } history_record_snapshot_t;
bool history_record_snapshot_capture(const counting_sim_t *sim,uint32_t pcs,
    bool reject_end_seen,history_record_snapshot_t *out);
bool history_record_snapshot_decode(const void *bytes,size_t size,uint64_t hash,
    uint32_t pcs,history_record_detail_t *out);
uint64_t history_record_snapshot_hash(const void *bytes,size_t size);
void history_record_snapshot_release(history_record_snapshot_t *snapshot);
#endif
