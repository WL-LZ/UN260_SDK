#!/usr/bin/env python3
"""Exercise a real last-batch history commit and screen upgrade readiness on separate bounded mounts."""
import argparse
import fcntl
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(os.environ.get('UN260_TEST_APP_ROOT',Path(__file__).resolve().parents[1]))
sys.path.insert(0,str(ROOT/'tools'))
from test_i18n_support import with_i18n

def function(text,name):
    match=re.search(r'^(?:static )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{',text,re.M)
    assert match,name
    brace=text.index('{',match.start());depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]',text[brace:],re.S):
        if token.group()=='{': depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth: return text[match.start():brace+token.end()]
    raise AssertionError(name)

def run(*args,**kw):
    return subprocess.run(list(map(str,args)),check=True,**kw)

def compile_fixture(work,history,app,previous=None):
    source=(ROOT/'un260/app_service/app_command_runtime.c').read_text()
    (work/'actual_data_gate.h').write_text(function(source,'app_command_runtime_prepare_ui_upgrade'))
    counting=(ROOT/'un260/app_service/app_counting_runtime.c').read_text()
    (work/'actual_history_poll.h').write_text(function(counting,'app_counting_runtime_poll_history'))
    (work/'history_runtime_under_test.h').write_text(function(counting,'app_counting_runtime_reset_session'))
    ui=(ROOT/'un260/lv_core/ui_upgrade_service.c').read_text()
    names=['ui_upgrade_service_set_status','ui_upgrade_service_child_busy','ui_upgrade_service_update_child_state',
           'ui_upgrade_service_reset','ui_upgrade_service_launch','ui_upgrade_service_start',
           'ui_upgrade_service_is_preparing_data','ui_upgrade_service_cancel_prepare','ui_upgrade_service_prepare_data_poll','ui_upgrade_service_poll']
    (work/'actual_upgrade_session_functions.h').write_text('\n\n'.join(function(ui,n) for n in names))
    fixture=(ROOT/'tools/test_upgrade_session.c').read_text().split('static void test_restart_exit_matrix')[0]
    fixture=fixture.replace('static app_ui_upgrade_readiness_t readiness;','').replace(
        'app_ui_upgrade_readiness_t app_command_runtime_prepare_ui_upgrade(void) { return readiness; }','').replace(
        '    readiness=APP_UI_UPGRADE_READY;','')
    (work/'ui.c').write_text(fixture+'\nvoid pressure_ui_reset(void){reset_fixture();}\n'
        'void pressure_ui_tick(void){now_ms++;}\nunsigned pressure_ui_launches(void){return forks;}\n')
    parts=['un260/storage/storage_worker.c','un260/history/history_object_store.c','un260/history/history_record_snapshot.c',
           'un260/history/history_record_reader.c','un260/history/history_read_task.c','un260/history/history_query.c',
           'un260/counting/counting_serial_text.c','un260/history/history_record_detail.c','un260/history/history_export_sn_parser.c',
           'un260/protocol/protocol_frame.c','un260/counting/counting_data_store.c','un260/counting/counting_history_service.c','un260/app_service/upgrade_session.c',
           'un260/lv_system/ui_update_message.c']
    fixture_source=Path(__file__).resolve().with_name('test_history_upgrade_pressure.c')
    binary=work/('previous-test' if previous else 'test')
    extra=[]
    if previous:
        overlay=work/'previous'
        for name,sub in [('ui_history_data_fs.c','un260/lv_system'),('history_object_store.h','un260/history')]:
            target=overlay/sub/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(previous/name,target)
        extra=['-I'+str(overlay),'-I'+str(ROOT/'un260/lv_system')]
    sources=with_i18n([ROOT/p for p in parts],ROOT)
    if previous: sources=[previous/'history_object_store.c' if Path(x).name=='history_object_store.c' else x for x in sources]
    run('cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pthread',
        '-fsanitize=undefined','-fno-sanitize-recover=all',*extra,'-I'+str(work),'-I'+str(ROOT),'-I'+str(ROOT/'tools'),
         '-DHISTORY_UPDATER_LOCK_DIR="'+str(work)+'"','-DHISTORY_UPDATER_TRANSACTION="'+str(work/'transaction')+'"',
        '-DUI_HISTORY_STORE_DIR="'+str(history)+'"','-DUI_HISTORY_APP_VOLUME="'+str(app)+'"',
        fixture_source,work/'ui.c',*sources,'-Wl,--wrap=malloc','-o',binary)
    return binary

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expect-blocked',action='store_true',help='show the original blocked operation')
    parser.add_argument('--previous-dir',type=Path,help='previous unmodified history sources for downgrade tests')
    args=parser.parse_args()
    if not os.environ.get('UN260_HISTORY_PRIVATE_MOUNTS'):
        env=dict(os.environ,UN260_HISTORY_PRIVATE_MOUNTS='1')
        sys.exit(subprocess.call(['unshare','-Urnm','--',sys.executable,__file__,*sys.argv[1:]],env=env))
    sys.path.insert(0,str(ROOT.parents[2]/'tools/un260-update'))
    from test_history_space import free_kb,make_history,squeeze
    with tempfile.TemporaryDirectory(prefix='un260-history-pressure-') as directory:
        w=Path(directory);root=w/'root';app=w/'app';root.mkdir();app.mkdir()
        run('mount','-t','tmpfs','-o','size=32m','none',root)
        try:
            run('mount','-t','tmpfs','-o','size=31m','none',app)
            try:
                history=root/'count_history'
                binary=compile_fixture(w,history,app)
                previous=compile_fixture(w,history,app,args.previous_dir) if args.previous_dir else None
                cutso=w/'cut.so'
                run('cc','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',
                    Path(__file__).resolve().with_name('test_history_pressure_cut.c'),'-ldl','-o',cutso)
                sync=w/'sync'; tools=ROOT/'tools'
                run('cc','-O2','-Wall','-Wextra','-Werror',tools/'un260_storage_sync.c',
                    tools/'un260_history_space.c',ROOT/'un260/history/history_object_store.c','-o',sync)
                idx=history/'index.cfg'; backing=app/'.un260-history-objects-v1/.committed-index-v1.cfg'
                def fresh(free=128,maximum=False):
                    for mount in (root,app):
                        for p in mount.iterdir():
                            if p.is_symlink() or p.is_file(): p.unlink()
                            else: shutil.rmtree(p)
                    history.mkdir()
                    (app/'.un260-owner').write_text('UN260_APP_VOLUME_V1\n')
                    files=make_history(history,mirrors=False)
                    if maximum:
                        # Charge the 31 MiB qualified app volume the actual
                        # uncompressed factory generation footprint.
                        with (app/'test_lvgl').open('xb') as image:
                            os.posix_fallocate(image.fileno(),0,9512704)
                        run(binary,'max-text')
                    old=idx.read_bytes()
                    squeeze(root,free,'unrelated-data')
                    return files,old
                def expect_commit(exe=binary):
                    run(exe,'commit')
                    assert not idx.is_symlink() and not (history/'.un260-index-switch-v1').exists()
                    result=run(binary,'read',capture_output=True,text=True)
                    assert 'next_id=1002' in result.stdout and 'total=256001' in result.stdout
                files,old=fresh()
                run(binary,'read')
                if args.expect_blocked:
                    run(binary,'blocked')
                    assert idx.read_bytes()==old
                    print(f'Confirmed blocking baseline: {len(files)} original records, index {len(old)} bytes, 128 KiB root free',flush=True)
                    return
                expect_commit()
                assert not backing.exists()
                for event,code in (('backup',81),('before-exchange',82),('exchange',83),('release',84),('commit',85)):
                    files,old=fresh()
                    env=dict(os.environ,LD_PRELOAD=str(cutso),UN260_HISTORY_CUT=event)
                    attempt=subprocess.run([str(binary),'commit'],env=env)
                    assert attempt.returncode==code,(event,attempt)
                    # No old attachment or stable ID disappears before the
                    # authoritative index changes. A 1001st normal append
                    # retires only the oldest slot after the new index commits.
                    if event!='commit':
                        assert idx.read_bytes()==old and backing.read_bytes()==old
                        for name,data in files.items(): assert (history/name).read_bytes()==data
                    if event in ('exchange','release'):
                        assert idx.is_symlink()
                        if previous: run(previous,'read')
                        # The upgraded package helper must accept exactly this
                        # owned recovery link, despite its ordinary O_NOFOLLOW rule.
                        run(sync,'--prepare-history-space',history,app,'0','2048')
                    if event=='commit':
                        assert not idx.is_symlink() and backing.exists()
                        result=run(binary,'read',capture_output=True,text=True)
                        assert 'next_id=1002' in result.stdout and not backing.exists()
                    elif event=='release' and previous:
                        # Previous firmware opens the same path and its regular
                        # index.cfg.tmp + rename republishes a normal root file.
                        run(previous,'commit');assert not idx.is_symlink()
                        run(binary,'read');assert not backing.exists()
                    else:
                        expect_commit()
                        assert not backing.exists()
                    print('PASS interrupted stage',event,flush=True)
                if previous:
                    # Old firmware can restore a root index after a cut before
                    # the exchanged old inode was removed. A validated startup
                    # must collect both exactly matching artifacts.
                    fresh()
                    interrupted=subprocess.run([str(binary),'commit'],env=dict(os.environ,LD_PRELOAD=str(cutso),UN260_HISTORY_CUT='exchange'))
                    assert interrupted.returncode==83 and idx.is_symlink()
                    filler=root/'unrelated-data'
                    with filler.open('r+b') as spare:
                        spare.truncate(filler.stat().st_size-1024*1024)
                    run(previous,'commit')
                    assert not idx.is_symlink() and (history/'.un260-index-switch-v1').is_file()
                    run(binary,'read')
                    assert not backing.exists() and not (history/'.un260-index-switch-v1').exists()
                    print('PASS older writer superseding an interrupted exchange; restart retired verified old inode and backup')
                # A real failed write cannot be reported as an upgrade. A
                # same-boot retry reuses the earlier confirmed ABC object.
                fresh(2048)
                run(binary,'eio-retry')
                assert not idx.is_symlink() and not backing.exists()
                print('PASS EIO preserves old database; normal retry reuses inode and launches once')
                fresh()
                squeeze(app,4096,'reserved-space')
                run(binary,'blocked');assert idx.read_bytes()==old and not idx.is_symlink()
                print('PASS protected app capacity keeps old index and blocks launch without false success')
                # True commercial limit: all seven text fields at full
                # escaped length plus 256,000 real ABC rows. Root and app
                # remain bounded throughout, including restart with no job.
                lock=w/'un260-updater.flock';txn=w/'transaction'
                def cut_at(command,event,code):
                    result=subprocess.run([str(binary),command],env=dict(os.environ,LD_PRELOAD=str(cutso),UN260_HISTORY_CUT=event))
                    assert result.returncode==code,(command,event,result)
                files,large=fresh(maximum=True)
                assert 15_000_000<len(large)<17_000_000
                available=free_kb(app)
                assert len(large)//1024+4096<available<len(large)//1024+16384
                run(binary,'lock-retry')
                assert not idx.is_symlink() and not backing.exists()
                assert free_kb(app)>=available-32
                assert 'next_id=1002' in run(binary,'read',capture_output=True,text=True).stdout
                after=idx.read_bytes()
                for name,data in files.items():
                    if int(name.split('-')[1])!=1: assert (history/name).read_bytes()==data
                assert sum(1 for line in after.splitlines() if line.endswith(b'_valid=1'))==1000
                assert b'record00_record_no=1001\n' in after
                print(f'PASS max-field 1000 records, index={len(large)} B, root=128 KiB, app=31 MiB, old ELF=9512704 B, free={available} KiB; contended updater lock then same job retry; 999 prior ABC plus final batch remain',flush=True)

                for event,code in (('restore-prepublish',86),('restore-published',87)):
                    files,large=fresh(maximum=True)
                    cut_at('commit','release',84)
                    assert idx.is_symlink() and backing.read_bytes()==large
                    if previous: run(previous,'read')
                    cut_at('read',event,code)
                    assert idx.read_bytes()==large and backing.read_bytes()==large
                    run(binary,'read')
                    assert not idx.is_symlink() and not backing.exists() and idx.read_bytes()==large
                    print('PASS no new business, exact original max index restored across',event,flush=True)
                for busy in ('lock','trial'):
                    fresh(maximum=True);cut_at('commit','release',84)
                    with lock.open('rb') as held:
                        if busy=='lock': fcntl.flock(held,fcntl.LOCK_EX)
                        else: txn.mkdir()
                        child=subprocess.Popen([str(binary),'read-wait'],stdout=subprocess.PIPE,text=True)
                        try:
                            assert child.stdout.readline().strip()=='HISTORY AVAILABLE WHILE BUSY'
                            assert idx.is_symlink() and idx.read_bytes()==large
                            if busy=='lock': fcntl.flock(held,fcntl.LOCK_UN)
                            else: txn.rmdir()
                            assert child.wait(timeout=12)==0
                        finally:
                            if child.poll() is None: child.kill();child.wait()
                    assert not idx.is_symlink() and not backing.exists() and idx.read_bytes()==large
                    print('PASS startup remained readable while',busy,'active, then returned app space with zero new records',flush=True)
                for case in ('click-before-return','click-during-return'):
                    fresh(maximum=True);cut_at('commit','release',84)
                    events={k:w/(case+'-'+k) for k in ('START','READY','RELEASE')}
                    env=dict(os.environ,LD_PRELOAD=str(cutso),**{'UN260_RETURN_'+k:str(path) for k,path in events.items()})
                    with lock.open('rb') as held:
                        fcntl.flock(held,fcntl.LOCK_EX)
                        child=subprocess.Popen([str(binary),case],env=env,stdout=subprocess.PIPE,text=True)
                        try:
                            assert child.stdout.readline().strip()=='HISTORY AVAILABLE WHILE BUSY'
                            fcntl.flock(held,fcntl.LOCK_UN);events['START'].touch()
                            assert child.wait(timeout=20)==0
                        finally:
                            if child.poll() is None: child.kill();child.wait()
                    assert not idx.is_symlink() and not backing.exists() and idx.read_bytes()==large
                    print('PASS',case,'single screen request waited for real worker and acquired the dispatcher gate after return',flush=True)
                fresh(maximum=True);squeeze(app,18*1024,'also-used')
                run(binary,'blocked')
                assert not idx.is_symlink() and idx.read_bytes()==large and not backing.exists()
                print('PASS real free bytes below full index plus 4 MiB reserve preserves complete committed database')
                for prior in (b'magic=1212765012\n',b'mag'):
                    fresh();(history/'index.cfg.tmp').write_bytes(prior)
                    expect_commit()
                    if len(prior)<5: assert (history/'index.cfg.tmp').read_bytes()==prior
                fresh(2048)
                unknown=root/'unrelated-file';unknown.write_bytes(b'foreign')
                tmp=history/'index.cfg.tmp'
                for link in ('hard','symlink','plain'):
                    if tmp.exists() or tmp.is_symlink(): tmp.unlink()
                    if link=='hard': os.link(unknown,tmp)
                    elif link=='symlink': tmp.symlink_to(unknown)
                    else: tmp.write_bytes(b'unrecognised workspace file')
                    expected=tmp.read_bytes()
                    run(binary,'commit')
                    assert unknown.read_bytes()==b'foreign' and tmp.read_bytes()==expected and not idx.is_symlink()
                print('PASS stale partial/full temp safely resumed; unknown/hard/symbolic temp protected')
                for reserved in ('.committed-index-v1.cfg','.index-copy-v1.tmp'):
                    fresh()
                    objs=app/'.un260-history-objects-v1';objs.mkdir(mode=0o700)
                    (objs/'.owner').write_bytes(b'UN260_HISTORY_OBJECTS_1\n')
                    alien=objs/reserved;alien.write_bytes(b'not a history file')
                    if reserved=='.committed-index-v1.cfg':
                        run(binary,'blocked');assert idx.read_bytes()==old
                    else: expect_commit()
                    assert alien.read_bytes()==b'not a history file'
                fresh()
                objs=app/'.un260-history-objects-v1';objs.mkdir(mode=0o700)
                (objs/'.owner').write_bytes(b'UN260_HISTORY_OBJECTS_1\n')
                scratch=objs/'.index-copy-v1.tmp';scratch.write_bytes(b'mag')
                expect_commit()
                assert scratch.read_bytes()==b'mag'
                print('PASS unknown backup/scratch preserved and interrupted managed scratch resumes')
                fresh();idx.unlink();os.mkfifo(idx)
                run(binary,'unreadable',timeout=5)
                assert not backing.exists()
                print('PASS nonblocking FIFO cannot hang history loading')
                # After an object and root link are published but before the
                # index commits, neither a foreign link nor altered stored bytes
                # are ever mistaken for a successful repeat of the same snapshot.
                for kind in ('link','bytes'):
                    fresh()
                    attempt=subprocess.run([str(binary),'commit'],env=dict(os.environ,LD_PRELOAD=str(cutso),UN260_HISTORY_CUT='backup'))
                    assert attempt.returncode==81
                    last=next(history.glob('detail-1001-*.abc'))
                    if kind=='link':
                        last.unlink();foreign=root/'external-data';foreign.write_bytes(b'not ours');last.symlink_to(foreign)
                    else:
                        last.resolve().write_bytes(b'corrupt')
                    run(binary,'blocked')
                    assert idx.read_bytes()==old and last.read_bytes()==(b'not ours' if kind=='link' else b'corrupt')
                print('PASS foreign/mismatched existing ABC link never authorizes upgrade')
                fresh()
                foreign=app/'foreign-index';foreign.write_bytes(old)
                idx.unlink();idx.symlink_to(foreign)
                run(binary,'unreadable')
                res=subprocess.run([str(sync),'--prepare-history-space',str(history),str(app),'0','2048'])
                assert res.returncode!=0 and foreign.read_bytes()==old
                print('PASS arbitrary index symlink rejected by application and update helper')
            finally: run('umount',app)
        finally: run('umount',root)

if __name__=='__main__': main()
