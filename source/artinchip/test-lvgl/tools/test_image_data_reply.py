#!/usr/bin/env python3
"""Compile the production stream/image parser at O0/O2 with UBSan."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sources = [root / 'tools/test_image_data_reply.c',
           root / 'un260/protocol/stream_data_reply.c',
           root / 'un260/protocol/protocol_frame.c']
with tempfile.TemporaryDirectory(prefix='un260-image-data-reply-') as temporary:
    for optimization in ('-O0', '-O2'):
        binary = Path(temporary) / optimization[1:]
        subprocess.run(['gcc', '-std=c11', optimization, '-g', '-Wall', '-Wextra',
                        '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                        f'-I{root}', *map(str, sources), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print(f'PASS GCC {optimization} with UBSan', flush=True)
