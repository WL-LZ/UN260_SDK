#ifndef COUNTING_DATA_TYPES_H
#define COUNTING_DATA_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define COUNTING_DATA_MAX_ITEMS 10000
#define COUNTING_DENOM_MAX_ITEMS 15

typedef struct {
    int value;
    uint16_t pcs;
    float amount;
} denom_t;

typedef struct {
    denom_t denom[COUNTING_DENOM_MAX_ITEMS];
    uint8_t denom_number;
    int total_pcs;
    float total_amount;
    char **sn_str;
    int sn_capacity;
    uint8_t *err_pcs;
    uint8_t *err_code;
    int err_capacity;
    bool is_paused;
    uint16_t err_num;      /* Number of parsed reject-detail entries. */
    uint16_t err_expected; /* Reject count reported by the count result. */
    int denom_mix[COUNTING_DATA_MAX_ITEMS];
    int last_total_pcs;
    float last_total_amount;
    int last_valid_pcs;
    int last_issue_pcs;
    int last_suspect_pcs;
    int last_damaged_pcs;
} counting_sim_t;

#endif
