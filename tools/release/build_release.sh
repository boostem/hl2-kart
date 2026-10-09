#!/usr/bin/env bash
# Packages HL2 Kart as a Source SDK Base 2013 Multiplayer sourcemod.
#
#   tools/release/build_release.sh
#
# Stages the mod in release/build/<name>/ and zips it to
# release/<name>-<version>.zip. Build the binaries first (cd src &&
# ./buildallprojects). See docs/release.md.
#
# Only files tracked in git under game/mod_hl2mp/ go in (no local configs,
# logs or screenshots), plus the built bin/linux64/*.so. materials/, models/,
# sound/, particles/ and shaders/ are packed into <name>_pak.vpk; maps, cfg,
# resource and scripts stay loose. The build fails if the VPK holds any path
# that one of the SDK's own VPKs also holds, so no Valve file ships.
#
# Environment overrides: SDK_DIR (the Source SDK Base 2013 Multiplayer
# install, found in the Steam libraries by default), RELEASE_NAME (hl2kart).

set -euo pipefail

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
MOD_SRC="$REPO/game/mod_hl2mp"
NAME=${RELEASE_NAME:-hl2kart}
VERSION=$(tr -d '[:space:]' < "$REPO/VERSION")
COMMIT=$(git -C "$REPO" rev-parse --short HEAD)
OUT="$REPO/release"
STAGE="$OUT/build/$NAME"
PAK_DIR="$OUT/build/${NAME}_pak"
ZIP="$OUT/$NAME-$VERSION.zip"

# Folders packed into the VPK; everything else stays loose.
PAK_FOLDERS=(materials models sound particles shaders)

# Tracked files that are Valve's (the SDK's own examples and HL2MP map).
EXCLUDE_RE='^(maps/dm_lockdown\.bsp|materials/example_model_material\.vmt|shaders/fxc/example_model_.*)$'

die() { echo "build_release: $*" >&2; exit 1; }

steam_libraries() {
	local root vdf
	for root in "$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/Steam"; do
		[[ -d "$root/steamapps" ]] || continue
		echo "$root"
		vdf="$root/steamapps/libraryfolders.vdf"
		if [[ -f "$vdf" ]]; then
			sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"[[:space:]]*$/\1/p' "$vdf"
		fi
	done
}

find_sdk() {
	local lib
	if [[ -n "${SDK_DIR:-}" ]]; then
		echo "$SDK_DIR"
		return
	fi
	while read -r lib; do
		if [[ -x "$lib/steamapps/common/Source SDK Base 2013 Multiplayer/bin/linux64/vpk" ]]; then
			echo "$lib/steamapps/common/Source SDK Base 2013 Multiplayer"
			return
		fi
	done < <(steam_libraries)
}

SDK=$(find_sdk)
[[ -n "$SDK" && -x "$SDK/bin/linux64/vpk" ]] || die "no Source SDK Base 2013 Multiplayer install found; set SDK_DIR"
vpk() { LD_LIBRARY_PATH="$SDK/bin/linux64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$SDK/bin/linux64/vpk" "$@"; }

shopt -s nullglob
BINARIES=("$MOD_SRC"/bin/linux64/*.so)
shopt -u nullglob
[[ ${#BINARIES[@]} -gt 0 ]] || die "no binaries in game/mod_hl2mp/bin/linux64; run: cd src && ./buildallprojects"

echo "Packaging $NAME $VERSION ($COMMIT) from $MOD_SRC"
rm -rf "$OUT/build" "$ZIP"
mkdir -p "$STAGE" "$PAK_DIR"

# Stage tracked files, splitting them between the VPK and the loose folder.
pak_re="^($(IFS='|'; echo "${PAK_FOLDERS[*]}"))/"
while IFS= read -r -d '' file; do
	rel=${file#game/mod_hl2mp/}
	[[ $rel =~ $EXCLUDE_RE ]] && continue
	case $rel in
	gameinfo.txt | steam.inf | hl2kart.fgd) continue ;;
	esac
	if [[ $rel =~ $pak_re ]]; then
		dest="$PAK_DIR/$rel"
	else
		dest="$STAGE/$rel"
	fi
	mkdir -p "$(dirname "$dest")"
	cp "$REPO/$file" "$dest"
done < <(git -C "$REPO" ls-files -z -- game/mod_hl2mp)

# Localisation files are looked up by the mod folder's name.
for file in "$STAGE"/resource/mod_hl2mp_*.txt; do
	[[ -e $file ]] && mv "$file" "$STAGE/resource/${NAME}_${file##*/mod_hl2mp_}"
