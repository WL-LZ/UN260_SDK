#!/usr/bin/env python3
"""Compatibility entrypoint. Both themes and all released locales stay in sync."""
import argparse,runpy
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--compact',action='store_true');p.parse_args()
runpy.run_path(str(Path(__file__).with_name('gen_boot_handwriting.py')),run_name='__main__')
