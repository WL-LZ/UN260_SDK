#ifndef COUNTING_MULTI_EXTRA_H
#define COUNTING_MULTI_EXTRA_H
#include "counting_multi.h"
/* Current protocol uses a one-byte serial index; 00/FF are delimiters. */
#define MULTI_SERIAL_MAX 254
typedef struct { uint16_t number; uint32_t value; char text[21]; } multi_serial_t;
typedef struct { multi_detail_status_t status; uint16_t count; multi_serial_t *rows; } multi_serial_cache_t;
typedef struct { uint8_t code, pcs; } multi_reject_t;
typedef struct { multi_detail_status_t status; uint16_t count; multi_reject_t rows[254]; } multi_reject_cache_t;
const multi_serial_cache_t *counting_multi_serials(unsigned index);
const multi_reject_cache_t *counting_multi_rejects(void);
uint32_t counting_multi_extra_revision(void);
bool counting_multi_extra_busy(void);
bool counting_multi_serial_request(unsigned index, uint32_t now);
bool counting_multi_reject_request(uint32_t now);
/* Returns true only for a frame owned by this service; no unowned publication. */
bool counting_multi_extra_reply(uint8_t cmd, const uint8_t *frame, uint8_t len, uint32_t now);
void counting_multi_extra_poll(uint32_t now);
#endif
