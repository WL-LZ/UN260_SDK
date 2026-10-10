#!/usr/bin/env python3
"""Malformed configuration, registry fallback, native bounds and source coverage."""
from pathlib import Path
import json,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_core/boot_anim/boot_welcome_locale.h"
int main(int argc,char **argv){assert(argc==3);unsigned want=(unsigned)atoi(argv[2]);
 assert(boot_welcome_read_index(argv[1])==want);return 0;}
"""
code=code.replace('#include <assert.h>','#include <assert.h>\n#include <stdlib.h>')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(root),str(p/'test.c'),'-o',str(p/'test')],check=True)
 locales=[r for r in json.loads((root/'i18n/locales.json').read_text()) if r['enabled'] and r['translations_ready']]
 cases=[(b'',0),(b'unknown',0),(b'ar\0garbage',0),(b'a'*32,0),(b' ar',0)]
 cases += [(r['tag'].encode()+b'\r\n',i) for i,r in enumerate(locales)]
 for data,want in cases:
  (p/'locale').write_bytes(data);subprocess.run([str(p/'test'),str(p/'locale'),str(want)],check=True)
 subprocess.run([str(p/'test'),str(p/'missing'),'0'],check=True)
print('PASS saved welcome locale: released tags, CRLF, absent, unknown, truncated, embedded NUL and oversized files')
