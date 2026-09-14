#!/usr/bin/env python3
"""Exercise the real S30dbus start function with isolated command stubs."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('script', type=Path)
args = parser.parse_args()
source = args.script.read_text()
start = source.index('start() {')
end = source.index('\nstop() {', start)
function = source[start:end]
with tempfile.TemporaryDirectory(prefix='un260-dbus-init-test-') as directory:
    scratch = Path(directory)
    marker, trace = scratch / 'ready-marker', scratch / 'daemon-called'
    function = function.replace('/var/lock/subsys/dbus-daemon', str(marker))
    for name, text in {
        'dbus-uuidgen': '#!/bin/sh\nexit "${TEST_UUID_EXIT:-0}"\n',
        'dbus-daemon': '#!/bin/sh\nprintf daemon > "$TEST_TRACE"\n'
                       'exit "${TEST_DAEMON_EXIT:-0}"\n',
    }.items():
        path = scratch / name
        path.write_text(text)
        path.chmod(0o755)
    program = scratch / 'test-start.sh'
    program.write_text('#!/bin/sh\nRETVAL=0\n' + function + '\nstart\nexit $RETVAL\n')
    for uuid_status, daemon_status, expected in ((0, 0, 0), (23, 0, 23), (0, 17, 17)):
        marker.unlink(missing_ok=True)
        trace.unlink(missing_ok=True)
        env = dict(os.environ, PATH=str(scratch) + ':' + os.environ['PATH'],
                   TEST_UUID_EXIT=str(uuid_status), TEST_DAEMON_EXIT=str(daemon_status),
                   TEST_TRACE=str(trace))
        result = subprocess.run(['sh', str(program)], env=env,
                                text=True, capture_output=True)
        assert result.returncode == expected, result
        assert marker.exists() == (expected == 0), result
        assert trace.exists() == (uuid_status == 0), result
        assert ('done' in result.stdout) == (expected == 0), result
        assert ('ERROR' in result.stdout) == (expected != 0), result
print('PASS: successful start, machine-id failure, daemon failure; status retained')
