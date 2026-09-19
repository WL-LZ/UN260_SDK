#!/usr/bin/env python3
"""Exercise exact production updater ownership transitions without running an updater."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]

def function(source,name):
    match=re.search(r'^(?:static )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert match,name
    start=source.index('{',match.start());depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]',source[start:],re.S):
        if token.group()=='{': depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[match.start():start+token.end()]
    raise AssertionError(name)

source=(root/'un260/lv_core/ui_upgrade_service.c').read_text()
names=['ui_upgrade_service_set_status','ui_upgrade_service_update_child_state',
       'ui_upgrade_service_reset','ui_upgrade_service_start']
with tempfile.TemporaryDirectory(prefix='un260-upgrade-session-') as tmp:
    temp=Path(tmp);header=temp/'actual_upgrade_session_functions.h'
    header.write_text('\n\n'.join(function(source,name) for name in names))
    exe=temp/'session'
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-g',
      '-fsanitize=address,undefined','-fno-sanitize-recover=all',f'-I{root}',f'-I{temp}',
      str(root/'tools/test_upgrade_session.c'),str(root/'un260/app_service/upgrade_session.c'),
      '-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
