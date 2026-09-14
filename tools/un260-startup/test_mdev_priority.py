#!/usr/bin/env python3
"""Run production coldplug pipeline in a fake sysfs; never load modules."""
from pathlib import Path
import os,subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
path=repo/'target/d211/d213_devkitf/rootfs_overlay/etc/init.d/S10mdev'
source=path.read_text()
subprocess.run(['/bin/sh','-n',str(path)],check=True)
with tempfile.TemporaryDirectory(prefix='un260-coldplug-') as directory:
    tmp=Path(directory);(tmp/'bin').mkdir();(tmp/'sys').mkdir()
    for i,value in enumerate(['aliasB','aliasA','aliasB']):
        p=tmp/'sys'/str(i);p.mkdir();(p/'modalias').write_text(value+'\n')
    modprobe=tmp/'bin/modprobe'
    modprobe.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$TEST_OUTPUT"\n')
    modprobe.chmod(0o755)
    for fallback in (False,True):
        text=source.split('\ncase "$1" in',1)[0].replace('/sys/',str(tmp/'sys')+'/')
        text=text.replace('start-stop-daemon','mock_start_daemon')
        text+='\nmock_start_daemon() { return 0; }\n'
        if fallback:
            text+='command() { return 1; }\n'
        text+='start\n'
        script=tmp/'test.sh';script.write_text(text)
        result=tmp/'args'
        env=dict(os.environ,PATH=str(tmp/'bin')+':/usr/bin:/bin',TEST_OUTPUT=str(result))
        subprocess.run(['/bin/sh',str(script)],env=env,check=True,timeout=5)
        assert result.read_text().splitlines()==['-abq','aliasA','aliasB']
        print('PASS synchronous complete module discovery, fallback='+str(fallback))
