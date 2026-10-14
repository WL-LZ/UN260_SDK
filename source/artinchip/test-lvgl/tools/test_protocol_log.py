from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-protocol-log-') as d:
 p=Path(d);(p/'usb').mkdir()
 cmd=['gcc','-std=gnu11','-g','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-pthread',f'-I{root}',f'-DUI_STATE_DIR="{p}/state"',f'-DPROTOCOL_LOG_USB_DIR="{p}/usb"',str(root/'tools/test_protocol_log.c'),str(root/'un260/storage/protocol_log.c'),'-o',str(p/'test')]
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
 cmd=[part.replace(str(p/'state'),str(p/'limit-state')) for part in cmd]
 cmd[1:1]=['-DSESSION_LIMIT=1024','-DTEST_LOG_LIMIT=1']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
