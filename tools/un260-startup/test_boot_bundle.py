#!/usr/bin/env python3
"""Validate actual bundle parser/SHA against hostile temporary files, no boot."""
from pathlib import Path
import hashlib
import os
import random
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
app=root/'source/artinchip/test-lvgl'
fixture=r'''
#define _GNU_SOURCE
#include "un260/boot/boot_bundle.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int __real_fstat(int,struct stat*);
int __real___fxstat(int,int,struct stat*);
static int inject_owner(int fd,struct stat *st,int result) {
 const char *foreign=getenv("TEST_FOREIGN_OWNER");
 if(!result&&foreign) {
  char link[64],path[4096];snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);
  ssize_t n=readlink(link,path,sizeof(path)-1);
  if(n>=0){path[n]=0;if(!strcmp(path,foreign))st->st_uid=geteuid()+1;}
 }
 return result;
}
int __wrap_fstat(int fd,struct stat *st) {return inject_owner(fd,st,__real_fstat(fd,st));}
int __wrap___fxstat(int version,int fd,struct stat *st) {
 return inject_owner(fd,st,__real___fxstat(version,fd,st));
}
static int descriptors(void) {
 DIR *d=opendir("/proc/self/fd");assert(d);int count=0;struct dirent *e;
 while((e=readdir(d)))if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))count++;
 closedir(d);return count;
}
int main(int argc,char **argv) {
 assert(argc>=3);
 if(!strcmp(argv[1],"sha")) {
  int fd=open(argv[2],O_RDONLY);assert(fd>=0);assert(lseek(fd,7,SEEK_SET)==7);
  char digest[65];assert(boot_bundle_sha256_fd(fd,digest));assert(lseek(fd,0,SEEK_CUR)==7);
  puts(digest);close(fd);return 0;
 }
 assert(argc==5);int before=descriptors();boot_bundle_t bundle;
 bool ok=boot_bundle_open(argv[2],argv[3],!strcmp(argv[4],"previous"),&bundle);
 if(ok) {
  assert(bundle.player_fd>=0&&bundle.theme_fd>=0);
  assert(fcntl(bundle.player_fd,F_GETFD)&FD_CLOEXEC);
  assert(fcntl(bundle.theme_fd,F_GETFD)&FD_CLOEXEC);
  assert(lseek(bundle.player_fd,0,SEEK_CUR)==0&&lseek(bundle.theme_fd,0,SEEK_CUR)==0);
  if(!strcmp(argv[1],"hold")) {
   unsigned char original[8],after[8];assert(pread(bundle.theme_fd,original,8,0)==8);
   char path[4096];snprintf(path,sizeof(path),"%s/%s/theme.pack",argv[3],bundle.id);
   assert(unlink(path)==0);int fd=open(path,O_CREAT|O_WRONLY,0644);assert(fd>=0);
   assert(write(fd,"replaced",8)==8);close(fd);
   assert(pread(bundle.theme_fd,after,8,0)==8&&!memcmp(original,after,8));
  }
  printf("%s %d %s\n",bundle.id,bundle.previous,bundle.mode);
 }
 int player=bundle.player_fd,theme=bundle.theme_fd;boot_bundle_close(&bundle);
 assert(bundle.player_fd==-1&&bundle.theme_fd==-1);
 if(player>=0){errno=0;assert(fcntl(player,F_GETFD)==-1&&errno==EBADF);}
 if(theme>=0){errno=0;assert(fcntl(theme,F_GETFD)==-1&&errno==EBADF);}
 int sentinel=open("/dev/null",O_RDONLY);assert(sentinel>=0);
 boot_bundle_close(&bundle);assert(fcntl(sentinel,F_GETFD)>=0);close(sentinel);
 assert(descriptors()==before);
 return ok?0:3;
}
'''

def digest(data): return hashlib.sha256(data).hexdigest()

def content_id(player_sha,theme_sha,mode):
    return digest(f'player_sha256={player_sha}\ntheme_sha256={theme_sha}\nmode={mode}\n'.encode())

