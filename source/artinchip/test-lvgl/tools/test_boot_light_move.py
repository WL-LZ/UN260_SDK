#!/usr/bin/env python3
"""Real IPC survives its directory being moved, as /dev is during switch-root."""
import ast,array,fcntl,os,socket,subprocess,tempfile,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
tree=ast.parse((root/'tools/test_boot_light.py').read_text())
fixture=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='fixture' for t in n.targets))
with tempfile.TemporaryDirectory(prefix='un260-ipc-move-') as d:
 p=Path(d);ipc=p/'ipc';ipc.mkdir();src=p/'test.c';src.write_text(fixture);exe=p/'test'
 defines=['-DBOOT_LIGHT_'+name+'="'+str(ipc/file)+'"' for name,file in [('SOCKET','socket'),('LOCK','lock'),('READY','ready')]]
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-I'+str(root),*defines,str(src),'-Wl,--wrap=open','-Wl,--wrap=ioctl','-lz','-o',str(exe)],check=True)
 fb=p/'fb';fb.write_bytes(bytes(1280*400*8))
 native=subprocess.Popen([str(exe)],env=dict(os.environ,TEST_FB=str(fb)))
 try:
  end=time.monotonic()+5
  while not (ipc/'ready').exists():
   assert native.poll() is None and time.monotonic()<end;time.sleep(.01)
  moved=p/'moved';ipc.rename(moved)
  with socket.socket(socket.AF_UNIX,socket.SOCK_SEQPACKET) as client:
   client.settimeout(2);client.connect(str(moved/'socket'));client.send(b'T')
   data,ancillary,flags,_=client.recvmsg(8,socket.CMSG_SPACE(4))
   assert len(data)==8 and not flags
   fd=array.array('i');fd.frombytes(ancillary[0][2]);assert len(fd)==1
   assert native.wait(timeout=3)==0
   assert os.fstat(fd[0]).st_size==1280*400*8;os.close(fd[0])
  assert not (moved/'socket').exists() and not (moved/'ready').exists()
  with (moved/'lock').open('r+') as lock:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 finally:
  if native.poll() is None:native.terminate();native.wait(timeout=3)
print('PASS moved IPC directory, real FD transfer, cleanup and released lease')
