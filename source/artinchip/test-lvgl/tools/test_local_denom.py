#!/usr/bin/env python3
"""Test local denomination counting; optional captured UART log argument."""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-local-denom-') as t:
    work=Path(t);stub=work/'un260/lv_drivers/lv_drivers.h'
    stub.parent.mkdir(parents=True);stub.write_text('void uart_debug_printf(const char *, ...);\n')
    exe=work/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-g','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(work),'-I'+str(root),str(root/'tools/test_local_denom.c'),*[str(root/'un260/counting'/n) for n in ['counting_denom_reply.c','counting_denom_query_service.c','counting_info_reply.c','counting_report_sync.c','counting_reject_sn_reply.c','counting_data_store.c']],'-o',str(exe)],check=True)
    subprocess.run([str(exe),*sys.argv[1:]],check=True)
