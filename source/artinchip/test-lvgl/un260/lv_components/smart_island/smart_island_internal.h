#ifndef SMART_ISLAND_INTERNAL_H
#define SMART_ISLAND_INTERNAL_H

#include "un260/lv_components/smart_island.h"
#include "un260/lv_components/lv_fault_popup.h"
#include <stddef.h>

#define SMART_ISLAND_WIDTH                261
#define SMART_ISLAND_ACTION_PAGE_CAPACITY 4U
#define SMART_ISLAND_ACTION_PAGE_COUNT    SMART_ISLAND_ACTION_PAGE_CAPACITY
#define SMART_ISLAND_ACTION_PAGE_DEFAULT_COUNT 3U
#define SMART_ISLAND_ACTION_EXPAND_H      112
#define SMART_ISLAND_PAGE_INDICATOR_Y     -6
#define SMART_ISLAND_BG_IDLE              0x111111U
#define SMART_ISLAND_COUNT_W               310
#define SMART_ISLAND_COUNT_CNT_W           SMART_ISLAND_COUNT_W
#define SMART_ISLAND_COUNT_H               44

typedef struct {
    lv_obj_t *root;
    lv_obj_t *modal;
    lv_obj_t *dot;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *time;
    lv_obj_t *badge;
    lv_obj_t *progress;
    lv_obj_t *counting_root;
    lv_obj_t *counting_gate;
    lv_obj_t *counting_gate_left;
    lv_obj_t *counting_gate_right;
    lv_obj_t *counting_sensor;
    lv_obj_t *counting_bill;
    lv_obj_t *counting_bill_mid;
    lv_obj_t *counting_bill_low;
    lv_obj_t *counting_value;
    lv_obj_t *counting_value_next;
    lv_obj_t *counting_value_track;
    lv_obj_t *counting_value_scan;
    lv_obj_t *counting_unit;
    lv_obj_t *counting_divider;
    lv_obj_t *counting_serial;
    lv_obj_t *counting_serial_track;
    lv_obj_t *counting_serial_scan;
    lv_obj_t *page_root;
    lv_obj_t *page_info;
    lv_obj_t *page_action;
    lv_obj_t *action_track;
    lv_obj_t *page_indicator;
    lv_obj_t *expand_title;
    lv_obj_t *expand_subtitle;
    lv_obj_t *expand_last;
    lv_obj_t *expand_divider;
    lv_obj_t *expand_footer;
    lv_obj_t *expand_extra;
    lv_obj_t *quality_bar_bg;
    lv_obj_t *quality_bar_fg;
    lv_obj_t *quality_percent;
    lv_obj_t *action_buttons[SMART_ISLAND_ACTION_PAGE_CAPACITY];
    lv_obj_t *action_labels[SMART_ISLAND_ACTION_PAGE_CAPACITY];
    lv_obj_t *action_arrows[SMART_ISLAND_ACTION_PAGE_CAPACITY];
} smart_island_object_refs_t;

typedef struct {
    uint8_t ids[SMART_ISLAND_ACTION_PAGE_CAPACITY];
    ui_text_id_t text_ids[SMART_ISLAND_ACTION_PAGE_CAPACITY];
    char texts[SMART_ISLAND_ACTION_PAGE_CAPACITY][32];
    smart_island_action_cb_t callback;
    uint8_t page_count;
    uint8_t page_index;
    bool ignore_click_once;
    bool ignore_action_click_once;
} smart_island_action_state_t;

typedef struct {
    bool pressed;
    bool swiped;
    lv_point_t start_pt;
} smart_island_swipe_state_t;

typedef struct {
    smart_island_scene_t scene;
    smart_island_visual_t visual;
    smart_island_page_t page;
    smart_island_content_t content;
    bool anim_running;
    int8_t page_slide_dir;
    uint32_t bg_current;
    uint32_t bg_from;
    uint32_t bg_to;
    bool bg_anim_running;
    bool message_pulse_running;
    smart_island_swipe_state_t swipe;
} smart_island_view_state_t;