def elf(dynamic=False,corruption=None):
    count=2 if dynamic else 1
    size=64+56*count+32
    ident=b'\x7fELF'+bytes([2,1,1])+bytes(9)
    header=struct.pack('<16sHHIQQQIHHHHHH',ident,2,243,1,0x400040,64,0,0,64,56,count,0,0,0)
    load=struct.pack('<IIQQQQQQ',1,5,0,0x400000,0x400000,size,size,4096)
    if corruption=='no-load': load=struct.pack('<IIQQQQQQ',0,0,0,0,0,0,0,0)
    if corruption=='load-outside': load=struct.pack('<IIQQQQQQ',1,5,size+1,0x400000,0,size,size,4096)
    if corruption=='entry-outside':
        header=struct.pack('<16sHHIQQQIHHHHHH',ident,2,243,1,0x700000,64,0,0,64,56,count,0,0,0)
    if corruption=='non-executable': load=struct.pack('<IIQQQQQQ',1,4,0,0x400000,0,size,size,4096)
    if corruption=='mem-too-small': load=struct.pack('<IIQQQQQQ',1,5,0,0x400000,0,size,size-1,4096)
    interp=struct.pack('<IIQQQQQQ',3,4,64+56*count,0,0,16,16,1) if dynamic else b''
    return header+load+interp+bytes(32)

