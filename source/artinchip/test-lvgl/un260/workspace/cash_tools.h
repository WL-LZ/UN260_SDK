#ifndef UN260_CASH_TOOLS_H
#define UN260_CASH_TOOLS_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define CASH_SCALE INT64_C(1000000)
#define CASH_LIMIT INT64_C(999999999999999)
#define CASH_ENTRIES 1024
#define CASH_NEW_LIMIT 512
#define CASH_DENOMS 15
typedef struct { int64_t value; uint32_t quantity; } cash_denom_t;
typedef struct {
    char code[4]; uint32_t record; uint64_t recorded_at;
    int64_t amount; uint8_t count; cash_denom_t denoms[CASH_DENOMS];
    uint32_t group, day, operator_id;
    char operator_name[25];
    int64_t original_amount;
} cash_snapshot_t;
typedef enum { CASH_OPENING=1, CASH_RECEIPT, CASH_PAYMENT, CASH_REVERSAL, CASH_EXCHANGE, CASH_AUDIT } cash_kind_t;
typedef struct {
    uint32_t id, reverse_of, corrects, operator_id;
    uint64_t time;
    uint8_t kind;
    char operator_name[25], note[65];
    cash_snapshot_t cash;
    cash_snapshot_t peer; /* Exchange received cash, or audit expected balance. */
    int64_t rate;
    uint8_t approved;
} cash_entry_t;
typedef struct { uint32_t version, count; cash_entry_t entries[CASH_ENTRIES]; } cash_journal_t;
typedef enum {
    CASH_OK, CASH_INVALID, CASH_PRECISION, CASH_OVERFLOW, CASH_DIV_ZERO,
    CASH_NEEDS_OPENING, CASH_ALREADY_OPEN, CASH_DUPLICATE, CASH_BALANCE,
    CASH_FULL, CASH_REVERSED
} cash_error_t;
bool cash_parse(const char *, int64_t *);
void cash_format(int64_t, unsigned decimals, char *, size_t);
cash_error_t cash_calculate(int64_t, char op, int64_t, int64_t *);
int cash_currency_decimals(const char *);
bool cash_amount_precision(const char code[4],int64_t amount);
cash_error_t cash_snapshot_total(cash_snapshot_t *);
bool cash_journal_opened(const cash_journal_t *,const char *);
int64_t cash_journal_balance(const cash_journal_t *,const char *);
bool cash_journal_reversed(const cash_journal_t *,uint32_t);
enum { CASH_ACTION_CORRECT=1, CASH_ACTION_VOID=2 };
unsigned cash_journal_entry_actions(const cash_journal_t *,uint32_t);
bool cash_entry_is_audit(const cash_journal_t *,const cash_entry_t *);
bool cash_journal_source_used(const cash_journal_t *,const cash_snapshot_t *);
cash_error_t cash_journal_append(cash_journal_t *,const cash_entry_t *);
/* Atomic in RAM: failure leaves the complete original journal unchanged. */
cash_error_t cash_journal_correct(cash_journal_t *,uint32_t,const cash_entry_t *,uint64_t);
cash_error_t cash_convert(int64_t amount,int64_t rate,const char *target,int64_t *out);
int64_t cash_entry_delta(const cash_journal_t *,const cash_entry_t *,const char *code);
bool cash_journal_valid(const cash_journal_t *);
#endif
