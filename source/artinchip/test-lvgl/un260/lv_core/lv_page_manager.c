
#include "lvgl/lvgl.h"
#include <string.h>
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/perf_stats.h"
#include"lv_page_declear.h"

#define UI_PAGE_STACK_CAPACITY 10
#define UI_PAGE_INVALID ((ui_page_t)-1)

typedef struct {
    ui_page_t current;
    ui_page_t stack[UI_PAGE_STACK_CAPACITY];
    int stack_top;
} ui_page_manager_context_t;

static ui_page_manager_context_t g_page_manager = {
    .current = UI_PAGE_INVALID,
    .stack_top = -1,
};

static bool g_page_cache_ready[UI_PAGE_COUNT];

typedef void (*ui_page_create_fn_t)(lv_obj_t *parent);
typedef void (*ui_page_destroy_fn_t)(void);
typedef bool (*ui_page_resume_fn_t)(void);
typedef void (*ui_page_suspend_fn_t)(void);

typedef enum {
    UI_PAGE_TRANSIENT = 0,
    UI_PAGE_RETAINED,
} ui_page_cache_policy_t;

typedef struct {
    ui_page_create_fn_t create;
    ui_page_destroy_fn_t destroy;
    ui_page_resume_fn_t resume;
    ui_page_suspend_fn_t suspend;
    ui_page_cache_policy_t cache_policy;
} ui_page_registration_t;

static void ui_manager_create_main(lv_obj_t *parent)
{
    ui_main_create(parent);
    resume_counting_sim();
}

static void ui_manager_create_debug(lv_obj_t *parent)
{
    LV_UNUSED(parent);
    ui_page_10_debug_create();
}

static const char *const g_page_names[UI_PAGE_COUNT] = {
    [UI_PAGE_BOOT_ANIM] = "BOOT_ANIM",
    [UI_PAGE_MAIN] = "MAIN",
    [UI_PAGE_LIST] = "LIST",
    [UI_PAGE_MENU] = "MENU",
    [UI_PAGE_SETTING] = "SETTINGS",
    [UI_PAGE_DETAIL] = "DETAIL",
    [UI_PAGE_SET_PASSAGE] = "PASSWORD",
    [UI_PAGE_CURR] = "CURRENCY",
    [UI_PAGE_BOOT] = "SELF_TEST",
    [UI_PAGE_CIS_CALIB] = "CIS_CALIB",
    [UI_PAGE_DEBUG] = "DEBUG",
    [UI_PAGE_TIMESET] = "TIME_SET",
    [UI_PAGE_SENSOR] = "SENSOR",
    [UI_PAGE_UPGRADE] = "UPGRADE",
    [UI_PAGE_MAIN_UPGRADE] = "MAIN_UPGRADE",
    [UI_PAGE_IMAGE_UPGRADE] = "IMAGE_UPGRADE",
    [UI_PAGE_UI_UPGRADE] = "UI_UPGRADE",
    [UI_PAGE_MOTOR_TEST] = "MOTOR_TEST",
    [UI_PAGE_PURE] = "PURE",
    [UI_PAGE_HISTORY] = "HISTORY",
    [UI_PAGE_PRINT_SETTING] = "PRINT_SETTINGS",
    [UI_PAGE_LANGUAGE_SETTING] = "LANGUAGE",
    [UI_PAGE_DOUBLE_NOTE_SETTING] = "DOUBLE_NOTE",
    [UI_PAGE_FLAP_SETTING] = "FLAP",
    [UI_PAGE_REJECT_POCKET_SETTING] = "REJECT_POCKET",
    [UI_PAGE_SERIAL_NUMBER_SETTING] = "SERIAL_NUMBER",
    [UI_PAGE_AGING_SETTING] = "AGING",
    [UI_PAGE_CFD_LEVEL_SETTING] = "CFD_LEVEL",
    [UI_PAGE_IMAGE_GET] = "IMAGE_GET",
    [UI_PAGE_PASSWORD_CHANGE] = "PASSWORD_CHANGE",
    [UI_PAGE_FACTORY_SETTING] = "FACTORY",
    [UI_PAGE_WAVE_GET] = "WAVE",
    [UI_PAGE_INNOVATION_CENTER] = "INNOVATION",
};

