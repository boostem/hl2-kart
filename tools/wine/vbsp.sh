#!/usr/bin/env bash
# usage: vbsp.sh [vbsp options] <map>
#   Runs the Windows vbsp.exe under wine with -game <tools gamedir>. Output goes next to the map file.
#   tools/wine/compile_map.sh runs vbsp, vvis and vrad in order. Settings: tools/wine/env.sh.
set -euo pipefail
[ $# -ge 1 ] || { sed -n '2,4p' "$0" | sed 's/^# \?//'; exit 1; }
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"
kart_check_tool vbsp.exe
map=$(realpath -- "${!#}")
[ -f "$map" ] || { echo "${!#} not found" >&2; exit 1; }
set -- "${@:1:$#-1}"
gamedir="$(kart_tools_gamedir)"
cd "$SDK2013_WIN/bin"
exec wine vbsp.exe -game "$(winpath "$gamedir")" "$@" "$(winpath "$map")"
