# Environment for running the Windows SDK 2013 tools under wine. Source it: . tools/wine/env.sh
# Override any of these in the environment before sourcing.
#   SDK2013_WIN    Windows depot of app 243750 (studiomdl.exe, vtex.exe, ... in bin/)
#   WINEPREFIX     wine prefix for the tools
#   SDK2013_LINUX  Linux install of Source SDK Base 2013 Multiplayer (content the tools read; vtf2tga)
export SDK2013_WIN="${SDK2013_WIN:-$HOME/sdk2013mp_win}"
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-sdk2013}"
export SDK2013_LINUX="${SDK2013_LINUX:-$HOME/.local/share/Steam/steamapps/common/Source SDK Base 2013 Multiplayer}"
export WINEDEBUG="${WINEDEBUG:--all}"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-mscoree,mshtml=}"   # no Mono/Gecko install prompts

KART_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
KART_MOD="$KART_ROOT/game/mod_hl2mp"

# winpath /abs/linux/path -> Z:\abs\linux\path (wine maps Z: to /)
winpath() {
  local p
  p="$(realpath -m -- "$1")"
  printf 'Z:%s\n' "${p//\//\\}"
}

# The tools-only game dir. studiomdl writes to <gamedir>/models, so the gamedir's models/ and materials/
# are symlinks into the mod; the mod's folders must exist first. The committed gameinfo.txt has the default SDK2013_LINUX baked in; for any
# other SDK path, write a copy with that path to a cache dir and use it instead.
kart_tools_gamedir() {
  local dir="$KART_ROOT/tools/wine/gamedir" default_sdk='Z:\home\boostem\.local\share\Steam\steamapps\common\Source SDK Base 2013 Multiplayer'
  local sdk; sdk="$(winpath "$SDK2013_LINUX")"
  mkdir -p "$KART_MOD/models" "$KART_MOD/materials"
  if [ "$sdk" != "$default_sdk" ]; then
    local out="${XDG_CACHE_HOME:-$HOME/.cache}/hl2kart-tools-gamedir/$(printf %s "$KART_ROOT" | md5sum | cut -c1-8)"
    mkdir -p "$out"
    local mod; mod="$(winpath "$KART_MOD")"
    DEFAULT_SDK="$default_sdk" SDK="$sdk" MOD="$mod" awk '
      function sub_lit(from, to,   i) { i = index($0, from); if (i) $0 = substr($0, 1, i - 1) to substr($0, i + length(from)) }
      { sub_lit("|gameinfo_path|..\\..\\..\\game\\mod_hl2mp", ENVIRON["MOD"]); sub_lit(ENVIRON["DEFAULT_SDK"], ENVIRON["SDK"]); print }' "$dir/gameinfo.txt" > "$out/gameinfo.txt"
    ln -sfn "$KART_MOD/models" "$out/models"
    ln -sfn "$KART_MOD/materials" "$out/materials"
    dir="$out"
  fi
  printf '%s\n' "$dir"
}

# kart_check_tool <name.exe>: fail early with a hint when the depot or prefix is missing.
kart_check_tool() {
  command -v wine >/dev/null || { echo "wine not found on PATH" >&2; return 1; }
  [ -f "$SDK2013_WIN/bin/$1" ] || { echo "$SDK2013_WIN/bin/$1 not found; set SDK2013_WIN to the Windows depot of app 243750" >&2; return 1; }
  [ -d "$WINEPREFIX" ] || { echo "wine prefix $WINEPREFIX not found; set WINEPREFIX" >&2; return 1; }
  [ -d "$SDK2013_LINUX/hl2" ] || { echo "$SDK2013_LINUX/hl2 not found; set SDK2013_LINUX to the Linux SDK install" >&2; return 1; }
}
