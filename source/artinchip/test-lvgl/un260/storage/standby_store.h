#ifndef UN260_STANDBY_STORE_H
#define UN260_STANDBY_STORE_H
#include <stdbool.h>
#include <stdint.h>
#define STANDBY_IMPORT_MAX 6
/* Preserve persisted IDs 0..2 (built-in), 3..8 (USB). */
#define STANDBY_PHOTO_MIST 9
#define STANDBY_PHOTO_COUNT 10
static inline bool standby_photo_is_imported(unsigned photo) { return photo >= 3 && photo < 9; }
typedef struct {
    uint32_t color;
    uint16_t x, y;
    uint8_t date_bits, date_style, hour12, greeting;
    /* Former reserved byte: zero in v1 files means the original 100% size. */
    uint8_t scheduled, photo, light_text, scale_percent;
    uint16_t date_x, date_y, dial_x, dial_y;
    uint32_t text_color;
    uint8_t auto_text, reserved[3];
} standby_layout_t;
static inline unsigned standby_layout_scale(const standby_layout_t *p) {
    return p->scale_percent ? p->scale_percent : 100;
}
static inline void standby_layout_clamp(standby_layout_t *p) {
    /* Precise content-dependent bounds are applied by the layout owner. */
    if(p->x>1240)p->x=1240;
    if(p->y<40)p->y=40;
    if(p->y>290)p->y=290;
    if(p->date_x>1240)p->date_x=1240;
    if(p->date_y<40)p->date_y=40;
    if(p->date_y>290)p->date_y=290;
    if(p->dial_x>1024)p->dial_x=1024;
    if(p->dial_y<40)p->dial_y=40;
    if(p->dial_y>78)p->dial_y=78;
}
typedef struct {
    uint32_t version;
    uint16_t minutes;
    uint8_t mode, active[2], reserved[3];
    standby_layout_t layout[2][3];
} standby_config_t;
void standby_defaults(standby_config_t *cfg);
bool standby_config_valid(const standby_config_t *cfg);
void standby_store_init(void);
const standby_config_t *standby_config(void);
bool standby_store_save(const standby_config_t *cfg);
bool standby_store_import(void);
bool standby_store_delete(unsigned photo);
bool standby_store_busy(void);
/* Poll on UI thread. Completed jobs never hold references to a page. */
bool standby_store_poll(char *message, unsigned capacity);
bool standby_photo_exists(unsigned slot);
const char *standby_photo_path(unsigned photo);
#endif
