#!/usr/bin/env python3
"""Regression test for executable RPATH growth followed by strip; copies only."""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

spec = importlib.util.spec_from_file_location(
    'elf_layout', Path(__file__).with_name('check-elf-layout.py'))
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)


def run(command):
    subprocess.run([str(part) for part in command], check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def require_valid(path):
    result = layout.check_elf(path)
    if not result['ok']:
        raise AssertionError(result)


def check_reader(scratch):
    for elf_class in (1, 2):
        for byte_order in (1, 2):
            endian = '<' if byte_order == 1 else '>'
            hfmt = '16sHHIIIIIHHHHHH' if elf_class == 1 else '16sHHIQQQIHHHHHH'
            pfmt = 'IIIIIIII' if elf_class == 1 else 'IIQQQQQQ'
            hs, ps = struct.calcsize(hfmt), struct.calcsize(pfmt)
            ident = b'\x7fELF' + bytes((elf_class, byte_order, 1)) + b'\0' * 9
            header = struct.pack(endian + hfmt, ident, 2, 243, 1, 0, hs, 0, 0,
                                 hs, ps, 1, 40 if elf_class == 1 else 64, 0, 0)
            p = ((1, 0, 0x10000, 0x10000, hs + ps, hs + ps, 5, 4096)
                 if elf_class == 1 else
                 (1, 5, 0, 0x10000, 0x10000, hs + ps, hs + ps, 4096))
            good = header + struct.pack(endian + pfmt, *p)
            fixture = scratch / f'elf-{elf_class}-{byte_order}'
            fixture.write_bytes(good)
            require_valid(fixture)
            fixture.write_bytes(good[:-1])
            try:
                layout.check_elf(fixture)
            except layout.LayoutError:
                pass
            else:
                raise AssertionError('truncation was accepted')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--patchelf', required=True, type=Path)
    parser.add_argument('--strip', required=True, type=Path)
    parser.add_argument('--fixture', required=True, type=Path,
                        help='unmodified ET_EXEC fixture, e.g. dbus build output')
    parser.add_argument('--target-root', required=True, type=Path)
    parser.add_argument('--smoke', nargs='*', type=Path, default=[])
    args = parser.parse_args()
    originals = [args.fixture] + args.smoke
    hashes = {p: hashlib.sha256(p.read_bytes()).digest() for p in originals}
    with tempfile.TemporaryDirectory(prefix='un260-patchelf-test-') as directory:
        scratch = Path(directory)
        check_reader(scratch)
        cases = 0
        for length in (80, 90, 120, 121, 160, 256, 1000):
            for strip_first in (False, True):
                artifact = scratch / 'rpath-fixture'
                shutil.copy2(args.fixture, artifact)
                strip = [args.strip, '--remove-section=.comment',
                         '--remove-section=.note', artifact]
                if strip_first:
                    run(strip)
                    require_valid(artifact)
                run([args.patchelf, '--set-rpath', '/' + 'a' * (length - 1), artifact])
                require_valid(artifact)
                for cycle in range(3):
                    run(strip)
                    require_valid(artifact)
                    run([args.patchelf, '--make-rpath-relative', args.target_root,
                         '--no-standard-lib-dirs', artifact])
                    require_valid(artifact)
                cases += 1
        for source in originals:
            artifact = scratch / 'smoke'
            shutil.copy2(source, artifact)
            for cycle in range(3):
                run([args.strip, '--remove-section=.comment',
                     '--remove-section=.note', artifact])
                run([args.patchelf, '--make-rpath-relative', args.target_root,
                     '--no-standard-lib-dirs', artifact])
                require_valid(artifact)
        assert all(hashlib.sha256(p.read_bytes()).digest() == hashes[p]
                   for p in originals), 'input fixture changed'
        print(f'PASS: reader 32/64-bit LE/BE, {cases} RPATH cases, '
              f'{len(originals)} x 3 smoke passes; inputs unchanged')


if __name__ == '__main__':
    main()
