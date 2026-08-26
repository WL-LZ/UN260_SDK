#include "lv_qr_popup.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_components/qrcodegen.h"
#include <stddef.h>
#include <string.h>

#define QR_POPUP_CARD_W          360
#define QR_POPUP_CARD_H          336
#define QR_POPUP_CODE_MAX_SIZE   220
#define QR_POPUP_PADDING_MODULE  4

static lv_obj_t* g_qr_popup_root = NULL;
static lv_obj_t* g_qr_popup_card = NULL;
static lv_obj_t* g_qr_popup_title = NULL;
static lv_obj_t* g_qr_popup_desc = NULL;
static lv_obj_t* g_qr_popup_close_btn = NULL;
static lv_obj_t* g_qr_popup_close_label = NULL;
static lv_obj_t* g_qr_popup_canvas = NULL;
static void* g_qr_popup_buf = NULL;
static uint32_t g_qr_popup_buf_size = 0;

static void qr_popup_close_event_cb(lv_event_t* e); //关闭二维码弹窗事件

static void qr_popup_release_buffer(void) //释放二维码画布缓存
{
    if (g_qr_popup_buf != NULL) {
        lv_mem_free(g_qr_popup_buf);
        g_qr_popup_buf = NULL;
        g_qr_popup_buf_size = 0;
    }
}

static bool qr_popup_prepare_buffer(uint16_t size) //为二维码画布准备缓存
{
    uint32_t buf_size = (uint32_t)size * (uint32_t)size * sizeof(lv_color_t);

    if (g_qr_popup_buf != NULL && g_qr_popup_buf_size == buf_size) {
        return true;
    }

    qr_popup_release_buffer();

    g_qr_popup_buf = lv_mem_alloc(buf_size);
    if (g_qr_popup_buf == NULL) {
        return false;
    }

    g_qr_popup_buf_size = buf_size;
    return true;
}

