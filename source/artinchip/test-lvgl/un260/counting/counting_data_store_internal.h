#ifndef COUNTING_DATA_STORE_INTERNAL_H
#define COUNTING_DATA_STORE_INTERNAL_H

#include "counting_data_store.h"

/* Only runtime/parser/reset owners may request a writable view. */
counting_sim_t *counting_data_mutable(void);

#endif
