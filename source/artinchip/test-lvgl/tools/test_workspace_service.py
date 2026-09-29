from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-workspace-service-') as directory:
    exe=Path(directory)/('test.exe' if os.name=='nt' else 'test')
    command=[os.environ.get('CC','cc'),'-std=gnu11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',f'-I{root}',str(root/'tools/test_workspace_service.c'),str(root/'un260/workspace/workspace_model.c'),'-o',str(exe)]
    if os.name!='nt':command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
    subprocess.run(command,check=True)
    subprocess.run([str(exe)],check=True)