static const ui_page_registration_t g_page_registry[UI_PAGE_COUNT] = {
    [UI_PAGE_BOOT_ANIM] = { ui_page_00_boot_anim_create, ui_page_00_boot_anim_destroy },
    [UI_PAGE_MAIN] = {
        .create = ui_manager_create_main,
        .destroy = ui_main_destroy,
        .resume = page_01_main_resume,
        .suspend = page_01_main_suspend,
        .cache_policy = UI_PAGE_RETAINED,
    },
    [UI_PAGE_LIST] = {
        .create = ui_page_02_list_create,
        .destroy = ui_page_02_list_destroy,
        .resume = ui_page_02_list_resume,
        .suspend = ui_page_02_list_suspend,
        .cache_policy = UI_PAGE_RETAINED,
    },
    [UI_PAGE_MENU] = {
        .create = ui_page_03_menu_create,
        .destroy = ui_page_03_menu_destroy,
        .resume = ui_page_03_menu_resume,
        .suspend = ui_page_03_menu_suspend,
        .cache_policy = UI_PAGE_RETAINED,
    },
    [UI_PAGE_SETTING] = {
        .create = ui_page_06_settings_create,
        .destroy = ui_page_06_settings_destroy,
        .resume = ui_page_06_settings_resume,
        .suspend = ui_page_06_settings_suspend,
        .cache_policy = UI_PAGE_RETAINED,
    },
    [UI_PAGE_SET_PASSAGE] = { ui_page_05_set_password_create, ui_page_05_set_password_destroy },
    [UI_PAGE_CURR] = {
        .create = ui_page_07_curr_create,
        .destroy = ui_page_07_curr_destroy,
        .resume = ui_page_07_curr_resume,
        .suspend = ui_page_07_curr_suspend,
        .cache_policy = UI_PAGE_RETAINED,
    },
    [UI_PAGE_BOOT] = { ui_page_08_curr_create, ui_page_08_curr_destroy },
    [UI_PAGE_CIS_CALIB] = { ui_page_cis_calib_create, ui_page_cis_calib_destroy },
    [UI_PAGE_DEBUG] = { ui_manager_create_debug, ui_page_10_debug_destroy },
    [UI_PAGE_TIMESET] = { ui_page_11_timeset_create, ui_page_11_timeset_destroy },
    [UI_PAGE_SENSOR] = { ui_page_12_sensor_create, ui_page_12_sensor_destroy },
    [UI_PAGE_UPGRADE] = { ui_page_13_upgrade_create, ui_page_13_upgrade_destroy },
    [UI_PAGE_MAIN_UPGRADE] = { ui_page_14_main_upgrade_create, ui_page_14_main_upgrade_destroy },
    [UI_PAGE_IMAGE_UPGRADE] = { ui_page_15_image_upgrade_create, ui_page_15_image_upgrade_destroy },
    [UI_PAGE_UI_UPGRADE] = { ui_page_16_ui_upgrade_create, ui_page_16_ui_upgrade_destroy },
    [UI_PAGE_MOTOR_TEST] = { ui_page_17_motor_test_create, ui_page_17_motor_test_destroy },
    [UI_PAGE_PURE] = { ui_page_18_pure_create, ui_page_18_pure_destroy },
    [UI_PAGE_HISTORY] = { ui_page_19_history_create, ui_page_19_history_destroy },
    [UI_PAGE_PRINT_SETTING] = { ui_page_20_set_print_create, ui_page_20_set_print_destroy },
    [UI_PAGE_LANGUAGE_SETTING] = { ui_page_21_set_language_create, ui_page_21_set_language_destroy },
    [UI_PAGE_DOUBLE_NOTE_SETTING] = { ui_page_22_set_double_note_create, ui_page_22_set_double_note_destroy },
    [UI_PAGE_FLAP_SETTING] = { ui_page_23_set_flap_create, ui_page_23_set_flap_destroy },
    [UI_PAGE_REJECT_POCKET_SETTING] = { ui_page_24_set_reject_pocket_create, ui_page_24_set_reject_pocket_destroy },
    [UI_PAGE_SERIAL_NUMBER_SETTING] = { ui_page_25_set_serial_number_create, ui_page_25_set_serial_number_destroy },
    [UI_PAGE_AGING_SETTING] = { ui_page_26_set_aging_create, ui_page_26_set_aging_destroy },
    [UI_PAGE_CFD_LEVEL_SETTING] = { ui_page_27_set_cfd_level_create, ui_page_27_set_cfd_level_destroy },
    [UI_PAGE_IMAGE_GET] = { ui_page_28_get_image_create, ui_page_28_get_image_destroy },
    [UI_PAGE_PASSWORD_CHANGE] = { ui_page_29_set_password_create, ui_page_29_set_password_destroy },
    [UI_PAGE_FACTORY_SETTING] = { ui_page_30_set_factory_create, ui_page_30_set_factory_destroy },
    [UI_PAGE_WAVE_GET] = { ui_page_31_get_wave_create, ui_page_31_get_wave_destroy },
    [UI_PAGE_INNOVATION_CENTER] = {
        .create = ui_page_32_innovation_create,
        .destroy = ui_page_32_innovation_destroy,
        .resume = ui_page_32_innovation_resume,
        .suspend = ui_page_32_innovation_suspend,
        .cache_policy = UI_PAGE_RETAINED,
    },
};

