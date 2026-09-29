from test_i18n_support import with_i18n
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-cashbook-store-') as tmp:
    tmp=Path(tmp);data=tmp/'state';usb=tmp/'usb';usb.mkdir();exe=tmp/'test'
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-g',f'-I{root}',f'-DCASHBOOK_DIRECTORY="{data}"',f'-DCASHBOOK_USB_DIRECTORY="{usb}"',str(root/'tools/test_cashbook_store.c'),str(root/'un260/workspace/cashbook.c'),str(root/'un260/storage/cashbook_store.c'),*map(str,with_i18n([],root)),'-pthread','-Wl,--wrap=fsync','-o',str(exe)],check=True)
    def run(*args):subprocess.run([str(exe),*args],check=True,timeout=30)
    run();run('reload')
    reports=list(usb.glob('*.csv'));assert len(reports)==1
    text=reports[0].read_text();assert '10000' in text and 'DAY_CLOSE' in text and 'Do not export names' not in text
    journal=data/'cashbook.v1';original=journal.read_bytes()
    journal.write_bytes(original+b'partial');run('reload');assert journal.read_bytes()==original
    run('sync-failure')
    corrupted=bytearray(original);corrupted[-1]^=1;journal.write_bytes(corrupted);run('corrupt');assert journal.read_bytes()==corrupted
