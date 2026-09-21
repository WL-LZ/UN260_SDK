/* Run the production component and both pages against deterministic LVGL fakes.
 * No target filesystem is touched; user_cfg persistence is an in-memory fixture. */
#include "pin_test_support.h"
#include "un260/lv_components/lv_damped_button.h"

lv_obj_t *lv_damped_button_create(lv_obj_t *parent,
                                 const lv_damped_button_style_t *style,
                                 const char *text, const lv_font_t *font)
{
    LV_UNUSED(font);
    lv_obj_t *button = lv_obj_create(parent);
    lv_label_set_text(button, text);
    button->color = style->normal_color;
    return button;
}

#include "../un260/lv_components/lv_pin_input.c"
void lv_damped_button_set_text(lv_obj_t *button, const char *text)
{
    lv_label_set_text(button, text);
}
void lv_damped_button_set_exact_palette(lv_obj_t *button, lv_color_t normal, lv_color_t pressed)
{
    button->color=normal;
    button->pressed_color = pressed;
}
lv_obj_t *lv_damped_button_get_label(lv_obj_t *button){return button;}
void lv_nav_button_mark_back(lv_obj_t *button){assert(lv_obj_is_valid(button));}

#include "../un260/lv_components/lv_pin_keypad.c"
#include "../un260/lv_core/page_05_set_password.c"
#include "../un260/lv_core/page_29_set_password.c"

static unsigned confirmed, cancelled;
static char confirmed_pin[5];

static void capture_confirm(const char *pin, void *data)
{
    LV_UNUSED(data);
    ++confirmed;
    snprintf(confirmed_pin, sizeof(confirmed_pin), "%s", pin);
}

static void destroy_on_confirm(const char *pin, void *data)
{
    capture_confirm(pin, NULL);
    lv_obj_del(data);
    /* The copied callback value must remain valid after its source is gone. */
    assert(strcmp(pin, confirmed_pin) == 0);
}

static void capture_cancel(void *data)
{
    ++cancelled;
    lv_pin_keypad_hide(data);
}

static void press_key(lv_pin_keypad_t *keypad, unsigned key)
{
    for (unsigned i=0; i<12; ++i) if (pin_keys[i] == key) {
        click(keypad->keys[i]); return;
    }
    assert(!"Unknown key");
}

static void type_pin(lv_pin_keypad_t *keypad, const char *pin)
{
    for (; *pin; ++pin) press_key(keypad, (unsigned)(*pin - '0'));
}

static void assert_empty(lv_pin_keypad_t *keypad)
{
    assert(keypad->input.length == 0);
    for (unsigned i=0; i<sizeof(keypad->input.value); ++i)
        assert(keypad->input.value[i] == 0);
}

static void test_input_model(void)
{
    lv_pin_input_t input = {0};
    assert(!lv_pin_input_is_complete(NULL));
    assert(!lv_pin_input_is_complete(""));
    assert(!lv_pin_input_is_complete("123"));
    assert(!lv_pin_input_is_complete("12345"));
    assert(!lv_pin_input_is_complete("12a4"));
    assert(lv_pin_input_is_complete("0012"));
    assert(!lv_pin_input_backspace(&input));
    assert(lv_pin_input_set(&input, "12"));
    assert(input.length == 2);
    assert(lv_pin_input_push(&input, 3));
    assert(lv_pin_input_push(&input, 4));
    assert(!lv_pin_input_push(&input, 5));
    assert(!lv_pin_input_push(&input, 10));
    assert(strcmp(input.value, "1234") == 0);
    assert(lv_pin_input_backspace(&input));
    assert(strcmp(input.value, "123") == 0);
    assert(!lv_pin_input_set(&input, "12345"));
    assert(input.length == 0 && !input.value[0]);
    assert(!lv_pin_input_set(&input, "12x"));
    assert(input.length == 0 && !input.value[0]);
    assert(lv_pin_input_set(&input, NULL));
}

