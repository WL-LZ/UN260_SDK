#ifndef LV_FAULT_POPUP_H
#define LV_FAULT_POPUP_H
#include "un260/machine_state/machine_fault.h"
typedef machine_fault_source_t fault_source_t;
#define FAULT_SRC_BOOT MACHINE_FAULT_BOOT
#define FAULT_SRC_START_COUNT MACHINE_FAULT_START
#define FAULT_SRC_RUNTIME MACHINE_FAULT_RUNTIME
#define FAULT_SRC_SENSOR MACHINE_FAULT_SENSOR
/* UI-thread entry points. Confirm acknowledges only, never transmits. */
void hide_fault_popup(void);
bool fault_popup_is_showing(void);
void fault_popup_set_auto_enabled(bool enabled);
bool fault_popup_get_auto_enabled(void);
void fault_popup_report_start_fault(uint8_t type, uint8_t code);
void fault_popup_report_start_no_note(void);
void fault_popup_report_runtime_fault(uint8_t code);
void fault_popup_report_boot_result(uint8_t step, uint8_t result);
/* Collect every accepted self-test step before presenting the complete set. */
void fault_popup_record_boot_result(uint8_t step, uint8_t result);
void fault_popup_report_sensor_mask(uint32_t mask);
/* Unknown runtime codes stay inspectable without an automatic modal. */
void fault_popup_record_runtime_notice(uint8_t code);
bool fault_popup_show_pending_now(void);
bool fault_popup_get_pending_fault(fault_source_t *source, uint8_t *type, uint8_t *code);
/* Actual runtime recovery / accepted start only. Boot/sensor reports survive. */
void fault_popup_clear_runtime(void);
#endif