static bool ui_manager_page_is_registered(ui_page_t page)
{
    return page >= UI_PAGE_BOOT_ANIM && page < UI_PAGE_COUNT &&
           g_page_registry[page].create != NULL;
}

//销毁当前页面
static const char *destroy_current_page(void)
{
    const ui_page_registration_t *registration;

    if (g_page_manager.current < UI_PAGE_BOOT_ANIM ||
        g_page_manager.current >= UI_PAGE_COUNT) {
        return "NONE";
    }

    registration = &g_page_registry[g_page_manager.current];
    if (registration->cache_policy == UI_PAGE_RETAINED &&
        registration->suspend != NULL) {
        registration->suspend();
        g_page_cache_ready[g_page_manager.current] = true;
        return "SUSPEND";
    }
    if (registration->destroy != NULL) {
        registration->destroy();
        return "DESTROY";
    }
    return "NONE";
}

static const char *create_new_page(ui_page_t page)
{
    const ui_page_registration_t *registration = &g_page_registry[page];

    if (registration->cache_policy == UI_PAGE_RETAINED &&
        registration->resume != NULL && registration->resume()) {
        g_page_cache_ready[page] = true;
        return "RESUME";
    }
    registration->create(lv_scr_act());
    if (registration->cache_policy == UI_PAGE_RETAINED) {
        g_page_cache_ready[page] = true;
    }
    return "CREATE";
}

static uint32_t ui_manager_profile_elapsed_us(uint64_t started_us)
{
    return app_clock_elapsed_us32(started_us, app_clock_monotonic_us());
}

typedef struct {
    ui_page_t from;
    ui_page_t to;
    uint8_t cmd_g;
    uint8_t cmd_s;
} page_switch_notify_rule_t;

static void ui_manager_send_protocol(uint8_t cmd_g, uint8_t cmd_s)
{
    protocol_send(cmd_g, &cmd_s, 1);
}

static void ui_manager_notify_page_switch(ui_page_t from, ui_page_t to)
{
    static const page_switch_notify_rule_t rules[] = {
        { UI_PAGE_MAIN, UI_PAGE_LIST, 0x40, 0x01 },
        { UI_PAGE_LIST, UI_PAGE_MAIN, 0x40, 0x00 },
    };

    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        if (rules[i].from == from && rules[i].to == to) {
            ui_manager_send_protocol(rules[i].cmd_g, rules[i].cmd_s);
            break;
        }
    }
}

