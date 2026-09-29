#!/usr/bin/env python3
"""One release identity for the factory filesystem and USB packages."""
import argparse
import os
from pathlib import Path
import re
import subprocess

def image_version(sdk):
    config = (sdk / 'target/d211/d213_devkitf/image_cfg.json').read_text()
    match = re.search(r'"version"\s*:\s*"([^"]+)"', config)
    if not match or not re.fullmatch(r'[A-Za-z0-9._-]{1,48}', match.group(1)):
        raise ValueError('Invalid image base version')
    return match.group(1)

def version(sdk, explicit=None):
    if explicit is not None and explicit != '':
        result = explicit
    else:
        base = image_version(sdk)
        try:
            rev = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', '--short=12', 'HEAD'],
                                          text=True, stderr=subprocess.DEVNULL).strip()
            dirty = subprocess.check_output(['git', '-C', str(sdk), 'status', '--porcelain', '--untracked-files=normal'],
                                            text=True, stderr=subprocess.DEVNULL)
            result = base + '-' + rev + ('-dirty' if dirty else '')
        except (OSError, subprocess.CalledProcessError):
            result = base + '-local'  # Preserve the exported-SDK build path.
    if not re.fullmatch(r'[A-Za-z0-9._-]{1,48}', result):
        raise ValueError('Invalid UN260 release version')
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', action='store_true')
    args = parser.parse_args()
    sdk = Path(__file__).resolve().parents[2]
    print(image_version(sdk) if args.base else version(sdk, os.environ.get('UN260_UPDATE_VERSION')))
