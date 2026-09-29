#!/usr/bin/env python3
"""Pure host test: locale changes must not mutate operations/acknowledgements."""
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-i18n-message-') as temp:
    exe=Path(temp)/('test.exe' if os.name=='nt' else 'test')
    command=[os.environ.get('CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),
        str(root/'tools/test_i18n_messages.c'),
        str(root/'un260/lv_system/ui_message.c'),str(root/'un260/lv_system/ui_report_i18n.c'),
        str(root/'un260/lv_components/ui_notice_state.c'),
        str(root/'un260/data_collection/data_collection.c'),
        str(root/'un260/counting/counting_reject_reason.c'),'-o',str(exe)]
    subprocess.run(command,check=True)
    subprocess.run([str(exe),str(Path(temp)/'report.html')],check=True)
    subprocess.run([os.environ.get('NODE','node'),str(root/'tools/test_report_i18n.js'),str(Path(temp)/'report.html')],check=True)
