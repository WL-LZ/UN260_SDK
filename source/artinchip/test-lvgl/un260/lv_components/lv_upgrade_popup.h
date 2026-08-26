#ifndef LV_UPGRADE_POPUP_H
#define LV_UPGRADE_POPUP_H

#include <stdbool.h>

#include "un260/lv_core/ui_upgrade_service.h"

void lv_upgrade_popup_process_detect(const ui_upgrade_detect_info_t* detect_info);
void lv_upgrade_popup_show_result(bool success, const char* desc_text);
bool lv_upgrade_popup_is_showing(void);
void lv_upgrade_popup_refresh_text(void);

#endif
