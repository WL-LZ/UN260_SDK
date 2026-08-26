#include "counting_data_store.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static counting_sim_t g_counting_data;

const counting_sim_t *counting_data_current(void)
{
    return &g_counting_data;
}

counting_sim_t *counting_data_mutable(void)
{
    return &g_counting_data;
}

static bool counting_data_capacity_is_valid(int capacity)
{
    return capacity >= 0 && capacity <= COUNTING_DATA_MAX_ITEMS;
}

static int counting_data_next_capacity(int current, int required)
{
    int capacity = current > 0 ? current : 32;

    while (capacity < required) {
        if (capacity > COUNTING_DATA_MAX_ITEMS / 2) {
            return COUNTING_DATA_MAX_ITEMS;
        }
        capacity *= 2;
    }
    return capacity;
}

void counting_data_clear_serials(counting_sim_t *sim_data)
{
    int capacity;

    if (sim_data == NULL) {
        return;
    }

    capacity = counting_data_capacity_is_valid(sim_data->sn_capacity)
        ? sim_data->sn_capacity : 0;
    if (sim_data->sn_str != NULL) {
        for (int i = 0; i < capacity; i++) {
            free(sim_data->sn_str[i]);
        }
        free(sim_data->sn_str);
    }
    sim_data->sn_str = NULL;
    sim_data->sn_capacity = 0;
    memset(sim_data->denom_mix, 0, sizeof(sim_data->denom_mix));
}

bool counting_data_ensure_serial_capacity(counting_sim_t *sim_data,
                                          int required_count)
{
    char **new_serials;
    int old_capacity;
    int new_capacity;

    if (sim_data == NULL || required_count <= 0 ||
        required_count > COUNTING_DATA_MAX_ITEMS) {
        return false;
    }

    old_capacity = sim_data->sn_str != NULL ? sim_data->sn_capacity : 0;
    if (!counting_data_capacity_is_valid(old_capacity)) {
        return false;
    }
    if (required_count <= old_capacity) {
        return true;
    }

    new_capacity = counting_data_next_capacity(old_capacity, required_count);
    new_serials = calloc((size_t)new_capacity, sizeof(*new_serials));
    if (new_serials == NULL) {
        return false;
    }
    if (old_capacity > 0) {
        memcpy(new_serials, sim_data->sn_str,
               sizeof(*new_serials) * (size_t)old_capacity);
    }

    free(sim_data->sn_str);
    sim_data->sn_str = new_serials;
    sim_data->sn_capacity = new_capacity;
    return true;
}

int counting_data_serial_scan_limit(const counting_sim_t *sim_data)
{
    if (sim_data == NULL || sim_data->sn_str == NULL ||
        !counting_data_capacity_is_valid(sim_data->sn_capacity)) {
        return 0;
    }
    return sim_data->sn_capacity;
}

int counting_data_serial_valid_count(const counting_sim_t *sim_data)
{
    int valid_count = 0;
    int scan_limit = counting_data_serial_scan_limit(sim_data);
    int mix_capacity;

    if (sim_data == NULL) {
        return 0;
    }

    mix_capacity = (int)(sizeof(sim_data->denom_mix) /
                         sizeof(sim_data->denom_mix[0]));
    if (scan_limit > mix_capacity) {
        scan_limit = mix_capacity;
    }

    for (int i = 0; i < scan_limit; i++) {
        if (sim_data->sn_str[i] != NULL && sim_data->denom_mix[i] > 0) {
            valid_count++;
        }
    }
    return valid_count;
}

int counting_data_serial_nth_valid_index(const counting_sim_t *sim_data,
                                         int nth)
{
    int valid_count = 0;
    int scan_limit = counting_data_serial_scan_limit(sim_data);
    int mix_capacity;

    if (sim_data == NULL || nth < 0) {
        return -1;
    }

    mix_capacity = (int)(sizeof(sim_data->denom_mix) /
                         sizeof(sim_data->denom_mix[0]));
    if (scan_limit > mix_capacity) {
        scan_limit = mix_capacity;
    }

    for (int i = 0; i < scan_limit; i++) {
        if (sim_data->sn_str[i] == NULL || sim_data->denom_mix[i] <= 0) {
            continue;
        }
        if (valid_count == nth) {
            return i;
        }
        valid_count++;
    }
    return -1;
}

void counting_data_clear_errors(counting_sim_t *sim_data)
{
    if (sim_data == NULL) {
        return;
    }

    free(sim_data->err_pcs);
    free(sim_data->err_code);
    sim_data->err_pcs = NULL;
    sim_data->err_code = NULL;
    sim_data->err_capacity = 0;
    sim_data->err_num = 0;
}

bool counting_data_ensure_error_capacity(counting_sim_t *sim_data,
                                         int required_count)
{
    uint8_t *new_pcs;
    uint8_t *new_code;
    int old_capacity;
    int new_capacity;

    if (sim_data == NULL || required_count <= 0 ||
        required_count > COUNTING_DATA_MAX_ITEMS) {
        return false;
    }

    old_capacity = sim_data->err_capacity;
    if (!counting_data_capacity_is_valid(old_capacity) ||
        (old_capacity > 0 && (sim_data->err_pcs == NULL ||
                              sim_data->err_code == NULL))) {
        return false;
    }
    if (required_count <= old_capacity) {
        return true;
    }

    new_capacity = counting_data_next_capacity(old_capacity, required_count);
    new_pcs = calloc((size_t)new_capacity, sizeof(*new_pcs));
    new_code = calloc((size_t)new_capacity, sizeof(*new_code));
    if (new_pcs == NULL || new_code == NULL) {
        free(new_pcs);
        free(new_code);
        return false;
    }

    if (old_capacity > 0) {
        memcpy(new_pcs, sim_data->err_pcs,
               sizeof(*new_pcs) * (size_t)old_capacity);
        memcpy(new_code, sim_data->err_code,
               sizeof(*new_code) * (size_t)old_capacity);
    }

    free(sim_data->err_pcs);
    free(sim_data->err_code);
    sim_data->err_pcs = new_pcs;
    sim_data->err_code = new_code;
    sim_data->err_capacity = new_capacity;
    return true;
}

int counting_data_error_detail_count(const counting_sim_t *sim_data)
{
    int count;

    if (sim_data == NULL || sim_data->err_pcs == NULL ||
        sim_data->err_code == NULL ||
        !counting_data_capacity_is_valid(sim_data->err_capacity)) {
        return 0;
    }

    count = (int)sim_data->err_num;
    return count < sim_data->err_capacity ? count : sim_data->err_capacity;
}

int counting_data_reject_pcs_count(const counting_sim_t *sim_data)
{
    int detail_count;
    int total = 0;

    if (sim_data == NULL) {
        return 0;
    }
    if (sim_data->err_expected > 0) {
        return (int)sim_data->err_expected;
    }

    detail_count = counting_data_error_detail_count(sim_data);
    for (int i = 0; i < detail_count; i++) {
        int pcs = (int)sim_data->err_pcs[i];

        if (pcs > INT_MAX - total) {
            return INT_MAX;
        }
        total += pcs;
    }
    return total;
}
