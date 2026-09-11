#include "counting_ui_runtime.h"
#include "ui_object_utils.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_resources/lv_img_init.h" 
#include "user_cfg.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_01_main_detail.h"
#include "un260/lv_core/page_02_list.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_system/ui_text.h"
#include "un260/currency/currency_state.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/counting/counting_reject_reason.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_update_batch.h"
#include "aic_ui/perf_stats.h"
// 全局变量定义

static lv_timer_t *s_sim_timer = NULL;
static lv_timer_t *s_safe_reset_timer = NULL;
static bool g_count_end_anim_pending = false;
static bool g_count_end_anim_armed = false;
static char g_count_end_anim_text[128];
#define COUNTING_SIM_SN_LENGTH 11
#define COUNTING_SIM_MAX_ITEMS COUNTING_DATA_MAX_ITEMS

//金额模拟
static const int USD_value[] = { 100,50,20,10,5,2,1 };
static const int CNY_value[] = { 100,50,20,10,5,1 };
static const int EUR_value[] = { 200,100,50,20,10,5 };
static const int GBP_value[] = { 50,20,10,5,2,1 };
static const int KRW_value[] = { 500,400,300,200,100,50,10,5,1 };
static const int EGP_value[] = { 200, 100, 50, 20, 10, 5, 1 };
static const int ISK_value[] = { 10000, 5000, 2000, 1000, 500, 100 };
static const int PHP_value[] = { 1000, 500, 200, 100, 50, 20 };
static const int SOS_value[] = { 1000, 500, 100, 50, 20, 10, 5, 1 };
static const int TRY_value[] = { 200, 100, 50, 20, 10, 5, 1 };
static const int AED_value[] = { 1000, 500, 200, 100, 50, 20, 10, 5};
static const int SAR_value[] = { 500, 200, 100, 50, 20, 10, 5, 1 };
static const int OMR_value[] = { 5000, 2000, 1000, 500, 100, 50 , 10 };
static const int QAR_value[] = { 500, 200, 100, 50,  10, 5, 1 };
static const int MAD_value[] = { 200, 100, 50, 20};
static const int DZD_value[] = { 2000, 1000, 500, 200};
static const int INR_value[] = { 500, 200, 100, 50, 20, 10};
static const int PKR_value[] = { 5000, 1000, 500, 100, 75, 50, 20 ,10};
static const int IQD_value[] = { 50000 , 25000, 10000, 5000, 1000, 500, 250 ,100 ,20 ,50 ,1};

