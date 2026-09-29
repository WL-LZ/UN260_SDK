#!/bin/sh
set -eu
exec python3 tools/un260-update/factory_layout.py --root "$1"
