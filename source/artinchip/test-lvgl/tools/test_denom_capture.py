#!/usr/bin/env python3
"""Replay received controller 0B absolute rows; 49 and 0D must never change A.

A is cleared by every 0B START and depends only on received 0B rows. Captured
39 state affects B/completion grouping, never A. Missing initial ADD is reported
and uses OFF for that unrelated fixture state; --all-add explicitly tests ON.
A final-only capture proves final projection, not live streaming or latency.
"""
from pathlib import Path
import argparse
import re
import subprocess
import tempfile

def frames_from_capture(path):
    raw = path.read_text(encoding='utf-8-sig', errors='replace')
    # Timestamps can split a frame. Remove their prefix, never invent bytes.
    raw = re.sub(r'\[\d\d:\d\d:\d\d\.\d+\][^\n]*?◆', ' ', raw)
    events, malformed = [], 0
    pattern = re.compile(r'(RX\[(\d+)\]:|Send CMD 0x[0-9A-Fa-f]+:)\s*((?:[0-9A-Fa-f]{2}(?:\s+|(?=RX\[)|$))+)')
    for match in pattern.finditer(raw):
        frame = bytes.fromhex(match[3])
        expected = int(match[2]) if match[2] else (frame[2] if len(frame) > 2 else 0)
        if len(frame) != expected or len(frame) < 5 or frame[:2] != b'\xfd\xdf' or frame[2] != expected:
            malformed += 1
            continue
        events.append((not bool(match[2]), raw.count('\n', 0, match.start()) + 1, frame))
    return events, malformed


def prepare(events, all_add):
    headers, details = {}, []
    wire_add = pending_add = None
    ordinal = 0
    for index, (tx, line, b) in enumerate(events):
        if tx and b[3] == 0x39 and len(b) == 6 and b[4] <= 1:
            pending_add = bool(b[4])
        elif not tx and b[3] == 0x39:
            if len(b) == 6 and b[4] == 0 and pending_add is not None:
                wire_add, pending_add = pending_add, None
            elif len(b) == 7 and b[4] == 2 and b[5] <= 1:
                wire_add = bool(b[5])
        elif not tx and len(b) == 7 and b[3:6] == bytes([0x0A, 1, 1]):
            ordinal += 1
            selected = True if all_add else bool(wire_add)
            headers[index] = (ordinal, selected, line)
            details.append((ordinal, selected, wire_add))
    if not headers:
        raise ValueError('Capture has no accepted counting START')
    return headers, details


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--add-source', choices=['wire'], default='wire',
                        help='Only captured 39 state; never infer ADD from 49 counts')
    parser.add_argument('--all-add', action='store_true')
    parser.add_argument('--output-dir', type=Path,
                        default=Path('/dev/shm/un260-controller-denom-live/capture'))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    events, malformed = frames_from_capture(args.capture)
    headers, details = prepare(events, args.all_add)
    print(f'Capture: complete frames={len(events)}; malformed/truncated fragments excluded={malformed}', flush=True)
    print('A source: absolute 0B only; every START clears counts and preserves denominations.', flush=True)
    print('ADD: ' + ('explicit ON scenario' if args.all_add else 'captured 39; unknown uses OFF only for B/completion fixture'), flush=True)
    print('Scope: prefixes before the first accepted 0A are excluded; final-only runs cannot prove live timing.', flush=True)
    for ordinal, add, wire in details:
        print(f'RUN {ordinal}: ADD={int(add)} captured_ADD={"unknown" if wire is None else int(wire)}', flush=True)
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='capture-', dir=args.output_dir) as directory:
        work = Path(directory)
        stub = work/'un260/lv_drivers/lv_drivers.h'
        stub.parent.mkdir(parents=True)
        stub.write_text('void uart_debug_printf(const char *, ...);\n')
        fixture = work/'capture.txt'
        with fixture.open('w') as out:
            for index, (tx, line, frame) in enumerate(events):
                if index in headers:
                    ordinal, add, startline = headers[index]
                    out.write(f'R {ordinal} {int(add)} {startline}\n')
                if not tx and frame[3] in (0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x49, 0x3B):
                    out.write(f'F {line} {frame.hex(" ")}\n')
        sources = ['counting_control_reply.c', 'counting_info_reply.c', 'counting_denom_reply.c',
                   'counting_denom_query_service.c', 'counting_report_sync.c',
                   'counting_reject_sn_reply.c', 'counting_data_store.c']
        executable = work/'test'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare',
                        '-g', '-O1', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-no-pie', '-I'+str(work), '-I'+str(root), str(root/'tools/test_denom_capture.c'),
                        *[str(root/'un260/counting'/name) for name in sources], '-o', str(executable)], check=True)
        return subprocess.run([str(executable), str(fixture)]).returncode


if __name__ == '__main__':
    raise SystemExit(main())
