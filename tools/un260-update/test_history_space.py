#!/usr/bin/env python3
"""Exercise direct update with 1000 histories on physically bounded filesystems.

Run on Linux with unprivileged user/mount namespaces. The held USB directory
must be on a filesystem supporting O_DIRECT (not tmpfs); root and app are
separate 32 MiB tmpfs mounts private to this process.
"""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

import test_manifest_space as manifests
from test_update_trial import APP, NEW_BOOT

def checked(*args, **kwargs):
    return subprocess.run(list(map(str,args)), check=True, **kwargs)

def free_kb(path):
    s=os.statvfs(path)
    return s.f_bavail*s.f_frsize//1024

def squeeze(path, to_kb, filename):
    amount=max(0,free_kb(path)-to_kb)*1024
    with (path/filename).open('xb') as file:
        os.posix_fallocate(file.fileno(),0,amount)
        os.fsync(file.fileno())

def full_detail(number):
    rows=256
    part=b''.join(struct.pack('<IIB',i+1,5,12)+f'{number:06d}{i+1:06d}'.encode()
                  for i in range(rows))
    data=struct.pack('<4s7I',b'ABC1',1,rows,1,rows,0,0,7)+struct.pack('<II',5,rows)+part
    h=14695981039346656037
    for b in data:
        h=((h^b)*1099511628211)&0xffffffffffffffff
    return data,h

def make_history(path, mirrors=True):
    path.mkdir(exist_ok=True,parents=True)
    parts=['magic=1212765012','version=5','total_notes_counted=256000',
           'next_record_no=1001','next_slot_no=1','record_count=1000','capacity=1000']
    refs={}
    for i in range(1000):
        slot=i+1
        data,h=full_detail(slot)
        filename=f'detail-{slot}-{h:016x}.abc'
        (path/filename).write_bytes(data)
        refs[filename]=data
        # This is the stable v5 field representation. The old diagnostic
        # writer can order fields differently, but cannot lose a single field.
        row={'valid':'1','selected':'0','slot_no':str(slot),'record_no':str(slot),
             'operator_id':'0','operator_name':'','pcs':'256','amount':'1280',
             'currency':'USD','time':'2026-11-09 09:00:00','denom':'5','sn':'',
             'sn_detail':'','err':'','start':'','end':'','log':'',
             'detail':f'{h:016x},{len(data)}','detail_amount':'1280','detail_codes':'00'*32}
        parts.extend(f'record{i:02d}_{k}={v}' for k,v in row.items())
        if mirrors:
            (path/f'{slot:02d}.rec').write_text(''.join(
                f'slot{slot:02d}_{k}={v}\n' for k,v in reversed(list(row.items()))))
    (path/'index.cfg').write_text('\n'.join(parts)+'\n')
    return refs

class HistorySpaceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.environ.get('UN260_HISTORY_PRIVATE_MOUNTS'):
            raise unittest.SkipTest('run this file directly to create private bounded mounts')
        manifests.ManifestSpaceTests.setUpClass()
    @classmethod
    def tearDownClass(cls):
        manifests.ManifestSpaceTests.tearDownClass()
    def setUp(self):
        self.f=manifests.ManifestSpaceTests(methodName='test_manifest_actual_tree_deletion_preserves_font_inode')
        f=self.f
        self.addCleanup(f.doCleanups)
        f.setUp()
        for path in (f.root,f.app_volume):
            template=path.with_name(path.name+'-initial')
            path.rename(template)
            path.mkdir()
            checked('mount','-t','tmpfs','-o','size=32m','none',path)
            self.addCleanup(checked,'umount',path)
            shutil.copytree(template,path,dirs_exist_ok=True)
        for rel in f.old:
            f.original_inodes[rel]=f.destination(rel).stat().st_ino
        self.addCleanup(f.tearDown)
        (f.app_volume/'.un260-owner').write_text('UN260_APP_VOLUME_V1\n')
        self.history=f.root/'etc/ui_state/count_history'
        self.refs=make_history(self.history)
        self.index=(self.history/'index.cfg').read_bytes()
        self.bad=self.history/'01.rec'
        self.bad.write_text('diagnostic with unique field or value\n')
        self.unknown=self.history/'001.rec'
        self.unknown.write_text('customer notes must survive\n')
        self.foreign=self.history/'future.data'
        self.foreign.write_bytes(b'unknown')
    def preserved(self):
        self.assertEqual((self.history/'index.cfg').read_bytes(),self.index)
        self.assertEqual(self.bad.read_text(),'diagnostic with unique field or value\n')
        self.assertEqual(self.unknown.read_text(),'customer notes must survive\n')
        self.assertEqual(self.foreign.read_bytes(),b'unknown')
        for name,data in self.refs.items():
            self.assertEqual((self.history/name).read_bytes(),data)
    def helper(self,goal,app_need=2048):
        f=self.f
        return checked(f.native/'sync','--prepare-history-space',self.history,f.app_volume,goal,app_need,
                       capture_output=True,text=True).stdout
    def test_two_installs_keep_all_records(self):
        f=self.f
        asset=manifests.STATIC+'/test-new.bin'
        f.new[asset]=(b'g'*(5*1024*1024),0o644)
        f.put(f.stage/'payload'/asset,*f.new[asset])
        f.refresh()
        squeeze(f.root,1024,'other-installed-files')
        before=free_kb(f.root)
        f.install('unset -f df')
        report=(f.base/'log').read_text()
        self.assertIn('History space: 1000 records',report)
        self.assertIn('999 matched diagnostic mirrors retired',report)
        self.assertTrue(any((self.history/n).is_symlink() for n in self.refs))
        self.preserved()
        self.assertGreaterEqual(free_kb(f.app_volume),16384)
        f.boot_id.write_text(NEW_BOOT+'\n')
        proc=f.app_process()
        f.engine('trial_confirm %d'%proc.pid)
        proc.terminate(); proc.wait(timeout=5)
        self.assertFalse(f.journal.exists())
        self.assertEqual(f.destination(APP).read_bytes(),f.new[APP][0])
        print(f'physical disposable-root preflight start = {before} KiB; real first install and trial confirmation passed')
        # Return near the floor again after the mirror cache is gone. Only
        # immutable details remain available for this next ordinary install.
        f.new[asset]=(b'h'*(4*1024*1024),0o644)
        f.put(f.stage/'payload'/asset,*f.new[asset])
        f.refresh()
        # Remove only this fixture's artificial filler to set a realistic
        # available capacity; no customer or updater path is touched.
        (f.root/'other-installed-files').unlink()
        squeeze(f.root,4100,'other-installed-files')
        f.install('unset -f df')
        self.preserved()
        self.assertGreaterEqual(free_kb(f.app_volume),16384)
        f.boot_id.write_text('33333333-3333-3333-3333-333333333333\n')
        f.engine('trial_rollback')
        self.preserved()
        self.assertEqual(f.destination(asset).read_bytes(),b'g'*(5*1024*1024))
        print('second direct update without mirrors + rollback: 1000 original IDs and ABC1 payloads survived')
    def test_live_writer_and_old_reader(self):
        f=self.f
        src=manifests.SDK/'source/artinchip/test-lvgl'
        reader=f.base/'reader.c'
        reader.write_text(r"""
#include "un260/lv_system/ui_history_data.h"
#include "un260/history/history_object_store.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(argc==1 || argc==4);
    if (argc==4) {
        FILE *fp=fopen(argv[2],"rb"); assert(fp);
        unsigned char data[8192];size_t size=fread(data,1,sizeof(data),fp);fclose(fp);
        assert(history_place_new_detail(UI_HISTORY_STORE_DIR,argv[3],data,size)==1);
    }
    unsigned id,size; uint64_t hash;
    while (scanf("%u %x %" SCNx64,&id,&size,&hash)==3) {
        ui_history_record_t record={.record_no=id,.pcs=256,.detail_bytes=size,.detail_hash=hash,.detail_amount=1280};
        history_record_detail_t d={0};
        assert(ui_history_record_read_detail(&record,&d));
        assert(d.serial_count==256 && d.serials[255].no==256 && d.denoms_complete && d.serials_complete);
        char sn[24];snprintf(sn,sizeof(sn),"%06u000256",id);
        assert(!strcmp(sn,d.serials[255].sn));
        history_record_detail_release(&d);
    }
}
""")
        binary=f.base/'reader'
        checked('cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),
                '-DUI_HISTORY_APP_VOLUME="'+str(f.app_volume)+'"','-DUI_HISTORY_STORE_DIR="'+str(self.history)+'"',
                reader, *(src/'un260/history'/p for p in ('history_object_store.c','history_record_reader.c',
                     'history_record_detail.c','history_record_snapshot.c','history_export_sn_parser.c')),
                src/'un260/protocol/protocol_frame.c','-o',binary)
        squeeze(f.root,10*1024,'writer-pressure')
        data,h=full_detail(1001)
        name=f'detail-1001-{h:016x}.abc'
        original=f.base/'original';original.write_bytes(data)
        known=''.join(f'{i} {len(data):x} {n.split("-")[2][:-4]}\n' for i,n in enumerate(self.refs,1))
        known+=f'1001 {len(data):x} {h:016x}\n'
        unfinished=f.app_volume/'.un260-history-init-ABCDEF'
        unfinished.mkdir(mode=0o700)
        (unfinished/'unknown').write_text('untouched')
        checked(binary,self.history,original,name,input=known,text=True)
        self.assertTrue((self.history/name).is_symlink())
        self.assertEqual((self.history/name).read_bytes(),data)
        self.assertGreaterEqual(free_kb(f.app_volume),16384)
        self.assertEqual((unfinished/'unknown').read_text(),'untouched')
        # Cut after object+directory publication but before canonical link; the
        # same bytes must be reused even if a new allocation would not fit.
        original_inode=(self.history/name).stat().st_ino
        (self.history/name).unlink()
        squeeze(f.app_volume,16384,'reserved-space')
        checked(binary,self.history,original,name,input=known,text=True)
        self.assertEqual((self.history/name).stat().st_ino,original_inode)
        print('PASS actual old reader decodes 1001 original NO / complete ABC serial rows including newly stored app object')

    def test_capacity_and_invalid_index(self):
        f=self.f
        squeeze(f.app_volume,16200,'other-app-files')
        goal=free_kb(f.root)+5000
        self.helper(goal)
        self.assertFalse(any((self.history/n).is_symlink() for n in self.refs))
        self.preserved()
        self.bad.write_bytes(b'unique')
        (self.history/'index.cfg').write_bytes(self.index[:-1])
        result=subprocess.run([str(f.native/'sync'),'--prepare-history-space',str(self.history),
                               str(f.app_volume),str(goal),'2048'])
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(self.bad.read_bytes(),b'unique')
    def test_interrupted_detail_and_nonowner(self):
        f=self.f
        # Do not need duplicate mirrors for recovery scenarios.
        for i in range(2,1001):
            (self.history/f'{i:02d}.rec').unlink()
        (f.app_volume/'.un260-owner').write_text('unknown\n')
        self.helper(free_kb(f.root)+1024)
        self.assertFalse((f.app_volume/'.un260-history-objects-v1').exists())
        (f.app_volume/'.un260-owner').write_text('UN260_APP_VOLUME_V1\n')
        self.helper(free_kb(f.root)+512)
        name=next(n for n in self.refs if (self.history/n).is_symlink())
        root_link=self.history/name
        object_file=root_link.resolve()
        swap=root_link.with_name('.un260-swap-'+name)
        swap.write_bytes(self.refs[name])   # interruption after exchange
        self.helper(free_kb(f.root)+512)
        self.assertFalse(swap.exists())
        self.preserved()
        # Old app deleted a record, leaving no canonical reference. A damaged
        # index must never trigger orphan collection. A quiescent complete
        # index next allows the owned object to be reclaimed.
        root_link.unlink()
        self.helper(free_kb(f.root)+20)
        self.assertTrue(object_file.exists(), 'still referenced by committed index')
        (self.history/'index.cfg').write_text(
             'magic=1212765012\nversion=5\ntotal_notes_counted=256000\nnext_record_no=1001\n'
             'next_slot_no=1\nrecord_count=0\ncapacity=1000\n')
        self.helper(free_kb(f.root)+20)
        self.assertFalse(object_file.exists())

if __name__=='__main__':
    if 'UN260_HISTORY_PRIVATE_MOUNTS' not in os.environ:
        env=dict(os.environ,UN260_HISTORY_PRIVATE_MOUNTS='1')
        sys.exit(subprocess.call(['unshare','-Urnm','--',sys.executable,__file__,*sys.argv[1:]],env=env))
    unittest.main()
