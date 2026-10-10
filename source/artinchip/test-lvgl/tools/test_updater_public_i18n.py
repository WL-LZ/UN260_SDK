#!/usr/bin/env python3
"""All new backend/rescue messages resolve in ten locales without truncation."""
import json
from pathlib import Path
import subprocess
import tempfile
import gen_i18n
from test_i18n_support import with_i18n

ROOT = Path(__file__).resolve().parents[1]
registry, catalogues = gen_i18n.catalogues()
keys = set()
for name in ('updater_platform.json', 'updater_rescue.json'):
    keys.update(json.loads((ROOT / 'i18n/fragments' / name).read_text())['en'])
rows = []
for locale in registry:
    for key in sorted(keys):
        value = catalogues[locale['tag']][key]
        assert isinstance(value, str) and value
        if locale['id']:
            assert value != catalogues['en'][key], (locale['tag'], key, 'English fallback')
        rows.append('{%d,%s,%s},' % (locale['id'], json.dumps(key, ensure_ascii=False), json.dumps(value, ensure_ascii=False)))
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_update_message.h"
#include "un260/lv_core/ui_upgrade_service.h"
static const struct {language_t language;const char *key,*expected;} rows[]={
''' + '\n'.join(rows) + r'''
};
int main(void){
 char text[UI_UPGRADE_RESULT_TEXT_CAPACITY];size_t largest=0;
 for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);++i){
  assert(strlen(rows[i].expected)<sizeof(text));
  assert(!strcmp(ui_tr_for(rows[i].language,rows[i].key),rows[i].expected));
  ui_update_message_render(rows[i].language,rows[i].key,text,sizeof(text));
  assert(!strcmp(text,rows[i].expected));
  if(strlen(text)>largest)largest=strlen(text);
 }
 printf("PASS %zu translated backend/rescue results, no English fallback or UTF8 truncation; largest=%zu capacity=%zu\n",sizeof(rows)/sizeof(rows[0]),largest,sizeof(text));
}
'''
with tempfile.TemporaryDirectory(prefix='un260-updater-ten-locales-') as name:
    directory = Path(name)
    source = directory / 'test.c'
    source.write_text(code)
    exe = directory / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-no-pie', '-I' + str(ROOT),
                    *map(str, with_i18n([source, ROOT / 'un260/lv_system/ui_update_message.c'], ROOT)),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
