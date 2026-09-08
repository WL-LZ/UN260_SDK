#!/usr/bin/env python3
"""Summarize existing PERF windows without mixing idle, entry and motion.

FPS is the firmware's rounded window rate, not a per-frame duration. `h` and
`out` are per-window avg/p95/max (us); never average their p95 as a global p95.
RENDER n>=10 is a sustained-render proxy, NOT proof the user was scrolling.
"""
import argparse
import collections
import hashlib
import json
import re
from pathlib import Path


def summarize(path):
    raw = path.read_bytes()
    text = raw.decode('utf-8-sig', errors='replace')
    pattern = re.compile(r'PERF page=(\w+)\(\d+\) act=(\w+) fps=(\d+) n=(\d+) '
                         r'inv=(\d+)/(\d+)/(\d+) h=(\d+)/(\d+)/(\d+) '
                         r'loop=(\d+)/(\d+)/(\d+) out=(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)')
    groups = collections.defaultdict(list)
    for match in pattern.finditer(text):
        page, act = match.group(1, 2)
        v = list(map(int, match.groups()[2:]))
        group = 'sustained_proxy' if act == 'RENDER' and v[1] >= 10 else act.lower()
        groups[page, group].append(v)
    rows = []
    for (page, group), values in sorted(groups.items()):
        frames = sum(v[1] for v in values)
        weighted = lambda index: round(sum(v[index]*v[1] for v in values)/frames, 2) if frames else 0
        rows.append(dict(page=page, group=group, windows=len(values), frames=frames,
                         fps_min=min(v[0] for v in values), fps_max=max(v[0] for v in values),
                         fps_window_mean=round(sum(v[0] for v in values)/len(values), 2),
                         handler_mean_us=weighted(5), flush_mean_us=weighted(11),
                         pan_mean_us=weighted(14), vsync_mean_us=weighted(15),
                         mirror_mean_us=weighted(16), max_handler_us=max(v[7] for v in values)))
    return dict(source=str(path), sha256=hashlib.sha256(raw).hexdigest(),
                caveat='sustained_proxy includes animations; compare identical actions and debug settings only', rows=rows)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    result = summarize(args.log)
    report = json.dumps(result, indent=2, ensure_ascii=False)
    print(report)
    if args.json:
        args.json.write_text(report + '\n', encoding='utf-8')
