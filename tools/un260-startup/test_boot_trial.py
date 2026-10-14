#!/usr/bin/env python3
"""Display-only gate on journals produced by the real upgrade engine.

No flash, mounts or display hardware. Parent directory trust is mapped only
outside the temporary fixture for the unprivileged host. Journal files, SHA,
no-follow opens, mode checks and descriptor lifetimes use production code.
"""
from pathlib import Path
import hashlib, importlib.util, os, shutil, subprocess, tempfile

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('trial_fixture',ROOT/'tools/un260-update/test_update_trial.py')
trial=importlib.util.module_from_spec(spec);spec.loader.exec_module(trial)
fixture=r'''
#define _GNU_SOURCE
#include "tools/un260-startup/boot_trial.h"
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <unistd.h>
int __real_fstat(int,struct stat*);
int __real___fxstat(int,int,struct stat*);
int __real_open(const char*,int,...);
int __wrap_open(const char *p,int flags,...) {
 mode_t mode=0;if(flags&O_CREAT){va_list a;va_start(a,flags);mode=va_arg(a,int);va_end(a);}
 if(!strcmp(p,"/tmp/un260-updater.flock"))p=getenv("LOCK_FILE");
 assert(p);return __real_open(p,flags,mode);
}
static void host_parent(int fd,struct stat *s) {
 char link[64],path[4096];snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);
 ssize_t n=readlink(link,path,sizeof(path)-1);assert(n>=0);path[n]=0;
 const char *root=getenv("FIXTURE_ROOT");assert(root);
 if(S_ISDIR(s->st_mode)&&strncmp(path,root,strlen(root))) {
  s->st_uid=geteuid();s->st_mode&=~0022;
 }
 if(getenv("FOREIGN_FILE")&&strstr(path,"/trial.targets"))s->st_uid=geteuid()+1;
}
int __wrap_fstat(int fd,struct stat *s){int r=__real_fstat(fd,s);if(!r)host_parent(fd,s);return r;}
int __wrap___fxstat(int v,int fd,struct stat *s){int r=__real___fxstat(v,fd,s);if(!r)host_parent(fd,s);return r;}
static int descriptors(void) {
 DIR*d=opendir("/proc/self/fd");assert(d);int n=0;struct dirent*e;
 while((e=readdir(d)))if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))n++;
 closedir(d);return n;
}
int main(int argc,char **argv) {
 if(argc==3)return boot_trial_visual_recovery_ready(argv[1],argv[2])?0:3;
 if(argc==2) {
  int before=descriptors();int fd=boot_trial_visual_lock();
  if(!strcmp(argv[1],"lock-invalid")){assert(fd<0&&descriptors()==before);return 0;}
  assert(fd>=0&&(fcntl(fd,F_GETFD)&FD_CLOEXEC));
  pid_t child=fork();assert(child>=0);
  if(!child){close(fd);assert(boot_trial_visual_lock()<0);_exit(0);}
  int status=0;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
  close(fd);fd=boot_trial_visual_lock();assert(fd>=0);close(fd);
  assert(descriptors()==before);return 0;
 }
 assert(argc==5);int before=descriptors();boot_trial_visual_t v;
 int r=boot_trial_visual_read(argv[1],argv[2],&v);
 assert(r==atoi(argv[3]));
 if(r==1) {
  boot_bundle_t b={0};strcpy(b.id,v.bundle_id);strcpy(b.manifest_sha256,v.manifest_sha256);
  assert(boot_trial_visual_matches(&v,&b));
  b.id[0]=b.id[0]=='a'?'b':'a';assert(!boot_trial_visual_matches(&v,&b));
  strcpy(b.id,v.bundle_id);b.manifest_sha256[0]^=1;assert(!boot_trial_visual_matches(&v,&b));
  strcpy(b.manifest_sha256,v.manifest_sha256);b.previous=true;assert(!boot_trial_visual_matches(&v,&b));
  assert(!strcmp(v.bundle_id,argv[4]));
 }
 assert(descriptors()==before);return 0;
}
'''
def snapshot(root):
    return {str(p.relative_to(root)):(p.lstat().st_mode,p.lstat().st_ino,
            os.readlink(p) if p.is_symlink() else hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None)
            for p in root.rglob('*')}
