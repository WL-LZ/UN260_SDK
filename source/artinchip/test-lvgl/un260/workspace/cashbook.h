#ifndef UN260_CASHBOOK_H
#define UN260_CASHBOOK_H
#include <stdbool.h>
#include <stdint.h>
#define CASHBOOK_RUNS 2048
#define CASHBOOK_CURRENCIES 32
#define CASHBOOK_CLOSES 64
#define CASHBOOK_SAMPLES 32
typedef struct { char code[4]; uint32_t pcs; uint64_t amount; } cashbook_money_t;
typedef struct {
    uint32_t source, day, operator_id, pcs, recount_group;
    char operator_name[25];
    uint8_t hour, minute, second, currencies;
    bool complete, cumulative;
    uint16_t sample_count;
    uint64_t samples[CASHBOOK_SAMPLES];
    cashbook_money_t money[CASHBOOK_CURRENCIES];
} cashbook_result_t;
typedef struct {
    uint32_t id, group, candidate;
    cashbook_result_t result;
} cashbook_run_t;
typedef struct {
    uint32_t id, day, selected;
    bool confirmed, review, excluded;
} cashbook_group_t;
typedef struct {
    uint32_t id, day, revision, groups, operator_id;
    bool current;
    uint8_t currencies;
    cashbook_money_t money[CASHBOOK_CURRENCIES];
    uint32_t selected[CASHBOOK_RUNS];
} cashbook_close_t;
typedef struct {
    uint32_t version, sequence, run_count, group_count, close_count, business_day;
    cashbook_run_t runs[CASHBOOK_RUNS];
    cashbook_group_t groups[CASHBOOK_RUNS];
    cashbook_close_t closes[CASHBOOK_CLOSES];
} cashbook_t;
typedef enum { CASHBOOK_INGEST, CASHBOOK_CONFIRM, CASHBOOK_SELECT,
    CASHBOOK_MERGE, CASHBOOK_SPLIT, CASHBOOK_EXCLUDE, CASHBOOK_RESTORE,
    CASHBOOK_CLOSE, CASHBOOK_REOPEN, CASHBOOK_ASSIGN_DAY, CASHBOOK_SET_DAY,
    CASHBOOK_CONFIRM_SINGLES } cashbook_operation_t;
typedef struct {
    cashbook_operation_t operation;
    uint32_t group, run, day, operator_id;
    cashbook_result_t result;
} cashbook_command_t;
void cashbook_defaults(cashbook_t *);
bool cashbook_valid(const cashbook_t *);
/* Transaction caller supplies a private draft. Failure never publishes it. */
bool cashbook_apply(cashbook_t *, const cashbook_command_t *, char *reason, unsigned capacity);
const cashbook_run_t *cashbook_run(const cashbook_t *, uint32_t id);
const cashbook_group_t *cashbook_group(const cashbook_t *, uint32_t id);
unsigned cashbook_attempts(const cashbook_t *, uint32_t group);
bool cashbook_closed(const cashbook_t *, uint32_t day);
bool cashbook_totals(const cashbook_t *, uint32_t day, cashbook_money_t *out,
                     unsigned *count, unsigned *confirmed, unsigned *pending);
bool cashbook_day_valid(uint32_t day);
#endif