static void test_component(void)
{
    lv_obj_t *parent = lv_obj_create(NULL);
    lv_pin_keypad_t keypad = {0};
    lv_pin_keypad_config_t config = {
        .confirm_cb = capture_confirm,
        .cancel_cb = capture_cancel,
        .user_data = &keypad,
    };
    assert(lv_pin_keypad_create(&keypad, parent, 80, 12));
    assert(keypad.root->x == 80 && keypad.root->y == 12);
    assert(keypad.root->w == 1120 && keypad.root->h == 320);
    assert(keypad.blink->paused);
    assert(lv_pin_keypad_show(&keypad, &config, ""));
    assert(!keypad.blink->paused && keypad.cursor->x == 44);
    for (unsigned i=0; i<4; ++i) assert(keypad.dots[i]->color == 0xB9C5CD);
    for (unsigned i=0; i<12; ++i) {
        assert(keypad.keys[i]->w == 216 && keypad.keys[i]->h == 64);
        assert(keypad.keys[i]->x == 424 + (int)(i % 3) * 228);
        assert(keypad.keys[i]->y == 16 + (int)(i / 3) * 76);
        assert(keypad.keys[i]->pressed_color == (pin_keys[i] == 11 ? LV_SETTINGS_PRIMARY_PRESSED : LV_SETTINGS_CONTROL_PRESSED));
    }
    assert(strstr(keypad.keys[9]->children[0]->text,"popup_icons/backspace.png"));
    assert(!strcmp(keypad.keys[11]->text,"Confirm"));
    assert(!strcmp(keypad.cancel->text,"Cancel") && keypad.cancel->h >= 44);
    keypad.blink->callback(keypad.blink);
    assert(keypad.cursor->opacity == LV_OPA_TRANSP);
    press_key(&keypad, 0);
    assert(keypad.dots[0]->color == 0x1D2B34 && keypad.dots[1]->color == 0xB9C5CD);
    assert(keypad.cursor->x == 108 && keypad.cursor->opacity == LV_OPA_COVER);
    press_key(&keypad, 11);
    assert(confirmed == 0 && strcmp(keypad.status->text,"Enter exactly 4 digits.")==0);
    type_pin(&keypad, "012");
    assert(keypad.blink->paused && lv_obj_has_flag(keypad.cursor,LV_OBJ_FLAG_HIDDEN));
    press_key(&keypad, 9);
    assert(strcmp(keypad.input.value, "0012") == 0);
    press_key(&keypad, 11);
    assert(confirmed == 1 && strcmp(confirmed_pin, "0012") == 0);
    press_key(&keypad, 10);
    assert(!keypad.blink->paused && !lv_obj_has_flag(keypad.cursor,LV_OBJ_FLAG_HIDDEN));
    assert(keypad.cursor->x == 236 && keypad.dots[3]->color == 0xB9C5CD);
    click(keypad.cancel);
    assert(cancelled == 1 && !lv_pin_keypad_is_visible(&keypad));
    assert(keypad.blink->paused && !keypad.confirm_cb && !keypad.cancel_cb);
    assert_empty(&keypad);
    lv_pin_keypad_show(&keypad, &config, "12345");
    assert_empty(&keypad);
    lv_obj_add_flag(parent,LV_OBJ_FLAG_HIDDEN);
    keypad.blink->callback(keypad.blink);
    assert(keypad.blink->paused);
    lv_obj_clear_flag(parent,LV_OBJ_FLAG_HIDDEN);
    lv_pin_keypad_show(&keypad, &config, "1");
    assert(!keypad.blink->paused);
    config.compact=true;config.auto_confirm=true;
    lv_pin_keypad_show(&keypad,&config,"1");
    assert(keypad.blink->paused&&keypad.keys[11]->x==486);
    assert(lv_obj_has_flag(keypad.eye,LV_OBJ_FLAG_HIDDEN));
    config.compact=false;config.auto_confirm=false;
    lv_pin_keypad_show(&keypad,&config,"1");
    assert(!keypad.blink->paused&&keypad.root->w==1120);
    assert(keypad.keys[11]->x==880&&keypad.keys[11]->w==216);
    assert(keypad.keys[11]->color==LV_SETTINGS_PRIMARY);
    assert(!strcmp(keypad.keys[11]->text,"Confirm"));
    assert(!lv_obj_has_flag(keypad.eye,LV_OBJ_FLAG_HIDDEN));
    lv_obj_del(parent);
    assert(!keypad.root && !keypad.blink);
    assert_empty(&keypad);
    lv_pin_keypad_destroy(&keypad);

    parent=lv_obj_create(NULL);
    lv_pin_keypad_create(&keypad,parent,80,12);
    config.confirm_cb=destroy_on_confirm;
    config.user_data=parent;
    lv_pin_keypad_show(&keypad,&config,"1111");
    press_key(&keypad,11);
    assert(!keypad.root && !keypad.blink && confirmed == 2);
}

