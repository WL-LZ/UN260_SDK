#!/usr/bin/env python3
"""ASan/UBSan offscreen maintenance renderer, no framebuffer access."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from PIL import Image

# The override permits a scratch test copy to exercise the real application sources.
APP = Path(os.environ.get('UN260_SOURCE_ROOT', str(Path(__file__).resolve().parents[2] / 'source/artinchip/test-lvgl')))
HEADER = APP / 'un260/lv_drivers/upgrade_display.h'
I18N_SOURCES = [APP / name for name in (
    'un260/lv_system/ui_i18n.c', 'un260/lv_system/ui_lang.c',
    'un260/lv_system/ui_update_message.c', 'un260/storage/ui_locale_store.c',
    'i18n/generated/lv_i18n.c',
)]


class DisplayTests(unittest.TestCase):
    def test_failure_does_not_promise_usb_log_or_blind_restart(self):
        source = HEADER.read_text()
        self.assertNotIn('RESTART TO RETRY', source)
        self.assertNotIn('DETAILS ARE IN UI UPDATE LOG ON USB', source)
        self.assertIn('KEEP BACKUPS AND CHECK THE ERROR', source)
        self.assertIn('USB COPY MAY BE UNAVAILABLE', source)

    def test_status_rendering_and_bounds(self):
        with tempfile.TemporaryDirectory(prefix='un260-update-display-') as directory:
            path = Path(directory)
            source = path / 'render.c'
            source.write_text('''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define UPGRADE_STATUS_PATH "%s"
#include "%s"
int main(int argc,char **argv) {
    (void)argc;
    unsigned stride=1280*4+64;
    uint8_t *pixels=calloc(400,stride);
    if(!pixels)return 1;
    upgrade_render(pixels,stride);
    for(int y=0;y<400;y++)for(unsigned x=1280*4;x<stride;x++)
        if(pixels[y*stride+x])return 2;
    FILE *out=fopen(argv[1],"wb");if(!out)return 3;
    fprintf(out,"P6\\n1280 400\\n255\\n");
    for(int y=0;y<400;y++)for(int x=0;x<1280;x++) {
        uint8_t *p=pixels+y*stride+x*4;
        unsigned char rgb[3]={p[2],p[1],p[0]};fwrite(rgb,1,3,out);
    }
    fclose(out);free(pixels);return 0;
}
''' % (path / 'status', HEADER))
            executable = path / 'render'
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-fno-pie', '-no-pie', '-I', str(APP), str(source),
                            *(str(item) for item in I18N_SOURCES), '-o', str(executable)], check=True)
            cases = {'running': 'progress=42\nstage=install\nmessage=Installing application\n',
                     'failure': 'progress=110\nstage=fail\nsuccess=0\nmessage=Storage migration paused keep USB backup and log no IMG required for diagnosis\n',
                     'success': 'progress=100\nsuccess=1\nmessage=Unified upgrade completed application is starting\n',
                     'usb-failure': 'progress=5\nstage=fail\nsuccess=0\nmessage=USB staging write failed; check filesystem or connection; preserve backup\n',
                     'negative': 'progress=-100\nmessage=' + 'x' * 400 + '\n',
                     'parameterized': 'progress=30\nmessage=Insufficient app volume space: need 100KB, free 20KB; keep USB log\n',
                     'untrusted': 'progress=20\nmessage=UNKNOWN %n%s%p diagnostic\n'}
            for name, data in cases.items():
                (path / 'status').write_text(data)
                destination = Path('/tmp') / ('un260-upgrade-' + name + '.ppm')
                subprocess.run([str(executable), str(destination)], check=True)
                self.assertEqual(destination.stat().st_size, 1280 * 400 * 3 + len(b'P6\n1280 400\n255\n'))
                Image.open(destination).save(destination.with_suffix('.png'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
