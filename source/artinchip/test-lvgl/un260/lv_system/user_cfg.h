#ifndef USER_CFG_H
#define USER_CFG_H
#include <stdbool.h>
#include <stdint.h>
#define LV_DEBUG 1
typedef enum {
    CURR_CNY_ITEM = 0,
    CURR_USD_ITEM ,
    CURR_EUR_ITEM ,
    CURR_GBP_ITEM ,
    CURR_KRW_ITEM,
    CURR_EGP_ITEM,
    CURR_ISK_ITEM,
    CURR_PHP_ITEM,
    CURR_SOS_ITEM,
    CURR_TRY_ITEM,
    CURR_AED_ITEM,
    CURR_SAR_ITEM,
    CURR_OMR_ITEM,
    CURR_QAR_ITEM,
    CURR_MAD_ITEM,
    CURR_DZD_ITEM,    
    CURR_INR_ITEM,
    CURR_PKR_ITEM,
    CURR_IQD_ITEM,
    CURR_COUNT
}curr_item_t;
#define UI_VERSION  "V1.0.0"

#define PCS_BATCH_MODE 0
#define AMOUNT_BATCH_MODE 1
//Function
#define SPEED_MODE 3
#define CFD_MODE 3
#define FO_MODE 4
#define ADD_MODE 2
#define WORK_MODE 2
#define PAGE_01_REPORT_ITEM 8
#define PAGE_02_A_ITEM 8
#define PAGE_02_B_ITEM 9
#define PAGE_02_C_ITEM 9
#define PAGE_02_DEBUG 1
#define PAGE_07_CURRENCIES 4
#define CONTROLLER_MAX_CURRENCIES 32
/* AUTO is shown before controller currencies; reserve one more slot for MIX. */
#define CURRENCY_FEATURE_SLOT_COUNT 2
#define MAX_CURRENCIES (CONTROLLER_MAX_CURRENCIES + CURRENCY_FEATURE_SLOT_COUNT)

#define PRINT_SETTING_HEAD_MAX_LEN 20
#define PRINT_SETTING_SPACE_MAX_LINES 99
#define PRINT_SETTING_CONTENT_LIST 0x01
#define PRINT_SETTING_CONTENT_SN 0x02
#define PRINT_SETTING_CONTENT_LIST_SN 0x03
#define DOUBLE_NOTE_LEVEL_MIN 1
#define DOUBLE_NOTE_LEVEL_MAX 3
#define FLAP_POSITION_UP 0x01
#define FLAP_POSITION_DOWN 0x02
#define REJECT_POCKET_MIN_CAPACITY 30
#define REJECT_POCKET_MAX_CAPACITY 100
#define SERIAL_NUMBER_LEVEL_OFF 0
#define SERIAL_NUMBER_LEVEL_MAX 3
#define CFD_SCENE_COUNT 3
#define CFD_ITEM_COUNT 4
#define CFD_LEVEL_MIN 1
#define CFD_LEVEL_MAX 5
#define USER_PASSWORD_MAX_LEN 31

bool user_cfg_password_load(void);
bool user_cfg_password_save(const char* password);
const char *user_cfg_password_get(void);
bool user_cfg_screenshot_load(void);
bool user_cfg_screenshot_save(bool enabled);
bool user_cfg_screenshot_enabled(void);
bool user_cfg_screen_recording_load(void);
bool user_cfg_screen_recording_save(bool enabled);
bool user_cfg_screen_recording_enabled(void);
bool user_cfg_performance_monitor_load(void);
bool user_cfg_performance_monitor_save(bool enabled);
bool user_cfg_performance_monitor_enabled(void);
bool user_cfg_performance_profile_load(void);
bool user_cfg_performance_profile_save(bool enabled);
bool user_cfg_performance_profile_enabled(void);
bool user_cfg_gesture_load(void);
bool user_cfg_gesture_save(bool enabled);
bool user_cfg_gesture_enabled(void);

 enum {
    MODE_NONE,
    MODE_MDC,
    MODE_CNT,
    MODE_VER,
    MODE_SDC

};


#endif // !USER_CFG_H
