#!/usr/bin/env python3
"""Menu/List/Set real-LVGL layout and query/lifecycle regression fixture.
Uses the shared panel runner with real fonts, query algorithms and data stores.
Hardware, persistence and navigation endpoints are explicitly mocked.
"""
import sys
from pathlib import Path
from test_panel_main_view import main
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
    if '--work-dir' not in sys.argv:sys.argv += ['--work-dir','/tmp/un260-panel-pages-host']
    sys.argv += ['--harness',str(ROOT/'tools/test_panel_pages_view.c')]
    for part in ('un260/lv_core/ui_frame_commit.c','un260/counting/counting_serial_query.c',
                 'un260/counting/counting_serial_text.c','un260/lv_components/lv_nav_button.c',
                 'un260/lv_components/lv_damped_button.c'):
        sys.argv += ['--extra-source',str(ROOT/part)]
    main()
