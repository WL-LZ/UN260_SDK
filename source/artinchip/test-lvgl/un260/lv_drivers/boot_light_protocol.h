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
/* Theme D revision 5: shared Main palette and complete three-cycle timeline.
 * A mixed old initramfs/new UI must not adopt the old visual elapsed time.
 * FD transfer/lease stays compatible; mismatch uses the normal intro. */
#include "un260/lv_core/boot_anim/boot_theme_config.h"
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
#define BOOT_LIGHT_MAGIC 0x554e4236U
#else
#define BOOT_LIGHT_MAGIC 0x554e4232U
#endif
typedef struct {uint32_t magic, elapsed_ms;} boot_light_reply_t;
#endif