static const int USD_value_num = sizeof(USD_value) / sizeof(USD_value[0]);
static const int CNY_value_num = sizeof(CNY_value) / sizeof(CNY_value[0]);
static const int EUR_value_num = sizeof(EUR_value) / sizeof(EUR_value[0]);
static const int GBP_value_num = sizeof(GBP_value) / sizeof(GBP_value[0]);
static const int KRW_value_num = sizeof(KRW_value) / sizeof(KRW_value[0]);
static const int EGP_value_num = sizeof(EGP_value) / sizeof(EGP_value[0]);
static const int ISK_value_num = sizeof(ISK_value) / sizeof(ISK_value[0]);
static const int PHP_value_num = sizeof(PHP_value) / sizeof(PHP_value[0]);
static const int SOS_value_num = sizeof(SOS_value) / sizeof(SOS_value[0]);
static const int TRY_value_num = sizeof(TRY_value) / sizeof(TRY_value[0]);
static const int AED_value_num = sizeof(AED_value) / sizeof(AED_value[0]);
static const int SAR_value_num = sizeof(SAR_value) / sizeof(SAR_value[0]);
static const int OMR_value_num = sizeof(OMR_value) / sizeof(OMR_value[0]);
static const int QAR_value_num = sizeof(QAR_value) / sizeof(QAR_value[0]);
static const int MAD_value_num = sizeof(MAD_value) / sizeof(MAD_value[0]);
static const int DZD_value_num = sizeof(DZD_value) / sizeof(DZD_value[0]);
static const int INR_value_num = sizeof(INR_value) / sizeof(INR_value[0]);
static const int PKR_value_num = sizeof(PKR_value) / sizeof(PKR_value[0]);
static const int IQD_value_num = sizeof(IQD_value) / sizeof(IQD_value[0]);
static bool sim_append_generated_serials(counting_sim_t *sim_data, int new_total)
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char **new_sn_str;
    int old_capacity;

    if (sim_data == NULL || new_total <= 0 ||
        new_total > COUNTING_SIM_MAX_ITEMS || sim_data->denom_number == 0) {
        return false;
    }

    old_capacity = sim_data->sn_str != NULL ? sim_data->sn_capacity : 0;
    if (old_capacity < 0 || old_capacity > new_total) {
        return false;
    }
    if (new_total == old_capacity) {
        return true;
    }

    new_sn_str = calloc((size_t)new_total, sizeof(*new_sn_str));
    if (new_sn_str == NULL) {
        return false;
    }
    if (old_capacity > 0) {
        memcpy(new_sn_str, sim_data->sn_str,
               sizeof(*new_sn_str) * (size_t)old_capacity);
    }

    for (int i = old_capacity; i < new_total; i++) {
        new_sn_str[i] = malloc(COUNTING_SIM_SN_LENGTH + 1U);
        if (new_sn_str[i] == NULL) {
            for (int j = old_capacity; j < i; j++) {
                free(new_sn_str[j]);
            }
            free(new_sn_str);
            return false;
        }
        for (int j = 0; j < COUNTING_SIM_SN_LENGTH; j++) {
            new_sn_str[i][j] = charset[lv_rand(0, sizeof(charset) - 2U)];
        }
        new_sn_str[i][COUNTING_SIM_SN_LENGTH] = '\0';
        sim_data->denom_mix[i] =
            sim_data->denom[lv_rand(0, sim_data->denom_number - 1)].value;
    }

    free(sim_data->sn_str);
    sim_data->sn_str = new_sn_str;
    sim_data->sn_capacity = new_total;
    return true;
}

//获取obj对象
lv_obj_t* find_obj_by_name(const char* name, ui_element_t* page_cfg_obj, int len) {
    if (!name || !page_cfg_obj) {
#if LV_DEBUG
        printf("find_obj_by_name: name or page_cfg_obj is NULL!\n");
#endif
        return NULL;
    }

    for (int i = 0; i < len; i++) {
        if (strcmp(page_cfg_obj[i].obj_name, name) == 0) {
            if (page_cfg_obj[i].obj_ref && lv_obj_is_valid(page_cfg_obj[i].obj_ref)) {
                return page_cfg_obj[i].obj_ref;
            }
            else {
#if LV_DEBUG
                printf("find_obj_by_name: obj_ref invalid for name %s\n", name);
#endif
                return NULL;
            }
        }
    }

#if LV_DEBUG
    printf("find_obj_by_name: name %s not found\n", name);
#endif
    return NULL;
}



static void label_set_text_if_changed(lv_obj_t* label, const char* text)
{
    const char* current;

    if (!label || !lv_obj_is_valid(label) ||
        !lv_obj_check_type(label, &lv_label_class)) {
        return;
    }

    text = text ? text : "";
    current = lv_label_get_text(label);
    if (current && strcmp(current, text) == 0) {
        return;
    }
    lv_label_set_text(label, text);
}



//刷新字符串
void update_label_by_name(ui_element_t* page_cfg_obj, int len,const char* name, const char* fmt, ...) {
    lv_obj_t* label = find_obj_by_name(name, page_cfg_obj, len);
    if (!label || !lv_obj_is_valid(label) || !lv_obj_check_type(label, &lv_label_class)) return;

    static char buf[64];
    va_list args;
    va_start(args, fmt);
    lv_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    label_set_text_if_changed(label, buf);
}

