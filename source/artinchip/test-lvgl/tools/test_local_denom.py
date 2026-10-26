#!/usr/bin/env python3
"""Test controller denomination projection; optionally replay a UART capture."""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('capture', nargs='?', type=Path)
parser.add_argument('--output-dir', type=Path,
                    default=Path('/dev/shm/un260-controller-denom-live/local-denom'))
args = parser.parse_args()
args.output_dir.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='local-denom-', dir=args.output_dir) as t:
    work = Path(t)
    stub = work/'un260/lv_drivers/lv_drivers.h'
    stub.parent.mkdir(parents=True)
    stub.write_text('void uart_debug_printf(const char *, ...);\n')
    exe = work/'test'
    sources = ['counting_denom_reply.c', 'counting_denom_query_service.c',
               'counting_info_reply.c', 'counting_report_sync.c',
               'counting_reject_sn_reply.c', 'counting_data_store.c']
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare',
                    '-g', '-O1', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-no-pie', '-I'+str(work), '-I'+str(root), str(root/'tools/test_local_denom.c'),
                    *[str(root/'un260/counting'/n) for n in sources], '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
if args.capture:
    subprocess.run([sys.executable, str(root/'tools/test_denom_capture.py'), str(args.capture),
                    '--output-dir', str(args.output_dir/'capture')], check=True)
