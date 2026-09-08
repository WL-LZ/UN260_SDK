#!/usr/bin/env python3
"""Host integration: real strict extractor, real updater, generated deltas.
Never targets the host root; sync is stubbed, so this is NOT physical power-cut QA.
"""
from pathlib import Path
import hashlib, io, os, shutil, subprocess, tarfile, tempfile, importlib.util
root=Path(__file__).resolve().parents[2]
work=Path(tempfile.mkdtemp(prefix='un260-fast-delta-'))
spec=importlib.util.spec_from_file_location('delta',root/'tools/un260-update/build_delta.py')
delta=importlib.util.module_from_spec(spec);spec.loader.exec_module(delta)
unpack=work/'unpack'
subprocess.run(['cc','-O2','-Wall','-Wextra',str(root/'source/artinchip/test-lvgl/tools/un260_unpack.c'),'-lz','-o',str(unpack)],check=True)
updater=root/'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
def archive(path,files,extra=None):
    checks=''.join(f'{hashlib.sha256(d).hexdigest()}  payload/{p}\n' for p,(d,m) in sorted(files.items())).encode()
    meta=f'format=UN260_UPGRADE\nschema=1\nproduct=UN260\nversion=test\npackage_id={hashlib.sha256(checks).hexdigest()}\n'.encode()
    entries={'manifest.ini':(meta,0o644),'checksums.sha256':(checks,0o644),
             'install.tsv':(''.join(f'file|{m:04o}|{p}\n' for p,(d,m) in sorted(files.items())).encode(),0o644)}
    entries.update({'payload/'+p:v for p,v in files.items()})
    with tarfile.open(path,'w:gz',format=tarfile.USTAR_FORMAT) as t:
        m=tarfile.TarInfo('payload');m.type=tarfile.DIRTYPE;t.addfile(m)
        for p,(d,mode) in entries.items():
            m=tarfile.TarInfo(p);m.size=len(d);m.mode=mode;t.addfile(m,io.BytesIO(d))
        if extra: extra(t)
base={'usr/local/bin/test_lvgl':(b'old app',0o755),'usr/local/lib/liblvgl.so':(b'lib',0o755),
      'usr/local/share/lvgl_data/old.png':(b'old image',0o644),'etc/un260/package-version':(b'v1',0o644)}
