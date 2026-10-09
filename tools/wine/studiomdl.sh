#!/usr/bin/env bash
# usage: studiomdl.sh <file.qc> [extra studiomdl options]
#   Compiles a model with the Windows studiomdl.exe under wine. Output goes where the QC's $modelname
#   says, under game/mod_hl2mp/models/. Settings: tools/wine/env.sh.
set -euo pipefail
[ $# -ge 1 ] || { sed -n '2,4p' "$0" | sed 's/^# \?//'; exit 1; }
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"
kart_check_tool studiomdl.exe
qc=$(realpath -- "$1"); shift
[ -f "$qc" ] || { echo "$qc not found" >&2; exit 1; }
gamedir="$(kart_tools_gamedir)"
cd "$SDK2013_WIN/bin"
exec wine studiomdl.exe -game "$(winpath "$gamedir")" -nop4 -verbose "$@" "$(winpath "$qc")"
