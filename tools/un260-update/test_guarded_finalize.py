#!/usr/bin/env python3
"""Exercise guarded completion on real host files; no actual USB/block devices."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_storage_sync import SDK, native_tool

SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']
UPDATER = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'

class FinalizeTests(unittest.TestCase):
    def run_fixture(self, fail_flush=False):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp); root=p/'root'; usb=p/'usb'; root.mkdir(); usb.mkdir()
            script=p/'run.sh'
            script.write_text(f'''UN260_UPDATER_LIBRARY_ONLY=1
. '{UPDATER}'
ROOT_PREFIX='{root}'
USB_MNT='{usb}'
SYNC_TOOL='{native_tool()}'
LOG='{p}/log'
RESULT_FILE='{p}/result'
STATUS_FILE='{p}/status'
STATUS_TEMP='{p}/status.tmp'
INSTALLED_STATE_DIR='{root}/state'
INSTALLED_HASH_PATH=$INSTALLED_STATE_DIR/installed.fnv64
JOURNAL=$INSTALLED_STATE_DIR/transaction
BUNDLE_FNV=1234567890abcdef
package_version=test-r2
GUARDED=1
USB_GUARDED=1
exec 7< "$ROOT_PREFIX" 8< "$USB_MNT"
# Real syncfs fd guard, synthetic mount identity only. Inject one final error.
usb_calls=0
usb_barrier() {{
    usb_calls=$((usb_calls+1))
    if [ '{int(fail_flush)}' = 1 ] && [ "$usb_calls" -ge 2 ]; then return 1; fi
    "$SYNC_TOOL" --fd 8 "$USB_MNT"
}}
printf 'test log\n' > "$LOG"
finish_success 'Upgrade complete'
''')
            r = subprocess.run(SHELL+[str(script)], capture_output=True, text=True)
            status = (p/'status').read_text()
            self.assertEqual(r.returncode == 0, not fail_flush, r.stderr)
            if fail_flush:
                self.assertIn('success=0', status)
                self.assertIn('USB finalization failed', status)
                self.assertFalse((root/'state/installed.fnv64').exists())
            else:
                self.assertIn('progress=100', status)
                self.assertIn('success=1', status)
                self.assertIn('result=success', (usb/'UN260_UPDATE_RESULT.txt').read_text())
                self.assertIn('test log', (usb/'ui_update.log').read_text())
                self.assertEqual((root/'state/installed.fnv64').read_text(), '1234567890abcdef\n')

    def test_durable_reports_precede_success(self): self.run_fixture()
    def test_final_flush_failure_blocks_success_and_hash_marker(self): self.run_fixture(True)

if __name__ == '__main__': unittest.main()