void sim_data_init(void)
{
    char curr_code[4];

    currency_state_get_active_code(curr_code);
    printf("Current currency enum: %d\n", currency_state_active_currency());

    counting_sim_t* sim_data = counting_data_mutable();
    counting_data_clear_serials(sim_data);
    counting_data_clear_errors(sim_data);
    memset(sim_data, 0, sizeof(counting_sim_t));
    const int* arr = NULL;
    int count = 0;

    switch (currency_state_code_to_item(curr_code))
    {
    case CURR_USD_ITEM:
        arr = USD_value;
        count = USD_value_num;
        break;
    case CURR_CNY_ITEM:
        arr = CNY_value;
        count = CNY_value_num;
        break;
    case CURR_GBP_ITEM:
        arr = GBP_value;
        count = GBP_value_num;
        break;
    case CURR_EUR_ITEM:
        arr = EUR_value;
        count = EUR_value_num;
        break;
    case CURR_KRW_ITEM:
        arr = KRW_value;
        count = KRW_value_num;
        break;
    case CURR_EGP_ITEM:
        arr = EGP_value;
        count = EGP_value_num;
        break;
    case CURR_ISK_ITEM:
        arr = ISK_value;
        count = ISK_value_num;
        break;
    case CURR_PHP_ITEM:
        arr = PHP_value;
        count = PHP_value_num;
        break;
    case CURR_SOS_ITEM:
        arr = SOS_value;
        count = SOS_value_num;
        break;
    case CURR_TRY_ITEM:
        arr = TRY_value;
        count = TRY_value_num;
        break;
    case CURR_AED_ITEM:
        arr = AED_value;
        count = AED_value_num;
        break;
    case CURR_SAR_ITEM:
        arr = SAR_value;
        count = SAR_value_num;
        break;
    case CURR_OMR_ITEM:
        arr = OMR_value;
        count = OMR_value_num;
        break;
    case CURR_QAR_ITEM:
        arr = QAR_value;
        count = QAR_value_num;
        break;
    case CURR_MAD_ITEM:
        arr = MAD_value;
        count = MAD_value_num;
        break;
    case CURR_DZD_ITEM:
        arr = DZD_value;
        count = DZD_value_num;
        break;
    case CURR_INR_ITEM:
        arr = INR_value;
        count = INR_value_num;
        break;
    case CURR_PKR_ITEM:
        arr = PKR_value;
        count = PKR_value_num;
        break;
    case CURR_IQD_ITEM:
        arr = IQD_value;
        count = IQD_value_num;
        break;
    default:
        arr = USD_value;
        count = USD_value_num;
        break;
    }
    for (int i = 0; i < count; i++)
    {
        sim_data->denom[i].value = arr[i];
        sim_data->denom[i].amount = 0;
        sim_data->denom[i].pcs = 0;
    }
    sim_data->denom_number = count;
    sim_data->total_amount = 0;
    sim_data->total_pcs = 0;
    sim_data->last_total_amount = 0;
    sim_data->last_total_pcs = 0;
    sim_data->last_valid_pcs = 0;
    sim_data->last_issue_pcs = 0;
    sim_data->last_suspect_pcs = 0;
    sim_data->last_damaged_pcs = 0;
    sim_data->is_paused = false;  // 初始化暂停标志为false

}


static bool sim_reject_detail_update(counting_sim_t *sim_data)
{
    if (!counting_data_ensure_error_capacity(sim_data, 1)) {
        sim_data->err_num = 0;
        sim_data->err_expected = 0;
        return false;
    }

    sim_data->err_code[0] = 0x15; /* UV */
    sim_data->err_pcs[0] = 1;
    sim_data->err_num = 1;
    sim_data->err_expected = 1;
    return true;
}

