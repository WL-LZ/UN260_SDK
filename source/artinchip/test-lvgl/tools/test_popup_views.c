#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_modal_dialog.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/lv_components/lv_alnum_keyboard.h"
#include "test_history_view_support.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
bool protocol_send_is_ready(void) {return false;}
int protocol_send(uint8_t c,const uint8_t *p,uint16_t n) {(void)c;(void)p;(void)n;return -1;}
void page_06_settings_set_status(const char *t,lv_color_t c) {(void)t;(void)c;}
static unsigned submitted,closed;
static char value[65];
static void input_done(const char *v,void *ctx) {(void)ctx;++submitted;strcpy(value,v);}
static void input_closed(void *ctx) {(void)ctx;++closed;}
static lv_obj_t *text_parent(lv_obj_t *o,const char *text)
{
    if(lv_obj_check_type(o,&lv_label_class) && !strcmp(lv_label_get_text(o),text)) return lv_obj_get_parent(o);
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i) {
        lv_obj_t *found=text_parent(lv_obj_get_child(o,i),text);if(found) return found;
    }
    return NULL;
}
void popup_test_views(void)
{
    unsigned baseline=history_test_timers();
    lv_modal_dialog_t dialog={0};
    lv_modal_dialog_config_t config={.title="Connection not confirmed",.body="Check the controller connection, then try again.",
        .primary_text="Try again",.secondary_text="Cancel",.title_font=&lv_font_instrument_sans_semibold_28,
        .body_font=&lv_font_instrument_sans_medium_16,.button_font=&lv_font_instrument_sans_medium_16,
        .primary_color=0x1462CC,.panel_height=280};
    assert(lv_modal_dialog_show(&dialog,lv_scr_act(),&config));history_test_tick(300);
    history_test_click(dialog.secondary_button);
    assert(lv_obj_get_style_bg_color(dialog.secondary_button,0).full==lv_color_hex(0xF1F4F5).full);
    history_test_bmp("popup-information");lv_modal_dialog_destroy(&dialog);history_test_tick(200);
    assert(settings_detail_dialog_show_ex(SETTINGS_DIALOG_SUCCESS,"Print settings saved","Your preferences apply to the next report.","Done",NULL,NULL,NULL,NULL));
    history_test_tick(300);history_test_bmp("popup-result");settings_detail_dialog_hide();history_test_tick(200);
    assert(settings_detail_keyboard_show_ex("Set value","120",6,SETTINGS_DETAIL_KEYBOARD_NUM,input_done,NULL,input_closed,NULL));
    history_test_tick(300);
    history_test_bmp("popup-numeric");
    lv_obj_t *key=text_parent(lv_scr_act(),"4");assert(key);history_test_click(key);
    key=text_parent(lv_scr_act(),"Apply");assert(key);history_test_click(key);
    assert(submitted==1 && closed==1 && !strcmp(value,"4"));
    assert(settings_detail_keyboard_show_ex("Set value","120",6,SETTINGS_DETAIL_KEYBOARD_NUM,input_done,NULL,input_closed,NULL));
    history_test_tick(300);key=text_parent(lv_scr_act(),"5");assert(key);history_test_click(key);
    history_test_tap(660,160); /* Inside card, not a key: must stay open. */
    assert(closed==1);
    history_test_tap(40,200);
    assert(submitted==1 && closed==2);
    assert(settings_detail_keyboard_show_ex("Report title","UNION",12,SETTINGS_DETAIL_KEYBOARD_TEXT,input_done,NULL,input_closed,NULL));
    history_test_tick(300);history_test_bmp("popup-text");
    assert(!text_parent(lv_scr_act(),"SPACE"));
    key=text_parent(lv_scr_act(),"Aa");assert(key);history_test_click(key);
    key=text_parent(lv_scr_act(),"a");assert(key);history_test_click(key);
    history_test_tap(140,200);assert(closed==2);
    history_test_tap(40,200);assert(closed==3 && submitted==1);
    lv_alnum_keyboard_config_t search={.title="Serial number",.placeholder="Enter serial number",.apply_text="Search",
        .clear_text="Clear",.cancel_text="Cancel",.max_length=24,.submit=input_done,.cancel=input_closed,.modern=true};
    lv_alnum_keyboard_t *alnum=lv_alnum_keyboard_create(lv_scr_act(),&search);assert(alnum);
    assert(lv_alnum_keyboard_set_text(alnum,"AB12345678"));lv_alnum_keyboard_show(alnum);
    history_test_tick(300);history_test_bmp("popup-search-keyboard");
    history_test_tap(140,200);assert(lv_alnum_keyboard_is_visible(alnum));
    history_test_tap(40,200);assert(!lv_alnum_keyboard_is_visible(alnum));
    assert(closed==4 && submitted==1);lv_alnum_keyboard_destroy(alnum);history_test_tick(200);
    assert(lv_qr_popup_show("UN260 popup scan test"));history_test_tick(300);history_test_bmp("popup-content");
    lv_qr_popup_hide();history_test_tick(200);assert(!lv_qr_popup_is_showing());
    assert(history_test_timers()==baseline);
    puts("PASS: information/result, numeric replace/apply/outside cancel, QR content and timer lifecycle");
}