void ui_manager_switch(ui_page_t page)
{
    ui_page_t from = g_page_manager.current;
    perf_profile_page_switch_sample_t sample = {
        .started_us = 0,
        .from_id = (uint32_t)from,
        .from_name = ui_manager_page_name(from),
        .to_id = (uint32_t)page,
        .to_name = ui_manager_page_name(page),
        .route = "NORMAL",
        .leave_action = "NONE",
        .enter_action = "NONE",
    };
    bool profile_enabled;
    uint64_t total_started_us = 0;
    uint64_t phase_started_us = 0;

    if (page == g_page_manager.current) return;
    if (!ui_manager_page_is_registered(page)) return;

    profile_enabled = perf_profile_is_enabled();
    if (profile_enabled) {
        total_started_us = app_clock_monotonic_us();
        sample.started_us = total_started_us;
        phase_started_us = total_started_us;
    }
    ui_manager_notify_page_switch(from, page);
    if (profile_enabled) {
        sample.notify_us = ui_manager_profile_elapsed_us(phase_started_us);
        phase_started_us = app_clock_monotonic_us();
    }
    sample.leave_action = destroy_current_page();
    if (profile_enabled) {
        sample.leave_us = ui_manager_profile_elapsed_us(phase_started_us);
        phase_started_us = app_clock_monotonic_us();
    }
    sample.enter_action = create_new_page(page);
    if (profile_enabled) {
        sample.enter_us = ui_manager_profile_elapsed_us(phase_started_us);
        phase_started_us = app_clock_monotonic_us();
    }
    g_page_manager.current = page;
    if (profile_enabled) {
        sample.commit_us = ui_manager_profile_elapsed_us(phase_started_us);
        sample.total_us = ui_manager_profile_elapsed_us(total_started_us);
        perf_profile_report_page_switch(&sample);
    }
}

void ui_manager_init(void) {
    // 初始化堆栈
    g_page_manager.stack_top = -1;
    memset(g_page_cache_ready, 0, sizeof(g_page_cache_ready));

    // 显示主页面

    ui_manager_switch(UI_PAGE_MAIN);
    sim_reset_counting_result(counting_data_mutable());

}


void ui_manager_push_page(ui_page_t page)
{
    int i;

    if (page == g_page_manager.current ||
        !ui_manager_page_is_registered(page)) return;

    //当前页入栈
    if (g_page_manager.current != UI_PAGE_INVALID) {
        if (g_page_manager.stack_top < UI_PAGE_STACK_CAPACITY - 1) {
            g_page_manager.stack[++g_page_manager.stack_top] =
                g_page_manager.current;
        } else {
            for (i = 0; i < UI_PAGE_STACK_CAPACITY - 1; i++) {
                g_page_manager.stack[i] = g_page_manager.stack[i + 1];
            }
            g_page_manager.stack[UI_PAGE_STACK_CAPACITY - 1] =
                g_page_manager.current;
        }
    }
    ui_manager_switch(page);
}

