#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, json, os, subprocess, tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--output', type=Path, default=Path(tempfile.gettempdir())/'un260-capture-request-timeout')
args = parser.parse_args(); root = args.source.resolve(); out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
test = Path(__file__).resolve().with_suffix('.c')
files = [root/p for p in ('un260/protocol/protocol_request.c', 'un260/protocol/protocol_frame_queue.c', 'un260/protocol/protocol_frame.c')]
inputs = files + [root/p for p in ('un260/protocol/protocol_request.h', 'un260/protocol/protocol_frame_queue.h', 'un260/protocol/protocol_frame.h', 'un260/protocol/stream_data_reply.h', 'un260/lv_system/app_clock.h', 'un260/app_service/app_runtime_wakeup.h')] + [test, Path(__file__).resolve()]
identity = lambda: {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
before = identity()
command = ['cc','-std=gnu11','-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I'+str(root),str(test),*map(str,files),'-pthread','-o',str(out/'request-timeout')]
compile_run = subprocess.run(command, capture_output=True, text=True)
run = subprocess.run([str(out/'request-timeout')], capture_output=True, text=True, env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1')) if compile_run.returncode==0 else compile_run
after = identity()
(out/'result.log').write_text(compile_run.stdout+compile_run.stderr+('' if run is compile_run else run.stdout+run.stderr))
report = {'rc':run.returncode,'source_stable':before==after,'before':before,'after':after,'command':command}
(out/'result.json').write_text(json.dumps(report,indent=2))
print((out/'result.log').read_text()); print(json.dumps({'rc':run.returncode,'source_stable':before==after,'inputs':len(inputs)}))
assert before == after, 'Source inputs changed during independent validation'
run.check_returncode()