done

mkdir -p "$STAGE/bin/linux64"
cp "${BINARIES[@]}" "$STAGE/bin/linux64/"
# The build keeps debug info in the .so files; players don't need it.
if command -v strip > /dev/null; then
	strip --strip-debug "$STAGE"/bin/linux64/*.so
else
	echo "warning: strip not found, shipping unstripped binaries" >&2
fi

# gameinfo.txt: search paths relative to the mod folder, and mount the VPK.
sed -e "s#mod_hl2mp/custom/\*#|gameinfo_path|custom/*#" \
	-e "s#mod_hl2mp/download#|gameinfo_path|download#" \
	-e "s#^\([[:space:]]*\)gamebin\([[:space:]]*\)|gameinfo_path|bin\$#&\n\n\1// The mod's own content.\n\1game+mod\t\t\t|gameinfo_path|${NAME}_pak.vpk#" \
	"$MOD_SRC/gameinfo.txt" > "$STAGE/gameinfo.txt"
grep -q "${NAME}_pak.vpk" "$STAGE/gameinfo.txt" || die "could not add the VPK to gameinfo.txt"
grep -q 'mod_hl2mp' "$STAGE/gameinfo.txt" && die "gameinfo.txt still refers to mod_hl2mp"

cat > "$STAGE/steam.inf" <<INF
PatchVersion=$VERSION
ClientVersion=$VERSION
ServerVersion=$VERSION
ProductName=$NAME
appID=243750
ServerAppID=244310
INF
echo "HL2 Kart $VERSION ($COMMIT)" > "$STAGE/version.txt"
cat > "$STAGE/bin/README.txt" <<TXT
bin/linux64 holds the Linux binaries. Windows needs client.dll and server.dll
in bin/x64, built on Windows (src/createallprojects.bat, then Visual Studio);
this package doesn't include them.
TXT

# Pack the content. `vpk <dir>` writes <dir>.vpk next to the directory.
( cd "$OUT/build" && vpk "${NAME}_pak" > /dev/null )
mv "$OUT/build/${NAME}_pak.vpk" "$STAGE/"
rm -rf "$PAK_DIR"

# Valve check: nothing in our VPK may share a path with the SDK's VPKs.
listing=$(vpk l "$STAGE/${NAME}_pak.vpk" | tr '[:upper:]' '[:lower:]' | sort -u)
[[ -n $listing ]] || die "the VPK is empty"
valve=$(for pak in "$SDK"/*/*_dir.vpk; do vpk l "$pak"; done | tr '[:upper:]' '[:lower:]' | sort -u)
clashes=$(comm -12 <(echo "$listing") <(echo "$valve"))
if [[ -n $clashes ]]; then
	echo "$clashes" >&2
	die "the VPK holds Valve paths (above)"
fi
if grep -v -E '^(materials|models|sound|particles|shaders)/' <<<"$listing"; then
	die "unexpected paths in the VPK (above)"
fi
echo "VPK: $(wc -l <<<"$listing") files, none of them Valve paths"

( cd "$OUT/build" && python3 -m zipfile -c "$ZIP" "$NAME" )
echo "Wrote $ZIP ($(du -h "$ZIP" | cut -f1))"
