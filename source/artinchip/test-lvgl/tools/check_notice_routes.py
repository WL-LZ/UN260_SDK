#!/usr/bin/env python3
"""Freeze reviewed top-notice routes, independently of renderer/style changes.

--update is an explicit review action, never part of a firmware build.
This source boundary inventory does not prove runtime branch coverage.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'tools/notice_routes.json'
API = re.compile(r'\b(ui_notice_\w+|app_setting_notice_(?:result|timeout)|app_ui_runtime_notice_started|app_local_setting_apply|ui_state_top_popup_\w+)\s*\(')
SKIP = {'un260/lv_components/ui_notice.c', 'un260/lv_components/ui_notice_state.c'}

def inventory(root):
    rows = []
    for path in sorted((root / 'un260').rglob('*')):
        if path.suffix not in ('.c', '.inc') or path.relative_to(root).as_posix() in SKIP:
            continue
        text = path.read_text()
        # Keep offsets while excluding comments; strings remain intact.
        text = re.sub(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*',
                      lambda m: m[0] if m[0].startswith('"') else re.sub(r'[^\n]', ' ', m[0]), text, flags=re.S)
        for match in API.finditer(text):
            start = match.end(); i = start; depth = 1; quoted = False
            while i < len(text) and depth:
                ch = text[i]
                if quoted and ch == '\\': i += 2; continue
                if ch == '"': quoted = not quoted
                elif not quoted:
                    if ch == '(': depth += 1
                    elif ch == ')': depth -= 1
                i += 1
            if depth: raise ValueError(f'Unbalanced call in {path}')
            if text[i:].lstrip().startswith('{'): continue  # definition, not a caller
            args = re.sub(r'\s+', ' ', text[start:i-1]).strip()
            rows.append({'file':path.relative_to(root).as_posix(), 'line':text.count('\n',0,match.start())+1,
                         'api':match[1], 'args':args})
    return rows

def signature(rows):
    return Counter((r['file'], r['api'], r['args']) for r in rows)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--update', action='store_true')
    parser.add_argument('--root', type=Path, default=ROOT)
    args = parser.parse_args()
    rows = inventory(args.root)
    manifest = args.root / 'tools/notice_routes.json'
    if args.update:
        manifest.write_text(json.dumps(rows, ensure_ascii=False, indent=2)+'\n')
    else:
        expected = signature(json.loads(manifest.read_text()))
        actual = signature(rows)
        if actual != expected:
            for label, changed in [('ADDED', actual-expected), ('REMOVED', expected-actual)]:
                for (file, api, values), count in changed.items():
                    print(f'{label} {count}x {file}: {api}({values})')
            raise SystemExit('Notice routes changed: review ownership/outcome/blockers, then explicitly update inventory.')
    print(f'PASS notice routes: {len(rows)} call sites in {len(set(r["file"] for r in rows))} files; style-independent boundary')

if __name__ == '__main__': main()
