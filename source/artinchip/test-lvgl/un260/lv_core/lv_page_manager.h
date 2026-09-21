#ifndef LV_PAGE_MANAGER_H
#define LV_PAGE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_PAGE_BOOT_ANIM = 0,
    UI_PAGE_MAIN,
    UI_PAGE_LIST,
    UI_PAGE_MENU,
    UI_PAGE_SETTING,
    UI_PAGE_DETAIL,
    UI_PAGE_SET_PASSAGE,
    UI_PAGE_CURR,
    UI_PAGE_BOOT,
    UI_PAGE_CIS_CALIB,
    UI_PAGE_DEBUG,
    UI_PAGE_TIMESET,
    UI_PAGE_SENSOR,
    UI_PAGE_UPGRADE,
    UI_PAGE_MAIN_UPGRADE,
    UI_PAGE_IMAGE_UPGRADE,
    UI_PAGE_UI_UPGRADE,
    UI_PAGE_MOTOR_TEST,
    UI_PAGE_PURE,
    UI_PAGE_HISTORY,
    UI_PAGE_PRINT_SETTING,
    UI_PAGE_LANGUAGE_SETTING,
    UI_PAGE_DOUBLE_NOTE_SETTING,
    UI_PAGE_FLAP_SETTING,
    UI_PAGE_REJECT_POCKET_SETTING,
    UI_PAGE_SERIAL_NUMBER_SETTING,
    UI_PAGE_AGING_SETTING,
    UI_PAGE_CFD_LEVEL_SETTING,
    UI_PAGE_IMAGE_GET,
    UI_PAGE_PASSWORD_CHANGE,
    UI_PAGE_FACTORY_SETTING,
    UI_PAGE_WAVE_GET,
    UI_PAGE_INNOVATION_CENTER,
    UI_PAGE_BRIGHTNESS_SETTING,
    UI_PAGE_STANDBY_SETTING,
    UI_PAGE_STANDBY,
    UI_PAGE_DISPLAY_TEST,
    UI_PAGE_COUNT
} ui_page_t;

/*
 * Asynchronous model updates are independent from page lifetime.  Retained
 * pages subscribe to the topics they render; an update refreshes the active
 * page at the next visual commit and is remembered for hidden cached pages
 * until resume. Navigation still populates its first frame synchronously.
 */
typedef uint32_t ui_data_topic_t;

enum {
    UI_DATA_TOPIC_NONE             = 0U,
    UI_DATA_TOPIC_DEVICE_VERSION   = 1U << 0,
    UI_DATA_TOPIC_CURRENCY_CATALOG = 1U << 1,
    UI_DATA_TOPIC_COUNTING_RESULT  = 1U << 2,
    UI_DATA_TOPIC_MACHINE_SETTINGS = 1U << 3,
    UI_DATA_TOPIC_DIAGNOSTICS      = 1U << 4,
    UI_DATA_TOPIC_ALL              = UINT32_MAX,
};

void ui_manager_init(void); // page管理
void ui_manager_switch(ui_page_t page); // page切换
void ui_manager_push_page(ui_page_t page); // 页面堆栈：进入新页面
bool ui_manager_adopt_precreated_page(ui_page_t page); // 接管已创建页面，避免过渡结束后重复创建
bool ui_manager_pop_page(void); // 页面堆栈：返回上一页
void ui_manager_clear_stack(void); // 清空页面堆栈
/* Paired two-finger Home/Return. Normal navigation invalidates the bookmark. */
bool ui_manager_suspend_to_home(void);
bool ui_manager_restore_from_home(void);
bool ui_manager_invalidate_page_cache(ui_page_t page); // 主动释放非活动缓存页
void ui_manager_invalidate_all_page_caches(void); // 释放所有非活动缓存页
/* Returns true when the retained page is ready (already cached or created and
 * suspended by this call).  Returns false while an optional data dependency
 * is not ready, allowing the boot scheduler to retry without creating stale
 * visual state. */
bool ui_manager_prewarm_page(ui_page_t page);
/* True only while the manager is constructing this page off-screen during
 * boot prewarm.  Page create functions use it to defer external side effects
 * (protocol traffic, hardware actions) until the real activation. */
bool ui_manager_is_prewarming_page(ui_page_t page);
ui_page_t ui_manager_get_current_page(void); // 获取当前页
const char *ui_manager_page_name(ui_page_t page); // 获取页面诊断名称
bool ui_manager_is_transitioning(void); // 页面正在同步提交或等待首帧输入保护
/* Idempotent per-page ownership for an asynchronous transition. Releasing
 * one owner never removes the manager's first-frame input guard. */
void ui_manager_hold_transition_input(ui_page_t owner, bool hold);
void ui_manager_publish_data_changed(ui_data_topic_t topics); // 发布异步数据更新

#ifdef __cplusplus
}
#endif

#endif // LV_PAGE_MANAGER_H
