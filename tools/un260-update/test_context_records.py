#!/usr/bin/env python3
"""Exercise production dispatcher context parsing, including corrupt records.

Native mount and lock descriptors are covered by test_update_dispatch. This
fixture checks the real context loader with private paths and helper probes;
record values must remain data even when they contain shell syntax.
"""
import hashlib
import os
from pathlib import Path
import secrets
import shlex
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
SOURCE = SDK / 'tools/un260-update/engine_context.sh'
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']

class ContextRecords(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-context-test-')
        self.base = Path(self.temp.name)
        self.ctx = Path('/tmp/un260-update-engine.' + secrets.token_hex(3))
        self.ctx.mkdir(mode=0o700)
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(lambda: __import__('shutil').rmtree(self.ctx))
        self.lock = self.base / 'lock'; self.lock.mkdir(mode=0o700)
        hashes = {}
        for role in ['engine', 'sync', 'unpack', 'app_storage', 'resource_cleanup']:
            file = self.ctx / ('engine.sh' if role == 'engine' else role)
            body = b'#!/bin/sh\nexit 0\n'
            if role in ['sync', 'unpack']:
                probe = 'UN260_STORAGE_SYNC_1' if role == 'sync' else 'UN260_UNPACK_1'
                body = ('#!/bin/sh\ncase "$1" in --probe) printf "%s\\n" '+probe+' ;; --fd) [ "$2" = 7 ] && [ -d /proc/self/fd/7 ] ;; *) exit 1 ;; esac\n').encode()
            file.write_bytes(body); file.chmod(0o600 if role == 'engine' else 0o755)
            hashes[role] = hashlib.sha256(body).hexdigest()
        self.records = ['UN260_UPDATE_CONTEXT_V1','pid=PID','usb_dev=none','stage=none',
            'bundle_path=none','bundle_sha256=none','bundle_fnv64=none']
        self.records += [r+'_sha256='+hashes[r] for r in hashes]
        self.records += ['start_time=1','phase_time=1','last_progress=0','last_stage=prepare',
            'guarded=1','usb_guarded=0','app_guarded=0','mode=app-check','action_arg=none']
        self.loader = self.base / 'loader.sh'
        self.loader.write_text(SOURCE.read_text().replace('DISPATCH_EXPECTED_UID=0',
            'DISPATCH_EXPECTED_UID='+str(os.getuid()), 1))

    def run_record(self, records=None, final_newline=True, vanish_after_preflight=False):
        records = self.records if records is None else records
        q = shlex.quote
        arguments = ['"pid=$$"' if r == 'pid=PID' else q(r.replace("\0", "\\000")) for r in records]
        sentinel = self.base / 'injected'
        script = '. '+q(str(self.loader))+'\n'
        script += 'LOCK_DIR='+q(str(self.lock))+'; UPDATE_DIR='+q(str(self.base/'usb/update'))+'\n'
        script += "printf '%s\\n' \"$$\" > \"$LOCK_DIR/pid\"\n"
        script += "printf '%b\\n' " + ' '.join(arguments)+' > '+q(str(self.ctx/'context'))+'\n'
        if not final_newline:
            original=q(str(self.ctx/'context'));short=q(str(self.ctx/'short'))
            script += 'bytes=$(wc -c < '+original+'); dd if='+original+' of='+short+' bs=1 count=$((bytes-1)) 2>/dev/null && mv '+short+' '+original+' || exit 90\n'
        script += 'chmod 0600 '+q(str(self.ctx/'context'))+'\n'
        script += 'boot_hex64() { [ "${#1}" = 64 ] && case "$1" in *[!0-9a-f]*) return 1 ;; *) return 0 ;; esac; }\n'
        script += 'checked_digest() { sum=$(sha256sum "$1") || return 1; printf "%s\\n" "${sum%% *}"; }\n'
        script += 'exec 7< /\n'
        if vanish_after_preflight:
            script += 'dispatch_context_load '+q(str(self.ctx))+' || exit 91\n'
            script += 'wc() { command wc "$@"; result=$?; if [ "$1" = -l ]; then rm -f '+q(str(self.ctx/'context'))+'; fi; return "$result"; }\n'
        script += 'dispatch_context_load '+q(str(self.ctx))+' || exit 19\n'
        script += 'dispatch_context_value mode\ndispatch_context_value start_time\n'
        r = subprocess.run(SHELL+['-c',script],capture_output=True,text=True,timeout=5)
        self.assertFalse(sentinel.exists(), 'record data executed')
        return r

    def test_valid_fixed_record_and_cached_values(self):
        r=self.run_record();self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,'app-check\n1\n')

    def test_missing_extra_reordered_and_duplicate_keys_reject(self):
        cases=[self.records[:-1],self.records+['unexpected=value'],
            self.records[:2]+[self.records[3],self.records[2]]+self.records[4:],
            self.records[:-1]+['mode=app-check']]
        for record in cases:
            with self.subTest(record=record):self.assertNotEqual(self.run_record(record).returncode,0)

    def test_bad_header_partial_line_equals_and_oversized_values_reject(self):
        for index,value in [(0,'WRONG'),(2,'usb_dev=none=extra'),(12,'start_time='+'1'*4096),(18,'app_guarded=1')]:
            rows=list(self.records);rows[index]=value
            with self.subTest(value=value[:30]):self.assertNotEqual(self.run_record(rows).returncode,0)
        self.assertNotEqual(self.run_record(final_newline=False).returncode,0)

    def test_shell_text_never_executes_and_invalid_semantics_reject(self):
        rows=list(self.records);rows[12]='start_time=$(touch '+str(self.base/'injected')+')'
        self.assertNotEqual(self.run_record(rows).returncode,0)
        for value in ['update','unsupported','trial-confirm']:
            rows=list(self.records);rows[19]='mode='+value
            with self.subTest(value=value):self.assertNotEqual(self.run_record(rows).returncode,0)

    def test_record_open_failure_never_reuses_cached_context(self):
        self.assertNotEqual(self.run_record(vanish_after_preflight=True).returncode,0)

    def test_embedded_nul_key_or_value_rejects(self):
        for index,value in [(0,'UN260_UPDATE_CONTEXT_V1\0'),(19,'mode\0=app-check'),(19,'mode=app-\0check')]:
            rows=list(self.records);rows[index]=value
            with self.subTest(index=index):self.assertNotEqual(self.run_record(rows).returncode,0)

    def test_wrong_helper_digest_mode_and_symlink_reject(self):
        helper=self.ctx/'app_storage';body=helper.read_bytes();helper.write_bytes(body+b'bad')
        self.assertNotEqual(self.run_record().returncode,0);helper.write_bytes(body)
        helper.chmod(0o777);self.assertNotEqual(self.run_record().returncode,0)
        helper.unlink();target=self.base/'helper';target.write_bytes(body);target.chmod(0o755);helper.symlink_to(target)
        self.assertNotEqual(self.run_record().returncode,0)

if __name__=='__main__':unittest.main(verbosity=2)
