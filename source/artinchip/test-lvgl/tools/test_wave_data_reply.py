#!/usr/bin/env python3
"""Compile the production wave/stream parser at O0/O2 with UBSan."""
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sources = [root / 'tools/test_wave_data_reply.c',
           root / 'un260/protocol/stream_data_reply.c',
           root / 'un260/protocol/protocol_frame.c',
           root / 'un260/protocol/protocol_frame_parser.c']
with tempfile.TemporaryDirectory(prefix='un260-wave-data-reply-') as temporary:
    for optimization in ('-O0', '-O2'):
        binary = Path(temporary) / optimization[1:]
        command = ['gcc', '-std=c11', optimization, '-g', '-Wall', '-Wextra',
                   '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                   f'-I{root}', *map(str, sources), '-o', str(binary)]
        print('RUN ' + shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
        print(f'PASS GCC {optimization} with UBSan', flush=True)
