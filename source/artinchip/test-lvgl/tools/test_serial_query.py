#!/usr/bin/env python3
"""Compile the real pure serial query/store with host regression tests."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class SerialQueryTests(unittest.TestCase):
    def test_real_query_and_store(self):
        configured = os.environ.get("CC")
        compiler = configured or shutil.which("cc") or shutil.which("gcc")
        self.assertIsNotNone(compiler, "A host C compiler is required; tests were not run")
        command = shlex.split(compiler) if configured else [compiler]
        sources = [ROOT / "tools/test_serial_query.c",
                   ROOT / "un260/counting/counting_serial_query.c",
                   ROOT / "un260/counting/counting_serial_text.c",
                   ROOT / "un260/counting/counting_data_store.c"]
        for source in sources:
            self.assertTrue(source.is_file(), f"Required real source missing: {source}")
        with tempfile.TemporaryDirectory(prefix="un260-serial-query-") as directory:
            executable = Path(directory) / ("query.exe" if os.name == "nt" else "query")
            command += ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                        "-I" + str(ROOT), *map(str, sources), "-o", str(executable)]
            if os.name != "nt":
                command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
            subprocess.run(command, check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
