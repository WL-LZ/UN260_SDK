"""Shared real locale/message sources for host fixtures (hardware stays mocked)."""
from pathlib import Path
import os

def with_i18n(sources, root):
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
