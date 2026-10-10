#!/usr/bin/env python3
"""No font/Pillow dependency: stale welcome resources fail an ordinary make."""
from pathlib import Path
import json,hashlib
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'un260/font/boot_handwriting_manifest.json').read_text())
assert hashlib.sha256((root/'tools/gen_boot_handwriting.py').read_bytes()).hexdigest()==manifest['generator_sha256'], 'Welcome generator changed: regenerate reviewed artwork'
locales=[r['tag'] for r in json.loads((root/'i18n/locales.json').read_text()) if r['enabled'] and r['translations_ready']]
assert locales==[r['tag'] for r in manifest['locales']], 'Regenerate welcome artwork for released languages'
catalog={}
for p in sorted((root/'i18n/locales').glob('*.json'))+sorted((root/'i18n/fragments').glob('*.json')):
    for tag,entries in json.loads(p.read_text()).items():
        if isinstance(entries,dict) and 'Welcome' in entries:catalog[tag]=entries['Welcome']
for r in manifest['locales']:
    assert catalog[r['tag']]==r['text'], 'Regenerate translated welcome: '+r['tag']
    font=(root/'aic_ui/font'/r['font']) if r['font']=='OoohBaby-Regular.ttf' else (root/'tools/boot_handwriting_fonts'/r['font'])
    assert hashlib.sha256(font.read_bytes()).hexdigest()==r['font_sha256'], 'Welcome font changed: '+r['font']
for filename,want in manifest['outputs'].items():
    assert hashlib.sha256((root/filename).read_bytes()).hexdigest()==want, 'Stale welcome resource: '+filename
print('PASS handwritten welcome registry, translations and shared asset hashes')
