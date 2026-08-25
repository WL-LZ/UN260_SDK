#include "un260/lv_core/page_02_list.h"
#include "un260/lv_resources/lv_image_declear.h" 
#include "un260/lv_resources/lv_img_init.h" 
#include "un260/currency/currency_state.h"
#include "un260/counting/counting_reject_reason.h"
#include "un260/counting/counting_data_store.h"
#include "lv_page_event.h"
#include "aic_ui/aic_ui.h"
#include "un260/lv_system/ui_object_utils.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/perf_stats.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t* list_page = NULL;

#define PAGE_02_DIRTY_SECTION_A (1U << PAGE_02_SECTION_A)
#define PAGE_02_DIRTY_SECTION_B (1U << PAGE_02_SECTION_B)
#define PAGE_02_DIRTY_SECTION_C (1U << PAGE_02_SECTION_C)
#define PAGE_02_DIRTY_CURRENCY  (1U << PAGE_02_SECTION_COUNT)
#define PAGE_02_DIRTY_ALL       (PAGE_02_DIRTY_SECTION_A | \
                                 PAGE_02_DIRTY_SECTION_B | \
                                 PAGE_02_DIRTY_SECTION_C | \
                                 PAGE_02_DIRTY_CURRENCY)

static uint32_t g_page_02_dirty = PAGE_02_DIRTY_ALL;
static char g_page_02_currency_code[4];

typedef struct {
    uint8_t curent_page;
    uint8_t total_page;
} page_02_report_status_t;

static page_02_report_status_t page_02_a_report_status;
static page_02_report_status_t page_02_b_report_status;
static page_02_report_status_t page_02_c_report_status;

// 添加长度变量
int page_02_list_len = 0;

#define PAGE_02_SCROLL_ROW_GAP          31
#define PAGE_02_SCROLL_ROW_Y_OFFSET     10
#define PAGE_02_SCROLL_COL_MAX          3
#define PAGE_02_SCROLL_POOL_MAX         (PAGE_02_C_ITEM + 1)
#define PAGE_02_SCROLL_EDGE_BUFFER      32

typedef struct {
    page_02_section_id_t section_id;
    lv_obj_t *container;
    lv_obj_t *spacer;
    lv_obj_t *cell[PAGE_02_SCROLL_POOL_MAX][PAGE_02_SCROLL_COL_MAX];
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    lv_coord_t col_x[PAGE_02_SCROLL_COL_MAX];
    lv_coord_t col_w[PAGE_02_SCROLL_COL_MAX];
    uint8_t page_size;
    uint8_t pool_row;
    uint8_t col_count;
    uint16_t total_row;
    uint16_t first_row;
    bool pressing;
    bool press_moved;
    lv_point_t press_point;
} page_02_scroll_section_t;

static page_02_scroll_section_t s_page_02_scroll_sections[PAGE_02_SECTION_COUNT];

static int page_02_a_valid_count_get(void); // 获取A区有效面额条数
static int page_02_b_valid_count_get(void); // 获取B区有效冠字号条数
static int page_02_b_nth_valid_index_get(int nth); // 获取B区第nth条有效数据索引
static page_02_scroll_section_t *page_02_scroll_section_get(page_02_section_id_t section_id); // 获取分区配置
static void page_02_scroll_section_init_config(void); // 初始化A/B/C滚动容器配置
static void page_02_scroll_section_style_init(lv_obj_t *obj); // 统一设置滚动容器样式
static lv_obj_t *page_02_scroll_cell_create(lv_obj_t *parent, lv_coord_t width); // 创建滚动单元标签
static void page_02_scroll_section_create(page_02_scroll_section_t *section); // 创建单个分区滚动容器
static void page_02_scroll_section_event_cb(lv_event_t *e); // 处理滚动与点击翻页
static void page_02_scroll_section_total_page_refresh(page_02_scroll_section_t *section); // 刷新分区总页数
static void page_02_scroll_section_spacer_refresh(page_02_scroll_section_t *section); // 刷新滚动内容高度
static void page_02_scroll_section_status_refresh(page_02_scroll_section_t *section); // 根据滚动位置刷新页码
static void page_02_scroll_section_row_bind(page_02_scroll_section_t *section, uint16_t pool_row, int data_index); // 绑定滚动行内容
static void page_02_scroll_section_visible_refresh(page_02_scroll_section_t *section); // 刷新可见行
static void page_02_scroll_section_sync_to_status(page_02_scroll_section_t *section, bool anim_en); // 根据页码同步滚动位置
static bool page_02_scroll_section_small_data(page_02_scroll_section_t *section); // 判断当前分区是否为小数据量
static uint16_t page_02_scroll_section_last_page_first_row_get(page_02_scroll_section_t *section); // 获取最后一页的起始行
static lv_coord_t page_02_scroll_section_real_bottom_scroll_y_get(page_02_scroll_section_t *section); // 获取真实内容底部的滚动位置
static lv_coord_t page_02_scroll_section_max_scroll_y_get(page_02_scroll_section_t *section); // 获取分区允许的最大滚动距离
static void page_02_a_page_refre(void);
static void page_02_b_page_refre(void);
static void page_02_c_page_refre(void);
static void page_02_curr_refre(void);
static void page_02_a_page_num_refre(void);
static void page_02_b_page_num_refre(void);
static void page_02_c_page_num_refre(void);

