#ifndef UN260_COUNTING_COUNTING_REJECT_REASON_H
#define UN260_COUNTING_COUNTING_REJECT_REASON_H

#include <stdint.h>

const char *counting_reject_reason_get(uint8_t code);
typedef struct { const char *title, *meaning, *causes, *action; } counting_reject_guide_t;
/* Protocol-defined meaning; causes are possibilities, never a diagnosis. */
const counting_reject_guide_t *counting_reject_guide_get(uint8_t code);

#endif
