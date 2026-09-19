#include "un260/lv_components/lv_modal_dialog.h"
#include "test_history_view_support.h"
#include <assert.h>
#include <stdio.h>
void popup_test_visibility(void)
{
    lv_modal_dialog_t dialog={0};
    lv_modal_dialog_config_t config={.title="Immediate display",.body="No transition or snapshot.",
        .primary_text="OK",.title_font=&lv_font_instrument_sans_semibold_28,
        .body_font=&lv_font_instrument_sans_medium_16,.button_font=&lv_font_instrument_sans_medium_16,
        .primary_color=0x1462CC};
    unsigned timers=history_test_timers();
    for(unsigned i=0;i<10;++i) {
        unsigned animations=lv_anim_count_running();
        assert(lv_modal_dialog_show(&dialog,lv_scr_act(),&config));
        assert(lv_modal_dialog_is_visible(&dialog));
        assert(lv_obj_get_style_opa(dialog.panel,0)==LV_OPA_COVER);
        assert(lv_anim_count_running()==animations);
        unsigned children=lv_obj_get_child_cnt(lv_scr_act());
        lv_modal_dialog_hide(&dialog);
        assert(!lv_modal_dialog_is_visible(&dialog));
        assert(lv_obj_get_child_cnt(lv_scr_act())==children);
        assert(lv_anim_count_running()==animations);
    }
    lv_modal_dialog_destroy(&dialog);history_test_tick(200);
    assert(history_test_timers()==timers);
    puts("PASS: popups show/hide immediately, no transition animations or ghost objects, repeated lifecycle");
}
