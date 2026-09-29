#include "machine_fault.h"
#include <string.h>

/* 32 sensor bits + five boot checks + latest start/runtime reports. */
#define MACHINE_FAULT_CAPACITY 40U
static machine_fault_record_t records[MACHINE_FAULT_CAPACITY];
static size_t count;

bool machine_fault_key_equal(machine_fault_key_t a, machine_fault_key_t b)
{
    return a.source == b.source && a.type == b.type && a.code == b.code;
}

bool machine_fault_find(machine_fault_key_t key, machine_fault_record_t *out)
{
    for (size_t i = 0; i < count; ++i) {
        if (!machine_fault_key_equal(records[i].key, key)) continue;
        if (out) *out = records[i];
        return true;
    }
    return false;
}

void machine_fault_clear_source(machine_fault_source_t source)
{
    size_t keep = 0;
    for (size_t i = 0; i < count; ++i)
        if (records[i].key.source != source) records[keep++] = records[i];
    count = keep;
}

void machine_fault_clear_code(machine_fault_source_t source, uint8_t code)
{
    size_t keep = 0;
    for (size_t i = 0; i < count; ++i)
        if (records[i].key.source != source || records[i].key.code != code)
            records[keep++] = records[i];
    count = keep;
}

bool machine_fault_report(machine_fault_key_t key)
{
    if (machine_fault_find(key, NULL)) return false;
    if (key.source == MACHINE_FAULT_START || key.source == MACHINE_FAULT_RUNTIME)
        machine_fault_clear_source(key.source);
    if (count == MACHINE_FAULT_CAPACITY) return false;
    records[count++] = (machine_fault_record_t){ key, false };
    return true;
}

void machine_fault_acknowledge(machine_fault_key_t key)
{
    for (size_t i = 0; i < count; ++i)
        if (machine_fault_key_equal(records[i].key, key)) records[i].acknowledged = true;
}

bool machine_fault_at(size_t index, machine_fault_record_t *out)
{
    if (index >= count) return false;
    if (out) *out = records[index];
    return true;
}

bool machine_fault_first_unread(machine_fault_record_t *out)
{
    for (size_t i = 0; i < count; ++i) {
        if (records[i].acknowledged) continue;
        if (out) *out = records[i];
        return true;
    }
    return false;
}

size_t machine_fault_count(void) { return count; }
void machine_fault_clear(void) { count = 0; memset(records, 0, sizeof(records)); }

bool machine_fault_sensor_snapshot(uint32_t mask)
{
    size_t keep = 0;
    bool changed = false;
    for (size_t i = 0; i < count; ++i) {
        machine_fault_record_t r = records[i];
        if (r.key.source == MACHINE_FAULT_SENSOR &&
            (r.key.code >= 32U || !(mask & (UINT32_C(1) << r.key.code)))) {
            changed = true;
            continue;
        }
        records[keep++] = r;
    }
    count = keep;
    for (uint8_t bit = 0; bit < 32; ++bit) {
        if (!(mask & (UINT32_C(1) << bit))) continue;
        machine_fault_key_t key = { MACHINE_FAULT_SENSOR, 0, bit };
        if (machine_fault_report(key)) changed = true;
    }
    return changed;
}
