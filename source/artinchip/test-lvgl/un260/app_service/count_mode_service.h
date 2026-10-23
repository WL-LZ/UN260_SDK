#ifndef COUNT_MODE_SERVICE_H
#define COUNT_MODE_SERVICE_H
#include "count_mode_policy.h"
#include "un260/currency/currency_service.h"

typedef enum { COUNT_CURRENCY_REJECTED, COUNT_CURRENCY_PENDING, COUNT_CURRENCY_UNCHANGED } count_currency_request_t;
bool count_mode_service_request(uint8_t mode);
/* True only while sending or awaiting the currency follow-on mode request. */
bool count_mode_service_currency_mode(void);
bool count_mode_service_confirmed(void);
bool count_mode_service_result_may_clear(void);
bool count_mode_service_busy(void);
count_currency_request_t count_mode_service_select_currency(uint8_t index, const char code[4]);
void count_mode_service_mode_result(bool success, bool timeout);
void count_mode_service_mode_sync(void);
void count_mode_service_currency_result(const currency_switch_result_t *result);
void count_mode_service_currency_sync(void);
void count_mode_service_poll(void);
/* Physical START revokes an unsent mode follow-on. */
void count_mode_service_count_started(void);
#endif
