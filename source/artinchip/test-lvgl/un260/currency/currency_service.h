#ifndef CURRENCY_SERVICE_H
#define CURRENCY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "un260/currency/currency_state.h"

typedef struct {
    bool success;
    bool timeout;
    bool controller_rejected;
    uint8_t target_index;
    char target_code[4];
} currency_switch_result_t;

bool currency_service_request_switch(uint8_t target_index, const char target_code[4]);
bool currency_service_take_switch_result(uint8_t status, currency_switch_result_t* result);
bool currency_service_take_switch_timeout(currency_switch_result_t* result);
bool currency_service_switch_pending(void);
/* Business owner authorizes one adjacent detail only. Cancellation and a new
 * request invalidate it; this does not change faults or presentation. */
bool currency_service_take_switch_rejection_detail(uint8_t command, uint8_t code);
void currency_service_cancel_switch(void);

#endif
