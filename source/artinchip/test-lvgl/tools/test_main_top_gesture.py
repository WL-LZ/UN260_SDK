#!/usr/bin/env python3
"""Main pull-down is now Quick controls; exercise real LVGL and raw touch.

The retained Innovation implementation has its own transition/capture tests.
This entry point verifies current routing, then runs Main's actual integration
harness including the quick drawer, swaps and short-tap regression cases.
"""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lvgl-dir',type=Path,default=ROOT.parents[1]/'third-party/lvgl-8.3.2')
args=parser.parse_args()
main=(ROOT/'un260/lv_core/page_01_main.c').read_text()
manager=(ROOT/'un260/lv_core/lv_page_manager.c').read_text()
boot=(ROOT/'un260/app_service/app_boot_runtime.c').read_text()
assert 'page_32_innovation_handle_attach' not in main
assert 'page_32_innovation_schedule_preload' not in manager
assert 'UI_PAGE_INNOVATION_CENTER' not in boot
assert 'ui_page_32_innovation_create' in manager  # retained, not deleted
assert 'page_01_main_pointer' in main and 'page_01_main_quick_attach' in main
assert 'page_01_main_quick_refresh_data' in manager
subprocess.run([sys.executable,str(ROOT/'tools/test_main_view.py'),
    '--lvgl-dir',str(args.lvgl_dir),'--sanitize','address,undefined'],check=True)
