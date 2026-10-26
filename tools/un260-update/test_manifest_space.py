#!/usr/bin/env python3
"""Host tests for omitting font audit metadata from the device resource tree.

Uses the real resource validator, full-inventory planner, installer, local inode
rollback and trial confirmation. The player is the existing check-only fixture;
files are ordinary host directories, not a physical NAND power-cut simulation.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import unittest

HERE = Path(__file__).resolve().parent
SDK = Path(os.environ.get('UN260_TEST_SDK', str(HERE.parents[1])))
sys.path.insert(0, str(SDK / 'tools/un260-update'))
import release_resources
from test_update_trial import TrialTests, APP, RESOURCE, TREE, NEW_BOOT, sha

STATIC = 'usr/local/share/lvgl_data'
AUDIT = STATIC + '/locale_fonts/manifest.json'
FONT = STATIC + '/locale_fonts/zz/font-16.bin'
OUTPUT = Path(os.environ['UN260_TEST_OUTPUT']) if os.environ.get('UN260_TEST_OUTPUT') else None


class ManifestSpaceTests(TrialTests):
    def setUp(self):
        super().setUp()
        # TrialTests starts immediately after replacement. Restore the original
        # inodes here so every test enters through actual plan/install functions.
        for rel in self.old:
            dest = self.destination(rel)
            Path(str(dest) + '.un260-old').replace(dest)
        for name in ('manifest', 'player', 'theme.pack'):
            (self.root / TREE / self.new_c[0] / name).unlink()
        shutil.rmtree(self.journal)
        self.font_data = b'unchanged locale font binary\0' * 128
        self.new[FONT] = (self.font_data, 0o644)
        self.old[FONT] = self.new[FONT]
        self.put(self.root / FONT, self.font_data, 0o644)
        self.original_inodes[FONT] = (self.root / FONT).stat().st_ino
        entry = dict(size=16, file='font-16.bin', bytes=len(self.font_data),
                     sha256=sha(self.font_data), codepoints=[32,65], text_keys=['Fixture'])
        locale = dict(family='Fixture font', source_sha256='a'*64, face_index=0,
                      candidate_entries=1, missing_translation_count=0,
                      codepoints=[32,65], shaped_codepoints=[], files=[entry],
                      license_metadata={'fixture':'new'}, bytes=len(self.font_data))
        metadata = dict(status='candidate-not-released', sizes=[16], locales={'zz':locale})
        self.audit_bytes = (json.dumps(metadata, sort_keys=True)+'\n').encode()
        old_metadata = json.loads(self.audit_bytes)
        old_metadata['locales']['zz']['license_metadata']['fixture'] = 'old'
        self.old[AUDIT] = ((json.dumps(old_metadata, sort_keys=True)+'\n').encode(), 0o644)
        self.new[AUDIT] = (self.audit_bytes, 0o644)
        self.put(self.root / AUDIT, self.old[AUDIT][0], 0o644)
        self.original_inodes[AUDIT] = (self.root / AUDIT).stat().st_ino
        self.source_app = self.base / 'source-app'
        self.put(self.source_app / 'aic_ui/lvgl_data/locale_fonts/manifest.json', self.audit_bytes, 0o644)
        self.user_history = self.root / 'etc/ui_state/count_history/keep'
        self.put(self.user_history, b'CUSTOMER HISTORY', 0o600)
        self.calibration = self.root / 'etc/ui_state/touch_calibration.cfg'
        self.put(self.calibration, b'CALIBRATION', 0o600)
        for rel, (data, mode) in self.new.items():
            self.put(self.stage / 'payload' / rel, data, mode)
        self.refresh()
        self.log_number = 0

    def tearDown(self):
        self.assertEqual(self.user_history.read_bytes(), b'CUSTOMER HISTORY')
        self.assertEqual(self.calibration.read_bytes(), b'CALIBRATION')
        self.assertEqual((self.root / FONT).read_bytes(), self.font_data)
        self.assertEqual((self.root / FONT).stat().st_ino, self.original_inodes[FONT])

    def refresh(self):
        self.table()
        checks = ''.join(sha(data)+'  payload/'+rel+'\n'
                         for rel,(data,mode) in sorted(self.new.items()))
        (self.stage/'checksums.sha256').write_text(checks)
        manifest = ''.join('file|%04o|%s\n'%(mode,rel)
                           for rel,(data,mode) in sorted(self.new.items())
                           if not rel.startswith(STATIC+'/'))
        manifest += 'tree|0644|'+STATIC+'\n'
        (self.stage/'install.tsv').write_text(manifest)
        (self.stage/'manifest.ini').write_text('package_type=ui\nboot_bundle_id='+self.new_c[0]+
                                             '\nboot_manifest_sha256='+self.new_c[1]+'\n')

    def omit_audit(self):
        original = release_resources.APP
        try:
            release_resources.APP = self.source_app
            report = release_resources.publish_locale_font_audit(self.stage/'payload')
        finally:
            release_resources.APP = original
        self.assertEqual(report['saved'], len(self.audit_bytes))
        self.assertEqual(report['fonts'], 1)
        self.assertEqual(Path(report['audit']).read_bytes(), self.audit_bytes)
        self.assertEqual((self.stage/'payload'/FONT).read_bytes(), self.font_data)
        self.assertFalse((self.stage/'payload'/AUDIT).exists())
        del self.new[AUDIT]
        self.refresh()

    def engine(self, action, expected=0, before=''):
        setup = '''umask 077
USB_MNT=$(dirname "$(dirname "$STAGE_DIR")")
UPDATE_DIR=$USB_MNT/update
# Host directories replace the mount probe only; actual held-FD sync remains.
[ ! -d "$USB_MNT" ] || exec 8< "$USB_MNT"
usb_barrier() { "$SYNC_TOOL" --fd 8 "$USB_MNT"; }
BACKUP_DIR=$UPDATE_DIR/.un260_backup
BACKUP_STATE=$BACKUP_DIR/state.tsv
PLAN_FILE=$UPDATE_DIR/.un260_plan
SEEN_TARGETS=$UPDATE_DIR/.un260_seen_targets
CHECKSUM_TARGETS=$UPDATE_DIR/.un260_checksum_targets
PAYLOAD_FILES=$UPDATE_DIR/.un260_payload_files
STATUS_FILE=$ROOT_PREFIX/../status
STATUS_TEMP=$ROOT_PREFIX/../status.tmp
RESULT_FILE=$ROOT_PREFIX/../result
WORK_OWNED=0
package_version=manifest-space-fixture
package_schema=1
df() { printf 'FS Size Used Free Use Mount\\nfixture 200000 1 100000 1%% /\\n'; }
'''
        result = self.run_shell(setup+before+'\n'+action, expected)
        if OUTPUT:
            OUTPUT.mkdir(parents=True,exist_ok=True)
            name=self._testMethodName+'-%02d.log'%self.log_number
            (OUTPUT/name).write_text(result.stdout+result.stderr+(self.base/'log').read_text())
        self.log_number+=1
        return result

    def install(self, before='', expected=0):
        return self.engine('validate_payload_checksums && execute_plan',expected,before)

    def verify_audit_rollback(self):
        self.assertEqual((self.root/AUDIT).read_bytes(),self.old[AUDIT][0])
        self.assertEqual((self.root/AUDIT).stat().st_ino,self.original_inodes[AUDIT])
        self.assertFalse(Path(str(self.root/AUDIT)+'.un260-old').exists())
        self.assertFalse(self.journal.exists())

    def test_manifest_low_space_from_actual_full_plan(self):
        # Generic bytes reproduce the observed 54 changed font-file lengths.
        # No customer firmware or serial records are embedded in this fixture.
        sizes = [38860,45796,53336,60592,70732,80600,88572,97536,110488,
                 120400,134476,147072,160020,186876,213480,246340,312956,395720,
                 27512,31900,35632,41664,48964,54264,61664,67624,76036,
                 83204,90848,99816,108824,123432,145256,170656,216928,269732,
                 49096,58076,67380,78136,88872,101572,111944,123048,136128,
                 150864,164560,183920,199732,231620,267120,307772,391308,496112]
        for rel in list(self.new):
            if rel in self.old and rel not in (APP,AUDIT):
                self.new[rel] = self.old[rel]
                self.put(self.stage/'payload'/rel,*self.new[rel])
        for i,size in enumerate(sizes):
            rel=STATIC+'/size-fixture/font-%02d.bin'%i
            self.new[rel]=(b'new-font-'+b'x'*(size-9),0o644)
            self.put(self.root/rel,b'old-font',0o644)
            self.put(self.stage/'payload'/rel,*self.new[rel])
        version='etc/un260/package-version'
        self.new[version]=(b'new-version'+b'x'*27,0o644)
        self.put(self.root/version,b'old-version'+b'x'*27,0o644)
        self.put(self.stage/'payload'/version,*self.new[version])
        # A check-only shell player with realistic stored lengths still passes
        # the actual boot descriptor/hash contract on this host.
        for rel in list(self.new):
            if rel.startswith(TREE+'/'):
                (self.stage/'payload'/rel).unlink();del self.new[rel]
        player=b'#!/bin/sh\nexit 0\n#'
        player+=b' '*(520368-len(player))
        theme=b'UNBTPK1\0'+bytes(20)+(1).to_bytes(4,'little')
        theme+=b'x'*(89950-len(theme))
        ph,th=sha(player),sha(theme)
        identity=sha(('player_sha256='+ph+'\ntheme_sha256='+th+'\nmode=animation\n').encode())
        manifest=('schema=un260-boot-bundle-v1\nid='+identity+'\nprotocol=1\nplayer_sha256='+ph+
                  '\nplayer_bytes=520368\ntheme_sha256='+th+'\ntheme_bytes=89950\nmode=animation\n').encode()
        self.new_c=(identity,sha(manifest))
        for name,data,mode in [('player',player,0o755),('theme.pack',theme,0o644),('manifest',manifest,0o644)]:
            rel=TREE+'/'+identity+'/'+name
            self.new[rel]=(data,mode);self.put(self.stage/'payload'/rel,data,mode)
        metadata=json.loads(self.audit_bytes)
        metadata['locales']['zz']['license_metadata']['padding']=''
        encoded=(json.dumps(metadata,sort_keys=True)+'\n').encode()
        metadata['locales']['zz']['license_metadata']['padding']='x'*(364504-len(encoded))
        self.audit_bytes=(json.dumps(metadata,sort_keys=True)+'\n').encode()
        self.assertEqual(len(self.audit_bytes),364504)
        self.new[AUDIT]=(self.audit_bytes,0o644)
        self.put(self.stage/'payload'/AUDIT,self.audit_bytes,0o644)
        self.put(self.source_app/'aic_ui/lvgl_data/locale_fonts/manifest.json',self.audit_bytes,0o644)
        # 402 target rows, of which only 60 differ, as on the observed upgrade.
        for i in range(402-len(self.new)):
            rel=STATIC+'/keep/resource-x-%03d.bin'%i
            data=b'unchanged'
            self.new[rel]=(data,0o644)
            self.put(self.root/rel,data,0o644);self.put(self.stage/'payload'/rel,data,0o644)
        self.refresh()
        # Row path padding makes the authenticated table's actual size occupy
        # the observed 50 KiB; no storage counters are injected into the engine.
        while len(self.table_data)<50866:
            rel=next(r for r in self.new if '/keep/' in r and len(r)<180)
            padding='x'*min(50866-len(self.table_data),180-len(rel))
            replacement=rel.replace('/keep/','/keep/'+padding)
            self.new[replacement]=self.new.pop(rel)
            (self.root/rel).rename(self.root/replacement)
            (self.stage/'payload'/rel).rename(self.stage/'payload'/replacement)
            self.refresh()
        self.assertLessEqual(len(self.table_data),51200)
        capacity='''df() {
 free=10488
 [ "$2" != "$USB_MNT" ] || free=14960208
 [ "$2" != "$APP_VOLUME_DIR" ] || free=100000
 printf 'FS Size Used Free Use Mount\\nfixture 200000 1 %s 1%% /\\n' "$free"
}
'''
        action='validate_payload_checksums && validate_install_manifest && boot_package_request && validate_storage_space'
        self.engine(action,1,capacity)
        first=(self.base/'log').read_text()
        self.assertIn('changed=60 unchanged=342',first)
        self.assertIn('root_staged=8566KB',first)
        self.assertIn('root_required=10704KB',first)
        self.assertFalse(self.journal.exists())
        self.assertEqual((self.root/AUDIT).read_bytes(),self.old[AUDIT][0])
        self.omit_audit()
        self.engine(action,0,capacity)
        after=(self.base/'log').read_text()[len(first):]
        self.assertIn('root_staged=8206KB',after)
        self.assertIn('root_required=10344KB',after)
        plan=(self.stage.parent/'.un260_plan').read_text()
        self.assertIn('delete|0644|'+AUDIT+'|-|',plan)
        self.install(capacity)
        self.assertFalse((self.root/AUDIT).exists())
        self.assertTrue(Path(str(self.root/AUDIT)+'.un260-old').is_file())
    def test_manifest_actual_tree_deletion_preserves_font_inode(self):
        self.omit_audit()
        self.install()
        plan=(self.stage.parent/'.un260_plan').read_text()
        self.assertIn('delete|0644|'+AUDIT+'|-|',plan)
        self.assertNotIn('file|0644|'+FONT+'|',plan)
        self.assertFalse((self.root/AUDIT).exists())
        old=Path(str(self.root/AUDIT)+'.un260-old')
        self.assertEqual(old.read_bytes(),self.old[AUDIT][0])
        self.assertEqual(old.stat().st_ino,self.original_inodes[AUDIT])
        self.assertIn('1|'+AUDIT+'\n',(self.journal/'state').read_text())
        self.assertNotIn(AUDIT,(self.journal/'trial.targets').read_text())
        self.engine('trial_is_committed')

    def test_manifest_failed_start_without_usb_restores_old(self):
        self.omit_audit();self.install()
        self.boot_id.write_text(NEW_BOOT+'\n')
        (self.base/'usb').rename(self.base/'disconnected-usb')
        self.engine('trial_rollback')
        self.verify_audit_rollback()
        self.assertEqual(self.destination(APP).read_bytes(),self.old[APP][0])
        self.assertEqual((self.root/RESOURCE).read_bytes(),self.old[RESOURCE][0])

    def test_manifest_healthy_pid_confirmation_cleans_old(self):
        self.omit_audit();self.install()
        self.boot_id.write_text(NEW_BOOT+'\n')
        process=self.app_process()
        self.engine('trial_confirm %d'%process.pid)
        self.assertFalse((self.root/AUDIT).exists())
        self.assertFalse(Path(str(self.root/AUDIT)+'.un260-old').exists())
        self.assertFalse(self.journal.exists())
        self.assertEqual(self.destination(APP).read_bytes(),self.new[APP][0])
        # A repeated full install is a no-op and creates no second transaction.
        self.install()
        self.assertFalse(self.journal.exists())
        self.assertIn('no replacements required',(self.base/'status').read_text())

    def test_manifest_cut_after_deleted_name_recovers_without_usb(self):
        self.omit_audit()
        fault='''rm() {
 command rm "$@" || return $?
 case "$*" in *locale_fonts/manifest.json) exit 88 ;; esac
}
'''
        self.install(fault,88)
        self.assertFalse((self.root/AUDIT).exists())
        self.assertTrue(Path(str(self.root/AUDIT)+'.un260-old').is_file())
        (self.base/'usb').rename(self.base/'disconnected-usb')
        self.engine('recover_transaction')
        self.verify_audit_rollback()

    def test_manifest_cut_before_replacement_recovers_without_usb(self):
        self.omit_audit()
        fault='''cp() {
 case "$2" in *liblvgl.so.un260-new) exit 88 ;; esac
 command cp "$@"
}
'''
        self.install(fault,88)
        (self.base/'usb').rename(self.base/'disconnected-usb')
        self.engine('recover_transaction')
        self.verify_audit_rollback()
        self.assertEqual(self.destination(APP).read_bytes(),self.old[APP][0])

    def test_manifest_confirmation_cleanup_cut_resumes(self):
        self.omit_audit();self.install()
        self.boot_id.write_text(NEW_BOOT+'\n')
        process=self.app_process()
        fault='''rm() {
 case "$*" in *locale_fonts/manifest.json.un260-old*) return 1 ;; esac
 command rm "$@"
}
'''
        self.engine('trial_confirm %d'%process.pid,1,fault)
        self.assertTrue((self.journal/'trial.confirmed').exists())
        self.engine('recover_transaction')
        self.assertFalse((self.root/AUDIT).exists())
        self.assertFalse(Path(str(self.root/AUDIT)+'.un260-old').exists())
        self.assertFalse(self.journal.exists())


if __name__=='__main__':
    # Do not silently run all inherited trial tests; those have their own suite.
    names=sorted(n for n in ManifestSpaceTests.__dict__ if n.startswith('test_manifest_'))
    suite=unittest.TestSuite(ManifestSpaceTests(n) for n in names)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(not result.wasSuccessful())