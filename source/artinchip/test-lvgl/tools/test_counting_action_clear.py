#!/usr/bin/env python3
"""Exercise production CLEAR request ownership and outcomes with fake transport/time."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.work_dir:
        args.work_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='un260-clear-action-', dir=args.work_dir) as directory:
        binary = Path(directory) / 'clear-action'
        command = ['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-no-pie', '-fsanitize=address,undefined',
                   '-fno-omit-frame-pointer', '-fno-sanitize-recover=all',
                   '-I' + str(ROOT), str(Path(__file__).with_suffix('.c')),
                   str(ROOT / 'un260/counting/counting_action_service.c'),
                   str(ROOT / 'un260/protocol/protocol_request.c'),
                   str(ROOT / 'un260/protocol/protocol_frame.c'), '-o', str(binary)]
        subprocess.run(command, check=True, timeout=60)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        subprocess.run([str(binary)], check=True, env=env, timeout=15)


if __name__ == '__main__':
    main()