static void sim_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    if (!page_01_main_is_visible()) return;
    counting_sim_t* sim_data = counting_data_mutable();
    
    if (sim_data->denom_number <= 0)
    {
#if LV_DEBUG
        printf("denom_number is invalid: %d\n", sim_data->denom_number);
#endif
        return;
    }
    int ridx = lv_rand(0, sim_data->denom_number - 1);
    int delta = lv_rand(1, 5);
    if (delta > UINT16_MAX - sim_data->denom[ridx].pcs) {
        sim_data->denom[ridx].pcs = UINT16_MAX;
    } else {
        sim_data->denom[ridx].pcs += (uint16_t)delta;
    }
    sim_data->denom[ridx].amount = sim_data->denom[ridx].value * sim_data->denom[ridx].pcs;
    
    int total_pcs = 0;
    float total_amount = 0;
    for (int i = 0; i < sim_data->denom_number; i++)
    {
        total_pcs += sim_data->denom[i].pcs;
        total_amount += sim_data->denom[i].amount;
    }
    if (total_pcs > sim_data->total_pcs)
    {
        if (!sim_append_generated_serials(sim_data, total_pcs)) {
#if LV_DEBUG
            printf("Failed to grow simulated serial data to %d\n", total_pcs);
#endif
            return;
        }
    }
    
    sim_data->total_pcs = total_pcs;
    sim_data->total_amount = total_amount;
    if (!sim_reject_detail_update(sim_data)) {
#if LV_DEBUG
        printf("Failed to update simulated reject detail\n");
#endif
    }

    ui_refresh_main_page();
}

static void safe_reset_cb(lv_timer_t* timer)
{
    if (timer == s_safe_reset_timer) {
        s_safe_reset_timer = NULL;
    }
    sim_reset_counting_result(counting_data_mutable());
}

static void sim_timer_stop(void)
{
    if (s_sim_timer == NULL) {
        return;
    }

    lv_timer_del(s_sim_timer);
    s_sim_timer = NULL;
}

static void safe_reset_timer_stop(void)
{
    if (s_safe_reset_timer == NULL) {
        return;
    }

    lv_timer_del(s_safe_reset_timer);
    s_safe_reset_timer = NULL;
}

static void safe_reset_timer_schedule(void)
{
    safe_reset_timer_stop();
    s_safe_reset_timer = lv_timer_create(safe_reset_cb, 5, NULL);
    if (s_safe_reset_timer != NULL) {
        lv_timer_set_repeat_count(s_safe_reset_timer, 1);
    }
}

void start_counting_sim(void)
{
    counting_sim_t *sim_data = counting_data_mutable();

    if (s_sim_timer != NULL) {
        if (sim_data->is_paused) {
            resume_counting_sim();
        }
        return;
    }

    sim_data_init();
    s_sim_timer = lv_timer_create(sim_timer_cb, 200, NULL);
}

void stop_counting_sim(void)
{
    if (!s_sim_timer) return;
    
    sim_timer_stop();
    
    counting_sim_t* sim_data = counting_data_mutable();
    sim_data->is_paused = false;  // 重置暂停标志

    safe_reset_timer_schedule();
}
//处理金额格式
void format_amount_with_comma(char* dest, size_t dest_size, float amount) {
    char temp[32];
    int len;
    int dest_index = 0;

    if (dest == NULL || dest_size == 0) {
        return;
    }

    snprintf(temp, sizeof(temp), "%.0f", amount);
    len = (int)strlen(temp);
    
    if (len <= 3) {
        lv_snprintf(dest, dest_size, "%s", temp);
        return;
    }

    for (int i = 0; i < len; i++) {
        if ((size_t)dest_index + 1 >= dest_size) break;

        dest[dest_index++] = temp[i];
        if (i < len - 1 && (len - i - 1) % 3 == 0) {
            if ((size_t)dest_index + 1 < dest_size) {
                dest[dest_index++] = ',';
            }
        }
    }

    dest[dest_index] = '\0';
}

void ui_count_end_anim_cancel(void)
{
    /* 清掉上一轮残留的结束动画请求，避免新会话被误触发 */
    g_count_end_anim_pending = false;
    g_count_end_anim_armed = false;
    g_count_end_anim_text[0] = '\0';
}

