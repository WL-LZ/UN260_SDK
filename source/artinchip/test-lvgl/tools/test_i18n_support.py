"""Shared real locale/message sources for host fixtures (hardware stays mocked)."""
from pathlib import Path
import os

def with_i18n(sources, root):
    names={Path(p).name for p in sources}
    if 'ui_text_page.c' in names and 'counting_reject_reason.c' not in names:
        sources=[*sources,Path(root)/'un260/counting/counting_reject_reason.c']
    if names.intersection({'settings_detail_ui.c','lv_alnum_keyboard.c','lv_settings.c','lv_damped_button.c'}) and not names.intersection({'ui_message_font.c','test_menu_view.c'}):
        sources=[*sources,Path(root)/'tools/tests/ui_message_font_stub.c']
    relative = [
        "un260/lv_system/ui_i18n.c",
        "un260/lv_system/ui_lang.c",
        "un260/lv_system/ui_message.c",
        "un260/storage/ui_locale_store.c",
        "i18n/generated/lv_i18n.c",
    ]
    return list(dict.fromkeys([*sources, *(Path(root) / item for item in relative)]))

def lvgl_source(root):
    return Path(os.environ.get("LVGL_SOURCE", Path(root).parents[1] / "third-party/lvgl-8.3.2"))