bool ui_manager_adopt_precreated_page(ui_page_t page)
{
    int i;
    ui_page_t from = g_page_manager.current;
    perf_profile_page_switch_sample_t sample = {
        .started_us = 0,
        .from_id = (uint32_t)from,
        .from_name = ui_manager_page_name(from),
        .to_id = (uint32_t)page,
        .to_name = ui_manager_page_name(page),
        .route = "ADOPT",
        .leave_action = "NONE",
        .enter_action = "ADOPT",
    };
    bool profile_enabled;
    uint64_t total_started_us = 0;
    uint64_t phase_started_us = 0;

    if (page == from || !ui_manager_page_is_registered(page)) return false;
    profile_enabled = perf_profile_is_enabled();
    if (profile_enabled) {
        total_started_us = app_clock_monotonic_us();
        sample.started_us = total_started_us;
        phase_started_us = total_started_us;
    }
    if (from != UI_PAGE_INVALID) {
        if (g_page_manager.stack_top < UI_PAGE_STACK_CAPACITY - 1) {
            g_page_manager.stack[++g_page_manager.stack_top] = from;
        } else {
            for (i = 0; i < UI_PAGE_STACK_CAPACITY - 1; i++) {
                g_page_manager.stack[i] = g_page_manager.stack[i + 1];
            }
            g_page_manager.stack[UI_PAGE_STACK_CAPACITY - 1] = from;
        }
    }
    if (profile_enabled) {
        sample.prepare_us = ui_manager_profile_elapsed_us(phase_started_us);
        phase_started_us = app_clock_monotonic_us();
    }
    ui_manager_notify_page_switch(from, page);
    if (profile_enabled) {
        sample.notify_us = ui_manager_profile_elapsed_us(phase_started_us);
        phase_started_us = app_clock_monotonic_us();
    }
    sample.leave_action = destroy_current_page();
    if (profile_enabled) {
        sample.leave_us = ui_manager_profile_elapsed_us(phase_started_us);
        phase_started_us = app_clock_monotonic_us();
    }
    g_page_manager.current = page;
    if (g_page_registry[page].cache_policy == UI_PAGE_RETAINED) {
        g_page_cache_ready[page] = true;
    }
    if (profile_enabled) {
        sample.commit_us = ui_manager_profile_elapsed_us(phase_started_us);
        sample.total_us = ui_manager_profile_elapsed_us(total_started_us);
        perf_profile_report_page_switch(&sample);
    }
    return true;
}

bool ui_manager_pop_page(void)
{
    ui_page_t previous_page;

    if (g_page_manager.stack_top < 0)
    {
        // 栈为空时，判断是否需要返回主页面
        if (g_page_manager.current != UI_PAGE_INVALID &&
            g_page_manager.current != UI_PAGE_MAIN) {
            ui_manager_switch(UI_PAGE_MAIN);
            return true;
        }
        return false;  
    }

    // 出栈
    previous_page = g_page_manager.stack[g_page_manager.stack_top--];
    ui_manager_switch(previous_page);
    return true;
}

void ui_manager_clear_stack(void)
{
    g_page_manager.stack_top = -1;
}

bool ui_manager_invalidate_page_cache(ui_page_t page)
{
    const ui_page_registration_t *registration;

    if (!ui_manager_page_is_registered(page) ||
        page == g_page_manager.current) {
        return false;
    }

    registration = &g_page_registry[page];
    if (registration->cache_policy != UI_PAGE_RETAINED ||
        registration->destroy == NULL) {
        return false;
    }

    registration->destroy();
    g_page_cache_ready[page] = false;
    return true;
}

void ui_manager_invalidate_all_page_caches(void)
{
    for (ui_page_t page = UI_PAGE_BOOT_ANIM; page < UI_PAGE_COUNT; page++) {
        (void)ui_manager_invalidate_page_cache(page);
    }
}

bool ui_manager_prewarm_page(ui_page_t page)
{
    const ui_page_registration_t *registration;
    bool profile_enabled;
    uint64_t started_us = 0;

    if (!ui_manager_page_is_registered(page) ||
        page == g_page_manager.current ||
        g_page_cache_ready[page]) {
        return false;
    }

    registration = &g_page_registry[page];
    if (registration->cache_policy != UI_PAGE_RETAINED ||
        registration->create == NULL ||
        registration->suspend == NULL) {
        return false;
    }

    profile_enabled = perf_profile_is_enabled();
    if (profile_enabled) {
        started_us = app_clock_monotonic_us();
    }

    registration->create(lv_scr_act());
    registration->suspend();
    g_page_cache_ready[page] = true;

    if (profile_enabled) {
        perf_profile_report_event_us(
            ui_manager_page_name(page), "PREWARM",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
    return true;
}


const char *ui_manager_page_name(ui_page_t page)
{
    if (page < UI_PAGE_BOOT_ANIM || page >= UI_PAGE_COUNT ||
        g_page_names[page] == NULL) {
        return "INVALID";
    }
    return g_page_names[page];
}

// 读取当前页
ui_page_t ui_manager_get_current_page(void) {
    return g_page_manager.current;
}
