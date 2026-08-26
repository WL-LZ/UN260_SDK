#ifndef COUNTING_DATA_STORE_H
#define COUNTING_DATA_STORE_H

#include <stdbool.h>

#include "counting_data_types.h"

/* Read-only view of the single application counting-result store. */
const counting_sim_t *counting_data_current(void);
void counting_data_clear_serials(counting_sim_t *sim_data);
bool counting_data_ensure_serial_capacity(counting_sim_t *sim_data,
                                          int required_count);
int counting_data_serial_scan_limit(const counting_sim_t *sim_data);
int counting_data_serial_valid_count(const counting_sim_t *sim_data);
int counting_data_serial_nth_valid_index(const counting_sim_t *sim_data,
                                         int nth);
void counting_data_clear_errors(counting_sim_t *sim_data);
bool counting_data_ensure_error_capacity(counting_sim_t *sim_data,
                                         int required_count);
int counting_data_error_detail_count(const counting_sim_t *sim_data);
int counting_data_reject_pcs_count(const counting_sim_t *sim_data);

#endif
