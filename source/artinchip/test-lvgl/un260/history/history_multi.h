#ifndef HISTORY_MULTI_H
#define HISTORY_MULTI_H
#include <stdbool.h>
#include <stdint.h>
#define HISTORY_MULTI_CURRENCIES 32
#define HISTORY_MULTI_DENOMS 15
typedef struct { uint32_t value, pcs; } history_multi_denom_t;
typedef struct {
    char code[4];
    uint32_t pcs, amount;
    uint8_t count;
    bool complete;
    history_multi_denom_t denoms[HISTORY_MULTI_DENOMS];
} history_multi_currency_t;
typedef struct {
    bool enabled, overflow, add;
    uint8_t count, rejects;
    uint32_t passes;
    history_multi_currency_t currencies[HISTORY_MULTI_CURRENCIES];
} history_multi_t;
#endif