target=dict(base);target['usr/local/bin/test_lvgl']=(b'new app',0o755)
target['usr/local/share/lvgl_data/new.png']=(b'new image',0o644)
del target['usr/local/share/lvgl_data/old.png']
base_upk=work/'base.upk';full=work/'full.upk';inc=work/'delta.upk'
archive(base_upk,base);archive(full,target);delta.build(base_upk,full,inc)
def run_case(name,package,initial,ok,crash=False):
    case=work/name;dev=case/'root';usb=case/'usb';upd=usb/'update';upd.mkdir(parents=True)
    for p,(d,m) in initial.items():
        f=dev/p;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(d);f.chmod(m)
    tool=dev/'usr/local/bin/un260_unpack';tool.parent.mkdir(parents=True,exist_ok=True);shutil.copy(unpack,tool)
    shutil.copy(package,upd/'UN260_UPDATE.upk')
    runner=case/'run.sh'
    runner.write_text(f'''#!/bin/sh
set -eu
UN260_UPDATER_LIBRARY_ONLY=1
. '{updater}'
ROOT_PREFIX='{dev}'
USB_MNT='{usb}'
UPDATE_DIR=$USB_MNT/update
BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk
STAGE_DIR=$UPDATE_DIR/.un260_stage
BACKUP_DIR=$UPDATE_DIR/.un260_backup
BACKUP_STATE=$BACKUP_DIR/state.tsv
PLAN_FILE=$UPDATE_DIR/.un260_plan
SEEN_TARGETS=$UPDATE_DIR/.un260_seen_targets
ARCHIVE_LIST=$UPDATE_DIR/.un260_archive.list
ARCHIVE_TYPES=$UPDATE_DIR/.un260_archive.types
CHECKSUM_TARGETS=$UPDATE_DIR/.un260_checksum_targets
PAYLOAD_FILES=$UPDATE_DIR/.un260_payload_files
INSTALLED_STATE_DIR=$ROOT_PREFIX/var/lib/un260-updater
INSTALLED_HASH_PATH=$INSTALLED_STATE_DIR/installed.fnv64
JOURNAL=$INSTALLED_STATE_DIR/transaction
STATUS_FILE=$USB_MNT/status
STATUS_TEMP=$USB_MNT/status.tmp
LOG=$USB_MNT/log
RESULT_FILE=$USB_MNT/result
sync() {{ :; }}
df() {{ printf 'Filesystem 1K-blocks Used Available Use%% Mounted\\nfixture 200000 1 199999 1%% /\\n'; }}
'''+('''rm() {
    command rm "$@" || return 1
    case "$*" in */lvgl_data/old.png) exit 88 ;; esac
}
if (install_bundle); then exit 2; fi
unset -f rm
recover_transaction
''' if crash else 'install_bundle\n'))
    r=subprocess.run(['sh',str(runner)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    (case/'output.log').write_text(r.stdout)
    assert (r.returncode==0)==ok,(name,r.stdout,(usb/'log').read_text())
    expected=initial if crash or not ok else target
    if name=='nochange': expected=initial
    for p,(d,m) in expected.items(): assert (dev/p).read_bytes()==d and ((dev/p).stat().st_mode&0o777)==m,(name,p)
    if ok and not crash and name!='nochange': assert not (dev/'usr/local/share/lvgl_data/old.png').exists()
    print('PASS',name)
run_case('delta',inc,base,True)
run_case('delta-repeat',inc,target,True)
run_case('full-single-pass',full,target,True)
wrong=dict(base);wrong['usr/local/lib/liblvgl.so']=(b'local modified',0o755)
run_case('wrong-baseline',inc,wrong,False)
wrong_mode=dict(base);wrong_mode['usr/local/lib/liblvgl.so']=(b'lib',0o644)
run_case('wrong-mode',inc,wrong_mode,False)
run_case('delete-crash',inc,base,True,True)
noop=work/'noop.upk';delta.build(base_upk,base_upk,noop)
run_case('nochange',noop,base,True)
missing_core=dict(target);del missing_core['usr/local/bin/test_lvgl']
bad_full=work/'missing-core.upk';archive(bad_full,missing_core)
bad_delta=work/'delete-core.upk';delta.build(base_upk,bad_full,bad_delta)
run_case('delete-core',bad_delta,base,False)
# Extractor rejects traversal, links, duplicate file, unsupported PAX, checksum
# errors and gzip truncation before installer root mutation.
def bad_member(t,name,kind=tarfile.REGTYPE):
    m=tarfile.TarInfo(name);m.type=kind;m.linkname='/tmp/outside' if kind==tarfile.SYMTYPE else '';m.size=0;t.addfile(m,io.BytesIO())
for name,member,kind in [('traversal','payload/../../outside',tarfile.REGTYPE),('link','payload/link',tarfile.SYMTYPE),
                         ('duplicate','manifest.ini',tarfile.REGTYPE),('pax','payload/pax',tarfile.XHDTYPE)]:
    p=work/(name+'.upk');archive(p,base,lambda t:bad_member(t,member,kind));stage=work/(name+'-stage');stage.mkdir()
    assert subprocess.run([str(unpack),str(p),str(stage)],capture_output=True).returncode!=0
    print('PASS extractor',name)
trunc=work/'truncated.upk';trunc.write_bytes(full.read_bytes()[:-8]);stage=work/'truncated-stage';stage.mkdir()
assert subprocess.run([str(unpack),str(trunc),str(stage)],capture_output=True).returncode!=0
print('PASS extractor gzip truncation; artifacts:',work)
