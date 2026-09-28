from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-workspace-service-') as directory:
    exe=Path(directory)/'test'
    subprocess.run(['cc','-std=gnu11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',f'-I{root}',str(root/'tools/test_workspace_service.c'),str(root/'un260/workspace/workspace_model.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
