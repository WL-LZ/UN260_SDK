#ifndef UN260_BOOT_WELCOME_TIMING_H
#define UN260_BOOT_WELCOME_TIMING_H
/* Shared by native first screen and LVGL. Milliseconds, from visual origin.
 * Immutable assets plus monotonic opacity; never restart at handoff. */
#include "boot_theme_config.h"
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
#define BOOT_ENTRANCE_START 0U
#define BOOT_ENTRANCE_DURATION 790U
#define BOOT_BRAND_EXIT_START 0U
#define BOOT_BRAND_EXIT_DURATION 1U
#define BOOT_WELCOME_START 550U
#define BOOT_WELCOME_DURATION 1750U
#define BOOT_WELCOME_SETTLED 2300U
#define BOOT_WELCOME_HOLD_END 7400U
#define BOOT_DOT_START 2500U
#define BOOT_DOT_PERIOD 1800U
#define BOOT_TEXT_Y 141
#define BOOT_TEXT_HEIGHT 128U
#define BOOT_DOT_Y 288
#define BOOT_DOT_X 623U
#define BOOT_DOT_STEP 14U
#define BOOT_DOT_SIZE 5U
#define BOOT_DOT_RADIUS_SQ 5
#else
#define BOOT_ENTRANCE_START 0U
#define BOOT_ENTRANCE_DURATION 600U
#define BOOT_BRAND_EXIT_START 600U
#define BOOT_BRAND_EXIT_DURATION 300U
#define BOOT_WELCOME_START 900U
#define BOOT_WELCOME_DURATION 650U
#define BOOT_WELCOME_SETTLED 1550U
#define BOOT_WELCOME_HOLD_END 2180U
#define BOOT_DOT_START 1600U
#define BOOT_DOT_PERIOD 1500U
#define BOOT_TEXT_Y 196
#define BOOT_TEXT_HEIGHT 128U
#define BOOT_DOT_Y 330
#define BOOT_DOT_X 619U
#define BOOT_DOT_STEP 17U
#define BOOT_DOT_SIZE 7U
#define BOOT_DOT_RADIUS_SQ 12
#endif
/* Board fallback: set to 0 for opacity-only text and stationary breathing
 * dots. Rebuild BOTH native/initramfs and LVGL; never mix their timelines. */
#ifndef BOOT_WELCOME_MOTION_SCALE
#define BOOT_WELCOME_MOTION_SCALE 1U
#endif
#if BOOT_WELCOME_MOTION_SCALE != 0 && BOOT_WELCOME_MOTION_SCALE != 1
#error "BOOT_WELCOME_MOTION_SCALE must be 0 or 1"
#endif
#endif
