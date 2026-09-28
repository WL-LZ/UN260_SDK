#ifndef UN260_APP_AUTO_QR_H
#define UN260_APP_AUTO_QR_H
#include <stdint.h>
void app_auto_qr_on_start(void);
void app_auto_qr_on_end(uint32_t now);
void app_auto_qr_cancel(void);
void app_auto_qr_poll(uint32_t now);
#endif