static void qr_popup_create(void) //创建二维码弹窗对象
{
    if (g_qr_popup_root != NULL && lv_obj_is_valid(g_qr_popup_root)) {
        return;
    }

    g_qr_popup_root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_qr_popup_root);
    lv_obj_set_size(g_qr_popup_root, 1280, 400);
    lv_obj_clear_flag(g_qr_popup_root, LV_OBJ_FLAG_SCROLLABLE);

    g_qr_popup_card = lv_obj_create(g_qr_popup_root);
    lv_obj_set_size(g_qr_popup_card, QR_POPUP_CARD_W, QR_POPUP_CARD_H);
    lv_obj_center(g_qr_popup_card);
    lv_obj_clear_flag(g_qr_popup_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_qr_popup_card, 26, 0);
    lv_obj_set_style_bg_color(g_qr_popup_card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(g_qr_popup_card, 1, 0);
    lv_obj_set_style_border_color(g_qr_popup_card, lv_color_hex(0xEAEAEA), 0);
    lv_obj_set_style_shadow_color(g_qr_popup_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(g_qr_popup_card, LV_OPA_10, 0);
    lv_obj_set_style_shadow_width(g_qr_popup_card, 18, 0);
    lv_obj_set_style_shadow_ofs_y(g_qr_popup_card, 6, 0);

    g_qr_popup_title = lv_label_create(g_qr_popup_card);
    lv_obj_set_width(g_qr_popup_title, 280);
    lv_obj_align(g_qr_popup_title, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_text_align(g_qr_popup_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(g_qr_popup_title, &lv_font_instrument_sans_semibold_22, 0);
    lv_obj_set_style_text_color(g_qr_popup_title, lv_color_hex(0x111111), 0);

    g_qr_popup_desc = lv_label_create(g_qr_popup_root);
    lv_obj_set_width(g_qr_popup_desc, QR_POPUP_CARD_W);
    lv_obj_set_style_text_align(g_qr_popup_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(g_qr_popup_desc, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_color(g_qr_popup_desc, lv_color_hex(0x666666), 0);

    g_qr_popup_canvas = lv_canvas_create(g_qr_popup_card);
    lv_obj_align(g_qr_popup_canvas, LV_ALIGN_CENTER, 0, 12);

    g_qr_popup_close_btn = lv_btn_create(g_qr_popup_card);
    lv_obj_set_size(g_qr_popup_close_btn, 116, 38);
    lv_obj_align(g_qr_popup_close_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_radius(g_qr_popup_close_btn, 12, 0);
    lv_obj_set_style_bg_color(g_qr_popup_close_btn, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_color(g_qr_popup_close_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_event_cb(g_qr_popup_close_btn, qr_popup_close_event_cb, LV_EVENT_CLICKED, NULL);

    g_qr_popup_close_label = lv_label_create(g_qr_popup_close_btn);
    lv_obj_center(g_qr_popup_close_label);
    lv_obj_set_style_text_font(g_qr_popup_close_label, &lv_font_instrument_sans_bold_16, 0);
    lv_obj_set_style_text_color(g_qr_popup_close_label, lv_color_hex(0xFFFFFF), 0);

    lv_obj_align_to(g_qr_popup_desc, g_qr_popup_close_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    lv_obj_add_flag(g_qr_popup_root, LV_OBJ_FLAG_HIDDEN);
}

static bool qr_popup_draw_code(const char* qr_text) //生成并绘制二维码图像
{
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    lv_draw_rect_dsc_t rect_dsc;
    int qr_size;
    int draw_size;
    int module_size;
    int x;
    int y;

    if (qr_text == NULL || qr_text[0] == '\0') {
        return false;
    }

    if (!qrcodegen_encodeText(qr_text, temp, qrcode, qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true)) {
        return false;
    }

    qr_size = qrcodegen_getSize(qrcode);
    module_size = QR_POPUP_CODE_MAX_SIZE / (qr_size + QR_POPUP_PADDING_MODULE * 2);
    if (module_size < 1) {
        module_size = 1;
    }

    draw_size = (qr_size + QR_POPUP_PADDING_MODULE * 2) * module_size;

    if (!qr_popup_prepare_buffer((uint16_t)draw_size)) {
        return false;
    }

    lv_canvas_set_buffer(g_qr_popup_canvas, g_qr_popup_buf, draw_size, draw_size, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(g_qr_popup_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_obj_set_size(g_qr_popup_canvas, draw_size, draw_size);
    lv_obj_align(g_qr_popup_canvas, LV_ALIGN_CENTER, 0, 6);

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_hex(0x000000);
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.border_width = 0;
    rect_dsc.radius = 0;

    for (y = 0; y < qr_size; y++) {
        for (x = 0; x < qr_size; x++) {
            if (!qrcodegen_getModule(qrcode, x, y)) {
                continue;
            }

            lv_canvas_draw_rect(g_qr_popup_canvas,
                (x + QR_POPUP_PADDING_MODULE) * module_size,
                (y + QR_POPUP_PADDING_MODULE) * module_size,
                module_size,
                module_size,
                &rect_dsc);
        }
    }

    return true;
}

static void qr_popup_refresh_text(void) //刷新二维码弹窗文本
{
    if (g_qr_popup_title != NULL) {
        lv_label_set_text(g_qr_popup_title, ui_text_get(UI_TEXT_WIDGET_QR_POPUP_TITLE));
    }

    if (g_qr_popup_desc != NULL) {
        lv_label_set_text(g_qr_popup_desc, ui_text_get(UI_TEXT_WIDGET_QR_POPUP_DESC));
    }

    if (g_qr_popup_close_label != NULL) {
        lv_label_set_text(g_qr_popup_close_label, ui_text_get(UI_TEXT_WIDGET_QR_POPUP_BTN_CLOSE));
    }
}

static void qr_popup_close_event_cb(lv_event_t* e) //关闭二维码弹窗事件
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    lv_qr_popup_hide();
}

bool lv_qr_popup_show(const char* qr_text) //显示二维码弹窗
{
    qr_popup_create();
    qr_popup_refresh_text();

    if (!qr_popup_draw_code(qr_text)) {
        return false;
    }

    lv_obj_clear_flag(g_qr_popup_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_qr_popup_root);
    return true;
}

void lv_qr_popup_hide(void) //关闭二维码弹窗
{
    if (g_qr_popup_root == NULL || !lv_obj_is_valid(g_qr_popup_root)) {
        return;
    }

    lv_obj_add_flag(g_qr_popup_root, LV_OBJ_FLAG_HIDDEN);
}

bool lv_qr_popup_is_showing(void) //判断二维码弹窗是否显示中
{
    if (g_qr_popup_root == NULL || !lv_obj_is_valid(g_qr_popup_root)) {
        return false;
    }

    return !lv_obj_has_flag(g_qr_popup_root, LV_OBJ_FLAG_HIDDEN);
}
