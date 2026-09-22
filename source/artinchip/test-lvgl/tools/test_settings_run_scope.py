#!/usr/bin/env python3
"""RUN is kept on Debug, three diagnostics and the data-collection page."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
pages = root / 'un260/lv_core'
callers = {}
for path in pages.glob('page_*.c'):
    code = path.read_text(encoding='utf-8')
    calls = len(re.findall(r'\bsettings_detail_add_run\s*\(', code))
    if calls:
        callers[path.name] = calls
    if path.name != 'page_06_settings.c':
        assert not re.search(r'"(?:RUN|Run|run)"', code), f'Duplicate private RUN button: {path.name}'
assert callers == {'page_10_debug.c': 1, 'page_12_sensor.c': 1, 'page_28_get_image.c': 1, 'page_31_get_wave.c': 1}, callers
collection = (pages / 'page_06_settings.c').read_text(encoding='utf-8')
assert 'dc_btn_start' in collection and 'data_collect_start_btn_event_cb' in collection
assert collection.count('"RUN"') == 1
assert '"RUN",true,data_collect_start_btn_event_cb' in collection
assert 'app_command_runtime_request_diagnostic_run()' in collection
assert '"End collection"' in collection
calibration = (pages / 'page_09_cis_cala.c').read_text(encoding='utf-8')
assert 'Press RUN' not in calibration and 'press RUN' not in calibration
assert '"Start", true, cis_start' in calibration
print('PASS RUN scope: Debug / Sensor / Image view / Magnetic image / Data collection; collection callback preserved, calibration instructions updated')
