#ifndef UN260_COUNTING_COUNTING_REJECT_REASON_H
#define UN260_COUNTING_COUNTING_REJECT_REASON_H

#include <stdint.h>

/* Exact protocol name; do not pass it through a translation lookup. */
const char *counting_reject_reason_get(uint8_t code);
typedef struct { const char *title, *meaning, *causes, *action; } counting_reject_guide_t;
/* Title is language-invariant; guidance fields are translation keys.
 * Causes are possibilities, never a diagnosis. */
const counting_reject_guide_t *counting_reject_guide_get(uint8_t code);

#endif
