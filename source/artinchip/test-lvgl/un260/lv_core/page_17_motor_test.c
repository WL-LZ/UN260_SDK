#include "page_17_motor_test.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_system/ui_text.h"

static lv_obj_t* motor_test_page = NULL;

typedef struct {
    const char* title;
    uint8_t cmd_g;
    uint8_t forward_cmd[2];
    uint8_t stop_cmd[2];
    lv_obj_t* status_label;
} motor_item_t;

static motor_item_t motors[] = {
    { "LEVEL 1 MOTOR", 0x52, {0x01, 0x01}, {0x00, 0x00}, NULL },
    { "LEVEL 2 MOTOR", 0x53, {0x01, 0x01}, {0x00, 0x00}, NULL },
    { "IMPELLER MOTOR", 0x54, {0x01, 0x01}, {0x01, 0x02}, NULL },
};

static void motor_status_set(motor_item_t* item, const char* status)
{
    if (item == NULL || item->status_label == NULL) return;
    lv_label_set_text(item->status_label, status);
}

static void motor_test_stop_all(void)
{
    settings_detail_send_command(motors[0].cmd_g, motors[0].stop_cmd, 2);
    settings_detail_send_command(motors[1].cmd_g, motors[1].stop_cmd, 2);
    settings_detail_send_command(motors[2].cmd_g, motors[2].stop_cmd, 2);

    motor_status_set(&motors[0], ui_text_get(UI_TEXT_SETTINGS_MOTOR_STOP));
    motor_status_set(&motors[1], ui_text_get(UI_TEXT_SETTINGS_MOTOR_STOP));
    motor_status_set(&motors[2], ui_text_get(UI_TEXT_SETTINGS_MOTOR_STOP));
}

static void motor_test_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    motor_test_stop_all();
    ui_manager_pop_page();
}

static void motor_forward_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    motor_item_t* item = (motor_item_t*)lv_event_get_user_data(e);
    if (!item) return;

    if (settings_detail_send_command(item->cmd_g, item->forward_cmd, 2)) {
        motor_status_set(item, ui_text_get(UI_TEXT_SETTINGS_MOTOR_FORWARD));
    }
}



static void motor_stop_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    motor_item_t* item = (motor_item_t*)lv_event_get_user_data(e);
    if (!item) return;

    if (settings_detail_send_command(item->cmd_g, item->stop_cmd, 2)) {
        motor_status_set(item, ui_text_get(UI_TEXT_SETTINGS_MOTOR_STOP));
    }
}


static lv_obj_t* create_motor_card(lv_obj_t* parent, motor_item_t* item, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, 390, 220);
    lv_obj_set_style_pad_all(card, 12, 0);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, item->title);
    lv_obj_set_style_text_font(title, &lv_font_instrument_sans_semibold_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x183B61), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    item->status_label = lv_label_create(card);
    lv_label_set_text(item->status_label, ui_text_get(UI_TEXT_SETTINGS_MOTOR_STOP));
    lv_obj_set_style_text_font(item->status_label, &lv_font_instrument_sans_medium_22, 0);
    lv_obj_set_style_text_color(item->status_label, lv_color_hex(0x183B61), 0);
    lv_obj_align(item->status_label, LV_ALIGN_TOP_MID, 0, 32);

    settings_detail_create_button(card, 120, 96, 150, 44,
                                  ui_text_get(UI_TEXT_SETTINGS_MOTOR_FORWARD),
                                  lv_color_hex(0x08C5D6),
                                  motor_forward_cb, item);
    settings_detail_create_button(card, 120, 150, 150, 44,
                                  ui_text_get(UI_TEXT_SETTINGS_MOTOR_STOP),
                                  lv_color_hex(0xE95F5F),
                                  motor_stop_cb, item);

    return card;
}

void ui_page_17_motor_test_create(lv_obj_t* parent)
{
    (void)parent;
    if (motor_test_page) return;

    lv_obj_t* content = NULL;
    motor_test_page = settings_detail_create_page(parent, ui_text_get(UI_TEXT_SETTINGS_MOTOR_TEST),
                                                  motor_test_esc_cb, &content);

    create_motor_card(content, &motors[0], 24, 48);
    create_motor_card(content, &motors[1], 444, 48);
    create_motor_card(content, &motors[2], 864, 48);
}

void ui_page_17_motor_test_destroy(void)
{
    motor_test_stop_all();

    if (motor_test_page) {
        lv_obj_del(motor_test_page);
        motor_test_page = NULL;
    }

    motors[0].status_label = NULL;
    motors[1].status_label = NULL;
    motors[2].status_label = NULL;
}