typedef struct {
    bool valid;
    fault_source_t source;
    uint8_t fault_type;
    uint8_t code;
} smart_island_warning_fault_t;

typedef struct {
    smart_island_warning_level_t level;
    bool marquee_running;
    bool collapse_running;
    uint8_t marquee_step;
    lv_coord_t text_width_compact;
    lv_coord_t text_width_expand;
    bool resume_animation_pending;
    bool resume_counting;
    smart_island_warning_fault_t fault;
    char text[64];
} smart_island_warning_state_t;

typedef struct {
    int pcs;
    int amount;
    int denomination;
    uint8_t mode;
    uint8_t gate_phase;
    uint32_t last_roll_tick;
    bool gate_anim_running;
    bool serial_anim_running;
    bool value_initialized;
    char display_value[16];
    char serial[16];
} smart_island_counting_state_t;

typedef struct {
    char result[64];
    char serial_ticker[64];
    char compact[196];
    char info_title[196];
    char info_summary[196];
    char info_footer[196];
    char info_extra[196];
    char idle_line1[96];
    char idle_line2[96];
    char idle_line3[96];
    uint8_t idle_quality_percent;
    bool idle_has_issue;
    bool idle_has_data;
    bool idle_no_count;
    bool analysis_valid;
    int analysis_valid_pcs;
    int analysis_suspect_pcs;
    int analysis_damaged_pcs;
} smart_island_text_state_t;

typedef struct {
    lv_timer_t *result_timer;
    bool created;
    bool pure_count_enabled;
    bool count_session_active;
    bool result_transition_pending;
    bool suspended;
    bool dirty;
} smart_island_lifecycle_state_t;

typedef struct {
    smart_island_object_refs_t objects;
    smart_island_action_state_t action;
    smart_island_view_state_t view;
    smart_island_warning_state_t warning;
    smart_island_counting_state_t counting;
    smart_island_text_state_t text;
    smart_island_lifecycle_state_t lifecycle;
} smart_island_context_t;

extern smart_island_context_t g_si_ctx;

/* 子模块之间共享的内部接口，不对 smart_island.h 使用者公开。 */
void smart_island_result_stop_timer(void);
void smart_island_warning_stop(void);
void smart_island_warning_resume_if_pending(void);
bool smart_island_warning_fault_show(void);
void smart_island_warning_fault_clear(void);
void smart_island_reset_page_positions(void);
void smart_island_reset_compact_header_position(void);
void smart_island_reset_time_position(void);
void smart_island_update_idle_time(void);
void smart_island_update_pages_visible(void);
const char *smart_island_text_or_default(const char *text, ui_text_id_t text_id);
void smart_island_anim_x_cb(void *var, int32_t value);
void smart_island_anim_translate_x_cb(void *var, int32_t value);

void smart_island_enable_gesture_on_obj(lv_obj_t *obj);
void smart_island_modal_click_cb(lv_event_t *event);
void smart_island_click_cb(lv_event_t *event);
void smart_island_action_btn_create(void);
void smart_island_action_page_refresh_language_texts(void);
void smart_island_page_indicator_sync(bool anim_en);

void smart_island_view_destroy_objects(void);
void smart_island_view_apply_visual(smart_island_visual_t visual, bool anim_en);
void smart_island_view_refresh_scene(void);
void smart_island_view_update_counting(void);
void smart_island_view_message_pulse(void);
void smart_island_view_notice_expand(void);
void smart_island_view_notice_collapse(lv_anim_ready_cb_t ready_cb);
void smart_island_view_notice_reset(void);
void smart_island_view_result_enter(void);
void smart_island_view_result_collapse(void);
void smart_island_view_idle_enter(void);
void smart_island_view_set_idle_line(char *dst,
                                     size_t dst_size,
                                     const char *text);

#endif /* SMART_ISLAND_INTERNAL_H */
