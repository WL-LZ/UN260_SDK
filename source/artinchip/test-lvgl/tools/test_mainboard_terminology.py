#!/usr/bin/env python3
"""Main control board terminology; stable msgids and unrelated controllers stay compatible."""
from pathlib import Path
import re
import gen_i18n

root=Path(__file__).resolve().parents[1]
registry,catalogs=gen_i18n.catalogues()
names={'en':'Mainboard','zh-Hans':'主板','ko':'메인 보드','ja':'メインボード',
       'ar':'اللوحة الرئيسية','tr':'Ana kart','fr':'Carte mère','pl':'Płyta główna',
       'ru':'Основная плата','de':'Hauptplatine'}
excluded={'Initializing DDR controller... [  OK  ]','Touch controller ONLINE [  OK  ]'}
for locale in registry:
    tag=locale['tag'];entries=catalogs[tag]
    assert entries['Controller']==names[tag],(tag,entries['Controller'])
    assert entries['UI_TEXT_QUICK_CONTROLLER']==names[tag]
    assert entries['UI_TEXT_SETTINGS_MAIN_BOARD_UPGRADE']==entries['Main Board Upgrade']
    assert entries['UI_TEXT_SETTINGS_MAIN_UPGRADE_TITLE']==entries['MAIN BOARD UPGRADE']
    for key in excluded:assert entries[key] and entries[key]!=names[tag]
english=catalogs['en'];seen=0
for key,value in english.items():
    if isinstance(value,str) and key not in excluded:
        assert not re.search(r'controller|main[- ]board',value,re.I),(key,value)
        if 'mainboard' in value.lower():
            seen+=1
            for tag,entries in catalogs.items():
                assert isinstance(entries[key],str) and entries[key]
                assert gen_i18n.format_args(value)==gen_i18n.format_args(entries[key]),(tag,key)
assert seen>=84,seen
support=(root/'un260/app_service/support_report.c').read_text()
assert 'Mainboard: %.80s' in support and 'Mainboard boot: %.80s' in support
assert 'Controller: %.80s' not in support
settings=(root/'un260/lv_core/page_06_settings.c').read_text()
assert 'device_info_main_app()' in settings and 'UI_N_("Controller")' in settings
assert 'UI_TEXT_QUICK_CONTROLLER' in (root/'un260/lv_system/ui_text.h').read_text()
print('PASS Mainboard terminology: %d keys, ten locales, printf contracts, stable keys and support fields'%seen)
