#!/usr/bin/env python3
"""Compile and exercise production report ownership/parsers with ASan/UBSan."""
from pathlib import Path
import subprocess
import tempfile
import re
import sys

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-report-sync-') as directory:
    work = Path(directory)
    stub = work/'un260/lv_drivers/lv_drivers.h'
    stub.parent.mkdir(parents=True)
    stub.write_text('void uart_debug_printf(const char *, ...);\n')
    files = ['counting_report_sync.c', 'counting_reject_sn_reply.c', 'counting_data_store.c']
    executable = work/'test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
        '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-I'+str(work), '-I'+str(root),
        str(root/'tools/test_counting_report_sync.c'),
        *[str(root/'un260/counting'/name) for name in files], '-o', str(executable)], check=True)
    arguments=[]
    if len(sys.argv)==2:
        # Rejoin terminal timestamps inserted halfway through a frame. Keep
        # only complete start-to-end reports; never fabricate missing bytes.
        raw=Path(sys.argv[1]).read_text(encoding='utf-8-sig')
        raw=re.sub(r'\[\d\d:\d\d:\d\d\.\d+\][^\n]*?◆', ' ', raw)
        reports=[];current=None
        for match in re.finditer(r'RX\[(\d+)\]:\s*((?:[0-9A-Fa-f]{2}(?:\s+|$))+)',raw):
            n=int(match[1]);frame=bytes.fromhex(match[2])
            if n!=25 or len(frame)!=n or frame[:4]!=bytes.fromhex('FD DF 19 0D'):continue
            if frame[4:24]==bytes(20):current=[]
            if current is None:continue
            current.append(bytes([n])+frame)
            if frame[4:24]==bytes([255])*20:
                reports.extend(current);current=None
        assert reports,'No complete captured reports'
        fixture=work/'captured.bin';fixture.write_bytes(b''.join(reports));arguments=[str(fixture)]
    subprocess.run([str(executable),*arguments], check=True)
