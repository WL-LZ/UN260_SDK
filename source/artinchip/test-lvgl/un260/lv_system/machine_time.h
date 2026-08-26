#ifndef MACHINE_TIME_H
#define MACHINE_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} machine_time_value_t;

void machine_time_confirm(const machine_time_value_t* value);
void machine_time_get(machine_time_value_t* value);
bool machine_time_is_valid(const machine_time_value_t* value);
void machine_time_normalize(machine_time_value_t* value);
void machine_time_tick(void);
void machine_time_init(void);
void machine_time_pause(bool pause);
void machine_time_format(char* out, uint32_t out_len);

#endif