def factory_dirs(root):
    root.chmod(0o755)
    for p in root.rglob('*'):
        if p.is_dir() and not p.is_symlink():p.chmod(0o700 if p.name=='transaction' else 0o755)
with tempfile.TemporaryDirectory(prefix='un260-visual-trial-') as directory:
    base=Path(directory);c=base/'test.c';c.write_text(fixture);exe=base/'test'
    subprocess.run(['cc','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined',
        '-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(ROOT/'source/artinchip/test-lvgl'),
        str(c),str(ROOT/'tools/un260-startup/boot_trial.c'),
        str(ROOT/'source/artinchip/test-lvgl/un260/boot/boot_bundle.c'),
        '-Wl,--wrap=fstat','-Wl,--wrap=__fxstat','-Wl,--wrap=open','-o',str(exe)],check=True)
    lock=base/'lock';env=dict(os.environ,FIXTURE_ROOT=str(base),LOCK_FILE=str(lock))
    subprocess.run([str(exe),'lock'],env=env,check=True)
    lock.chmod(0o644);subprocess.run([str(exe),'lock-invalid'],env=env,check=True)
    lock.unlink();lock.symlink_to(base/'other');subprocess.run([str(exe),'lock-invalid'],env=env,check=True)
    lock.unlink();lock.write_text('');lock.chmod(0o600);os.link(lock,base/'hardlink')
    subprocess.run([str(exe),'lock-invalid'],env=env,check=True)
    print('PASS updater lock contention, release, CLOEXEC, unsafe modes and links')
    rescue=base/'rescue.sh';receipt=base/'rescue.sha256'
    def check_rescue(data,expected=0,mutate=None):
        rescue.write_bytes(data);rescue.chmod(0o755)
        receipt.write_text(hashlib.sha256(data).hexdigest()+'\n');receipt.chmod(0o644)
        if mutate:mutate()
        result=subprocess.run([str(exe),str(rescue),str(receipt)],env=env)
        assert result.returncode==expected
    new_runtime=(ROOT/'package/artinchip/test-lvgl/S20test_lvgl').read_bytes()
    check_rescue(new_runtime)
    check_rescue(new_runtime.replace(b'# UN260_TRIAL_VISUAL_CLEANUP_V1\n',b''),3)
    check_rescue(new_runtime,3,lambda:receipt.write_text('a'*64+'\n'))
    check_rescue(new_runtime,3,lambda:rescue.chmod(0o777))
    check_rescue(new_runtime,3,lambda:receipt.chmod(0o666))
    rescue.unlink();rescue.symlink_to(base/'missing')
    assert subprocess.run([str(exe),str(rescue),str(receipt)],env=env).returncode==3
    rescue.unlink()
    print('PASS new fixed-rescue capability, old-base conservative fallback, hash and permission failures')
    trial.TrialTests.setUpClass()
    t=trial.TrialTests('test_prepare_retains_whole_transaction_and_cold_gate')
    engine_call=t.run_shell
    # The real dispatcher enters the engine with umask 077. The library-only
    # producer fixture otherwise inherits the build host's less strict umask.
    t.run_shell=lambda body,expected=0:engine_call('umask 077\n'+body,expected)
    try:
        t.setUp();t.prepare()
        passed=[]
        def check(name,change=None,expected=-1,environment=None):
            case=base/name;shutil.copytree(t.root,case)
            factory_dirs(case)
            boot=case/'boot-id';boot.write_text(trial.NEW_BOOT+'\n')
            journal=case/'var/lib/un260-updater/transaction'
            if change:change(journal,boot)
            before=snapshot(case)
            result=subprocess.run([str(exe),str(journal),str(boot),str(expected),t.new_c[0]],
                env=dict(os.environ,FIXTURE_ROOT=str(case),**(environment or {})),capture_output=True,text=True)
            assert result.returncode==0,(name,result.stdout,result.stderr)
            assert snapshot(case)==before,('display gate changed persistent state',name)
            passed.append(name)
        check('committed-cold-trial',expected=1)
        check('no-transaction',lambda j,b:shutil.rmtree(j),expected=0)
        check('same-boot',lambda j,b:b.write_text(trial.OLD_BOOT+'\n'))
        check('bad-boot-id',lambda j,b:b.write_text('bad\n'))
        check('missing-boot-id',lambda j,b:b.unlink())
        for name in ('committed','state','trial','trial.targets','trial.boot-active',
                     'trial.boot-active.previous','trial.installed.fnv64','trial.unified.completed'):
            check('missing-'+name,lambda j,b,n=name:(j/n).unlink())
        for name in ('trial.rollback','trial.confirmed','trial.tmp','boot-publish.tmp','boot-gc.tmp','unknown'):
            check('conflict-'+name,lambda j,b,n=name:(j/n).write_text(''))
        check('dirty-commit',lambda j,b:(j/'committed').write_text('x'))
        check('target-hash',lambda j,b:(j/'trial.targets').write_text('corrupt\n'))
        check('snapshot-hash',lambda j,b:(j/'trial.boot-active').write_text('corrupt\n'))
        check('private-mode',lambda j,b:(j/'trial').chmod(0o644))
        check('journal-mode',lambda j,b:j.chmod(0o755))
        check('writable-parent',lambda j,b:j.parent.chmod(0o777))
        check('writable-state',lambda j,b:(j/'state').chmod(0o666))
        check('foreign-file',environment={'FOREIGN_FILE':'1'})
        for name in ('trial','trial.targets','state','committed'):
            def symlink(j,b,n=name):
                saved=j.parent/('saved-'+n);(j/n).rename(saved);(j/n).symlink_to(saved)
            check('symlink-'+name,symlink)
        def parent_link(j,b):
            parent=j.parent;saved=parent.with_name('saved');parent.rename(saved);parent.symlink_to(saved)
        check('parent-symlink',parent_link)
        def edit_trial(j,transform):
            p=j/'trial';p.write_text(transform(p.read_text()))
        for name,transform in [('truncated',lambda x:x[:-1]),('extra',lambda x:x+'x\n'),
            ('nul',lambda x:x.replace('boot_id=','boot_id=\0')),
            ('bad-uuid',lambda x:x.replace(trial.OLD_BOOT,'x'*36)),
            ('zero-count',lambda x:x.rsplit('targets_count=',1)[0]+'targets_count=0\n'),
            ('huge-count',lambda x:x.rsplit('targets_count=',1)[0]+'targets_count=8193\n'),
            ('missing-bundle',lambda x:x.replace('boot_bundle_id='+t.new_c[0],'boot_bundle_id=none'))]:
            check('record-'+name,lambda j,b,f=transform:edit_trial(j,f))
        # The same real journal remains confirmable and fully rollbackable after display reads.
        process=t.app_process();t.run_shell('trial_confirm %d'%process.pid)
        assert not t.journal.exists()
        t.doCleanups();t.setUp();t.prepare();factory_dirs(t.root)
        before=snapshot(t.root)
        subprocess.run([str(exe),str(t.journal),str(t.boot_id),'1',t.new_c[0]],
            env=dict(os.environ,FIXTURE_ROOT=str(t.root)),check=True)
        assert snapshot(t.root)==before
        t.run_shell('trial_rollback');t.assert_old()
        print('PASS %d real-engine display policy cases; read-only snapshots, FD checks, health confirmation and full rollback'%len(passed))
    finally:
        t.doCleanups();trial.TrialTests.tearDownClass()
