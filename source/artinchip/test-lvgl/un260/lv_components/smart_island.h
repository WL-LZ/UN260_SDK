#ifndef SMART_ISLAND_H
#define SMART_ISLAND_H

#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_text.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SMART_ISLAND_SCENE_IDLE = 0,
    SMART_ISLAND_SCENE_COUNTING,
    SMART_ISLAND_SCENE_RESULT,
    SMART_ISLAND_SCENE_WARNING,
    SMART_ISLAND_SCENE_UPDATE,
    SMART_ISLAND_SCENE_QR
} smart_island_scene_t;

typedef enum {
    SMART_ISLAND_VISUAL_MINI = 0,
    SMART_ISLAND_VISUAL_COMPACT,
    SMART_ISLAND_VISUAL_EXPANDED
} smart_island_visual_t;

typedef enum {
    SMART_ISLAND_PAGE_INFO = 0,
    SMART_ISLAND_PAGE_ACTION
} smart_island_page_t;

typedef enum {
    SMART_ISLAND_WARNING_LEVEL_WARNING = 0,
    SMART_ISLAND_WARNING_LEVEL_ERROR
} smart_island_warning_level_t;

typedef void (*smart_island_action_cb_t)(uint8_t action_id); //动作按钮回调

typedef struct {
    char title[64];
    char subtitle[64];
    uint16_t progress;
    uint8_t icon;
    bool show_progress;
} smart_island_content_t;

#define SMART_ISLAND_ACTION_QR            1
#define SMART_ISLAND_ACTION_TIME_SETTING  2
#define SMART_ISLAND_ACTION_FUNC3         3
#define SMART_ISLAND_ACTION_FUNC4         4
#define SMART_ISLAND_ACTION_TOUCH_GUIDE   5

void smart_island_create(lv_obj_t *parent); //创建灵动岛
void smart_island_destroy(void); //销毁灵动岛
void smart_island_set_suspended(bool suspended); //主页面隐藏时暂停对象刷新
void smart_island_refresh_time(void); //刷新默认时间显示
void smart_island_set_visual(smart_island_visual_t visual, bool anim_en); //设置视觉形态
void smart_island_set_scene(smart_island_scene_t scene, const char *title, const char *subtitle); //设置场景与文本
void smart_island_notify_count_start(void); //通知：开始点钞
void smart_island_update_counting(int pcs, float amount); //更新跑钞态实时数量/金额
void smart_island_notify_count_end(const char *result_text); //通知：点钞结束
void smart_island_notify_serial_number(int denomination, const char *serial_number); //通知：本把冠字号更新
void smart_island_set_count_analysis(int valid_pcs, int suspect_pcs, int damaged_pcs); //更新本轮点钞分析
void smart_island_clear_count_analysis(void); //清除本轮点钞分析
void smart_island_notify_warning(const char *warn_text); //通知：警告出现
void smart_island_notify_warning_level(const char *warn_text, smart_island_warning_level_t level); //通知：警告/报错
void smart_island_notify_update(uint16_t progress, const char *text); //通知：升级中
void smart_island_notify_qr(const char *text); //通知：二维码相关提示
void smart_island_restore_idle(void); //恢复默认待机态
bool smart_island_is_expanded(void); //是否处于展开态

void smart_island_set_page(smart_island_page_t page, bool anim_en); //设置灵动岛页面
smart_island_page_t smart_island_get_page(void); //获取灵动岛页面
void smart_island_register_action_cb(smart_island_action_cb_t cb); //注册动作按钮回调
bool smart_island_action_page_set_count(uint8_t count); //设置动作页数量
bool smart_island_action_page_set_lang_item(uint8_t index, uint8_t action_id, ui_text_id_t text_id); //设置动作页多语言按键
bool smart_island_action_page_set_item(uint8_t index, uint8_t action_id, const char *text); //设置动作页按键
void smart_island_refresh_language_texts(void); //刷新灵动岛多语言文本
void smart_island_refresh_summary(void); //刷新灵动岛分析摘要
void smart_island_set_idle_info_line1(const char *text); //设置Idle展开第1行（NULL/空字符串恢复默认）
void smart_island_set_idle_info_line2(const char *text); //设置Idle展开第2行（NULL/空字符串显示none）
void smart_island_set_idle_info_line3(const char *text); //设置Idle展开第3行（NULL/空字符串显示none）
void smart_island_close(void); //关闭灵动岛
void smart_island_open_info_page(void); //打开信息页
void smart_island_open_action_page(void); //打开功能页
bool smart_island_is_attached_to(lv_obj_t *parent); //灵动岛是否挂载在指定父对象下
void smart_island_set_pure_count_enabled(bool enabled); //设置纯净跑钞模式开关
bool smart_island_pure_count_is_enabled(void); //获取纯净跑钞模式开关

#endif // SMART_ISLAND_H
