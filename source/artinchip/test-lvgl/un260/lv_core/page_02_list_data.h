#ifndef PAGE_02_LIST_DATA_H
#define PAGE_02_LIST_DATA_H
#include "un260/counting/counting_data_types.h"
/* Page-owned index projection, never a second result store. Rebuild on data
 * notifications, not for every row during a finger movement. */
typedef struct {
    uint16_t serial[COUNTING_DATA_MAX_ITEMS];
    uint8_t denom[COUNTING_DENOM_MAX_ITEMS];
    uint16_t serial_count;
    uint8_t denom_count;
} page_02_list_data_t;
void page_02_list_data_denoms(page_02_list_data_t *view, const counting_sim_t *data);
void page_02_list_data_serials(page_02_list_data_t *view, const counting_sim_t *data);
#endif
