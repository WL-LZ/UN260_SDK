#ifndef UN260_SUPPORT_REPORT_H
#define UN260_SUPPORT_REPORT_H
#include <stdbool.h>
#include <stddef.h>
/* Fixed whitelist: no serial numbers, note images, raw frames, names or PINs. */
bool support_report_build(char *out,size_t capacity);
#endif
