#include "ui_lang.h"
static language_t g_ui_language = LANGUAGE_EN;

language_t ui_lang_get(void) //获取当前界面语言
{
    switch (g_ui_language) {
    case LANGUAGE_CN:
        return LANGUAGE_CN;

    case LANGUAGE_KR:
        return LANGUAGE_KR;

    case LANGUAGE_EN:
    default:
        return LANGUAGE_EN;
    }
}

void ui_lang_set(language_t lang) //设置当前界面语言
{
    switch (lang) {
    case LANGUAGE_CN:
    case LANGUAGE_KR:
    case LANGUAGE_EN:
        g_ui_language = lang;
        break;

    default:
        g_ui_language = LANGUAGE_EN;
        break;
    }
}