ui_element_t page_02_list_obj[] = {

    //////////////////////////////////////////////////////
  //***************    BG_IMG_LIST  *******************//
////////////////////////////////////////////////////////

    { "page_02_list_img.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 0, 0, 1280, 400, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

  //////////////////////////////////////////////////////
 //***************    BTN_LIST   *********************/
//////////////////////////////////////////////////////

    { "02_home_btn", LV_OBJ_TYPE_BUTTON,NULL,
        { 1154, 276, 101, 78, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         page_01_back_btn_event_cb, 0, NULL, NULL ,
         UI_BTN_STYLE_APPLE},

    { "02_print", LV_OBJ_TYPE_BUTTON,NULL,
        { 1154, 160, 101, 78, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         page_01_print_btn_event_cb, 0, NULL, NULL ,
         UI_BTN_STYLE_APPLE},
    { "02_a_up", LV_OBJ_TYPE_BUTTON,NULL,
        { 25, 79, 359, 141, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         page_03_a_up_event_cb, 0, NULL, NULL ,
         UI_BTN_STYLE_NO_FEEDBACK},
    { "02_a_down", LV_OBJ_TYPE_BUTTON,NULL,
        { 25, 220, 359, 141, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         page_03_a_down_event_cb, 0, NULL, NULL ,
         UI_BTN_STYLE_NO_FEEDBACK},
    { "02_b_up", LV_OBJ_TYPE_BUTTON,NULL,
        { 405, 79, 392, 141, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         page_03_b_up_event_cb, 0, NULL, NULL ,
         UI_BTN_STYLE_NO_FEEDBACK},
    { "02_b_down", LV_OBJ_TYPE_BUTTON,NULL,
        { 405, 220, 392, 141, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         page_03_b_down_event_cb, 0, NULL, NULL ,
         UI_BTN_STYLE_NO_FEEDBACK},
    { "02_c_up", LV_OBJ_TYPE_BUTTON,NULL,
        { 820, 79, 309, 141, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         page_03_c_up_event_cb, 0, NULL, NULL ,
         UI_BTN_STYLE_NO_FEEDBACK},
    { "02_c_down", LV_OBJ_TYPE_BUTTON,NULL,
        { 820, 220, 309, 141, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         page_03_c_down_event_cb, 0, NULL, NULL ,
         UI_BTN_STYLE_NO_FEEDBACK},

  //////////////////////////////////////////////////////
 //***************  IMAGE_LIST **********************//
//////////////////////////////////////////////////////

    { "page_02_home_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 1179, 289, 43, 43, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "page_01_print_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 1187, 180, 43, 43, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

  //////////////////////////////////////////////////////
 //***************  LABEL_LIST **********************//
//////////////////////////////////////////////////////

    { "02_list_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 610, 13, 70, 36, 112, 112, 112 },
        { "LIST", 80, 80, 80, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_CENTER },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_a_denom_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 58, 58, 70, 36, 255, 255, 255 },
        { "DENOM", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_a_pcs_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 171, 58, 70, 36, 255, 255, 255 },
        { "PCS", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_a_amount_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 267, 58, 80, 36, 255, 255, 255 },
        { "AMOUNT", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_b_no_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 444, 58, 70, 36, 255, 255, 255 },
        { "NO", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_b_sn_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 516, 58, 70, 36, 255, 255, 255 },
        { "SN", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_b_denom_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 679, 58, 70, 36, 255, 255, 255 },
        { "DENOM", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_c_no_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 834, 58, 70, 36, 255, 255, 255 },
        { "NO", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_c_denom_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 887, 58, 70, 36, 255, 255, 255 },
        { "PCS", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

    { "02_c_reject_title_label", LV_OBJ_TYPE_LABEL, NULL ,
        { 973, 58, 70, 36, 255, 255, 255 },
        { "REJECT", 255, 255, 255, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
         NULL, 0, NULL, NULL ,
         UI_BTN_STYLE_NONE},

//************  a_TOTAL ****************//
    { "02_a_denom_total", LV_OBJ_TYPE_LABEL, NULL,
      { 58, 337, 80, 36, 255, 255, 255 },
      { "TOTAL", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
      { 255, 18, 0, false },
      NULL, 0, 0, NULL,
      UI_BTN_STYLE_NONE },

    { "02_a_pcs_amount", LV_OBJ_TYPE_LABEL, NULL,
      { 171, 337, 70, 36, 255, 255, 255 },
      { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
      { 255, 18, 0, false },
      NULL, 0, 0, NULL,
      UI_BTN_STYLE_NONE },

    { "02_a_amount_total", LV_OBJ_TYPE_LABEL, NULL,
      { 267, 337, 80, 36, 255, 255, 255 },
      { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_LEFT },
      { 255, 18, 0, false },
      NULL, 0, 0, NULL,
      UI_BTN_STYLE_NONE },

    { "02_history_btn", LV_OBJ_TYPE_BUTTON, NULL,
      { 1149, 87, 110, 57, 255, 255, 255 },
      { "HISTORY", 33, 43, 54, &lv_font_instrument_sans_bold_16, LV_TEXT_ALIGN_CENTER },
      { 255, 18, 0, true },
      page_02_history_btn_event_cb, LV_EVENT_CLICKED, NULL, NULL,
      UI_BTN_STYLE_APPLE },

        { "02_a_page_refre", LV_OBJ_TYPE_LABEL, NULL,
          { 151, 371, 99, 27, 121, 150, 0 },
          { "1/2", 255, 255, 255, &lv_font_instrument_sans_medium_20, LV_TEXT_ALIGN_CENTER },
          { 255, 18, 0, false },
          NULL, 0, 0, NULL,
          UI_BTN_STYLE_NONE },
                
        { "02_b_page_refre", LV_OBJ_TYPE_LABEL, NULL,
          { 545, 371, 99, 27, 121, 150, 0 },
          { "1/2", 255, 255, 255, &lv_font_instrument_sans_medium_20, LV_TEXT_ALIGN_CENTER },
          { 255, 18, 0, false },
          NULL, 0, 0, NULL,
          UI_BTN_STYLE_NONE },
                
        { "02_c_page_refre", LV_OBJ_TYPE_LABEL, NULL,
          { 925, 371, 99, 27, 121, 150, 0 },
          { "200/200", 255, 255, 255, &lv_font_instrument_sans_medium_20, LV_TEXT_ALIGN_CENTER },
          { 255, 18, 0, false },
          NULL, 0, 0, NULL,
          UI_BTN_STYLE_NONE },

};

static int page_02_a_valid_count_get(void) // 获取A区有效面额条数
{
    int valid_count = 0;
    int denom_count = counting_data_current()->denom_number;

    if (denom_count > (int)(sizeof(counting_data_current()->denom) / sizeof(counting_data_current()->denom[0]))) {
        denom_count = (int)(sizeof(counting_data_current()->denom) / sizeof(counting_data_current()->denom[0]));
    }

    for (int i = 0; i < denom_count; i++) {
        if (counting_data_current()->denom[i].value > 0) {
            valid_count++;
        }
    }

    return valid_count;
}

static int page_02_b_valid_count_get(void) // 获取B区有效冠字号条数
{
    return counting_data_serial_valid_count(counting_data_current());
}

static int page_02_b_nth_valid_index_get(int nth) // 获取B区第nth条有效数据索引
{
    return counting_data_serial_nth_valid_index(counting_data_current(), nth);
}

static page_02_scroll_section_t *page_02_scroll_section_get(page_02_section_id_t section_id) // 获取分区配置
{
    if (section_id >= PAGE_02_SECTION_COUNT) return NULL;
    return &s_page_02_scroll_sections[section_id];
}

static void page_02_scroll_section_init_config(void) // 初始化A/B/C滚动容器配置
{
    memset(s_page_02_scroll_sections, 0, sizeof(s_page_02_scroll_sections));

    s_page_02_scroll_sections[PAGE_02_SECTION_A].section_id = PAGE_02_SECTION_A;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].x = 25;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].y = 79;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].w = 359;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].h = 248;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].col_x[0] = 47;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].col_x[1] = 146;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].col_x[2] = 242;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].col_w[0] = 80;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].col_w[1] = 70;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].col_w[2] = 100;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].page_size = PAGE_02_A_ITEM;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].pool_row = PAGE_02_A_ITEM + 1;
    s_page_02_scroll_sections[PAGE_02_SECTION_A].col_count = 3;

    s_page_02_scroll_sections[PAGE_02_SECTION_B].section_id = PAGE_02_SECTION_B;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].x = 405;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].y = 79;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].w = 392;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].h = 279;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].col_x[0] = 39;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].col_x[1] = 111;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].col_x[2] = 274;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].col_w[0] = 60;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].col_w[1] = 150;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].col_w[2] = 80;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].page_size = PAGE_02_B_ITEM;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].pool_row = PAGE_02_B_ITEM + 1;
    s_page_02_scroll_sections[PAGE_02_SECTION_B].col_count = 3;

    s_page_02_scroll_sections[PAGE_02_SECTION_C].section_id = PAGE_02_SECTION_C;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].x = 820;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].y = 79;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].w = 333;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].h = 279;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].col_x[0] = 14;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].col_x[1] = 67;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].col_x[2] = 153;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].col_w[0] = 45;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].col_w[1] = 70;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].col_w[2] = 170;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].page_size = PAGE_02_C_ITEM;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].pool_row = PAGE_02_C_ITEM + 1;
    s_page_02_scroll_sections[PAGE_02_SECTION_C].col_count = 3;
}

