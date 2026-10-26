#!/usr/bin/env python3
"""Exercise production report ownership/parsers and bounded recovery with sanitizers."""
import argparse
from contextlib import nullcontext
from pathlib import Path
import json
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('capture', nargs='?', type=Path)
parser.add_argument('--output-dir', type=Path)
parser.add_argument('--recovery-only', action='store_true')
args = parser.parse_args()
if args.output_dir:
    args.output_dir.mkdir(parents=True, exist_ok=True)
context = nullcontext(str(args.output_dir.resolve())) if args.output_dir else tempfile.TemporaryDirectory(prefix='un260-report-sync-')
with context as directory:
    work = Path(directory)
    stub = work/'un260/lv_drivers/lv_drivers.h'
    stub.parent.mkdir(parents=True, exist_ok=True)
    stub.write_text('void uart_debug_printf(const char *, ...);\n')
    files = ['counting_report_sync.c', 'counting_reject_sn_reply.c', 'counting_data_store.c']
    arguments=[]
    if args.capture:
        # Rejoin timestamps inserted within a frame, retaining only complete
        # captured reports. Missing bytes are never manufactured.
        raw=args.capture.read_text(encoding='utf-8-sig')
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
    source=(root/'tools/test_counting_report_sync.c').read_text()
    cases=[name for name in re.findall(r'\bCASE\((\w+)\)',source) if name!='n']
    assert cases and len(cases)==len(set(cases))
    runs=[]
    for optimization in ['O0','O2']:
        executable=work/('test-'+optimization)
        command=['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-'+optimization,
            '-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie',
            '-I'+str(work),'-I'+str(root),str(root/'tools/test_counting_report_sync.c'),
            *[str(root/'un260/counting'/name) for name in files],'-o',str(executable)]
        compiled=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        (work/('compile-'+optimization+'.log')).write_text(compiled.stdout)
        compiled.check_returncode()
        scenarios=[(name,['--recovery',name]) for name in cases]
        if not args.recovery_only:scenarios.insert(0,('existing-suite',arguments))
        for name, scenario in scenarios:
            run=subprocess.run([str(executable),*scenario],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
            (work/(optimization+'-'+name+'.log')).write_text(run.stdout)
            runs.append({'optimization':optimization,'case':name,'exit_code':run.returncode})
            print(optimization, 'PASS' if run.returncode==0 else 'FAIL', name,flush=True)
            if run.returncode:print(run.stdout,flush=True)
    (work/'results.json').write_text(json.dumps(runs,indent=2)+'\n')
    passed=sum(run['exit_code']==0 for run in runs)
    print('SUMMARY',passed,'/',len(runs),'runs passed',flush=True)
    raise SystemExit(0 if passed==len(runs) else 1)