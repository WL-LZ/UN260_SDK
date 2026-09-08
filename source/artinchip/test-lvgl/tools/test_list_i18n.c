#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "un260/counting/counting_reject_reason.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"

extern const ui_text_item_t g_ui_text_page_group[UI_TEXT_PAGE_MAX];

_Static_assert(UI_TEXT_PAGE_MAX <= UI_TEXT_WIDGET_BASE,
               "Page text IDs must not overlap the widget group");

int main(void)
{
    static const language_t languages[] = {LANGUAGE_EN, LANGUAGE_CN, LANGUAGE_KR};
    static const char *const ranges[] = {
        "1-10 of 200", "1-10 / 共200条", "1-10 / 총 200"
    };
    static const char *const empty_serial[] = {
        "No serial numbers", "暂无冠字号", "일련번호 없음"
    };
    static const char *const empty_reject[] = {
        "No reject details", "暂无拒钞明细", "거부 내역 없음"
    };
    unsigned int code;
    size_t language;
    char buffer[96];

    for (int id = UI_TEXT_LIST_DENOMINATIONS; id < UI_TEXT_PAGE_MAX; ++id) {
        const ui_text_item_t *item = &g_ui_text_page_group[id];
        assert(item->en != NULL && item->en[0] != '\0');
        assert(item->cn != NULL && item->cn[0] != '\0');
        assert(item->kr != NULL && item->kr[0] != '\0');
    }

    for (language = 0; language < sizeof(languages) / sizeof(languages[0]); ++language) {
        ui_lang_set(languages[language]);
        for (int id = UI_TEXT_LIST_DENOMINATIONS; id < UI_TEXT_PAGE_MAX; ++id) {
            assert(ui_text_get((ui_text_id_t)id)[0] != '\0');
        }
        for (code = 0; code <= 0x31; ++code) {
            const char *reason = ui_text_counting_reject_reason((uint8_t)code);
            assert(reason != NULL && reason[0] != '\0');
            if (languages[language] == LANGUAGE_EN) {
                assert(strcmp(reason, counting_reject_reason_get((uint8_t)code)) == 0);
            }
        }
        for (code = 0x32; code <= 0xFF; ++code) {
            assert(strcmp(ui_text_counting_reject_reason((uint8_t)code),
                          ui_text_get(UI_TEXT_COUNTING_REJECT_UNKNOWN)) == 0);
        }
        snprintf(buffer, sizeof(buffer), ui_text_get(UI_TEXT_LIST_RANGE_FMT),
                 1U, 10U, 200U);
        assert(strcmp(buffer, ranges[language]) == 0);
        snprintf(buffer, sizeof(buffer), ui_text_get(UI_TEXT_LIST_PAGE_FMT), 2U, 8U);
        assert(strcmp(buffer, "2 / 8") == 0);
        assert(strcmp(ui_text_get(UI_TEXT_LIST_NO_SERIAL_NUMBERS), empty_serial[language]) == 0);
        assert(strcmp(ui_text_get(UI_TEXT_LIST_NO_REJECT_DETAILS), empty_reject[language]) == 0);
    }

    ui_lang_set((language_t)255);
    assert(ui_lang_get() == LANGUAGE_EN);
    assert(strcmp(ui_text_get(UI_TEXT_LIST_DENOMINATIONS), "Denominations") == 0);
    assert(strcmp(ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_DENOM), "DENOM") == 0);
    assert(strcmp(ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_PCS), "PCS") == 0);
    assert(strcmp(ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_AMOUNT), "AMOUNT") == 0);
    assert(strcmp(ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_NO), "NO") == 0);
    puts("list i18n: EN/CN/KR registration, formats, all reject codes and fallback passed");
    return 0;
}
