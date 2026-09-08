#!/usr/bin/env python3
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
cc = os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
if not cc:
    raise SystemExit('C compiler required')
with tempfile.TemporaryDirectory(prefix='un260-commit-') as work:
    exe = Path(work)/('test.exe' if os.name == 'nt' else 'test')
    subprocess.run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(root), str(root/'tools/test_ui_frame_commit.c'),
                    str(root/'un260/lv_core/ui_frame_commit.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
