#!/usr/bin/env python3
"""Source keys must fit in descriptors and notice snapshots without truncation."""
from pathlib import Path
import json
import re

root=Path(__file__).resolve().parents[1]
header=(root/'un260/lv_system/ui_message.h').read_text(encoding='utf-8')
capacity=int(re.search(r'#define UI_MESSAGE_KEY_CAPACITY (\d+)',header).group(1))
notice=(root/'un260/lv_components/ui_notice_state.h').read_text(encoding='utf-8')
assert re.search(r'#define UI_NOTICE_TITLE_CAPACITY UI_MESSAGE_KEY_CAPACITY',notice)
assert int(re.search(r'#define UI_NOTICE_DETAIL_CAPACITY (\d+)',notice).group(1))>=capacity
keys=set()
for folder in ['locales','fragments']:
    for path in (root/'i18n'/folder).glob('*.json'):
        for messages in json.loads(path.read_text(encoding='utf-8')).values():
            if isinstance(messages,dict):keys.update(messages)
assert keys
for key in keys:
    assert len(key.encode('utf-8'))<capacity, f'Source key exceeds snapshot capacity: {key!r}'
longest=max(len(key.encode('utf-8')) for key in keys)
print(f'i18n source keys: PASS ({len(keys)} keys, max {longest} UTF-8 bytes, capacity {capacity})')
