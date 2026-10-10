#ifndef UN260_BOOT_BUNDLE_H
#define UN260_BOOT_BUNDLE_H
#include <stdbool.h>
#include <stdint.h>

#define BOOT_BUNDLE_ACTIVE_PATH "/etc/un260/boot-active"
#define BOOT_BUNDLE_VERSIONS_PATH "/usr/local/lib/un260-boot/versions"
#define BOOT_BUNDLE_ACTIVE_MAGIC "UN260_BOOT_ACTIVE_V1"
#define BOOT_BUNDLE_READY_PATH "/dev/.un260-boot-light.ready"
#define BOOT_BUNDLE_LIVE_PATH "/dev/.un260-boot-bundle"
#define BOOT_BUNDLE_PROTOCOL 1U
#define BOOT_BUNDLE_MAX_PLAYER_BYTES (2U * 1024U * 1024U)
#define BOOT_BUNDLE_MAX_THEME_BYTES (4U * 1024U * 1024U)

typedef struct {
    char id[65];
    char manifest_sha256[65];
    char theme_sha256[65];
    char mode[10];
    int player_fd;
    int theme_fd;
    bool previous;
} boot_bundle_t;

/* Validates immutable files using the opened descriptors, never a shell or
 * untrusted path. previous_only is used after a failed first-frame attempt. */
bool boot_bundle_open(const char *active_path, const char *versions_path,
                      bool previous_only, boot_bundle_t *bundle);
void boot_bundle_close(boot_bundle_t *bundle);
bool boot_bundle_sha256_fd(int fd, char digest[65]);
#endif
