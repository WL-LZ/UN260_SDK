#!/usr/bin/env python3
"""Exercise the production mode-key contact recognizer with deterministic touch events."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'un260/lv_core/page_01_main.c').read_text()
start=source.index('typedef struct {\n    lv_obj_t *button;\n    lv_point_t start;')
with tempfile.TemporaryDirectory(prefix='un260-mode-contact-') as directory:
    work=Path(directory)
    (work/'contact_under_test.h').write_text(source[start:])
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-I'+str(work),str(root/'tools/test_main_mode_contact.c'),'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
