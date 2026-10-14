#ifndef UN260_BUZZER_SERVICE_H
#define UN260_BUZZER_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
/* Serial lifetime owns the worker. No LVGL or model access on that worker. */
bool buzzer_service_start(void);
void buzzer_service_stop(void);
void buzzer_service_click(void);
/* Drop unsent feedback or close an active pulse; retain OFF recovery. */
void buzzer_service_cancel(void);
void buzzer_service_on_reply(uint8_t status);
#endif
