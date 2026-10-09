#!/usr/bin/env bash
# usage: assets_src/props/build_all.sh [prop ...]
#   Rebuilds the track dressing kit (all props, or the ones named): Blender builds the meshes and bakes the
#   textures, make_materials.py writes the VTFs and VMTs, studiomdl compiles the models into
#   game/mod_hl2mp/models/kart/props/. Needs blender, tools/venv (tools/setup_venv.sh) and the wine tools.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
blender -b -P assets_src/props/build_props.py -- "$@" 2>&1 | grep -E '^built|Error|Traceback' || true
tools/venv/bin/python assets_src/props/make_materials.py >/dev/null
props=("$@")
[ ${#props[@]} -gt 0 ] || props=($(ls assets_src/props/*/*.qc | xargs -n1 basename | sed 's/\.qc$//'))
for p in "${props[@]}"; do
	log=$(tools/wine/studiomdl.sh "assets_src/props/$p/$p.qc" 2>&1) || { echo "$log" | tail -20; echo "studiomdl failed: $p" >&2; exit 1; }
	echo "$log" | grep -iE 'warning|error' | grep -v 'MESA'  || true
	echo "compiled models/kart/props/$p.mdl"
done
