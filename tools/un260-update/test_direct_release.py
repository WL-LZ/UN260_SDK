#!/usr/bin/env python3
"""Routine full packages use the deployed updater's existing transaction now."""
import hashlib
import unittest
from pathlib import Path
import test_app_volume_updater as volume
from verify_direct_release import verify


class DirectReleaseTests(volume.VolumeUpdaterTests):
    def pack(self, capability=volume.CAPABILITY):
        if 'package_type=' not in capability:
            capability += 'package_type=ui\n'
        super().pack(capability)

    def test_completed_migration_does_not_rearm_after_routine_update(self):
        request=self.root/'etc/un260/unified-request'
        request.parent.mkdir(parents=True,exist_ok=True)
        request.write_bytes(b'previous completed migration request\n')
        completed=self.root/'var/lib/un260-updater/unified.completed'
        completed.parent.mkdir(parents=True,exist_ok=True)
        old_id=hashlib.sha256(request.read_bytes()).hexdigest()+'\n'
        completed.write_text(old_id)
        # Call the exact pending predicate used by S00lvgl after the transaction.
        self.run_update(action='install_bundle\nif unified_pending; then exit 72; fi',
                        extra='continue_unified() { echo UNEXPECTED_CONTINUATION >&2; exit 73; }')
        self.assertEqual((self.vol/'test_lvgl').read_bytes(),self.files[volume.APP][0])
        self.assertEqual(completed.read_text(),old_id)
        self.assertIn('success=1',(self.root/'status').read_text())
        self.assertNotIn('Bridge ready',(self.root/'status').read_text())
        self.run_update(action='install_bundle\nif unified_pending; then exit 72; fi')
        self.assertEqual(completed.read_text(),old_id)

    def test_factory_board_without_request_installs_before_reboot(self):
        self.run_update(action='install_bundle\nif unified_pending; then exit 72; fi')
        self.assertEqual((self.vol/'test_lvgl').read_bytes(),self.files[volume.APP][0])
        self.assertFalse((self.root/'etc/un260/unified-request').exists())
        self.assertIn('reboot is required',(self.root/'status').read_text())

    def test_pending_storage_migration_still_refuses_normal_package(self):
        pending=self.root/'etc/un260/app-storage/pending'
        pending.parent.mkdir(parents=True,exist_ok=True);pending.write_text('KEEP')
        self.run_update(ok=False)
        self.assertEqual(pending.read_text(),'KEEP')
        self.assertEqual((self.vol/'test_lvgl').read_bytes(),b'old app')
        self.assertIn('Finish the pending storage migration',(self.root/'status').read_text())

    def target_fixture(self):
        target=self.p/'built-target'
        for name,(data,mode) in self.files.items():
            path=target/name;path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(data);path.chmod(mode)
        return target

    def test_qualification_checks_application_identity_and_no_request(self):
        target=self.target_fixture()
        self.assertEqual(verify(self.full,target),len(self.files))
        (target/volume.APP).write_bytes(b'WRONG BUILD')
        with self.assertRaises(AssertionError):verify(self.full,target)
        target=self.target_fixture()
        self.files['etc/un260/unified-request']=(b'UNWANTED REQUEST',0o644)
        self.pack()
        with self.assertRaises(AssertionError):verify(self.full,target)

    def test_qualification_rejects_two_stage_package_type(self):
        self.pack(volume.CAPABILITY+'package_type=ui-unified\n')
        with self.assertRaises(AssertionError):verify(self.full,self.target_fixture())


if __name__=='__main__':unittest.main()