static void test_login_page(void)
{
    lv_obj_t *screen=lv_obj_create(NULL);
    ui_page_05_set_password_create(screen);
    assert(g_password_page.keypad.auto_confirm);
    assert(g_password_page.keypad.compact);
    assert(g_password_page.keypad.root->x==412&&g_password_page.keypad.root->y==18);
    assert(g_password_page.keypad.root->w==850&&g_password_page.keypad.root->h==364);
    assert(g_password_page.keypad.blink->paused);
    assert(lv_obj_has_flag(g_password_page.keypad.cursor,LV_OBJ_FLAG_HIDDEN));
    for(unsigned i=0;i<12;i++){
        unsigned slot=i==9?11:i==11?9:i;
        static const int x[]={486,601,715},y[]={55,128,201,273},h[]={65,65,64,65};
        assert(g_password_page.keypad.keys[i]->w==107&&g_password_page.keypad.keys[i]->h==h[slot/3]);
        assert(g_password_page.keypad.keys[i]->x==x[slot%3]);
        assert(g_password_page.keypad.keys[i]->y==y[slot/3]);
        assert(g_password_page.keypad.keys[i]->color==((i==9||i==11)?0xFBFCFD:0xF0F3F5));
        assert(g_password_page.keypad.keys[i]->pressed_color==0xE2E9EE);
    }
    assert(g_password_page.keypad.cancel->x==791&&g_password_page.keypad.cancel->y==14);
    assert(g_password_page.keypad.cancel->w==40&&g_password_page.keypad.cancel->h==36);
    assert(!g_password_page.keypad.cancel->text[0]);
    assert(strstr(g_password_page.keypad.close_icon->text,"pin_icons/close.png"));
    assert(strstr(g_password_page.keypad.backspace_icon->text,"pin_icons/erase.png"));
    assert(lv_obj_has_flag(g_password_page.keypad.eye,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(g_password_page.keypad.eyebrow,LV_OBJ_FLAG_HIDDEN));
    assert(strstr(g_password_page.keypad.leading_icon->text,"pin_icons/lock.png"));
    assert(strstr(g_password_page.keypad.footnote->text,"For authorized configuration"));
    assert(!strcmp(g_password_page.keypad.keys[10]->text,"0"));
    assert(!strcmp(g_password_page.keypad.keys[11]->text,"Clear"));
    type_pin(&g_password_page.keypad,"0000");
    assert(switched_page == -1);
    assert_empty(&g_password_page.keypad);
    assert(strstr(g_password_page.keypad.status->text,"Incorrect password"));
    assert(g_password_page.keypad.error);
    press_key(&g_password_page.keypad,1);
    assert(!g_password_page.keypad.error&&strstr(g_password_page.keypad.status->text,"Opens automatically"));
    press_key(&g_password_page.keypad,10);
    assert_empty(&g_password_page.keypad);
    type_pin(&g_password_page.keypad,"1111");
    assert(switched_page == UI_PAGE_SETTING);
    assert_empty(&g_password_page.keypad);
    assert(g_password_page.keypad.blink->paused);
    ui_page_05_set_password_suspend();
    assert(ui_page_05_set_password_resume());
    assert_empty(&g_password_page.keypad);
    assert(strstr(g_password_page.keypad.status->text,"Opens automatically"));
    type_pin(&g_password_page.keypad,"12");
    ui_page_05_set_password_suspend();
    assert_empty(&g_password_page.keypad);
    assert(g_password_page.keypad.blink->paused);
    ui_page_05_set_password_resume();
    type_pin(&g_password_page.keypad,"12");
    press_key(&g_password_page.keypad,11);assert_empty(&g_password_page.keypad);
    type_pin(&g_password_page.keypad,"45");
    click(g_password_page.page); /* Outside cancels, never navigates/commits. */
    assert(switched_page == UI_PAGE_SETTING&&!ui_page_05_set_password_is_open());
    assert_empty(&g_password_page.keypad);
    ui_page_05_set_password_resume();
    assert(ui_page_05_set_password_request_back());
    assert(!ui_page_05_set_password_request_back());
    ui_page_05_set_password_destroy();
    assert(!g_password_page.page && !g_password_page.keypad.blink);
    ui_page_05_set_password_create(screen);
    lv_obj_del(screen);
    assert(!g_password_page.page && !g_password_page.keypad.root);
    assert(!ui_page_05_set_password_resume());
}

static void test_visibility(void)
{
    lv_obj_t *parent = lv_obj_create(NULL);
    lv_pin_keypad_t keypad = {0};
    lv_pin_keypad_config_t config = {
        .digits_visible = user_cfg_password_visibility_enabled(),
        .save_visibility = user_cfg_password_visibility_save,
    };
    lv_pin_keypad_create(&keypad, parent, 80, 12);
    lv_pin_keypad_show(&keypad, &config, "");
    assert(!keypad.digits_visible && !visibility_save_count);
    /* Text action has its own large target to the right of the digit slots. */
    assert(keypad.eye->x > keypad.dots[3]->x + keypad.dots[3]->w);
    assert(keypad.eye->w == 104 && keypad.eye->h == 48);
    type_pin(&keypad, "00");
    assert(!strcmp(keypad.eye->text, "Show"));
    for (unsigned i=0; i<4; ++i) assert(!keypad.digits[i]->text[0]);
    click(keypad.eye);
    assert(keypad.digits_visible && saved_visibility && visibility_save_count == 1);
    assert(save_count == 0); /* Eye must never call the password writer. */
    assert(!strcmp(keypad.eye->text, "Hide"));
    for (unsigned i=0; i<2; ++i) {
        assert(!strcmp(keypad.digits[i]->text,"0"));
        assert(lv_obj_has_flag(keypad.dots[i],LV_OBJ_FLAG_HIDDEN));
        assert(!lv_obj_has_flag(keypad.digits[i],LV_OBJ_FLAG_HIDDEN));
    }
    assert(!lv_obj_has_flag(keypad.dots[2],LV_OBJ_FLAG_HIDDEN));
    assert(keypad.dots[2]->color == 0xB9C5CD && !keypad.blink->paused);
    type_pin(&keypad, "42");
    assert(!strcmp(keypad.digits[2]->text,"4") && !strcmp(keypad.digits[3]->text,"2"));
    assert(!strcmp(keypad.input.value,"0042") && keypad.blink->paused);
    press_key(&keypad,10);
    assert(!keypad.digits[3]->text[0] && !keypad.blink->paused);
    click(keypad.eye);
    assert(!saved_visibility && visibility_save_count == 2);
    for (unsigned i=0; i<4; ++i) {
        assert(!keypad.digits[i]->text[0]);
        assert(lv_obj_has_flag(keypad.digits[i],LV_OBJ_FLAG_HIDDEN));
        assert(!lv_obj_has_flag(keypad.dots[i],LV_OBJ_FLAG_HIDDEN));
    }
    assert(!strcmp(keypad.input.value,"004"));
    visibility_save_success=false;
    click(keypad.eye);
    assert(!keypad.digits_visible && !saved_visibility);
    assert(strstr(keypad.status->text,"Could not save"));
    visibility_save_success=true;
    click(keypad.eye);
    lv_pin_keypad_hide(&keypad);
    assert_empty(&keypad);
    for (unsigned i=0; i<4; ++i) assert(!keypad.digits[i]->text[0]);
    assert(keypad.blink->paused && !keypad.save_visibility);
    unsigned writes=visibility_save_count;
    config.digits_visible=user_cfg_password_visibility_enabled();
    lv_pin_keypad_show(&keypad,&config,"1");
    assert(keypad.digits_visible && !strcmp(keypad.digits[0]->text,"1"));
    assert(visibility_save_count==writes);
    lv_pin_keypad_destroy(&keypad);
    lv_obj_del(parent);

    /* First-concept login always masks; the full change-PIN editor keeps its preference. */
    parent=lv_obj_create(NULL);
    ui_page_05_set_password_create(parent);
    assert(!g_password_page.keypad.digits_visible);
    assert(lv_obj_has_flag(g_password_page.keypad.eye,LV_OBJ_FLAG_HIDDEN));
    assert(saved_visibility);
    ui_page_05_set_password_destroy();
    ui_page_29_set_password_create(parent);
    for (unsigned i=0; i<PASSWORD_FIELD_COUNT; ++i) {
        bool before=saved_visibility;
        if(!lv_pin_keypad_is_visible(&password_setting_keypad))click(field_cards[i]);
        assert(active_field==(password_field_t)i);
        assert(password_setting_keypad.digits_visible == before);
        click(password_setting_keypad.eye);
        assert(saved_visibility != before);
        type_pin(&password_setting_keypad,i==0?"1111":"0022");
        press_key(&password_setting_keypad,11);
    }
    assert(!lv_pin_keypad_is_visible(&password_setting_keypad));
    assert(!save_count);
    ui_page_29_set_password_destroy();
    ui_page_05_set_password_create(parent);
    assert(!g_password_page.keypad.digits_visible);
    ui_page_05_set_password_destroy();
    lv_obj_del(parent);
    saved_visibility=false;
}

static void enter_field(password_field_t field, const char *pin)
{
    if(!lv_pin_keypad_is_visible(&password_setting_keypad))click(field_cards[field]);
    assert(active_field==field);
    assert(lv_pin_keypad_is_visible(&password_setting_keypad));
    assert(lv_obj_has_flag(password_setting_form,LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(password_setting_keypad.title->text,field_titles[field])==0);
    lv_pin_keypad_clear(&password_setting_keypad);
    type_pin(&password_setting_keypad,pin);
    press_key(&password_setting_keypad,11);
    assert_empty(&password_setting_keypad);
    assert(lv_pin_keypad_is_visible(&password_setting_keypad)==(field<PASSWORD_FIELD_CONFIRM));
    assert(lv_obj_has_flag(password_setting_form,LV_OBJ_FLAG_HIDDEN)==(field<PASSWORD_FIELD_CONFIRM));
    assert(strcmp(field_text[field],pin)==0);
}

static void save_form(void)
{
    lv_event_t event = {LV_EVENT_CLICKED, NULL, NULL};
    password_setting_save_cb(&event);
}

static void test_change_password_page(void)
{
    lv_obj_t *screen=lv_obj_create(NULL);
    ui_page_29_set_password_create(screen);
    click(field_cards[PASSWORD_FIELD_CURRENT]);type_pin(&password_setting_keypad,"9999");
    press_key(&password_setting_keypad,11);
    assert(active_field==PASSWORD_FIELD_CURRENT&&!field_text[PASSWORD_FIELD_CURRENT][0]);
    assert(strstr(password_setting_keypad.status->text,"Incorrect current PIN"));
    enter_field(PASSWORD_FIELD_CURRENT,"1111");
    enter_field(PASSWORD_FIELD_NEW,"0022");
    type_pin(&password_setting_keypad,"0033");press_key(&password_setting_keypad,11);
    assert(active_field==PASSWORD_FIELD_CONFIRM&&!field_text[PASSWORD_FIELD_CONFIRM][0]);
    assert(strstr(password_setting_keypad.status->text,"do not match"));
    enter_field(PASSWORD_FIELD_CONFIRM,"0022");
    assert(save_count == 0); /* Field confirmation alone never changes auth. */
    assert(pin_gesture_policy && pin_gesture_policy(GESTURE_ACTION_HOME));
    assert(pin_overlay && pin_discard && pop_count == 0);
    settings_detail_dialog_hide();
    click(password_frame.back);
    assert(pin_overlay && pin_discard && pop_count == 0);
    settings_detail_dialog_hide();
    click(field_cards[PASSWORD_FIELD_CONFIRM]);
    press_key(&password_setting_keypad,10);
    press_key(&password_setting_keypad,3);
    click(password_frame.back);
    assert(pop_count == 0 && !lv_pin_keypad_is_visible(&password_setting_keypad));
    assert(strcmp(field_text[PASSWORD_FIELD_CONFIRM],"0022")==0);
    assert_empty(&password_setting_keypad);
    click(field_cards[PASSWORD_FIELD_CONFIRM]);
    press_key(&password_setting_keypad,10);
    press_key(&password_setting_keypad,8);
    assert(lv_obj_has_flag(password_setting_keypad.root,LV_OBJ_FLAG_CLICKABLE));
    lv_event_t inside={LV_EVENT_CLICKED,password_setting_keypad.root,NULL};
    password_setting_outside(&inside);
    assert(lv_pin_keypad_is_visible(&password_setting_keypad));
    click(password_setting_page);
    assert(!lv_pin_keypad_is_visible(&password_setting_keypad));
    assert(strcmp(field_text[PASSWORD_FIELD_NEW],"0022")==0 && save_count==0);
    assert_empty(&password_setting_keypad);
    assert(password_setting_keypad.blink->paused);
    assert(!lv_obj_has_flag(password_frame.footer,LV_OBJ_FLAG_HIDDEN));
    strcpy(field_text[PASSWORD_FIELD_CURRENT],"1234");
    save_form(); assert(save_count == 0 && strstr(toast_text,"Incorrect current"));
    strcpy(field_text[PASSWORD_FIELD_CURRENT],"1111");
    strcpy(field_text[PASSWORD_FIELD_CONFIRM],"0033");
    save_form(); assert(save_count == 0 && strstr(toast_text,"do not match"));
    strcpy(field_text[PASSWORD_FIELD_CONFIRM],"0022");
    strcpy(field_text[PASSWORD_FIELD_CURRENT],"12a4");
    save_form(); assert(save_count == 0 && strstr(toast_text,"exactly 4 digits"));
    strcpy(field_text[PASSWORD_FIELD_CURRENT],"1111");
    save_success=false;
    save_form(); assert(save_count == 1 && strcmp(saved_password,"1111")==0);
    assert(strcmp(field_text[PASSWORD_FIELD_NEW],"0022")==0);
    save_success=true;
    save_form(); assert(save_count == 2 && strcmp(saved_password,"0022")==0);
    for (unsigned i=0;i<PASSWORD_FIELD_COUNT;++i) assert(!field_text[i][0]);
    assert(strcmp(toast_text,"Password saved")==0);
    click(password_frame.back); assert(pop_count == 1);
    enter_field(PASSWORD_FIELD_CURRENT,"0022");
    type_pin(&password_setting_keypad,"00");
    lv_obj_del(screen);
    assert(!password_setting_page && !password_setting_keypad.root && !password_setting_keypad.blink);
    for (unsigned i=0;i<PASSWORD_FIELD_COUNT;++i) assert(!field_text[i][0]);
    ui_page_29_set_password_destroy();
}

int main(void)
{
    test_input_model();
    test_component();
    test_visibility();
    test_login_page();
    test_change_password_page();
    assert(live_count == 0 && timers_created == timers_deleted);
    puts("PIN keypad: PASS 4-digit model/geometry/dots/blink/backspace/confirm/cancel");
    puts("PIN pages: PASS default1111/auth/reject/field isolation/save/mismatch/ESC/lifetime cleanup");
    puts("PIN visibility: PASS eye/partial digits/leading zero/hide cleanup/save failure/shared preference");
    return 0;
}
