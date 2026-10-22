#!/usr/bin/env python3
"""Verify protocol reject names and report identities against all real locales."""
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

from test_i18n_support import with_i18n

ROOT = Path(__file__).resolve().parents[1]
# Protocol framework 10, command 0x0C, currency error table. Preserve its spelling
# and punctuation. 0x31 has no English name in the protocol, so retain its code.
# This independent fixture must not be derived from the production name table.
EXPECTED_NAMES = [
    "No Error",
    "IMG Feature1 Suspect", "IMG Feature2 Suspect", "IMG Feature3 Suspect",
    "IMG Feature4 Suspect", "IMG Feature5 Suspect", "IMG Feature6 Suspect",
    "IMG Feature7 Suspect", "IMG Feature8 Suspect", "IMG Feature9 Suspect",
    "IMG Feature10 Suspect", "IMG Feature11 Suspect", "IMG Feature12 Suspect",
    "IMG Feature13 Suspect", "IMG Feature14 Suspect", "IMG Feature15 Suspect",
    "ST Full", "MG Quantity Suspect", "MG Position Suspect", "MT Quantity Suspect",
    "MT Code Suspect", "UV Suspect", "Double Suspect 1", "Double Suspect 2",
    "Long", "Short", "GAP", "IMG Time out", "IMG Size Unknow", "IMG Ort Unknow",
    "IMG Version Unknow", "IMG Face Error", "IMG Ort Error", "ANGLE", "IR-OVD",
    "IR –MT", "Hole", "DogEar", "DIRT", "Tape", "Tears", "Crumples", "De_ink",
    "Soiling", "eError_Limpness", "IMG Face&Ort Error", "OCR1 Error", "OCR2 Error",
    "OCR3 Error", "0x31",
]
EXPECTED_LOCALES = {"en", "zh-Hans", "ko", "ja", "ar", "tr", "fr", "pl", "ru", "de"}


def verify_reports(directory):
    locales = set()
    for report in sorted(directory.glob("report-*.html")):
        script = report.read_text(encoding="utf-8")
        match = re.search(r"const text=(\{.*?\});window\.reportLocale=(\"[^\"]+\")", script, re.S)
        assert match is not None, f"Missing report catalogue: {report.name}"
        translations = json.loads(match.group(1))
        locale = json.loads(match.group(2))
        assert locale not in locales, f"Duplicate locale: {locale}"
        locales.add(locale)
        assert report.name == f"report-{locale}.html"
        for code, name in enumerate(EXPECTED_NAMES):
            assert translations.get(name) == name, (
                f"{locale}: 0x{code:02X} must remain {name!r}, got {translations.get(name)!r}"
            )
        assert translations.get("Unknown Error", "Unknown Error") == "Unknown Error", (
            f"{locale}: unknown reject name was translated: {translations.get('Unknown Error')!r}"
        )
        if locale in {"zh-Hans", "ko"}:
            assert translations.get("Amount") not in {None, "Amount"}, (
                f"{locale}: ordinary report labels must still be translated"
            )
    assert locales == EXPECTED_LOCALES, f"Unexpected report locales: {locales}"
    print("report i18n: 10 captured locales, 50 protocol-name identities and ordinary translations passed")


def main():
    assert len(EXPECTED_NAMES) == 0x32
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
    if not compiler:
        raise SystemExit("Host C compiler required")
    with tempfile.TemporaryDirectory(prefix="un260-list-i18n-") as directory:
        work = Path(directory)
        fixture = "static const char *const expected_names[] = {\n"
        fixture += "".join(f"    {json.dumps(name, ensure_ascii=False)},\n" for name in EXPECTED_NAMES)
        fixture += "};\n"
        (work / "protocol_error_names.inc").write_text(fixture, encoding="utf-8")
        executable = work / ("test.exe" if os.name == "nt" else "test")
        sources = with_i18n([ROOT / name for name in [
            "tools/test_list_i18n.c", "un260/lv_system/ui_text_page.c",
            "un260/lv_system/ui_text_widget.c", "un260/counting/counting_reject_reason.c",
            "un260/lv_system/ui_report_i18n.c",
        ]], ROOT)
        command = shlex.split(compiler) if os.environ.get("CC") else [compiler]
        command += ["-std=c11", "-D_POSIX_C_SOURCE=200809L", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(ROOT), "-I" + str(work), *map(str, sources), "-o", str(executable)]
        if os.name != "nt":
            command += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
        subprocess.run(command, check=True)
        subprocess.run([str(executable), str(work)], check=True)
        verify_reports(work)


if __name__ == "__main__":
    main()
