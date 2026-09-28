"""Menu glyph PNGs are build inputs; runtime uses their verified compiled registry."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'aic_ui/generated_assets/manifest.json').read_text())
entries={e['name']:e for e in manifest['entries']}
files=list((root/'aic_ui/lvgl_data/menu_icons').glob('*.png'))
assert files,'Generate Menu assets first'
for path in files:
    key='menu_icons/'+path.name
    assert key in entries,'Menu asset would require a missing external file: '+key
    assert hashlib.sha256(path.read_bytes()).hexdigest()==entries[key]['source_sha256'],key
print('PASS all',len(files),'Menu glyphs embedded; no duplicate external PNGs required')
