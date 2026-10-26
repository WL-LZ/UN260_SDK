#ifndef UN260_UI_INTERACTION_TRACE_H
#define UN260_UI_INTERACTION_TRACE_H

#include "un260/lv_drivers/uart_io.h"

/* Enable only while diagnosing touch/animation/navigation timing.
 * Controller TX/RX and protocol diagnostics use their original log paths. */
#ifndef UN260_UI_INTERACTION_TRACE
#define UN260_UI_INTERACTION_TRACE 0
#endif

#define ui_interaction_trace(...) \
    do { if (UN260_UI_INTERACTION_TRACE) uart_debug_printf(__VA_ARGS__); } while (0)

#endif
