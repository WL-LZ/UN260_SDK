#ifndef MACHINE_FAULT_H
#define MACHINE_FAULT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MACHINE_FAULT_BOOT = 0,
    MACHINE_FAULT_START,
    MACHINE_FAULT_RUNTIME,
    MACHINE_FAULT_SENSOR,
    MACHINE_FAULT_BATCH
} machine_fault_source_t;

typedef struct {
    machine_fault_source_t source;
    uint8_t type;
    uint8_t code;
} machine_fault_key_t;

typedef struct {
    machine_fault_key_t key;
    bool acknowledged;
} machine_fault_record_t;

/* UI-thread-owned projection of controller reports. Acknowledging never
 * changes controller state; only a later report removes active records. */
bool machine_fault_report(machine_fault_key_t key);
void machine_fault_acknowledge(machine_fault_key_t key);
bool machine_fault_find(machine_fault_key_t key, machine_fault_record_t *out);
bool machine_fault_at(size_t index, machine_fault_record_t *out);
bool machine_fault_first_unread(machine_fault_record_t *out);
size_t machine_fault_count(void);
void machine_fault_clear(void);
void machine_fault_clear_source(machine_fault_source_t source);
void machine_fault_clear_code(machine_fault_source_t source, uint8_t code);
bool machine_fault_sensor_snapshot(uint32_t mask);
bool machine_fault_key_equal(machine_fault_key_t a, machine_fault_key_t b);
#endif
