#!/usr/bin/env python3
"""Full production history/storage/export path; temporary files only."""
from pathlib import Path
import subprocess,tempfile,os
from test_i18n_support import with_i18n
from test_main_view import function
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-history-abc-') as temp:
    work=Path(temp);store=work/'history';exe=work/'test'
    source=(ROOT/'un260/app_service/app_counting_runtime.c').read_text()
    (work/'history_runtime_under_test.h').write_text(function(source,'app_counting_runtime_reset_session'))
    parts=['tools/test_history_abc.c','un260/storage/storage_worker.c',
        'un260/counting/counting_history_service.c',
        'un260/counting/counting_data_store.c',
        'un260/history/history_record_snapshot.c','un260/history/history_record_reader.c',
        'un260/history/history_read_task.c','un260/history/history_record_detail.c',
        'un260/history/history_export_sn_parser.c','un260/history/history_query.c',
        'un260/counting/counting_serial_text.c','un260/counting/counting_info_reply.c','un260/protocol/protocol_frame.c',
        'un260/history/history_export_text.c','un260/lv_system/ui_report_i18n.c',
        'un260/counting/counting_reject_reason.c']
    sources=with_i18n([ROOT/p for p in parts],ROOT)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra',
        '-Werror','-Wno-misleading-indentation','-pthread','-fsanitize='+os.environ.get('UN260_TEST_SANITIZERS','undefined'),'-fno-sanitize-recover=all',
        '-I'+str(ROOT/'tools/tests/history_export_stubs'),'-I'+str(ROOT),'-I'+str(work),
        '-DUI_HISTORY_STORE_DIR="'+str(store)+'"',*map(str,sources),'-Wl,--wrap=malloc','-o',str(exe)],check=True)
    subprocess.run([str(exe),'write'],check=True)
    subprocess.run([str(exe),'read'],check=True)
