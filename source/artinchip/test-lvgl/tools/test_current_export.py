#!/usr/bin/env python3
"""Generate real current reports, then execute emitted scripts in a minimal DOM."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_i18n_support import with_i18n

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path)
args=parser.parse_args()

def run(directory):
    directory.mkdir(parents=True,exist_ok=True)
    binary=directory/'current-export-test'
    sources=with_i18n([ROOT/'un260/lv_system/ui_report_i18n.c',
        ROOT/'un260/counting/counting_data_store.c',
        ROOT/'un260/counting/counting_reject_reason.c',
        ROOT/'un260/machine_state/machine_state.c'],ROOT)
    subprocess.run([os.environ.get('CC') or shutil.which('cc') or 'gcc',
        '-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
        '-I'+str(ROOT/'tools/tests/history_export_stubs'),'-I'+str(ROOT),
        str(ROOT/'tools/test_current_export.c'),*(str(p) for p in sources),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(directory)],check=True)
    history_binary=directory/'history-ordinal-test'
    subprocess.run([os.environ.get('CC') or shutil.which('cc') or 'gcc',
        '-pthread','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
        '-I'+str(ROOT/'tools/tests/history_export_stubs'),'-I'+str(ROOT),
        str(ROOT/'tools/test_history_report_ordinals.c'),
        *(str(p) for p in with_i18n([ROOT/'un260/lv_system/ui_report_i18n.c'],ROOT)),
        *(str(ROOT/name) for name in ['un260/history/history_record_detail.c','un260/history/history_record_snapshot.c',
          'un260/history/history_record_reader.c','un260/storage/storage_worker.c','un260/history/history_export_sn_parser.c',
          'un260/history/history_export_text.c','un260/protocol/protocol_frame.c','un260/counting/counting_reject_reason.c']),
        '-o',str(history_binary)],check=True)
    subprocess.run([str(history_binary),str(directory)],check=True)
    subprocess.run([os.environ.get('NODE','node'),str(ROOT/'tools/test_current_export.js'),str(directory/'manifest.json'),str(directory/'history-manifest.json')],check=True)
if args.output:
    run(args.output.resolve())
else:
    with tempfile.TemporaryDirectory(prefix='un260-current-export-') as directory:
        run(Path(directory))
