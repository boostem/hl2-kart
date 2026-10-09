#!/usr/bin/env bash
# Self-test for img2vtf.py, snd_convert.sh and wav_loop.py; outputs go to a temp dir, not the repo.
# VTF2TGA=path/to/bin/linux64/vtf2tga (Linux SDK) additionally verifies the VTF with Valve's reader.
set -euo pipefail
cd "$(dirname "$0")/.."
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
PY=tools/venv/bin/python

magick -size 256x256 gradient:red-blue "$T/in.png"
$PY tools/img2vtf.py "$T/in.png" test/tex --game-dir "$T" --type world --normal "$T/in.png"
V=$T/game/mod_hl2mp/materials/test
[ "$(head -c3 "$V/tex.vtf")" = "VTF" ] && grep -q LightmappedGeneric "$V/tex.vmt" && grep -q '\$bumpmap' "$V/tex.vmt"
if [ -n "${VTF2TGA:-}" ]; then
  LD_LIBRARY_PATH="$(dirname "$VTF2TGA")" "$VTF2TGA" -i "$V/tex.vtf" -o "$T/out.tga"
  test -s "$T/out.tga"
fi

ffmpeg -v error -f lavfi -i "sine=frequency=440:duration=1" -ac 2 "$T/tone.wav"
tools/snd_convert.sh "$T/tone.wav" "$T/sfx.wav"
tools/snd_convert.sh "$T/tone.wav" "$T/music.mp3"
$PY tools/wav_loop.py "$T/sfx.wav"
probe=$(ffprobe -v error -show_entries stream=sample_rate,channels,sample_fmt -of default=nw=1 "$T/sfx.wav" | sort | tr '\n' ' ')
[ "$probe" = "channels=1 sample_fmt=s16 sample_rate=44100 " ] || { echo "bad wav: $probe"; exit 1; }
grep -q 'cue ' "$T/sfx.wav"
echo "self-test passed"
