#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-factory-service-') as directory:
    temp = Path(directory)
    (temp / 'lvgl').mkdir()
    (temp / 'lvgl/lvgl.h').write_text('typedef struct _lv_obj_t lv_obj_t;\n')
    command = ['cc', '-std=gnu11', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie', f'-I{temp}', f'-I{root}',
        str(root / 'tools/test_ui_factory_reset_service.c'), '-o', str(temp / 'test')]
    subprocess.run(command, check=True)
    subprocess.run([str(temp / 'test')], check=True)
