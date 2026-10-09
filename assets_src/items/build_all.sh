#!/usr/bin/env bash
# usage: assets_src/items/build_all.sh [item ...]
#   Rebuilds the kart item models (all, or the ones named): Blender builds the meshes and bakes the textures,
#   make_materials.py draws the other textures and writes every VTF and VMT, studiomdl compiles the models into
#   game/mod_hl2mp/models/kart/items/. Needs blender, tools/venv (tools/setup_venv.sh) and the wine tools.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
blender -b -P assets_src/items/build_items.py -- "$@" 2>&1 | grep -E '^built|Error|Traceback' || true
tools/venv/bin/python assets_src/items/make_materials.py >/dev/null
items=("$@")
[ ${#items[@]} -gt 0 ] || items=($(ls assets_src/items/*/*.qc | xargs -n1 basename | sed 's/\.qc$//'))
for p in "${items[@]}"; do
	log=$(tools/wine/studiomdl.sh "assets_src/items/$p/$p.qc" 2>&1) || { echo "$log" | tail -20; echo "studiomdl failed: $p" >&2; exit 1; }
	echo "$log" | grep -iE 'warning|error' | grep -v 'MESA'  || true
	echo "compiled models/kart/items/$p.mdl"
done
