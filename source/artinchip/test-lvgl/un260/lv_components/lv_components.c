#include "lv_components.h"
#include "ui_notice.h"
#include "lv_modal_dialog.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/machine_state/machine_state.h"
#include <stdio.h>
#include <string.h>

/* This is a navigation decision, not a passive message. */
static lv_modal_dialog_t boot_diagnostics;
static void open_boot_diagnostics(void *context)
{
    (void)context;
    lv_modal_dialog_hide(&boot_diagnostics);
    ui_manager_switch(UI_PAGE_SENSOR);
}
void show_boot_selftest_error_popup(const char *message)
{
    if (lv_modal_dialog_is_visible(&boot_diagnostics)) return;
    const lv_modal_dialog_config_t config = {
        .title="Self-test interrupted", .body=message,
        .primary_text="Open diagnostics",
        .title_font=&lv_font_instrument_sans_semibold_28,
        .body_font=&lv_font_instrument_sans_medium_16,
        .button_font=&lv_font_instrument_sans_semibold_16,
        .panel_width=740,.panel_height=260,.primary_width=210,
        .accent_color=0xD08825,.primary_color=0x1559B7,.secondary_color=0x72808B,
        .primary_action=open_boot_diagnostics
    };
    lv_modal_dialog_show(&boot_diagnostics,lv_scr_act(),&config);
}
const char *get_system_error_desc(uint8_t code)
{
    const char *known=machine_runtime_error_desc(code);
    if(known)return known;
    static char description[40];
    snprintf(description,sizeof(description),"Controller report 0x%02X",code);
    return description;
}
const char *get_counting_error_desc(uint8_t type,uint8_t code)
{
    if(type==1 && code==2)return "No banknotes detected";
    const char *known=type==2 ? machine_start_error_desc(code) : NULL;
    return known ? known : "Unrecognized start report";
}
