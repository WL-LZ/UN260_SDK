#!/usr/bin/env python3
"""Exercise the actual package-specific dependency rules in serial/parallel GNU make."""
from pathlib import Path
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]


class FreshnessTests(unittest.TestCase):
    def test_app_enters_incremental_build_after_source_check_every_time(self):
        for jobs in ('1', '8'):
            with self.subTest(jobs=jobs), tempfile.TemporaryDirectory(prefix='un260-fresh-make-') as directory:
                root = Path(directory)
                makefile = root / 'Makefile'
                makefile.write_text('''TEST_LVGL_TARGET_BUILD := stamp-built
HIDDEN_PREFIX := fixture
include %s
.PHONY: all fixture-test-lvgl-check-update
all: stamp-built
fixture-test-lvgl-check-update:
\t@sleep .05; echo check >> events
stamp-built:
\t@tail -n 1 events | grep -qx check
\t@echo incremental-build >> events; touch $@
''' % (SDK / 'package/artinchip/test-lvgl/test-lvgl.mk'))
                for _ in range(2):
                    subprocess.run(['make', '-s', '-j' + jobs, 'all'], cwd=root, check=True, capture_output=True)
                self.assertEqual((root / 'events').read_text().splitlines(),
                                 ['check', 'incremental-build', 'check', 'incremental-build'])

    def test_external_lvgl_config_change_rebuilds_only_when_needed(self):
        with tempfile.TemporaryDirectory(prefix='un260-lvgl-config-') as directory:
            root = Path(directory)
            headers = root / 'source/artinchip/test-lvgl'
            headers.mkdir(parents=True)
            for name in ('lv_conf.h', 'lv_conf_library.h'): (headers / name).write_text('CONFIG')
            (root / 'package/third-party/lvgl').mkdir(parents=True)
            # Include the production rule but satisfy its real mk prerequisite.
            (root / 'package/third-party/lvgl/lvgl.mk').write_text('fixture configure input')
            (root / 'Makefile').write_text('''LVGL_TARGET_BUILD := stamp-built
LVGL_TARGET_CONFIGURE := stamp-configured
include %s
.PHONY: all
all: stamp-built
stamp-built: stamp-configured
\t@echo build >> events; touch $@
stamp-configured:
\t@touch $@
''' % (SDK / 'package/third-party/lvgl/lvgl.mk'))
            subprocess.run(['make', '-s', 'all'], cwd=root, check=True, capture_output=True)
            subprocess.run(['make', '-s', 'all'], cwd=root, check=True, capture_output=True)
            self.assertEqual((root / 'events').read_text().splitlines(), ['build'])
            for name in ('lv_conf.h', 'lv_conf_library.h'):
                # Ensure a genuinely newer external header, without a long wait.
                import os
                newer = (root / 'stamp-built').stat().st_mtime_ns + 1000000
                os.utime(headers / name, ns=(newer, newer))
                subprocess.run(['make', '-s', '-j8', 'all'], cwd=root, check=True, capture_output=True)
            self.assertEqual((root / 'events').read_text().splitlines(), ['build', 'build', 'build'])


if __name__ == '__main__': unittest.main()
