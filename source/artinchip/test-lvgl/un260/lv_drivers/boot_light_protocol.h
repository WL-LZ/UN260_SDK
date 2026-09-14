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
#define BOOT_LIGHT_MAGIC 0x554e4231U
typedef struct {uint32_t magic, elapsed_ms;} boot_light_reply_t;
#endif
