#ifndef COUNT_MODE_POLICY_H
#define COUNT_MODE_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "un260/lv_system/user_cfg.h"

/* Selected feature (AUT/MUL), never the detected/result currency. */
static inline bool count_mode_allowed(const char *code, uint8_t mode)
{
    if (mode != MODE_MDC && mode != MODE_SDC && mode != MODE_CNT) return false;
    if (code && !strncmp(code, "MUL", 3)) return mode == MODE_MDC;
    if (code && !strncmp(code, "AUT", 3)) return mode != MODE_CNT;
    return true;
}
static inline uint8_t count_mode_resolve(const char *code, uint8_t mode)
{
    return count_mode_allowed(code, mode) ? mode : MODE_MDC;
}
static inline uint8_t count_mode_next(const char *code, uint8_t mode)
{
    if (!count_mode_allowed(code, mode)) return MODE_MDC;
    if (mode == MODE_MDC && count_mode_allowed(code, MODE_SDC)) return MODE_SDC;
    if (mode == MODE_SDC && count_mode_allowed(code, MODE_CNT)) return MODE_CNT;
    return MODE_MDC;
}
#endif
