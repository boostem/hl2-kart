#!/usr/bin/env bash
# usage: compile_map.sh <map.vmf> [--final]
#   Compiles a map with vbsp, vvis and vrad under wine and copies the .bsp to game/mod_hl2mp/maps/.
#   Default is a fast compile (vvis -fast, vrad -fast); --final runs full vvis and vrad -both -final.
#   Fails when vbsp reports a leak. Work files and logs go to a build dir outside the repo
#   ($KART_MAP_BUILD, default ~/.cache/hl2kart-maps/<map>). Settings: tools/wine/env.sh.
set -euo pipefail
usage() { sed -n '2,6p' "$0" | sed 's/^# \?//'; exit 1; }
final=0 vmf=
for a in "$@"; do
  case $a in
    --final) final=1 ;;
    -*) usage ;;
    *) [ -z "$vmf" ] || usage; vmf=$a ;;
  esac
done
[ -n "$vmf" ] || usage
[ -f "$vmf" ] || { echo "$vmf not found" >&2; exit 1; }
here="$(dirname "${BASH_SOURCE[0]}")"
. "$here/env.sh"
name=$(basename "$vmf" .vmf)
build="${KART_MAP_BUILD:-${XDG_CACHE_HOME:-$HOME/.cache}/hl2kart-maps/$name}"
mkdir -p "$build"
rm -f "$build/$name".{bsp,prt,lin,log}
cp -- "$vmf" "$build/$name.vmf"

# step <log> <tool.sh> <args...>: run a tool, keep its output in <log>, fail when it fails.
step() {
  local log=$1; shift
  echo "== $(basename "$1" .sh) ${*:2}"
  "$@" 2>&1 | tr -d '\r' | tee "$log"
}

step "$build/vbsp.out" "$here/vbsp.sh" "$build/$name.vmf"
if grep -qi 'leak' "$build/vbsp.out" || [ -f "$build/$name.lin" ]; then
  echo "vbsp: $name LEAKED (pointfile: $build/$name.lin)" >&2; exit 1
fi
[ -f "$build/$name.bsp" ] || { echo "vbsp did not write $build/$name.bsp" >&2; exit 1; }
if [ $final = 1 ]; then
  step "$build/vvis.out" "$here/vvis.sh" "$build/$name.bsp"
  step "$build/vrad.out" "$here/vrad.sh" -both -final "$build/$name.bsp"
else
  step "$build/vvis.out" "$here/vvis.sh" -fast "$build/$name.bsp"
  step "$build/vrad.out" "$here/vrad.sh" -fast "$build/$name.bsp"
fi
mkdir -p "$KART_MOD/maps"
cp -- "$build/$name.bsp" "$KART_MOD/maps/"
echo "wrote $KART_MOD/maps/$name.bsp ($(du -h "$KART_MOD/maps/$name.bsp" | cut -f1)); logs in $build"
