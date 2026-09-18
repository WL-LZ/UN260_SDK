#ifndef COUNTING_MULTI_H
#define COUNTING_MULTI_H
#include <stdbool.h>
#include <stdint.h>
#define COUNTING_MULTI_MAX 32
#define COUNTING_MULTI_DENOMS 15
typedef enum { MULTI_DETAIL_NONE, MULTI_DETAIL_LOADING, MULTI_DETAIL_READY,
    MULTI_DETAIL_EMPTY, MULTI_DETAIL_INVALID, MULTI_DETAIL_TIMEOUT } multi_detail_status_t;
typedef struct { uint32_t value; uint16_t pcs; } multi_denom_t;
typedef struct {
    char code[4];
    uint32_t amount;
    uint16_t pcs;
    multi_detail_status_t status;
    uint8_t denom_count;
    multi_denom_t denom[COUNTING_MULTI_DENOMS];
} multi_currency_t;
typedef struct {
    multi_currency_t currencies[COUNTING_MULTI_MAX];
    uint8_t count, reject;
    uint32_t total_pcs, revision, generation;
    uint32_t group_generation, passes;
    bool add;
    bool counting, overflow;
} counting_multi_t;
const counting_multi_t *counting_multi_current(void);
/* Most recent accepted currency summary in this pass; NULL before its first frame. */
const multi_currency_t *counting_multi_latest(void);
void counting_multi_reset(void);
void counting_multi_begin(bool add);
/* Validated 16-byte per-currency / 13-byte global 0E; totals are never added twice. */
bool counting_multi_info(const uint8_t *frame, uint8_t len);
bool counting_multi_request(unsigned index, uint32_t now_ms);
void counting_multi_denom(const uint8_t *frame, uint8_t len, uint32_t now_ms);
void counting_multi_poll(uint32_t now_ms);
/* Runtime calls only while idle in MULTI, regardless of the visible page. */
void counting_multi_prefetch(uint32_t now_ms);
bool counting_multi_query_busy(void);
/* Quarantine an outstanding legacy 0B request when entering MULTI. */
void counting_multi_drain_legacy(void);
#endif
