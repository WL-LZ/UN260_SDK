#!/usr/bin/env python3
"""Inventory C display literals; reject untranslated literals at known UI sinks.

This is a lexical regression check, not a substitute for tracing dynamic values
or inspecting raster assets. Source references make every exclusion reviewable.
"""
import argparse
import ast
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r'/\*.*?\*/|//[^\n]*|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|[^\s]', re.S)
IDENT = re.compile(r'^[A-Za-z_]\w*$')
SINKS = {
    'lv_label_set_text': {1}, 'lv_label_set_text_static': {1},
    'lv_label_set_text_fmt': {1}, 'lv_textarea_set_placeholder_text': {1},
    'lv_dropdown_set_options': {1}, 'lv_dropdown_set_options_static': {1},
    'lv_roller_set_options': {1}, 'lv_table_set_cell_value': {3},
    'lv_table_set_cell_value_fmt': {3}, 'lv_list_add_btn': {2},
    'lv_list_add_text': {1}, 'lv_settings_label': {1},
    'lv_settings_button': {5}, 'ui_notice_post': {2, 3},
    'settings_detail_dialog_show': {0, 1, 2, 3},
    'settings_detail_action_block': {1},
}
KEY_SINKS = {'ui_notice_post_text': {2, 3}, 'ui_notice_post_message': {2},
             'settings_detail_action_block': {1}, 'app_ui_runtime_notice_started': {1}}
SINKS.update(KEY_SINKS)
MARKERS = {'ui_tr', 'ui_trc', 'ui_trn', 'ui_tr_for', 'UI_N_', '_', '_p'}
# Display values with stable external identity; never translate currency/protocol
# identifiers or change customer data. Other short English words still fail.
INVARIANTS = {
    'UN260', 'USB', 'SD', 'UART', 'LVGL', 'CPU', 'RAM', 'FPS', 'FPGA', 'PNG', 'CSV',
    'CNY', 'USD', 'EUR', 'GBP', 'KRW', 'EGP', 'ISK', 'PHP', 'SOS', 'TRY',
    'AED', 'SAR', 'OMR', 'QAR', 'MAD', 'DZD', 'INR', 'PKR', 'IQD',
    'MDC', 'SDC', 'CNT', 'MUL', 'AUT', 'PS1', 'PS2', 'PS3', 'PS4',
    'PS5L', 'PS5R', 'SAFE', 'RJ', 'ST', 'CIS', 'UV', 'IR', 'MG', 'MT',
}

def string_value(token):
    token = re.sub(r'^(?:u8|u|U|L)(?=")', '', token)
    if not token.startswith('"'):
        return None
    try:
        return ast.literal_eval(token)
    except (ValueError, SyntaxError):
        return token[1:-1]

def translatable(value):
    # Format conversions alone, separators, numeric placeholders and keyboard
    # characters are data. Explanatory words around conversions are UI copy.
    if value in INVARIANTS:
        return False
    plain = re.sub(r'%(?:\d+\$)?[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?[hljztL]*[diuoxXfFeEgGaAcspn%]', '', value)
    plain = re.sub(r'0[xX][0-9a-fA-F]+', '', plain)
    return bool(re.search(r'[A-Za-z]{2}', plain) or any(ord(c) > 127 and c.isalpha() for c in plain))

def analyze(source, name):
    tokens = [(m.group(), m.start(), m.end()) for m in TOKEN.finditer(source)
              if not m.group().startswith(('/*', '//'))]
    calls = []
    for i in range(len(tokens) - 1):
        if not IDENT.match(tokens[i][0]) or tokens[i+1][0] != '(':
            continue
        depth, begin, args = 0, i+2, []
        for j in range(i+2, len(tokens)):
            token = tokens[j][0]
            if token in ('(', '[', '{'):
                depth += 1
            elif token == ')' and depth == 0:
                args.append((begin, j)); calls.append((tokens[i][0], i, j, args)); break
            elif token in (')', ']', '}'):
                depth -= 1
            elif token == ',' and depth == 0:
                args.append((begin, j)); begin = j+1
    covered, source_keys = set(), set()
    localized = []
    for function, i, j, args in calls:
        if function in MARKERS:
            for k in range(i+2, j):
                value = string_value(tokens[k][0])
                if value is not None:
                    if function != 'UI_N_':
                        covered.add(k)
                    else:
                        source_keys.add(k)
                    localized.append({'file': name, 'line': source.count('\n', 0, tokens[k][1])+1,
                                      'marker': function, 'text': value})
    findings = []
    seen = set()
    for function, _, _, args in calls:
        if function not in SINKS:
            continue
        for arg in SINKS[function]:
            if arg >= len(args):
                continue
            start, end = args[arg]
            for k in range(start, end):
                value = string_value(tokens[k][0])
                if value is None or k in covered or (function in KEY_SINKS and k in source_keys) or not translatable(value) or k in seen:
                    continue
                seen.add(k)
                findings.append({'file': name, 'line': source.count('\n', 0, tokens[k][1])+1,
                                 'sink': function, 'argument': arg, 'text': value})
    return findings, localized

def scan(root=ROOT):
    findings, localized, files = [], [], 0
    for path in sorted((root/'un260').rglob('*')):
        if path.suffix not in ('.c', '.h', '.inc') or not path.is_file():
            continue
        name = path.relative_to(root).as_posix()
        if name.startswith(('un260/font/', 'un260/lv_resources/', 'un260/lv_system/i18n/')):
            continue
        raw, marked = analyze(path.read_text(encoding='utf-8'), name)
        findings.extend(raw); localized.extend(marked); files += 1
    return {'schema': 1, 'files': files, 'marked_occurrences': len(localized),
            'unique_marked_literals': len({item['text'] for item in localized}),
            'unlocalized_display_literals': findings, 'localized': localized,
            'scope': 'C/C++ lexical UI sinks; dynamic result descriptors, static tables, reports and baked assets require the accompanying manual audit'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = scan()
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes((json.dumps(result, ensure_ascii=False, indent=2)+'\n').encode('utf-8'))
    remaining = result['unlocalized_display_literals']
    print(f"Visible text: {result['files']} files, {result['marked_occurrences']} marked literals, {len(remaining)} unlocalized sink literals")
    for item in remaining[:60]:
        print(f"{item['file']}:{item['line']}: {item['sink']}: {item['text']!r}")
    if args.check and remaining:
        raise SystemExit(1)

if __name__ == '__main__':
    main()
