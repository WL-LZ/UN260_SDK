#include "un260/lv_system/ui_message.h"
#ifndef UN260_CASHBOOK_STORE_H
#define UN260_CASHBOOK_STORE_H
#include "un260/workspace/cashbook.h"
typedef enum { CASHBOOK_REF_UNKNOWN, CASHBOOK_REF_CHECKING, CASHBOOK_REF_CLEAR, CASHBOOK_REF_USED, CASHBOOK_REF_FAILED } cashbook_reference_t;
bool cashbook_store_check_operator(uint32_t id);
cashbook_reference_t cashbook_store_operator_reference(uint32_t id);
void cashbook_store_init(void);
bool cashbook_store_poll(void);
bool cashbook_store_ready(void);
bool cashbook_store_busy(void);
/* Completion result; inspect only after busy becomes false. */
bool cashbook_store_last_success(void);
const cashbook_t *cashbook_store_get(void);
const char *cashbook_store_message(void);
/* Durable after poll publishes the new sequence. No LVGL pointer crosses threads. */
bool cashbook_store_submit(const cashbook_command_t *);
typedef struct {uint32_t id,first_day,last_day;} cashbook_archive_t;
#define CASHBOOK_ARCHIVES 128
const cashbook_t *cashbook_store_view(void);
bool cashbook_store_view_archived(void);
void cashbook_store_close_view(void);
unsigned cashbook_store_archives(const cashbook_archive_t **items);
bool cashbook_store_scan_archives(void);
bool cashbook_store_open_archive(uint32_t id);
bool cashbook_store_archive(void);
bool cashbook_store_export(void);
const ui_message_t *cashbook_store_message_info(void);
#endif