void ui_count_end_anim_begin(const char *result_text)
{
    /* 记录结束动画请求，交给下一轮主循环处理 */
    if (result_text && result_text[0] != '\0') {
        lv_snprintf(g_count_end_anim_text, sizeof(g_count_end_anim_text), "%s", result_text);
    } else {
        g_count_end_anim_text[0] = '\0';
    }

    g_count_end_anim_pending = true;
    g_count_end_anim_armed = false;
}

void ui_count_end_anim_poll(void)
{
    if (!g_count_end_anim_pending) {
        return;
    }

    if (!g_count_end_anim_armed) {
        /* 先挂一轮，避开当前这次页面刷新 */
        g_count_end_anim_armed = true;
        return;
    }

    /* 第二轮主循环再真正切到结束态 */
    g_count_end_anim_pending = false;
    g_count_end_anim_armed = false;
    smart_island_notify_count_end(g_count_end_anim_text[0] ? g_count_end_anim_text : NULL);
    smart_island_refresh_summary();
}

void cleanup_counting_sim(void)
{
    counting_sim_t *sim_data = counting_data_mutable();

    sim_timer_stop();
    safe_reset_timer_stop();

    ui_count_end_anim_cancel();


    counting_data_clear_errors(sim_data);
    counting_data_clear_serials(sim_data);
    memset(sim_data, 0, sizeof(*sim_data));
}

void pause_counting_sim(void)
{
    if (!s_sim_timer) return;
    
    counting_sim_t* sim_data = counting_data_mutable();
    if (!sim_data->is_paused) {
        lv_timer_pause(s_sim_timer);
        sim_data->is_paused = true;  
#if LV_DEBUG
        printf("计数模拟已暂停\n");
#endif
    }
}

void resume_counting_sim(void)
{
    if (!s_sim_timer) return;
    
    counting_sim_t* sim_data = counting_data_mutable();
    if (sim_data->is_paused) {
        lv_timer_resume(s_sim_timer);
        sim_data->is_paused = false; 
#if LV_DEBUG
        printf("计数模拟已恢复\n");
#endif
    }
}

static void sim_reset_counting_data(counting_sim_t *sim_data,
                                    bool clear_denominations)
{
    int denom_count;

    if (sim_data == NULL) return;
    counting_data_clear_errors(sim_data);
    counting_data_clear_serials(sim_data);

    if (clear_denominations) {
        memset(sim_data->denom, 0, sizeof(sim_data->denom));
        sim_data->denom_number = 0;
        sim_data->last_total_pcs = 0;
        sim_data->last_total_amount = 0.0f;
        sim_data->last_valid_pcs = 0;
        sim_data->last_issue_pcs = 0;
        sim_data->last_suspect_pcs = 0;
        sim_data->last_damaged_pcs = 0;
    } else {
        denom_count = sim_data->denom_number;
        if (denom_count >
            (int)(sizeof(sim_data->denom) / sizeof(sim_data->denom[0]))) {
            denom_count =
                (int)(sizeof(sim_data->denom) / sizeof(sim_data->denom[0]));
        }
        for (int i = 0; i < denom_count; i++) {
            sim_data->denom[i].pcs = 0;
            sim_data->denom[i].amount = 0;
        }
    }

    sim_data->total_pcs = 0;
    sim_data->total_amount = 0.0f;
    if (!counting_data_monetary_result_supported(sim_data)) {
        /* Unsupported MULTI last totals must not reappear as a single-currency
         * summary or become the next ADD baseline after explicit clearing. */
        sim_data->last_total_pcs = 0;
        sim_data->last_total_amount = 0.0f;
    }
    counting_data_reset_result_scope(sim_data);
    sim_data->err_expected = 0;
    smart_island_clear_count_analysis();
    page_01_main_scroll_reset();
    page_02_list_report_reset();
    ui_refresh_main_page();
    smart_island_refresh_summary();
}

void sim_reset_for_currency(counting_sim_t* sim_data)
{
    sim_reset_counting_data(sim_data, true);
}

void sim_reset_counting_result(counting_sim_t* sim_data)
{
    if (sim_data == NULL) return;
    sim_reset_counting_data(sim_data, false);
}
