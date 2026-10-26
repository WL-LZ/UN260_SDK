#ifndef UN260_BOOT_THEME_CONFIG_H
#define UN260_BOOT_THEME_CONFIG_H
#define UI_BOOT_ANIM_THEME_A 1
#define UI_BOOT_ANIM_THEME_B 2
#define UI_BOOT_ANIM_THEME_C 3
#define UI_BOOT_ANIM_THEME_D 4
#ifndef UI_BOOT_ANIM_THEME
#define UI_BOOT_ANIM_THEME UI_BOOT_ANIM_THEME_D
#endif
#if UI_BOOT_ANIM_THEME < 1 || UI_BOOT_ANIM_THEME > 4
#error "UI_BOOT_ANIM_THEME must be 1, 2, 3 or 4"
#endif

/* Product selection: A = original welcome/self-test; B = customer artwork.
 * Change only this default, then rebuild the complete firmware. The early
 * bundle generator reads the same selection. Historical renderer IDs above
 * are independent; both product variants use renderer D. */
#define UI_BOOT_VARIANT_A 1
#define UI_BOOT_VARIANT_B 2
#ifndef UI_BOOT_VARIANT
#define UI_BOOT_VARIANT UI_BOOT_VARIANT_A
#endif
#if UI_BOOT_VARIANT != UI_BOOT_VARIANT_A && UI_BOOT_VARIANT != UI_BOOT_VARIANT_B
#error "UI_BOOT_VARIANT must be UI_BOOT_VARIANT_A or UI_BOOT_VARIANT_B"
#endif
/* CMake seals the selection used to choose resources. Host tests may select
 * either variant without this seal; production must not silently mix them. */
#if defined(UI_BOOT_BUILD_VARIANT) && UI_BOOT_VARIANT != UI_BOOT_BUILD_VARIANT
#error "Boot variant differs from configured resources; change boot_theme_config.h and reconfigure"
#endif
#define UI_BOOT_EARLY_ENABLED (UI_BOOT_ANIM_THEME == 3 || UI_BOOT_ANIM_THEME == 4)
#endif
