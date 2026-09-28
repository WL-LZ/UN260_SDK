from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-cashbook-') as tmp:
    exe=Path(tmp)/'cashbook'
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-g',f'-I{root}',str(root/'tools/test_cashbook.c'),str(root/'un260/workspace/cashbook.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
