#!/usr/bin/env bash
# Creates tools/venv (gitignored) with the Python packages the asset tools need. No sudo.
# Pillow does the resizing and DXT compression. (no_vtf, suggested in the planning notes, only
# decodes VTFs and does not run on current click, so img2vtf.py writes the VTF container itself.)
set -euo pipefail
cd "$(dirname "$0")"
python3 -m venv venv
venv/bin/pip install --quiet --upgrade pip
venv/bin/pip install --quiet Pillow
echo "tools/venv ready"
