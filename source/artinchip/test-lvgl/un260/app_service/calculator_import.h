#ifndef UN260_CALCULATOR_IMPORT_H
#define UN260_CALCULATOR_IMPORT_H
#include "un260/workspace/calculator_math.h"
unsigned calculator_import_currencies(char (*codes)[4],unsigned capacity);
bool calculator_import_history_ready(void);
const char *calculator_import_current_blocker(void);
unsigned calculator_import_current_count(void);
bool calculator_import_current(unsigned index,calculator_snapshot_t *out);
#endif
