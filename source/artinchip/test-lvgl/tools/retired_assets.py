"""Explicit retirement list, never a filename-based automatic unused-image purge."""
import hashlib
import json
from pathlib import Path
import re


def load_retired(root, check_references=True):
    root = Path(root)
    policy = json.loads((root / 'tools/retired_assets.json').read_text())
    if policy['schema'] != 1:
        raise RuntimeError('Unsupported retirement policy')
    result = {}
    for entry in policy['retired']:
        name = entry['name']
        if not re.fullmatch(r'(ui_icons|settings_icons)/[A-Za-z0-9_]+\.png', name):
            raise RuntimeError('Retirement outside reviewed icon scope: ' + name)
        if name in result or not re.fullmatch('[0-9a-f]{64}', entry['sha256']):
            raise RuntimeError('Invalid retirement entry: ' + name)
        path = root / 'aic_ui/lvgl_data' / name
        if path.is_symlink() or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('Retired source changed; review instead of removing: ' + name)
        result[name] = entry
    if check_references:
        # Macro/dynamic-name call sites were manually reviewed; literal tokens
        # also catch future reuse of the existing UI_ICON/ENTRY_ICON API.
        for path in (root / 'un260').rglob('*'):
            if path.suffix not in ('.c', '.h', '.inc') or 'font' in path.parts:
                continue
            text = path.read_text(errors='replace')
            for name, entry in result.items():
                if entry['reference_token'] in text:
                    raise RuntimeError('Retired icon referenced by %s: %s' % (path, name))
    return result
