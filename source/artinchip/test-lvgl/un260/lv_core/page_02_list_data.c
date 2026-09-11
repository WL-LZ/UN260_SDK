#include "page_02_list_data.h"
#include "un260/counting/counting_data_store.h"
#include <stddef.h>
void page_02_list_data_denoms(page_02_list_data_t *view, const counting_sim_t *data)
{
    view->denom_count = 0;
    if (!counting_data_monetary_result_supported(data)) return;
    for (unsigned i = 0; i < data->denom_number && i < COUNTING_DENOM_MAX_ITEMS; ++i)
        if (data->denom[i].value > 0) view->denom[view->denom_count++] = i;
}
void page_02_list_data_serials(page_02_list_data_t *view, const counting_sim_t *data)
{
    view->serial_count = 0;
    int limit = counting_data_serial_scan_limit(data);
    if (limit > COUNTING_DATA_MAX_ITEMS) limit = COUNTING_DATA_MAX_ITEMS;
    for (int i = 0; i < limit; ++i)
        if (data->sn_str[i] != NULL && data->denom_mix[i] > 0)
            view->serial[view->serial_count++] = i;
}
