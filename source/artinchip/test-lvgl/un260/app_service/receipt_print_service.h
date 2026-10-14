#ifndef UN260_RECEIPT_PRINT_SERVICE_H
#define UN260_RECEIPT_PRINT_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { RECEIPT_PRINT_NONE, RECEIPT_PRINT_SENT, RECEIPT_PRINT_FAILED } receipt_print_state_t;
typedef struct {
    uint32_t revision,record_no;
    receipt_print_state_t state;
} receipt_print_status_t;
/* Record zero means the live result. A saved ID is allowed only while it is
 * still the controller's current result. No historical payload protocol exists. */
const char *receipt_print_blocker(uint32_t record_no);
bool receipt_print_request(uint32_t record_no);
const receipt_print_status_t *receipt_print_status(void);
#endif
