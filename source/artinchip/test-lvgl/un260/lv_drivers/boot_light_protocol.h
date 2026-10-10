#ifndef UN260_BOOT_LIGHT_PROTOCOL_H
#define UN260_BOOT_LIGHT_PROTOCOL_H
#include <stdint.h>
#ifndef BOOT_LIGHT_SOCKET
#define BOOT_LIGHT_SOCKET "/dev/.un260-boot-light.sock"
#endif
#ifndef BOOT_LIGHT_LOCK
#define BOOT_LIGHT_LOCK "/dev/.un260-boot-light.lock"
#endif
#ifndef BOOT_LIGHT_READY
#define BOOT_LIGHT_READY "/dev/.un260-boot-light.ready"
#endif
#ifndef BOOT_LIGHT_UNIFIED_READY
#define BOOT_LIGHT_UNIFIED_READY "/dev/.un260-boot-unified"
#endif
/* Theme D revision 7: localized pre-shaped handwritten welcome.
 * A mixed old initramfs/new UI must not adopt the old visual elapsed time.
 * FD transfer/lease stays compatible; mismatch uses the normal intro. */
#include "un260/lv_core/boot_anim/boot_theme_config.h"
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
#define BOOT_LIGHT_MAGIC 0x554e4238U
#else
#define BOOT_LIGHT_MAGIC 0x554e4233U
#endif
typedef struct {uint32_t magic, elapsed_ms;} boot_light_reply_t;

/* Root-resident player protocol. Visual revisions belong to the selected
 * bundle, while the transport ABI stays independent of artwork and locale. */
#define BOOT_LIGHT_UNIFIED_MAGIC 0x554e4331U
#define BOOT_LIGHT_UNIFIED_ABI 1U
#define BOOT_LIGHT_FIRST_FRAME 1U
#define BOOT_LIGHT_HANDOFF 2U
#define BOOT_LIGHT_ERROR 4U
#define BOOT_LIGHT_MODE_ANIMATION 1U
#define BOOT_LIGHT_MODE_LOGO 2U
#define BOOT_LIGHT_PROBE 'P'
#define BOOT_LIGHT_UI_READY 'R'
#define BOOT_LIGHT_UI_ERROR 'E'
typedef struct {
    uint32_t magic, abi, flags, mode, language, elapsed_ms;
    uint64_t origin_ms;
    uint8_t theme_id[32];
} boot_light_status_t;
#endif
