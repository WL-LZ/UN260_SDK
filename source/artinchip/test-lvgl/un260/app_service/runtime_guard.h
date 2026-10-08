#ifndef UN260_RUNTIME_GUARD_H
#define UN260_RUNTIME_GUARD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { RUNTIME_FAULT_UART_TX = 1, RUNTIME_FAULT_UART_RX = 2, RUNTIME_FAULT_STALL = 3,
       RUNTIME_FAULT_COUNT_MEMORY = 4 };
#ifdef UN260_RUNTIME_GUARD_ENABLED
int runtime_guard_run(int (*application)(void));
int runtime_guard_reconcile(void);
bool runtime_guard_previous_fault(void);
void runtime_guard_heartbeat(void);
void runtime_guard_fault(unsigned reason);
bool runtime_guard_has_fault(void);
void runtime_guard_rx(const void *data, size_t length);
void runtime_guard_tx(const void *data, size_t length, int completed);
void runtime_guard_count(int pcs, float amount, unsigned phase, bool busy, bool pending, bool multi);
#else
/* Component-only builds have no process supervisor. Production enables the
 * hooks explicitly on its executable; no LVGL library ABI dependency. */
static inline bool runtime_guard_previous_fault(void) { return false; }
static inline void runtime_guard_heartbeat(void) {}
static inline void runtime_guard_fault(unsigned r) { (void)r; }
static inline bool runtime_guard_has_fault(void) { return false; }
static inline void runtime_guard_rx(const void *d,size_t n) { (void)d;(void)n; }
static inline void runtime_guard_tx(const void *d,size_t n,int c) { (void)d;(void)n;(void)c; }
static inline void runtime_guard_count(int p,float a,unsigned s,bool b,bool h,bool m)
{ (void)p;(void)a;(void)s;(void)b;(void)h;(void)m; }
#endif
#endif
