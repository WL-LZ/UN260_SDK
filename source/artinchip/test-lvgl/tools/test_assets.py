#!/usr/bin/env python3
"""Verify every generated pixel plus incrementality, nested names and new assets."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
dest = ROOT / 'aic_ui/generated_assets'
manifest = json.loads((dest / 'manifest.json').read_text())
assert manifest['total_raw_bytes'] <= 256 * 1024
assert all(max(e['width'], e['height']) <= 64 for e in manifest['entries'])
assert all((ROOT / 'aic_ui/lvgl_data' / e['name']).is_file() for e in manifest['external'])
for e in manifest['entries']:
    text = (dest / e['c_file']).read_text()
    literals = re.findall(r'^"((?:\\x[0-9a-f]{2})+)"$', text, re.M)
    pixels = bytes.fromhex(''.join(literals).replace('\\x', ''))
    assert len(pixels) == e['bytes'], e['name']
    assert hashlib.sha256(pixels).hexdigest() == e['pixel_sha256'], e['name']
    decoded = Image.frombytes('RGBA' if e['alpha'] else 'RGB',
                              (e['width'], e['height']), pixels, 'raw',
                              'BGRA' if e['alpha'] else 'BGR').convert('RGBA')
    with Image.open(ROOT / 'aic_ui/lvgl_data' / e['name']) as original:
        assert decoded.tobytes() == original.convert('RGBA').tobytes(), e['name']
print('assets: all %d images pixel-identical (including alpha)' % len(manifest['entries']))
spec = importlib.util.spec_from_file_location('converter', ROOT / 'tools/convert_lvgl_assets.py')
converter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(converter)
fixture = Path(tempfile.mkdtemp(prefix='un260-assets-test-'))
converter.ROOT = fixture
sys.argv = ['convert_lvgl_assets.py']
source = fixture / 'aic_ui/lvgl_data'
source.mkdir(parents=True)
(source / 'a').mkdir()
Image.new('RGBA', (3, 2), (30, 60, 90, 128)).save(source / 'a.png')
Image.new('RGB', (2, 1), (10, 20, 30)).save(source / 'a' / 'b.png')
converter.main()
out = fixture / 'aic_ui/generated_assets'
first = json.loads((out / 'manifest.json').read_text())
assert [e['name'] for e in first['entries']] == ['a.png', 'a/b.png']
times = {p: p.stat().st_mtime_ns for p in out.rglob('*') if p.is_file()}
converter.main()
assert all(p.stat().st_mtime_ns == t for p, t in times.items())
Image.new('RGBA', (1, 1), (1, 2, 3, 0)).save(source / 'new.png')
converter.main()
assert len(json.loads((out / 'manifest.json').read_text())['entries']) == 3
(source / 'new.png').unlink()  # Test-owned input only.
converter.main()
assert not (out / 'new.png.c').exists()
sys.argv = ['convert_lvgl_assets.py', '--check']
converter.main()
# Compile the real generated registry and C payloads for the nested fixture.
c_test = fixture / 'test.c'
c_test.write_text('''#include "aic_ui/compiled_asset.h"
#include <assert.h>
int main(void) {
const un260_compiled_asset_t *a=un260_compiled_asset_find("L:/usr/local/share/lvgl_data/a.png");
assert(a && a->width==3 && a->has_alpha && a->pixels[0]==90 && a->pixels[2]==30 && a->pixels[3]==128);
assert(un260_compiled_asset_find("/usr/local/share/lvgl_data/a/b.png"));
assert(!un260_compiled_asset_find("L:/usr/local/share/lvgl_data/missing.png"));
assert(!un260_compiled_asset_find("/tmp/a.png"));
assert(un260_compiled_asset_count()==2);
return 0; }
''')
exe = fixture / 'test'
subprocess.run(['cc', '-I' + str(ROOT), str(c_test)] + [str(p) for p in out.rglob('*.c')] + ['-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
# A growing icon must migrate back to the external path, removing only its
# generated .c. New large images must never expand the executable.
sys.argv = ['convert_lvgl_assets.py']
Image.new('RGB', (1280, 400), (1, 2, 3)).save(source / 'background.png')
Image.new('RGB', (65, 64), (1, 2, 3)).save(source / 'a.png')
converter.main()
assert not (out / 'a.png.c').exists()
assert (source / 'a.png').exists()
policy = json.loads((out / 'manifest.json').read_text())
assert {e['name'] for e in policy['external']} == {'a.png', 'background.png'}
for i in range(20):
    Image.new('RGB', (64, 64), (i, 2, 3)).save(source / ('icon%02d.png' % i))
converter.main()
policy = json.loads((out / 'manifest.json').read_text())
assert policy['total_raw_bytes'] <= 256 * 1024
assert any(e['reason'] == 'budget' for e in policy['external'])
sys.argv = ['convert_lvgl_assets.py', '--check']
converter.main()
print('assets: PASS (determinism, nested lookup, new image, stale generated removal, C byte order)')
print('Test artifacts:', fixture)
