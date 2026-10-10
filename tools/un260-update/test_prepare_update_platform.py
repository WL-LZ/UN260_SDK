#!/usr/bin/env python3
"""Real platform assembly on private synthetic TARGETs; no board or make."""
import contextlib
import hashlib
import importlib.util
import io
from pathlib import Path
import shutil
import subprocess
import struct
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("un260_platform_assembler", SDK / "tools/un260-update/prepare_update_platform.py")
ASSEMBLER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ASSEMBLER)
SOURCES = (
    "target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh",
    "tools/un260-update/engine_context.sh",
    "tools/un260-update/engine_trial.sh",
    "tools/un260-update/startup_entry.sh",
    "tools/un260-update/startup_watch.sh",
    "tools/un260-update/un260_update_dispatch.sh",
    "package/artinchip/test-lvgl/S20test_lvgl",
)
RESCUE = "usr/local/lib/un260-updater/rescue/"
_build = tempfile.TemporaryDirectory(prefix="un260-platform-ELF-")

def host_static_elf():
    """A real complete static host ELF; it is never executed as a display."""
    binary = Path(_build.name) / "display"
    if not binary.exists():
        source = Path(_build.name) / "display.c"
        source.write_text("void _start(void) { __builtin_trap(); }\n")
        subprocess.run(["gcc", "-nostdlib", "-static", "-Wl,--entry=_start",
                        str(source), "-o", str(binary)], check=True, capture_output=True)
    data = bytearray(binary.read_bytes())
    # Structural target ABI fixture only; instruction bytes are never executed.
    struct.pack_into("<H", data, 18, 243)
    return bytes(data)

def sha(data):
    return hashlib.sha256(data).hexdigest()

class PlatformTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="prepare-platform-fixture-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name).absolute()
        self.repo = self.work / "repo"
        self.target = self.work / "target"
        self.target.mkdir()
        for relative in SOURCES:
            destination = self.repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(SDK / relative, destination)
        data = host_static_elf()
        for role, relative in ASSEMBLER.ROLES.items():
            path = self.target / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data if role in ("sync", "unpack") else b"#!/bin/sh\nexit 0\n")
            path.chmod(0o755)
        display = self.target / "usr/local/bin/un260_upgrade_display"
        display.write_bytes(data); display.chmod(0o755)

    def prepare(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            ASSEMBLER.prepare(self.repo, self.target)
        self.assertIn("UPDATE_PLATFORM_READY engine=", output.getvalue())
        return output.getvalue()

    def assert_file(self, relative, data, mode):
        path = self.target / relative
        self.assertFalse(path.is_symlink())
        self.assertEqual(path.read_bytes(), data, relative)
        self.assertEqual(path.stat().st_mode & 0o7777, mode, relative)

    def expected_engine(self):
        text = (self.repo / SOURCES[0]).read_text()
        for marker, name in (("CONTEXT", "engine_context.sh"), ("TRIAL", "engine_trial.sh")):
            token = "# UN260_ENGINE_" + marker + "_MODULE"
            self.assertEqual(text.count(token), 1)
            text = text.replace(token, (self.repo / "tools/un260-update" / name).read_text().rstrip())
        return text.encode()

    def test_real_assembly_all_fixed_files_descriptors_modes_and_SHA(self):
        self.assertEqual(ASSEMBLER.ROLES, {
            "engine": "usr/bin/ui_update.sh",
            "sync": "usr/local/bin/un260_storage_sync",
            "unpack": "usr/local/bin/un260_unpack",
            "app_storage": "usr/local/bin/un260_app_storage",
            "resource_cleanup": "usr/local/bin/un260_resource_cleanup",
        })
        expected_engine = self.expected_engine()
        runtime = (self.repo / "package/artinchip/test-lvgl/S20test_lvgl").read_bytes()
        entry = (self.repo / "tools/un260-update/startup_entry.sh").read_bytes()
        watch = (self.repo / "tools/un260-update/startup_watch.sh").read_bytes()
        dispatcher = (self.repo / "tools/un260-update/un260_update_dispatch.sh").read_bytes()
        self.prepare()
        self.assert_file("usr/bin/ui_update.sh", expected_engine, 0o755)
        self.assert_file("usr/local/lib/un260-startup/runtime.sh", runtime, 0o755)
        self.assert_file("etc/init.d/S00lvgl", entry, 0o755)
        self.assertNotEqual(entry, runtime)
        self.assert_file("usr/local/bin/un260_update_dispatch", dispatcher, 0o755)
        manifest = ["UN260_UPDATE_CORE_V1"]
        for role, relative in ASSEMBLER.ROLES.items():
            data = (self.target / relative).read_bytes()
            name = "engine.sh" if role == "engine" else role
            self.assert_file(RESCUE + name, data, 0o600 if role == "engine" else 0o755)
            manifest.append(role + "_sha256=" + sha(data))
        descriptor = ("\n".join(manifest) + "\n").encode()
        self.assertEqual(len(descriptor.splitlines()), 6)
        self.assert_file(RESCUE + "manifest", descriptor, 0o644)
        self.assert_file("etc/un260/update-core.manifest", descriptor, 0o644)
        self.assert_file("etc/un260/updater-platform", b"UN260_UPDATER_PLATFORM_V1\n", 0o644)
        self.assert_file("etc/un260/startup-runtime.sha256", (sha(runtime) + "\n").encode(), 0o644)
        for name, data in (("startup.sh", runtime), ("watch.sh", watch),
                           ("display", host_static_elf())):
            self.assert_file(RESCUE + name, data, 0o755)
            hashname = "startup.sha256" if name == "startup.sh" else "watch.sha256" if name == "watch.sh" else "display.sha256"
            self.assert_file(RESCUE + hashname, (sha(data) + "\n").encode(), 0o644)
        self.assert_file(RESCUE + "startup-entry.sha256", (sha(entry) + "\n").encode(), 0o644)
        self.assertEqual(list(self.target.rglob("*.un260-platform-tmp")), [])

    def test_repeat_assembly_is_idempotent_then_updates_matching_core(self):
        self.prepare()
        before = {str(p.relative_to(self.target)): (p.read_bytes(), p.stat().st_mode)
                  for p in self.target.rglob("*") if p.is_file()}
        self.prepare()
        after = {str(p.relative_to(self.target)): (p.read_bytes(), p.stat().st_mode)
                 for p in self.target.rglob("*") if p.is_file()}
        self.assertEqual(before, after)
        sync = self.target / ASSEMBLER.ROLES["sync"]
        sync.write_bytes(host_static_elf() + b"new-core-fixture")
        self.prepare()
        self.assertEqual((self.target / RESCUE / "sync").read_bytes(), sync.read_bytes())
        descriptor = (self.target / RESCUE / "manifest").read_text()
        self.assertIn("sync_sha256=" + sha(sync.read_bytes()) + "\n", descriptor)
        self.assertEqual((self.target / "etc/un260/update-core.manifest").read_text(), descriptor)

    def test_reject_symlink_target_root(self):
        target = self.work / "target-link"; target.symlink_to(self.target, target_is_directory=True)
        with self.assertRaises(AssertionError): ASSEMBLER.prepare(self.repo, target)

    def test_reject_symlink_outputs_inside_and_outside_target_without_touching_victim(self):
        for relative, outside in (("usr/bin/ui_update.sh", True),
                                  ("usr/local/lib/un260-startup/runtime.sh", False),
                                  (RESCUE + "manifest", False),
                                  ("etc/un260/startup-runtime.sha256", True),
                                  (RESCUE + "display", False)):
            with self.subTest(relative=relative):
                path = self.target / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                if path.exists(): path.unlink()
                victim = (self.work if outside else self.target) / "victim"
                victim.write_bytes(b"DO NOT TOUCH")
                path.symlink_to(victim)
                with self.assertRaises(AssertionError): self.prepare()
                self.assertEqual(victim.read_bytes(), b"DO NOT TOUCH")
                path.unlink()

    def test_reject_symlink_output_parent(self):
        destination = self.target / "usr/local/lib/un260-startup"
        destination.parent.mkdir(parents=True, exist_ok=True)
        other = self.target / "alternate"; other.mkdir()
        destination.symlink_to(other, target_is_directory=True)
        with self.assertRaises(AssertionError): self.prepare()
        self.assertEqual(list(other.iterdir()), [])

    def test_reject_existing_temporary_file_or_dangling_link_and_preserve_it(self):
        path = self.target / ASSEMBLER.ROLES["engine"]
        temporary = path.with_name(path.name + ".un260-platform-tmp")
        for shape in ("file", "symlink"):
            with self.subTest(shape=shape):
                original = path.read_bytes()
                if shape == "file": temporary.write_bytes(b"PRESERVE")
                else: temporary.symlink_to(self.work / "absent")
                with self.assertRaises(AssertionError): self.prepare()
                self.assertEqual(path.read_bytes(), original)
                if shape == "file": self.assertEqual(temporary.read_bytes(), b"PRESERVE")
                else: self.assertTrue(temporary.is_symlink())
                temporary.unlink()

    def test_missing_or_repeated_module_marker_fails_before_target_write(self):
        path = self.repo / SOURCES[0]
        original = path.read_text()
        target_before = (self.target / ASSEMBLER.ROLES["engine"]).read_bytes()
        for marker in ("CONTEXT", "TRIAL"):
            token = "# UN260_ENGINE_" + marker + "_MODULE"
            for changed in (original.replace(token, "# removed"),
                            original + "\n" + token + "\n"):
                with self.subTest(marker=marker, duplicate=changed != original.replace(token, "# removed")):
                    path.write_text(changed)
                    with self.assertRaises(AssertionError): self.prepare()
                    self.assertEqual((self.target / ASSEMBLER.ROLES["engine"]).read_bytes(), target_before)
        path.write_text(original)

    def test_missing_core_files_rejected(self):
        for role in ("sync", "unpack", "app_storage", "resource_cleanup"):
            with self.subTest(role=role):
                path = self.target / ASSEMBLER.ROLES[role]
                data = path.read_bytes(); path.unlink()
                with self.assertRaises(AssertionError): self.prepare()
                path.write_bytes(data); path.chmod(0o755)

    def test_core_zero_and_above_limit_rejected_exact_limit_accepted(self):
        path = self.target / ASSEMBLER.ROLES["sync"]
        for length in (0, 262145):
            with self.subTest(length=length):
                path.write_bytes(b"x" * length)
                with self.assertRaises(AssertionError): self.prepare()
        path.write_bytes(b"x" * 262144)
        self.prepare()
        self.assertEqual((self.target / RESCUE / "sync").stat().st_size, 262144)

    def test_core_source_symlink_rejected(self):
        path = self.target / ASSEMBLER.ROLES["unpack"]
        path.unlink(); path.symlink_to(self.work / "outside")
        (self.work / "outside").write_bytes(host_static_elf())
        with self.assertRaises(AssertionError): self.prepare()

    def test_display_missing_badmagic_symlink_and_oversize_rejected(self):
        path = self.target / "usr/local/bin/un260_upgrade_display"
        original = path.read_bytes()
        for data in (None, b"not an ELF", b"\x7fELF" + b"x" * 1048573):
            with self.subTest(data_size=None if data is None else len(data)):
                path.unlink()
                if data is not None: path.write_bytes(data)
                with self.assertRaises(AssertionError): self.prepare()
                if path.exists(): path.unlink()
                path.write_bytes(original)
        path.unlink(); outside = self.work / "display"; outside.write_bytes(original); path.symlink_to(outside)
        with self.assertRaises(AssertionError): self.prepare()

    def test_actual_shell_syntax_rejects_damaged_runtime_entry_watch_dispatcher_engine(self):
        files = ("package/artinchip/test-lvgl/S20test_lvgl",
                 "tools/un260-update/startup_entry.sh", "tools/un260-update/startup_watch.sh",
                 "tools/un260-update/un260_update_dispatch.sh",
                 "target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh")
        for relative in files:
            with self.subTest(relative=relative):
                path = self.repo / relative; original = path.read_text()
                path.write_text(original + "\n'unclosed\n")
                with self.assertRaises(subprocess.CalledProcessError): self.prepare()
                path.write_text(original)

    def test_static_ELF_architecture_headers_and_tables_rejected(self):
        original = host_static_elf()
        path = self.target / "usr/local/bin/un260_upgrade_display"
        modifications = [
            ("ELF32", 4, "B", 1), ("big-endian", 5, "B", 2),
            ("old-ident-version", 6, "B", 0),
            ("non-RISCV-machine", 18, "H", 62), ("non-EXEC-kind", 16, "H", 3),
            ("header-version", 20, "I", 0), ("short-header", 52, "H", 63),
            ("bad-program-size", 54, "H", 55), ("no-programs", 56, "H", 0),
            ("too-many-programs", 56, "H", 65),
            ("program-table-outside-file", 32, "Q", len(original)),
            ("section-table-outside-file", 40, "Q", len(original)),
            ("short-section-header", 58, "H", 63),
            ("section-name-index-outside-table", 62, "H", 65535),
        ]
        for name, offset, kind, value in modifications:
            with self.subTest(name=name):
                data = bytearray(original); struct.pack_into("<" + kind, data, offset, value)
                path.write_bytes(data)
                with self.assertRaises(AssertionError): self.prepare()
        phoff = struct.unpack_from("<Q", original, 32)[0]
        path.write_bytes(original[:phoff + 55])
        with self.assertRaises(AssertionError): self.prepare()

    def test_static_ELF_dynamic_segment_load_bounds_alignment_and_entry_rejected(self):
        original = host_static_elf()
        path = self.target / "usr/local/bin/un260_upgrade_display"
        phoff = struct.unpack_from("<Q", original, 32)[0]
        phsize, phnum = struct.unpack_from("<HH", original, 54)
        programs = [phoff + i * phsize for i in range(phnum)]
        executable = next(offset for offset in programs
                          if struct.unpack_from("<I", original, offset)[0] == 1
                          and struct.unpack_from("<I", original, offset + 4)[0] & 1)
        changes = [
            ("PT_INTERP", executable, "I", 3),
            ("PT_DYNAMIC", executable, "I", 2),
            ("segment-file-offset-outside-file", executable + 8, "Q", len(original) + 1),
            ("segment-file-size-outside-file", executable + 32, "Q", len(original) + 1),
            ("segment-files-larger-than-memory", executable + 40, "Q", 1),
            ("segment-zero-memory", executable + 40, "Q", 0),
            ("segment-non-power-two-align", executable + 48, "Q", 3),
            ("entry-not-in-file-backed-executable-segment", 24, "Q", 0),
        ]
        for name, offset, kind, value in changes:
            with self.subTest(name=name):
                data = bytearray(original); struct.pack_into("<" + kind, data, offset, value)
                path.write_bytes(data)
                with self.assertRaises(AssertionError): self.prepare()
        for name in ("no-LOAD", "no-executable-LOAD"):
            data = bytearray(original)
            for offset in programs:
                if struct.unpack_from("<I", data, offset)[0] == 1:
                    if name == "no-LOAD": struct.pack_into("<I", data, offset, 4)
                    else:
                        flags = struct.unpack_from("<I", data, offset + 4)[0]
                        struct.pack_into("<I", data, offset + 4, flags & ~1)
            with self.subTest(name=name):
                path.write_bytes(data)
                with self.assertRaises(AssertionError): self.prepare()

    def test_complete_static_ELF_without_section_table_is_valid(self):
        data = bytearray(host_static_elf())
        struct.pack_into("<Q", data, 40, 0)
        struct.pack_into("<HHH", data, 58, 0, 0, 0)
        (self.target / "usr/local/bin/un260_upgrade_display").write_bytes(data)
        self.prepare()
        self.assert_file(RESCUE + "display", data, 0o755)

    def test_truncated_ELF_magic_only_display_must_not_be_certified(self):
        (self.target / "usr/local/bin/un260_upgrade_display").write_bytes(b"\x7fELF")
        with self.assertRaises(AssertionError): self.prepare()

if __name__ == "__main__":
    unittest.main(verbosity=2)
