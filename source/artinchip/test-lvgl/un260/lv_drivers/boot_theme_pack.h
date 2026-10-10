#ifndef UN260_BOOT_THEME_PACK_H
#define UN260_BOOT_THEME_PACK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Disk fields are little endian, explicitly decoded rather than struct-cast.
 * The versioned player and this pack are published as one immutable bundle. */
#define BOOT_THEME_PACK_HEADER_BYTES 184U
#define BOOT_THEME_PACK_MAX_PACKED (2U * 1024U * 1024U)
#define BOOT_THEME_PACK_MAX_RAW (8U * 1024U * 1024U)
#define BOOT_THEME_WIDTH 1280U
#define BOOT_THEME_HEIGHT 400U
#define BOOT_THEME_BG_BYTES (BOOT_THEME_WIDTH * BOOT_THEME_HEIGHT * 4U)
#define BOOT_THEME_ICON_BYTES (96U * 96U * 4U)
#define BOOT_THEME_TEXT_BYTES (500U * 128U * 4U)
#define BOOT_THEME_BASE_BYTES (BOOT_THEME_BG_BYTES + BOOT_THEME_ICON_BYTES + 2U * BOOT_THEME_TEXT_BYTES)
#define BOOT_THEME_LANGUAGE_BYTES 32U
#define BOOT_THEME_MODE_ANIMATION 1U
#define BOOT_THEME_MODE_LOGO 2U

typedef struct {
    uint32_t entrance_start, entrance_duration, brand_exit_start, brand_exit_duration;
    uint32_t welcome_start, welcome_duration, welcome_settled;
    uint32_t dot_start, dot_period, text_y, dot_y, dot_x, dot_step, dot_size, dot_radius;
    uint32_t active_y, active_h, motion_scale;
} boot_theme_scene_t;
typedef struct {
    uint8_t *data;
    size_t data_bytes;
    uint32_t theme, mode, language_count, language_table, native_offset, native_bytes;
    uint32_t logo_offset, min_show_ms, language;
    bool frozen;
    uint8_t identity[32];
    boot_theme_scene_t scene;
} boot_theme_pack_t;

/* Neither loader owns fd. On any failure pack is reset and its memory freed.
 * No screen or persistent writes, including --check and locale selection. */
bool boot_theme_pack_load_fd(int fd, boot_theme_pack_t *pack);
bool boot_theme_pack_load_path(const char *path, boot_theme_pack_t *pack);
bool boot_theme_pack_select_language(boot_theme_pack_t *pack, const char *tag);
/* Finalize mode and locale once; release unused Logo/language pixels before
 * the full application starts. A frozen snapshot cannot switch language. */
bool boot_theme_pack_freeze(boot_theme_pack_t *pack);
void boot_theme_pack_free(boot_theme_pack_t *pack);
#endif