with tempfile.TemporaryDirectory(prefix='un260-bundle-validation-') as directory:
    tmp=Path(directory)
    source=tmp/'fixture.c';source.write_text(fixture)
    exe=tmp/'fixture'
    subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-fno-sanitize-recover=all','-I'+str(app),str(source),
                    str(app/'un260/boot/boot_bundle.c'),'-Wl,--wrap=fstat',
                    '-Wl,--wrap=__fxstat','-o',str(exe)],check=True)
    rng=random.Random(260)
    vectors=[b'',b'abc',b'a'*1000000]
    vectors += [bytes(rng.randrange(256) for _ in range(n)) for n in (1,55,56,57,63,64,65,8191,8192,8193,16385)]
    for data in vectors:
        path=tmp/'sha-input';path.write_bytes(data)
        result=subprocess.run([str(exe),'sha',str(path)],text=True,capture_output=True,check=True)
        assert result.stdout.strip()==digest(data)
    print(f'PASS SHA-256 {len(vectors)} hashlib vectors and descriptor offset preservation')

    passed=[]
    def run_case(name,mutate=None,valid=True,previous=False,previous_only=False,hold=False,mode='animation'):
        case=tmp/name;case.mkdir()
        versions=case/'versions';versions.mkdir();versions.chmod(0o755)
        active=case/'active'
        ids=[]
        manifests=[]
        for index in range(2):
            player=elf();theme=b'UN260 theme bytes '+bytes([index])*257
            ident=content_id(digest(player),digest(theme),mode);ids.append(ident)
            folder=versions/ident;folder.mkdir();folder.chmod(0o755)
            (folder/'player').write_bytes(player);(folder/'player').chmod(0o755)
            (folder/'theme.pack').write_bytes(theme);(folder/'theme.pack').chmod(0o644)
            text=(f'schema=un260-boot-bundle-v1\nid={ident}\nprotocol=1\n'
                  f'player_sha256={digest(player)}\nplayer_bytes={len(player)}\n'
                  f'theme_sha256={digest(theme)}\ntheme_bytes={len(theme)}\nmode={mode}\n')
            manifest=folder/'manifest';manifest.write_text(text);manifest.chmod(0o644)
            manifests.append(digest(text.encode()))
        def publish(with_previous=False):
            active.write_text('UN260_BOOT_ACTIVE_V1\n'+ids[0]+'\n'+manifests[0]+'\n'+
                              (ids[1]+'\n'+manifests[1] if with_previous else 'none\nnone')+'\n')
            active.chmod(0o644)
        def rewrite_file(kind,data):
            folder=versions/ids[0];(folder/kind).write_bytes(data)
            text=(folder/'manifest').read_text().splitlines()
            prefix='player' if kind=='player' else 'theme'
            text=[prefix+'_sha256='+digest(data) if x.startswith(prefix+'_sha256=') else
                  prefix+'_bytes='+str(len(data)) if x.startswith(prefix+'_bytes=') else x for x in text]
            fields=dict(line.split('=',1) for line in text)
            new_id=content_id(fields['player_sha256'],fields['theme_sha256'],fields['mode'])
            text=['id='+new_id if x.startswith('id=') else x for x in text]
            if new_id!=ids[0]:
                folder.rename(versions/new_id);folder=versions/new_id;ids[0]=new_id
            text='\n'.join(text)+'\n';(folder/'manifest').write_text(text);manifests[0]=digest(text.encode())
            publish(previous)
        def rewrite_manifest(text):
            (versions/ids[0]/'manifest').write_bytes(text);manifests[0]=digest(text);publish(previous)
        publish(previous)
        env=os.environ.copy()
        context=dict(case=case,versions=versions,active=active,ids=ids,
                     publish=publish,rewrite_file=rewrite_file,rewrite_manifest=rewrite_manifest,env=env)
        if mutate: mutate(context)
        result=subprocess.run([str(exe),'hold' if hold else 'check',str(active),str(versions),
                               'previous' if previous_only else 'active'],env=env,text=True,capture_output=True)
        assert result.returncode==(0 if valid else 3),(name,result.returncode,result.stdout,result.stderr)
        if valid:
            expected_previous=previous_only or (previous and mutate is not None)
            expected=ids[1] if expected_previous else ids[0]
            assert result.stdout.strip()==f'{expected} {int(expected_previous)} {mode}',(name,result.stdout)
        passed.append(name)

    def active_edit(transform): return lambda c:c['active'].write_bytes(transform(c['active'].read_bytes()))
    def manifest_edit(transform): return lambda c:c['rewrite_manifest'](transform((c['versions']/c['ids'][0]/'manifest').read_bytes()))
    def chmod(kind,permissions):
        return lambda c:(c['versions']/c['ids'][0]/kind).chmod(permissions)
    def replace_with_link(kind):
        def change(c):
            target=c['versions']/c['ids'][0]/kind
            saved=c['case']/('saved-'+kind.replace('.','-'));target.rename(saved);target.symlink_to(saved)
        return change
    def corrupt_same_size(kind):
        def change(c):
            path=c['versions']/c['ids'][0]/kind
            data=bytearray(path.read_bytes());data[-1]^=1;path.write_bytes(data)
        return change
    run_case('valid-animation')
    run_case('valid-logo',mode='logo')
    run_case('previous-explicit',previous=True,previous_only=True)
    run_case('previous-none',previous_only=True,valid=False)
    run_case('fd-snapshot-after-path-replacement',hold=True)
    run_case('active-truncated',active_edit(lambda b:b[:-1]),valid=False)
    run_case('active-extra-line',active_edit(lambda b:b+b'extra\n'),valid=False)
    run_case('active-nul',active_edit(lambda b:b.replace(b'none',b'no\x00e',1)),valid=False)
    run_case('active-traversal',lambda c:c['active'].write_bytes(
             c['active'].read_bytes().replace(c['ids'][0].encode(),b'../../escape',1)),valid=False)
    run_case('manifest-truncated',manifest_edit(lambda b:b[:-1]),valid=False)
    run_case('manifest-duplicate',manifest_edit(lambda b:b+b'mode=animation\n'),valid=False)
    run_case('manifest-unknown-field',manifest_edit(lambda b:b+b'unknown=yes\n'),valid=False)
    run_case('manifest-bad-protocol',manifest_edit(lambda b:b.replace(b'protocol=1',b'protocol=2')),valid=False)
    run_case('manifest-bad-mode',manifest_edit(lambda b:b.replace(b'mode=animation',b'mode=unsupported')),valid=False)
    run_case('manifest-mode-changed-with-old-id',manifest_edit(lambda b:b.replace(b'mode=animation',b'mode=logo')),valid=False)
    run_case('manifest-bad-size',manifest_edit(lambda b:b.replace(b'theme_bytes=275',b'theme_bytes=276')),valid=False)
    run_case('manifest-byte-overflow',manifest_edit(lambda b:b.replace(b'player_bytes=152',b'player_bytes=999999999999999999999999')),valid=False)
    run_case('player-oversize',lambda c:c['rewrite_file']('player',bytes(2*1024*1024+1)),valid=False)
    run_case('theme-oversize',lambda c:c['rewrite_file']('theme.pack',bytes(4*1024*1024+1)),valid=False)
    run_case('manifest-wrong-digest',corrupt_same_size('manifest'),valid=False)
    run_case('player-wrong-digest',corrupt_same_size('player'),valid=False)
    run_case('theme-wrong-digest',corrupt_same_size('theme.pack'),valid=False)
    run_case('fallback-corrupt-player',lambda c:(c['versions']/c['ids'][0]/'player').write_bytes(b'corrupt'),previous=True)
    run_case('fallback-corrupt-theme',lambda c:(c['versions']/c['ids'][0]/'theme.pack').write_bytes(b'corrupt'),previous=True)
    def both_corrupt(c):
        for ident in c['ids']:
            (c['versions']/ident/'player').write_bytes(b'corrupt')
    run_case('both-versions-corrupt',both_corrupt,valid=False,previous=True)
    run_case('dynamic-player',lambda c:c['rewrite_file']('player',elf(dynamic=True)),valid=False)
    run_case('elf-truncated',lambda c:c['rewrite_file']('player',elf()[:63]),valid=False)
    run_case('elf-program-header-truncated',lambda c:c['rewrite_file']('player',elf()[:100]),valid=False)
    for corruption in ('no-load','load-outside','entry-outside','non-executable','mem-too-small'):
        run_case('elf-'+corruption,lambda c,k=corruption:c['rewrite_file']('player',elf(corruption=k)),valid=False)
    for kind in ('player','theme.pack','manifest'):
        run_case(kind+'-symlink',replace_with_link(kind),valid=False)
        run_case(kind+'-group-writable',chmod(kind,0o775 if kind=='player' else 0o664),valid=False)
        run_case(kind+'-setuid',chmod(kind,0o4755 if kind=='player' else 0o4644),valid=False)
        run_case(kind+'-foreign-owner',lambda c,k=kind:c['env'].update(TEST_FOREIGN_OWNER=str(c['versions']/c['ids'][0]/k)),valid=False)
    run_case('player-not-executable',chmod('player',0o644),valid=False)
    run_case('versions-group-writable',lambda c:c['versions'].chmod(0o775),valid=False)
    run_case('version-group-writable',lambda c:(c['versions']/c['ids'][0]).chmod(0o775),valid=False)
    run_case('versions-foreign-owner',lambda c:c['env'].update(TEST_FOREIGN_OWNER=str(c['versions'])),valid=False)
    run_case('version-foreign-owner',lambda c:c['env'].update(TEST_FOREIGN_OWNER=str(c['versions']/c['ids'][0])),valid=False)
    def active_symlink(c):
        saved=c['case']/'active-save';c['active'].rename(saved);c['active'].symlink_to(saved)
    run_case('active-symlink',active_symlink,valid=False)
    run_case('active-foreign-owner',lambda c:c['env'].update(TEST_FOREIGN_OWNER=str(c['active'])),valid=False)
    run_case('active-group-writable',lambda c:c['active'].chmod(0o664),valid=False)
    def standalone_previous(c):
        previous_hash=digest((c['versions']/c['ids'][1]/'manifest').read_bytes())
        record='UN260_BOOT_ACTIVE_V1\n'+c['ids'][1]+'\n'+previous_hash+'\nnone\nnone\n'
        backup=Path(str(c['active'])+'.previous');backup.write_text(record);backup.chmod(0o644)
        c['active'].write_bytes(b'torn descriptor')
    run_case('active-torn-independent-previous',standalone_previous,previous=True)
    print(f'PASS {len(passed)} bundle validation/fallback/FD cases; only temporary files changed')
