#!/usr/bin/env python3
"""Run the production fault record/catalogue test with real language support."""
from pathlib import Path
import subprocess,tempfile
from test_i18n_support import with_i18n
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-fault-catalog-') as temp:
    exe=Path(temp)/'test'
    sources=with_i18n([root/p for p in ('tools/tests/test_machine_fault.c','un260/machine_state/machine_fault.c','un260/lv_components/fault_guide/fault_guide_catalog.c')],root)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root),*map(str,sources),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
