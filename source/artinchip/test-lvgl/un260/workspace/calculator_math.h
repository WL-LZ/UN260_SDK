#ifndef UN260_CALCULATOR_TOOLS_H
#define UN260_CALCULATOR_TOOLS_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define CALCULATOR_SCALE INT64_C(1000000)
#define CALCULATOR_LIMIT INT64_C(999999999999999)
#define CALCULATOR_DENOMS 15
typedef struct { int64_t value; uint32_t quantity; } calculator_denom_t;
typedef struct {
    char code[4]; uint32_t record; uint64_t recorded_at;
    int64_t amount; uint8_t count; calculator_denom_t denoms[CALCULATOR_DENOMS];
    uint32_t operator_id;
    char operator_name[25];
} calculator_snapshot_t;
typedef enum {
    CALCULATOR_OK, CALCULATOR_INVALID, CALCULATOR_PRECISION, CALCULATOR_OVERFLOW, CALCULATOR_DIV_ZERO
} calculator_error_t;
bool calculator_parse(const char *, int64_t *);
void calculator_format(int64_t, unsigned decimals, char *, size_t);
calculator_error_t calculator_calculate(int64_t, char op, int64_t, int64_t *);
int calculator_currency_decimals(const char *);
bool calculator_amount_precision(const char code[4],int64_t amount);
calculator_error_t calculator_snapshot_total(calculator_snapshot_t *);
calculator_error_t calculator_convert(int64_t amount,int64_t rate,const char *target,int64_t *out);
#endif
