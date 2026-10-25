#!/usr/bin/env python3
"""Replay an external UART capture against production A-region counting modules.

The private capture is never copied into the repository. --add-source wire uses
only captured 39 requests/acknowledgements; controller explicitly supplies test
preconditions inferred from independent 0E totals and the number of 49 notes.
The latter is NOT production mode detection. --all-add exercises the same
continuous capture with ADD enabled at every START (CLEAR acknowledgements stay).
"""
from pathlib import Path
import argparse
import collections
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


def denom_table(events):
    current = None
    complete = None
    for tx, _, b in events:
        if tx or b[3] != 0x0B or len(b) != 16:
            continue
        if b[4:15] == bytes(11):
            current = collections.Counter()
        elif b[4:15] == bytes([255]) * 11:
            if current is not None:
                complete = current
            current = None
        elif current is not None:
            try:
                value, count = int(b[4:12]), int(b[12:15])
            except ValueError:
                current = None
                continue
            current[value] += count
    return complete


def prepare(events, source, all_add):
    starts = [i for i, (tx, _, b) in enumerate(events)
              if not tx and len(b) == 7 and b[3:6] == bytes([0x0A, 1, 1])]
    if not starts:
        raise ValueError('Capture has no accepted counting START')
    starts.append(len(events))
    run_headers, details = {}, []
    base_qty = 0
    wire_add = None
    pending_add = None
    cursor = 0
    for ordinal, (begin, finish) in enumerate(zip(starts, starts[1:]), 1):
        for tx, _, b in events[cursor:begin]:
            if tx and b[3] == 0x39 and len(b) == 6:
                pending_add = bool(b[4])
            elif not tx and b[3] == 0x39:
                if len(b) == 6 and b[4] == 0 and pending_add is not None:
                    wire_add, pending_add = pending_add, None
                elif len(b) == 7 and b[4] == 2 and b[5] <= 1:
                    wire_add = bool(b[5])
            elif not tx and b[3] == 0x3B and b[4] == 1:
                base_qty = 0
        block = events[begin:finish]
        stop_index = None
        last_qty = last_amount = note_count = 0
        for offset, (tx, _, b) in enumerate(block):
            if tx:
                continue
            if b[3] == 0x49 and len(b) == 24:
                try:
                    note_count += int(b[4:11]) > 0
                except ValueError:
                    pass
            elif b[3] == 0x0E and len(b) == 13:
                amount, qty = int.from_bytes(b[4:8], 'big'), int.from_bytes(b[8:10], 'big')
                if b[11] <= 1:
                    if qty or amount:
                        last_qty, last_amount = qty, amount
                elif 2 <= b[11] <= 5:
                    if qty:
                        last_qty, last_amount = qty, amount
                    stop_index = offset
                    break
        table = denom_table(block[stop_index + 1:]) if stop_index is not None else None
        skipped = ordinal == 1 or stop_index is None or table is None
        if base_qty and last_qty == base_qty + note_count:
            inferred, reason = True, '0E_total=retained_total+49_count'
        elif note_count and last_qty == note_count:
            inferred, reason = False, '0E_total=49_count (fresh controller result)'
        elif not note_count and not last_amount:
            inferred, reason = bool(wire_add), 'CNT/no monetary 49; captured ADD or OFF default'
        else:
            inferred, reason = bool(wire_add), 'unresolved/incomplete capture; captured ADD or OFF default'
        selected = True if all_add else (bool(wire_add) if source == 'wire' else inferred)
        run_headers[begin] = (ordinal, selected, skipped, last_qty, last_amount, events[begin][1], table or {})
        details.append((ordinal, selected, wire_add, note_count, reason, skipped))
        base_qty = last_qty
        # Process settings/clears after this count before the next START.
        cursor = begin + (stop_index + 1 if stop_index is not None else 1)
    return run_headers, details


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--add-source', choices=['wire', 'controller'], default='wire')
    parser.add_argument('--all-add', action='store_true')
    args = parser.parse_args()
    events, malformed = frames_from_capture(args.capture)
    headers, details = prepare(events, args.add_source, args.all_add)
    print(f'Capture: complete frames={len(events)}; malformed/truncated fragments excluded={malformed}', flush=True)
    print('ADD source: ' + ('forced ON continuous replay' if args.all_add else args.add_source), flush=True)
    print('Scope: first count excluded because its retained starting table is absent; first complete 0B seeds later counts.', flush=True)
    for ordinal, add, wire, notes, reason, skipped in details:
        print(f'RUN {ordinal}: ADD={int(add)} captured_ADD={"unknown" if wire is None else int(wire)} independent_49={notes}; {reason}; {"EXCLUDED" if skipped else "CHECK"}', flush=True)
    if any(wire is not None and add != wire for _, add, wire, _, _, skipped in details if not skipped):
        print('Scenario override: selected test ADD differs from captured 39 state; this is an explicit test precondition, not an inferred missing request.', flush=True)
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='un260-denom-capture-') as directory:
        work = Path(directory)
        stub = work/'un260/lv_drivers/lv_drivers.h'
        stub.parent.mkdir(parents=True)
        stub.write_text('void uart_debug_printf(const char *, ...);\n')
        fixture = work/'capture.txt'
        with fixture.open('w') as out:
            for index, (tx, line, frame) in enumerate(events):
                if index in headers:
                    ordinal, add, skip, qty, amount, startline, table = headers[index]
                    out.write(f'R {ordinal} {int(add)} {int(skip)} {qty} {amount} {startline}\n')
                    for value, pcs in sorted(table.items()):
                        out.write(f'X {value} {pcs}\n')
                if not tx and frame[3] in (0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x49, 0x3B):
                    out.write(f'F {line} {frame.hex(" ")}\n')
        sources = ['counting_control_reply.c', 'counting_info_reply.c', 'counting_denom_reply.c',
                   'counting_denom_query_service.c', 'counting_report_sync.c',
                   'counting_reject_sn_reply.c', 'counting_data_store.c']
        executable = work/'test'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare',
                        '-g', '-O1', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-I'+str(work), '-I'+str(root), str(root/'tools/test_denom_capture.c'),
                        *[str(root/'un260/counting'/name) for name in sources], '-o', str(executable)], check=True)
        return subprocess.run([str(executable), str(fixture)]).returncode


if __name__ == '__main__':
    raise SystemExit(main())
