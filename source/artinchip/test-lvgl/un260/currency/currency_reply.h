#ifndef CURRENCY_REPLY_H
#define CURRENCY_REPLY_H

#include <stdint.h>

#include "currency_service.h"

typedef enum {
    CURRENCY_REPLY_INVALID = 0,
    CURRENCY_REPLY_IGNORED,
    CURRENCY_REPLY_SWITCH_SUCCESS,
    CURRENCY_REPLY_SWITCH_FAILURE,
    CURRENCY_REPLY_BOOT_ACTIVE,
} currency_reply_kind_t;

typedef struct {
    currency_reply_kind_t kind;
    currency_switch_result_t switch_result;
    char active_code[4];
} currency_reply_result_t;

currency_reply_result_t currency_reply_handle(const uint8_t *buf, uint8_t len);

#endif
