from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-cash-store-') as directory:
 p=Path(directory);state=p/'state';exe=p/'test'
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie',f'-DUI_STATE_DIR="{state}"','-I.',
  'tools/test_cash_store.c','un260/workspace/cash_tools.c','un260/storage/cash_journal_store.c','un260/storage/storage_worker.c','-pthread','-Wl,--wrap=fsync','-o',str(exe)],cwd=r,check=True)
 for mode in ['write','read','legacy','read']:subprocess.run([str(exe),mode],check=True)
 file=state/'cash_journal.v1';data=bytearray(file.read_bytes());data[32]^=1;file.write_bytes(data)
 subprocess.run([str(exe),'corrupt'],check=True);assert file.read_bytes()==data
 print('PASS all persistent storage scenarios; corrupt bytes unchanged; no production state used')
