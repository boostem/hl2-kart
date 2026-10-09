#!/usr/bin/env bash
# usage: vtex.sh <file.tga|file.psd> [material dir]
#   Compiles a texture with the Windows vtex.exe under wine into game/mod_hl2mp/materials/<material dir>/.
#   Without [material dir], it is taken from $basetexture in a sibling <file>.vmt, which is copied there too.
#   vtex options (nomip, nolod, clamps, normal, ...) go in a sibling <file>.txt, which vtex reads itself.
#   Settings: tools/wine/env.sh.
set -euo pipefail
[ $# -ge 1 ] && [ $# -le 2 ] || { sed -n '2,6p' "$0" | sed 's/^# \?//'; exit 1; }
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"
kart_check_tool vtex.exe
src=$(realpath -- "$1")
[ -f "$src" ] || { echo "$1 not found" >&2; exit 1; }
vmt="${src%.*}.vmt"
if [ $# -eq 2 ]; then
  matdir=$2
elif [ -f "$vmt" ]; then
  base=$(sed -n 's/^[[:space:]]*"\?\$basetexture"\?[[:space:]]\+"\?\([^"[:space:]]*\)"\?.*/\1/Ip' "$vmt" | head -n1)
  [ -n "$base" ] || { echo "no \$basetexture in $vmt; pass the material dir" >&2; exit 1; }
  matdir=$(dirname "${base//\\//}")
else
  echo "no $vmt to read the material dir from; pass it as the second argument" >&2; exit 1
fi
matdir=${matdir#/}; matdir=${matdir%/}
gamedir="$(kart_tools_gamedir)"
outdir="$KART_MOD/materials/$matdir"
mkdir -p "$outdir"
(cd "$SDK2013_WIN/bin" && wine vtex.exe -nopause -game "$(winpath "$gamedir")" -outdir "$(winpath "$outdir")" "$(winpath "$src")")
vtf="$outdir/$(basename "${src%.*}").vtf"
[ -f "$vtf" ] || { echo "vtex did not write $vtf" >&2; exit 1; }   # vtex skips unchanged sources ("up-to-date")
if [ -f "$vmt" ]; then cp -- "$vmt" "$outdir/"; echo "copied $(basename "$vmt") to $outdir"; fi
echo "wrote $vtf"