static void page_02_scroll_section_style_init(lv_obj_t *obj) // 统一设置滚动容器样式
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_pad_right(obj, 0, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static lv_obj_t *page_02_scroll_cell_create(lv_obj_t *parent, lv_coord_t width) // 创建滚动单元标签
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_size(label, width, 36);
    lv_obj_set_style_text_color(label, lv_color_hex(0x5D5D5D), 0);
    lv_obj_set_style_text_font(label, &lv_font_instrument_sans_medium_16, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, "");

    return label;
}

static void page_02_scroll_section_create(page_02_scroll_section_t *section) // 创建单个分区滚动容器
{
    if (section == NULL || list_page == NULL) return;

    section->container = lv_obj_create(list_page);
    lv_obj_remove_style_all(section->container);
    lv_obj_set_pos(section->container, section->x, section->y);
    lv_obj_set_size(section->container, section->w, section->h);
    lv_obj_set_scroll_dir(section->container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(section->container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(section->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(section->container, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(section->container, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(section->container, LV_OBJ_FLAG_CLICKABLE);
    page_02_scroll_section_style_init(section->container);
    lv_obj_add_event_cb(section->container, page_02_scroll_section_event_cb, LV_EVENT_PRESSED, section);
    lv_obj_add_event_cb(section->container, page_02_scroll_section_event_cb, LV_EVENT_PRESSING, section);
    lv_obj_add_event_cb(section->container, page_02_scroll_section_event_cb, LV_EVENT_RELEASED, section);
    lv_obj_add_event_cb(section->container, page_02_scroll_section_event_cb, LV_EVENT_PRESS_LOST, section);
    lv_obj_add_event_cb(section->container, page_02_scroll_section_event_cb, LV_EVENT_SCROLL, section);
    lv_obj_add_event_cb(section->container, page_02_scroll_section_event_cb, LV_EVENT_SCROLL_END, section);
    lv_obj_add_event_cb(section->container, page_02_scroll_section_event_cb, LV_EVENT_CLICKED, section);

    for (int row = 0; row < section->pool_row; row++) {
        for (int col = 0; col < section->col_count; col++) {
            section->cell[row][col] = page_02_scroll_cell_create(section->container, section->col_w[col]);
            lv_obj_set_pos(section->cell[row][col], section->col_x[col], PAGE_02_SCROLL_ROW_Y_OFFSET + row * PAGE_02_SCROLL_ROW_GAP);
        }
    }

    section->spacer = lv_obj_create(section->container);
    lv_obj_remove_style_all(section->spacer);
    lv_obj_set_size(section->spacer, 1, section->h);
    lv_obj_set_pos(section->spacer, 0, section->h - 1);
    lv_obj_clear_flag(section->spacer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(section->spacer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_background(section->spacer);
}

static void page_02_scroll_section_total_page_refresh(page_02_scroll_section_t *section) // 刷新分区总页数
{
    if (section == NULL) return;

    switch (section->section_id) {
    case PAGE_02_SECTION_A:
        section->total_row = page_02_a_valid_count_get();
        page_02_a_report_status.total_page = (section->total_row == 0) ? 1 : ((section->total_row + PAGE_02_A_ITEM - 1) / PAGE_02_A_ITEM);
        break;
    case PAGE_02_SECTION_B:
        section->total_row = page_02_b_valid_count_get();
        page_02_b_report_status.total_page = (section->total_row == 0) ? 1 : ((section->total_row + PAGE_02_B_ITEM - 1) / PAGE_02_B_ITEM);
        break;
    case PAGE_02_SECTION_C:
        section->total_row = counting_data_error_detail_count(counting_data_current());
        page_02_c_report_status.total_page = (section->total_row == 0) ? 1 : ((section->total_row + PAGE_02_C_ITEM - 1) / PAGE_02_C_ITEM);
        break;
    default:
        break;
    }
}

static void page_02_scroll_section_spacer_refresh(page_02_scroll_section_t *section) // 刷新滚动内容高度
{
    lv_coord_t content_h;
    lv_coord_t max_scroll_y;

    if (section == NULL || section->spacer == NULL) return;

    page_02_scroll_section_total_page_refresh(section);
    content_h = PAGE_02_SCROLL_ROW_Y_OFFSET + (lv_coord_t)section->total_row * PAGE_02_SCROLL_ROW_GAP;
    content_h += PAGE_02_SCROLL_EDGE_BUFFER;
    max_scroll_y = page_02_scroll_section_max_scroll_y_get(section);
    if (content_h < section->h + max_scroll_y) {
        content_h = section->h + max_scroll_y;
    }
    if (content_h < section->h) {
        content_h = section->h;
    }

    lv_obj_set_pos(section->spacer, 0, content_h - 1);
}

static bool page_02_scroll_section_small_data(page_02_scroll_section_t *section) // 判断当前分区是否为小数据量
{
    if (section == NULL) return false;
    return section->total_row <= section->page_size;
}

static uint16_t page_02_scroll_section_last_page_first_row_get(page_02_scroll_section_t *section) // 获取最后一页的起始行
{
    if (section == NULL) return 0;
    if (section->total_row == 0) return 0;
    if (section->total_row <= section->page_size) return 0;
    return (uint16_t)(((section->total_row - 1) / section->page_size) * section->page_size);
}

static lv_coord_t page_02_scroll_section_real_bottom_scroll_y_get(page_02_scroll_section_t *section) // 获取真实内容底部的滚动位置
{
    lv_coord_t content_h;

    if (section == NULL) return 0;

    content_h = PAGE_02_SCROLL_ROW_Y_OFFSET + (lv_coord_t)section->total_row * PAGE_02_SCROLL_ROW_GAP;
    content_h += PAGE_02_SCROLL_EDGE_BUFFER;
    if (content_h <= section->h) {
        return 0;
    }

    return content_h - section->h;
}

static lv_coord_t page_02_scroll_section_max_scroll_y_get(page_02_scroll_section_t *section) // 获取分区允许的最大滚动距离
{
    if (section == NULL) return 0;
    if (page_02_scroll_section_small_data(section)) return 0;

    return page_02_scroll_section_last_page_first_row_get(section) * PAGE_02_SCROLL_ROW_GAP
        + PAGE_02_SCROLL_EDGE_BUFFER;
}

static void page_02_scroll_section_row_bind(page_02_scroll_section_t *section, uint16_t pool_row, int data_index) // 绑定滚动行内容
{
    int actual_index;

    if (section == NULL || pool_row >= section->pool_row) return;

    if (data_index < 0 || data_index >= section->total_row) {
        for (int col = 0; col < section->col_count; col++) {
            lv_obj_add_flag(section->cell[pool_row][col], LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    for (int col = 0; col < section->col_count; col++) {
        lv_obj_clear_flag(section->cell[pool_row][col], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(section->cell[pool_row][col], section->col_x[col],
            PAGE_02_SCROLL_ROW_Y_OFFSET + data_index * PAGE_02_SCROLL_ROW_GAP);
        lv_obj_set_style_text_font(section->cell[pool_row][col], &lv_font_instrument_sans_medium_16, 0);
    }

    switch (section->section_id) {
    case PAGE_02_SECTION_A:
        lv_label_set_text_fmt(section->cell[pool_row][0], "%d", counting_data_current()->denom[data_index].value);
        lv_label_set_text_fmt(section->cell[pool_row][1], "%d", counting_data_current()->denom[data_index].pcs);
        lv_label_set_text_fmt(section->cell[pool_row][2], "%.0f", counting_data_current()->denom[data_index].amount);
        break;
    case PAGE_02_SECTION_B:
        actual_index = page_02_b_nth_valid_index_get(data_index);
        if (actual_index < 0) {
            for (int col = 0; col < section->col_count; col++) {
                lv_obj_add_flag(section->cell[pool_row][col], LV_OBJ_FLAG_HIDDEN);
            }
            return;
        }
        lv_label_set_text_fmt(section->cell[pool_row][0], "%d", data_index + 1);
        lv_label_set_text(section->cell[pool_row][1], counting_data_current()->sn_str[actual_index]);
        lv_label_set_text_fmt(section->cell[pool_row][2], "%d", counting_data_current()->denom_mix[actual_index]);
        break;
    case PAGE_02_SECTION_C:
        lv_label_set_text_fmt(section->cell[pool_row][0], "%d", data_index + 1);
        if (counting_data_current()->err_pcs != NULL) {
            lv_label_set_text_fmt(section->cell[pool_row][1], "%d", counting_data_current()->err_pcs[data_index]);
        } else {
            lv_label_set_text(section->cell[pool_row][1], "-");
        }
        if (counting_data_current()->err_code != NULL) {
            lv_label_set_text(section->cell[pool_row][2],
                              counting_reject_reason_get(counting_data_current()->err_code[data_index]));
        } else {
            lv_label_set_text(section->cell[pool_row][2], "Unknown Error");
        }
        break;
    default:
        break;
    }
}

static void page_02_scroll_section_visible_refresh(page_02_scroll_section_t *section) // 刷新可见行
{
    lv_coord_t scroll_top;
    uint16_t first_row;
    uint16_t last_page_first_row;

    if (section == NULL || section->container == NULL) return;

    page_02_scroll_section_spacer_refresh(section);
    scroll_top = lv_obj_get_scroll_top(section->container);
    if (scroll_top < 0) {
        scroll_top = 0;
    }
    first_row = (uint16_t)(scroll_top / PAGE_02_SCROLL_ROW_GAP);
    last_page_first_row = page_02_scroll_section_last_page_first_row_get(section);
    if (first_row > last_page_first_row) {
        first_row = last_page_first_row;
    }
    section->first_row = first_row;

    for (uint16_t row = 0; row < section->pool_row; row++) {
        page_02_scroll_section_row_bind(section, row, (int)first_row + row);
    }
}

static void page_02_scroll_section_status_refresh(page_02_scroll_section_t *section) // 根据滚动位置刷新页码
{
    uint8_t current_page;
    lv_coord_t scroll_top;
    lv_coord_t real_bottom_scroll_y;

    if (section == NULL) return;

    page_02_scroll_section_total_page_refresh(section);
    scroll_top = lv_obj_get_scroll_top(section->container);
    if (scroll_top < 0) {
        scroll_top = 0;
    }
    real_bottom_scroll_y = page_02_scroll_section_real_bottom_scroll_y_get(section);

    // 最后一页条目不足一整页时，到达真实底部就应进入最后一页
    if (!page_02_scroll_section_small_data(section)
        && scroll_top >= real_bottom_scroll_y) {
        current_page = (uint8_t)((section->total_row + section->page_size - 1) / section->page_size);
    } else {
        current_page = (section->first_row / section->page_size) + 1;
    }
    if (current_page < 1) current_page = 1;

    switch (section->section_id) {
    case PAGE_02_SECTION_A:
        if (current_page > page_02_a_report_status.total_page) current_page = page_02_a_report_status.total_page;
        page_02_a_report_status.curent_page = current_page;
        page_02_a_page_num_refre();
        break;
    case PAGE_02_SECTION_B:
        if (current_page > page_02_b_report_status.total_page) current_page = page_02_b_report_status.total_page;
        page_02_b_report_status.curent_page = current_page;
        page_02_b_page_num_refre();
        break;
    case PAGE_02_SECTION_C:
        if (current_page > page_02_c_report_status.total_page) current_page = page_02_c_report_status.total_page;
        page_02_c_report_status.curent_page = current_page;
        page_02_c_page_num_refre();
        break;
    default:
        break;
    }
}

static void page_02_scroll_section_sync_to_status(page_02_scroll_section_t *section, bool anim_en) // 根据页码同步滚动位置
{
    uint16_t first_row = 0;

    if (section == NULL || section->container == NULL) return;

    switch (section->section_id) {
    case PAGE_02_SECTION_A:
        if (page_02_a_report_status.curent_page > 0) {
            first_row = (page_02_a_report_status.curent_page - 1) * PAGE_02_A_ITEM;
        }
        break;
    case PAGE_02_SECTION_B:
        if (page_02_b_report_status.curent_page > 0) {
            first_row = (page_02_b_report_status.curent_page - 1) * PAGE_02_B_ITEM;
        }
        break;
    case PAGE_02_SECTION_C:
        if (page_02_c_report_status.curent_page > 0) {
            first_row = (page_02_c_report_status.curent_page - 1) * PAGE_02_C_ITEM;
        }
        break;
    default:
        break;
    }

    if (first_row > page_02_scroll_section_last_page_first_row_get(section)) {
        first_row = page_02_scroll_section_last_page_first_row_get(section);
    }

    lv_obj_scroll_to_y(section->container, first_row * PAGE_02_SCROLL_ROW_GAP, anim_en ? LV_ANIM_ON : LV_ANIM_OFF);
    page_02_scroll_section_visible_refresh(section);
    page_02_scroll_section_status_refresh(section);
}

static void page_02_scroll_section_event_cb(lv_event_t *e) // 处理滚动与点击翻页
{
    lv_event_code_t code;
    page_02_scroll_section_t *section;

    if (e == NULL) return;

    code = lv_event_get_code(e);
    section = lv_event_get_user_data(e);
    if (section == NULL) return;

    if (code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_event_get_indev(e);
        if (indev == NULL) return;
        lv_indev_get_point(indev, &section->press_point);
        section->pressing = true;
        section->press_moved = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_event_get_indev(e);
        lv_point_t point;
        lv_coord_t delta_x;
        lv_coord_t delta_y;

        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        delta_x = LV_ABS(point.x - section->press_point.x);
        delta_y = LV_ABS(point.y - section->press_point.y);
        if (delta_x > 8 || delta_y > 8) {
            section->press_moved = true;
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        section->pressing = false;
        return;
    }

    if (code == LV_EVENT_SCROLL) {
        page_02_scroll_section_visible_refresh(section);
        page_02_scroll_section_status_refresh(section);
        return;
    }

    if (code == LV_EVENT_SCROLL_END) {
        if (section->pressing) {
            page_02_scroll_section_visible_refresh(section);
            page_02_scroll_section_status_refresh(section);
            return;
        }
        if (page_02_scroll_section_small_data(section)) {
            lv_obj_scroll_to_y(section->container, 0, LV_ANIM_ON);
            page_02_scroll_section_visible_refresh(section);
            page_02_scroll_section_status_refresh(section);
            return;
        }
        if (section->section_id == PAGE_02_SECTION_A ||
            section->section_id == PAGE_02_SECTION_C) {
            lv_coord_t scroll_top;
            lv_coord_t last_page_scroll_y;

            scroll_top = lv_obj_get_scroll_top(section->container);
            last_page_scroll_y = (lv_coord_t)page_02_scroll_section_last_page_first_row_get(section) * PAGE_02_SCROLL_ROW_GAP;
            if (scroll_top > last_page_scroll_y) {
                page_02_scroll_section_status_refresh(section);
                page_02_scroll_section_sync_to_status(section, true);
                return;
            }
        }
        if (section->section_id == PAGE_02_SECTION_B) {
            lv_coord_t scroll_top;
            lv_coord_t last_page_scroll_y;

            scroll_top = lv_obj_get_scroll_top(section->container);
            last_page_scroll_y = (lv_coord_t)page_02_scroll_section_last_page_first_row_get(section) * PAGE_02_SCROLL_ROW_GAP;
            if (scroll_top > last_page_scroll_y) {
                page_02_scroll_section_status_refresh(section);
                page_02_scroll_section_sync_to_status(section, true);
                return;
            }
        }
        page_02_scroll_section_visible_refresh(section);
        page_02_scroll_section_status_refresh(section);
        return;
    }

    if (code == LV_EVENT_CLICKED) {
        section->press_moved = false;
        return; //A/B/C都只保留滑动，不再通过点击翻页
    }
}

static void page_02_a_page_refre(void)
{
    update_label_by_name(page_02_list_obj, page_02_list_len,
                         "02_a_pcs_amount", "%d", counting_data_current()->total_pcs);
    update_label_by_name(page_02_list_obj, page_02_list_len,
                         "02_a_amount_total", "%.0f", counting_data_current()->total_amount);
    page_02_list_section_refresh(PAGE_02_SECTION_A);
}

static void page_02_b_page_refre(void)
{
    if (page_02_list_len <= 0) return;
    page_02_list_section_refresh(PAGE_02_SECTION_B);
}

static void page_02_c_page_refre(void)
{
    if (page_02_list_len <= 0) return;
    page_02_list_section_refresh(PAGE_02_SECTION_C);
}

static void page_02_curr_refre(void)
{
    char curr_code[4];

    currency_state_get_active_code(curr_code);
    update_label_by_name(page_02_list_obj, page_02_list_len,
                         "02_page_curr", "%s", curr_code);
}

static void page_02_a_page_num_refre(void)
{
    char page_text[16];

    snprintf(page_text, sizeof(page_text), "%d/%d",
             page_02_a_report_status.curent_page,
             page_02_a_report_status.total_page);
    update_label_by_name(page_02_list_obj, page_02_list_len,
                         "02_a_page_refre", "%s", page_text);
}

static void page_02_b_page_num_refre(void)
{
    char page_text[16];
    int valid_total = page_02_b_valid_count_get();

    page_02_b_report_status.total_page = valid_total == 0
        ? 1
        : (valid_total + PAGE_02_B_ITEM - 1) / PAGE_02_B_ITEM;
    if (page_02_b_report_status.curent_page < 1) {
        page_02_b_report_status.curent_page = 1;
    } else if (page_02_b_report_status.curent_page >
               page_02_b_report_status.total_page) {
        page_02_b_report_status.curent_page = page_02_b_report_status.total_page;
    }

    snprintf(page_text, sizeof(page_text), "%d/%d",
             page_02_b_report_status.curent_page,
             page_02_b_report_status.total_page);
    update_label_by_name(page_02_list_obj, page_02_list_len,
                         "02_b_page_refre", "%s", page_text);
}

static void page_02_c_page_num_refre(void)
{
    char page_text[16];

    snprintf(page_text, sizeof(page_text), "%d/%d",
             page_02_c_report_status.curent_page,
             page_02_c_report_status.total_page);
    update_label_by_name(page_02_list_obj, page_02_list_len,
                         "02_c_page_refre", "%s", page_text);
}

void page_02_list_section_refresh(page_02_section_id_t section_id) // 刷新指定分区滚动内容
{
    page_02_scroll_section_t *section = page_02_scroll_section_get(section_id);

    if (section == NULL || section->container == NULL || !lv_obj_is_valid(section->container)) return;
    page_02_scroll_section_visible_refresh(section);
    page_02_scroll_section_status_refresh(section);
}

void page_02_list_section_refresh_all(void) // 刷新全部分区滚动内容
{
    for (int i = 0; i < PAGE_02_SECTION_COUNT; i++) {
        page_02_list_section_refresh((page_02_section_id_t)i);
    }
}

static bool page_02_list_is_visible(void)
{
    return list_page != NULL && lv_obj_is_valid(list_page) &&
           !lv_obj_has_flag(list_page, LV_OBJ_FLAG_HIDDEN);
}

void page_02_list_section_mark_dirty(page_02_section_id_t section_id)
{
    if (section_id < PAGE_02_SECTION_COUNT) {
        g_page_02_dirty |= (1U << section_id);
    }
}

void page_02_list_section_data_ready(page_02_section_id_t section_id)
{
    page_02_report_status_t *status = NULL;
    int item_count;
    int page_size;

    switch (section_id) {
    case PAGE_02_SECTION_A:
        status = &page_02_a_report_status;
        item_count = page_02_a_valid_count_get();
        page_size = PAGE_02_A_ITEM;
        break;
    case PAGE_02_SECTION_B:
        status = &page_02_b_report_status;
        item_count = page_02_b_valid_count_get();
        page_size = PAGE_02_B_ITEM;
        break;
    case PAGE_02_SECTION_C:
        status = &page_02_c_report_status;
        item_count = counting_data_error_detail_count(counting_data_current());
        page_size = PAGE_02_C_ITEM;
        break;
    default:
        return;
    }

    status->curent_page = 1;
    status->total_page = item_count == 0 ? 1
        : (item_count + page_size - 1) / page_size;

    page_02_list_section_mark_dirty(section_id);
    if (!page_02_list_is_visible()) {
        return;
    }

    switch (section_id) {
    case PAGE_02_SECTION_A:
        page_02_a_page_refre();
        page_02_a_page_num_refre();
        break;
    case PAGE_02_SECTION_B:
        page_02_b_page_refre();
        page_02_b_page_num_refre();
        break;
    case PAGE_02_SECTION_C:
        page_02_c_page_refre();
        page_02_c_page_num_refre();
        break;
    default:
        break;
    }
    g_page_02_dirty &= ~(1U << section_id);
}

void page_02_list_report_reset(void)
{
    int sn_count = counting_data_serial_valid_count(counting_data_current());
    int error_count = counting_data_error_detail_count(counting_data_current());

    page_02_a_report_status.curent_page = 1;
    page_02_a_report_status.total_page = counting_data_current()->denom_number == 0
        ? 1 : (counting_data_current()->denom_number + PAGE_02_A_ITEM - 1) / PAGE_02_A_ITEM;
    page_02_b_report_status.curent_page = 1;
    page_02_b_report_status.total_page = sn_count == 0
        ? 1 : (sn_count + PAGE_02_B_ITEM - 1) / PAGE_02_B_ITEM;
    page_02_c_report_status.curent_page = 1;
    page_02_c_report_status.total_page = error_count == 0
        ? 1 : (error_count + PAGE_02_C_ITEM - 1) / PAGE_02_C_ITEM;
}

void page_02_list_section_scroll_to_page(page_02_section_id_t section_id, bool anim_en) // 按页码同步滚动位置
{
    page_02_scroll_section_t *section = page_02_scroll_section_get(section_id);

    if (section == NULL) return;
    page_02_scroll_section_spacer_refresh(section);
    page_02_scroll_section_sync_to_status(section, anim_en);
}

void page_02_list_section_page_step(page_02_section_id_t section_id, int step, bool anim_en) // 分区翻页，供点击与其它入口复用
{
    page_02_scroll_section_t *section = page_02_scroll_section_get(section_id);

    if (section == NULL) return;
    page_02_scroll_section_total_page_refresh(section);

    switch (section_id) {
    case PAGE_02_SECTION_A:
        page_02_a_report_status.curent_page += step;
        if (page_02_a_report_status.curent_page == 0) page_02_a_report_status.curent_page = page_02_a_report_status.total_page;
        if (page_02_a_report_status.curent_page > page_02_a_report_status.total_page) page_02_a_report_status.curent_page = 1;
        page_02_a_page_refre();
        page_02_a_page_num_refre();
        break;
    case PAGE_02_SECTION_B:
        page_02_b_report_status.curent_page += step;
        if (page_02_b_report_status.curent_page == 0) page_02_b_report_status.curent_page = page_02_b_report_status.total_page;
        if (page_02_b_report_status.curent_page > page_02_b_report_status.total_page) page_02_b_report_status.curent_page = 1;
        page_02_b_page_refre();
        page_02_b_page_num_refre();
        break;
    case PAGE_02_SECTION_C:
        page_02_c_report_status.curent_page += step;
        if (page_02_c_report_status.curent_page == 0) page_02_c_report_status.curent_page = page_02_c_report_status.total_page;
        if (page_02_c_report_status.curent_page > page_02_c_report_status.total_page) page_02_c_report_status.curent_page = 1;
        page_02_c_page_refre();
        page_02_c_page_num_refre();
        break;
    default:
        break;
    }

    page_02_list_section_scroll_to_page(section_id, anim_en);
}



void ui_page_02_list_create(lv_obj_t* parent)
{
    (void)parent;
    page_02_list_report_reset();

    //creat page_main 
    if (list_page) return;
    list_page = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(list_page);
    lv_obj_set_pos(list_page, 0, 0);
    lv_obj_set_size(list_page, 1280, 400);
    lv_obj_clear_flag(list_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list_page, LV_SCROLLBAR_MODE_OFF); // 滚动条关闭

    // 计算数组长度
    page_02_list_len = sizeof(page_02_list_obj) / sizeof(ui_element_t);
    
    //创建图片
    lv_ui_obj_init(list_page, page_02_list_obj, page_02_list_len);
    page_02_scroll_section_init_config();
    for (int i = 0; i < PAGE_02_SECTION_COUNT; i++) {
        page_02_scroll_section_create(&s_page_02_scroll_sections[i]);
    }
    page_02_a_page_refre();
    page_02_b_page_refre();
    page_02_c_page_refre();
    page_02_curr_refre();
    page_02_a_page_num_refre();
    page_02_b_page_num_refre();
    page_02_c_page_num_refre();
    page_02_list_section_refresh_all();
    currency_state_get_active_code(g_page_02_currency_code);
    g_page_02_dirty = 0;

}

bool ui_page_02_list_resume(void)
{
    char active_code[4];
    uint32_t dirty;
    bool profile_enabled;
    uint64_t profile_started_us = 0;

    if (list_page == NULL || !lv_obj_is_valid(list_page)) {
        return false;
    }

    currency_state_get_active_code(active_code);
    if (strncmp(active_code, g_page_02_currency_code, 3) != 0) {
        g_page_02_dirty |= PAGE_02_DIRTY_CURRENCY;
    }
    dirty = g_page_02_dirty;
    profile_enabled = perf_profile_is_enabled();
    if (profile_enabled) {
        profile_started_us = app_clock_monotonic_us();
    }
    lv_obj_clear_flag(list_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(list_page);
    if (g_page_02_dirty & PAGE_02_DIRTY_SECTION_A) {
        page_02_a_page_refre();
        page_02_a_page_num_refre();
    }
    if (g_page_02_dirty & PAGE_02_DIRTY_SECTION_B) {
        page_02_b_page_refre();
        page_02_b_page_num_refre();
    }
    if (g_page_02_dirty & PAGE_02_DIRTY_SECTION_C) {
        page_02_c_page_refre();
        page_02_c_page_num_refre();
    }
    if (g_page_02_dirty & PAGE_02_DIRTY_CURRENCY) {
        page_02_curr_refre();
        memcpy(g_page_02_currency_code, active_code,
               sizeof(g_page_02_currency_code));
    }
    g_page_02_dirty = 0;
    if (profile_enabled) {
        perf_profile_report_event_us(
            "LIST", dirty == 0 ? "RESUME_CLEAN" : "RESUME_DIRTY",
            app_clock_elapsed_us32(profile_started_us,
                                   app_clock_monotonic_us()));
    }
    return true;
}

void ui_page_02_list_suspend(void)
{
    if (list_page == NULL || !lv_obj_is_valid(list_page)) {
        return;
    }

    for (int i = 0; i < PAGE_02_SECTION_COUNT; i++) {
        page_02_scroll_section_t *section = &s_page_02_scroll_sections[i];

        section->pressing = false;
        section->press_moved = false;
        if (section->container && lv_obj_is_valid(section->container)) {
            lv_anim_del(section->container, NULL);
        }
    }
    lv_obj_add_flag(list_page, LV_OBJ_FLAG_HIDDEN);
}

void ui_page_02_list_destroy(void)
{
    memset(s_page_02_scroll_sections, 0, sizeof(s_page_02_scroll_sections));
    if (list_page) {
        lv_obj_del(list_page);
        list_page = NULL;
    }
    memset(g_page_02_currency_code, 0, sizeof(g_page_02_currency_code));
    g_page_02_dirty = PAGE_02_DIRTY_ALL;
}
