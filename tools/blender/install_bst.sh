#!/usr/bin/env bash
# usage: tools/blender/install_bst.sh
#   Installs the latest Blender Source Tools (Artfunkel, GPL) into the user add-on directory of the installed
#   Blender and enables it headless. No sudo. BST publishes no GitHub releases, so this takes the newest tag.
set -euo pipefail
command -v blender >/dev/null || { echo "blender not found (pacman -S blender)" >&2; exit 1; }
repo=Artfunkel/BlenderSourceTools
tag=$(curl -fsS "https://api.github.com/repos/$repo/tags" | python3 -c 'import json,sys; print(json.load(sys.stdin)[0]["name"])')
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
echo "Downloading Blender Source Tools $tag"
curl -fsSL "https://api.github.com/repos/$repo/zipball/refs/tags/$tag" -o "$tmp/bst.zip"
python3 -I -c 'import sys,zipfile; sys.exit(0 if zipfile.is_zipfile(sys.argv[1]) else 1)' "$tmp/bst.zip" \
	|| { echo "download is not a zip" >&2; exit 1; }
mkdir "$tmp/x"; unzip -q "$tmp/bst.zip" -d "$tmp/x"
src=$(find "$tmp/x" -maxdepth 2 -type d -name io_scene_valvesource | head -1)
[ -n "$src" ] || { echo "io_scene_valvesource not found in the zip" >&2; exit 1; }
# Legacy add-ons (bl_info) live in <user scripts>/addons, asked from Blender itself so the version is right.
addons=$(blender -b --python-expr "import bpy; print('ADDONS=' + bpy.utils.user_resource('SCRIPTS', path='addons', create=True))" 2>/dev/null | sed -n 's/^ADDONS=//p')
[ -n "$addons" ] || { echo "could not find Blender's user add-on directory" >&2; exit 1; }
rm -rf "$addons/io_scene_valvesource"
cp -r "$src" "$addons/io_scene_valvesource"
echo "Installed to $addons/io_scene_valvesource"
blender -b --python-expr "
import bpy, addon_utils
addon_utils.enable('io_scene_valvesource', default_set=True, persistent=True)
bpy.ops.wm.save_userpref()
import io_scene_valvesource as m
print('BST_ENABLED', m.bl_info['version'])
" 2>&1 | grep -E 'BST_ENABLED|Error|Traceback' || { echo "enabling the add-on failed" >&2; exit 1; }
