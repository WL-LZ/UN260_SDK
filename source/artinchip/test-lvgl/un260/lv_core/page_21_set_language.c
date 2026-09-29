#include "un260/lv_components/ui_notice.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_21_set_language.h"
#include "lv_page_manager.h"
#include "settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/gesture/gesture_service.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    const ui_locale_t *locale;
    lv_obj_t *row, *check;
} language_row_t;
static lv_settings_frame_t language_frame;
static language_t language_original, language_draft;
static lv_obj_t *language_save, *language_preview;
static language_row_t *language_rows;
static size_t language_row_count;
static bool language_home;
static bool language_dirty(void) { return language_original != language_draft; }
static void language_refresh(void)
{
    const char *name = ui_tr("Unavailable");
    for (size_t i = 0; i < language_row_count; ++i) {
        language_row_t *row = &language_rows[i];
        bool selected = language_draft == row->locale->id;
        lv_obj_set_style_bg_color(row->row, lv_color_hex(selected ? 0xEDF4FF : 0xF3F6F8), 0);
        lv_obj_set_style_border_color(row->row, lv_color_hex(selected ? 0x8CACDA : 0xE3E9ED), 0);
        if (selected) { lv_obj_clear_flag(row->check, LV_OBJ_FLAG_HIDDEN); name = row->locale->name; }
        else lv_obj_add_flag(row->check, LV_OBJ_FLAG_HIDDEN);
    }
    settings_detail_action_block(language_save, language_dirty() ? NULL : UI_N_("No changes to save."));
    lv_label_set_text(language_frame.message, language_dirty() ? ui_tr("Unsaved changes") : ui_tr("No changes"));
    lv_label_set_text(language_preview, name);
}
static void language_leave(void *data)
{
    (void)data;
    if (language_home) ui_manager_suspend_to_home(); else ui_manager_pop_page();
}
static void language_discard(void)
{
    settings_detail_dialog_show(ui_tr("Discard changes?"), ui_tr("Your changes have not been applied."),
        ui_tr("Discard"), ui_tr("Keep editing"), language_leave, NULL, NULL);
}
static void language_back(lv_event_t *event)
{
    (void)event; language_home = false;
    if (language_dirty()) language_discard(); else language_leave(NULL);
}
static bool language_gesture(gesture_action_t action)
{
    if (settings_detail_overlay_is_open()) return true;
    if (action != GESTURE_ACTION_HOME || !language_dirty()) return false;
    language_home = true; language_discard(); return true;
}
static void language_cancel(lv_event_t *event)
{ (void)event; language_home = false; language_leave(NULL); }
static void language_apply(lv_event_t *event)
{
    (void)event;
    if (!language_dirty()) return;
    if (!ui_lang_save(language_draft)) {
        ui_notice_post_text(UI_NOTICE_ERROR, "settings.language", UI_N_("Language change not confirmed"), UI_N_("Check storage and retry."));
        return;
    }
    /* Invalidate inactive UI before returning; business state/drafts stay owned by pages. */
    ui_manager_on_language_changed();
    language_home = false; language_leave(NULL);
    ui_notice_post_text(UI_NOTICE_SUCCESS, "settings.language", UI_N_("Language changed"), NULL);
}
static void language_choice(lv_event_t *event)
{
    language_t id = (language_t)(uintptr_t)lv_event_get_user_data(event);
    for (size_t i = 0; i < language_row_count; ++i)
        if (language_rows[i].locale->id == id) { language_draft = id; language_refresh(); return; }
}
void ui_page_21_set_language_create(lv_obj_t *parent)
{
    if (language_frame.root) return;
    language_original = language_draft = ui_lang_get(); language_home = false;
    const lv_settings_header_t header = { .title = ui_tr("Language"), .subtitle = ui_tr("Device / Settings"), .icon = "Languages-active", .back = language_back };
    language_frame = lv_settings_frame_create(parent, &header);
    lv_obj_t *body = language_frame.body;
    language_row_count = 0;
    for (size_t i = 0; i < ui_lang_count(); ++i)
        if (ui_lang_is_available(ui_lang_at(i))) ++language_row_count;
    language_rows = lv_mem_alloc(language_row_count * sizeof(*language_rows));
    if (!language_rows) {
        language_row_count = 0;
        lv_settings_label(body, ui_tr("Language settings unavailable"), 24, 36, &lv_font_instrument_sans_medium_18, 0x536B79);
        lv_settings_button(language_frame.footer, 1102, 0, 130, 44, ui_tr("Back"), false, language_cancel, NULL);
        ui_notice_post_text(UI_NOTICE_ERROR, "settings.language", UI_N_("Language settings unavailable"), UI_N_("Try again."));
        return;
    }
    memset(language_rows, 0, language_row_count * sizeof(*language_rows));
    lv_settings_label(body, ui_tr("Interface language"), 20, 12, &lv_font_instrument_sans_semibold_14, 0x536B79);
    lv_obj_t *list = lv_settings_grid(body, 17, 40, 610, 184); lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    size_t row_index = 0;
    for (size_t i = 0; i < ui_lang_count(); ++i) {
        const ui_locale_t *locale = ui_lang_at(i);
        if (!ui_lang_is_available(locale)) continue;
        language_row_t *row = &language_rows[row_index++]; row->locale = locale;
        lv_obj_t *button = lv_settings_button(list, 0, 0, 590, 76, "", false, language_choice, (void *)(uintptr_t)locale->id);
        row->row = button;
        lv_obj_set_style_bg_color(button, lv_color_hex(0xEDF4FF), 0); lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(0x8CACDA), 0); lv_obj_set_style_radius(button, 13, 0);
        lv_obj_t *code = lv_settings_box(button, 18, 16, 44, 44, 0xFFFFFF); lv_obj_set_style_radius(code, 10, 0);
        lv_obj_t *label = lv_settings_label(code, locale->code, 0, 0, &lv_font_instrument_sans_semibold_18, 0x155CBA); lv_obj_center(label);
        lv_settings_label(button, locale->name, 78, 25, &lv_font_instrument_sans_semibold_22, 0x174F9D);
        row->check = lv_settings_icon(button, "Check-active", 548, 26);
    }
    lv_settings_box(body, 644, 12, 1, 212, 0xE3E9ED);
    lv_settings_label(body, ui_tr("Preview"), 666, 12, &lv_font_instrument_sans_semibold_14, 0x536B79);
    lv_obj_t *sample = lv_settings_box(body, 666, 42, 544, 151, 0xF5F7F9); lv_obj_set_style_radius(sample, 13, 0);
    lv_settings_icon(sample, "Settings", 18, 14);
    lv_settings_label(sample, ui_tr("Device"), 52, 14, &lv_font_instrument_sans_semibold_20, 0x1D2B34);
    lv_settings_box(sample, 18, 52, 508, 1, 0xE3E9ED);
    lv_settings_label(sample, ui_tr("Language"), 18, 66, &lv_font_instrument_sans_medium_16, 0x1D2B34);
    language_preview = lv_settings_label(sample, ui_lang_current()->name, 390, 66, &lv_font_instrument_sans_medium_16, 0x536B79);
    lv_settings_box(sample, 18, 101, 508, 1, 0xE3E9ED);
    lv_settings_label(sample, ui_tr("Date & time"), 18, 115, &lv_font_instrument_sans_medium_16, 0x1D2B34);
    lv_settings_label(sample, ui_tr("24-hour"), 390, 115, &lv_font_instrument_sans_medium_16, 0x536B79);
    lv_settings_label(body, ui_tr("The interface changes after Save."), 666, 207, &lv_font_instrument_sans_medium_12, 0x536B79);
    lv_settings_button(language_frame.footer, 976, 0, 116, 44, ui_tr("Cancel"), false, language_cancel, NULL);
    language_save = lv_settings_button(language_frame.footer, 1102, 0, 130, 44, ui_tr("Save"), true, language_apply, NULL);
    language_refresh();
    gesture_service_set_page_policy(UI_PAGE_LANGUAGE_SETTING, NULL, language_gesture);
}
void ui_page_21_set_language_destroy(void)
{
    gesture_service_clear_page_policy(UI_PAGE_LANGUAGE_SETTING); settings_detail_dialog_hide();
    if (language_frame.root) lv_obj_del(language_frame.root);
    memset(&language_frame, 0, sizeof(language_frame)); language_save = language_preview = NULL;
    if (language_rows) lv_mem_free(language_rows);
    language_rows = NULL; language_row_count = 0;
}
