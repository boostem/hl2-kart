#!/usr/bin/env bash
# Runs HL2 Kart as a headless dedicated server.
#
#   game/run_server.sh [extra srcds arguments]
#
# Needs a Source SDK Base 2013 Dedicated Server install (Steam app 244310):
#   steamcmd +login anonymous +app_update 244310 validate +quit
# The script looks for it in $SRCDS_DIR, then in the Steam libraries. The
# Source SDK Base 2013 Multiplayer install has srcds_linux64 too, but not the
# server libraries (bin/linux64/dedicated_srv.so) it loads, so it won't do.
#
# Environment overrides: SRCDS_DIR, KART_MAP (kart_arena), KART_MAXPLAYERS (12),
# KART_PORT (27015). Settings live in mod_hl2mp/cfg/server.cfg.

set -euo pipefail

GAME_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MOD_DIR="$GAME_ROOT/mod_hl2mp"
MAP=${KART_MAP:-kart_arena}
MAXPLAYERS=${KART_MAXPLAYERS:-12}
PORT=${KART_PORT:-27015}

is_srcds_dir() {
	[[ -x "$1/srcds_linux64" && -f "$1/bin/linux64/dedicated_srv.so" ]]
}

# Steam library roots: the default ones, steamcmd's, and every library listed
# in libraryfolders.vdf.
steam_libraries() {
	local root vdf
	for root in "$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/.steam/steamcmd" "$HOME/Steam"; do
		[[ -d "$root/steamapps" ]] || continue
		echo "$root"
		vdf="$root/steamapps/libraryfolders.vdf"
		if [[ -f "$vdf" ]]; then
			sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"[[:space:]]*$/\1/p' "$vdf"
		fi
	done
}

find_srcds() {
	local lib name
	while read -r lib; do
		for name in "Source SDK Base 2013 Dedicated Server" "Source SDK Base 2013 Multiplayer"; do
			if is_srcds_dir "$lib/steamapps/common/$name"; then
				echo "$lib/steamapps/common/$name"
				return 0
			fi
		done
	done < <(steam_libraries)
	return 1
}

if [[ -n "${SRCDS_DIR:-}" ]]; then
	if ! is_srcds_dir "$SRCDS_DIR"; then
		echo "SRCDS_DIR=$SRCDS_DIR has no srcds_linux64 and bin/linux64/dedicated_srv.so." >&2
		exit 1
	fi
elif ! SRCDS_DIR=$(find_srcds); then
	echo "No Source SDK Base 2013 Dedicated Server found. Install it (app 244310) with" >&2
	echo "  steamcmd +login anonymous +app_update 244310 validate +quit" >&2
	echo "or point SRCDS_DIR at it." >&2
	exit 1
fi

if [[ ! -f "$MOD_DIR/bin/linux64/server.so" ]]; then
	echo "$MOD_DIR/bin/linux64/server.so is missing: build the mod first (cd src && ./buildallprojects)." >&2
	exit 1
fi

if [[ ! -f "$HOME/.steam/sdk64/steamclient.so" ]]; then
	echo "Warning: ~/.steam/sdk64/steamclient.so is missing; the server can't log on to Steam without it." >&2
	echo "         Copy it from steamcmd (linux64/steamclient.so) or install the Steam client." >&2
fi

echo "Dedicated server: $SRCDS_DIR"
echo "Mod: $MOD_DIR"

cd "$SRCDS_DIR"
export LD_LIBRARY_PATH="$SRCDS_DIR/bin/linux64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec ./srcds_linux64 -game "$MOD_DIR" -console -port "$PORT" +maxplayers "$MAXPLAYERS" +map "$MAP" "$@"
