#!/usr/bin/env python3
"""Routine full packages use the deployed updater's existing transaction now."""
import hashlib
import unittest
import tempfile
from pathlib import Path
import tarfile
import io
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

class DirectQualificationTests(unittest.TestCase):
    def setUp(self):
        from test_legacy_test_release import fixture_target, package
        self.temp = tempfile.TemporaryDirectory(prefix='un260-direct-qualification-')
        self.addCleanup(self.temp.cleanup)
        self.p = Path(self.temp.name)
        self.target = self.p / 'target'
        fixture_target(self.target)
        self.full = self.p / 'full.upk'
        result = package(self.target, self.full, legacy=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def rewrite(self, name, data):
        changed = self.p / 'changed.upk'
        with tarfile.open(self.full) as src, tarfile.open(changed, 'w:gz') as dst:
            for item in src.getmembers():
                content = src.extractfile(item).read() if item.isfile() else None
                if item.name == name:
                    content = data
                    item.size = len(data)
                dst.addfile(item, io.BytesIO(content) if content is not None else None)
        return changed

    def test_qualification_checks_application_identity_and_no_request(self):
        self.assertGreater(verify(self.full, self.target), 10)
        (self.target / volume.APP).write_bytes(b'WRONG BUILD')
        with self.assertRaises(AssertionError): verify(self.full, self.target)

    def test_qualification_rejects_missing_factory_platform(self):
        (self.target / 'etc/un260/updater-platform').unlink()
        with self.assertRaises(ValueError): verify(self.full, self.target)

    def test_qualification_rejects_continuation_request(self):
        from test_legacy_test_release import package
        extra = self.p / 'extra'
        path = extra / 'etc/un260/unified-request'
        path.parent.mkdir(parents=True)
        path.write_bytes(b'UNWANTED REQUEST\n')
        path.chmod(0o644)
        result = package(self.target, self.full, legacy=False, extra=extra)
        self.assertEqual(result.returncode, 0, result.stderr)
        with self.assertRaises(AssertionError): verify(self.full, self.target)

    def test_qualification_rejects_two_stage_package_type(self):
        with tarfile.open(self.full) as archive:
            metadata = archive.extractfile('manifest.ini').read()
        changed = self.rewrite('manifest.ini', metadata.replace(b'package_type=ui\n', b'package_type=ui-unified\n'))
        with self.assertRaises(AssertionError): verify(changed, self.target)


if __name__=='__main__':unittest.main()
