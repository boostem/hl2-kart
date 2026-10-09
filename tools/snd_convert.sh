#!/usr/bin/env bash
# usage: snd_convert.sh <in> <out.wav|out.mp3>
#   .wav -> 44.1 kHz 16-bit PCM mono (sound effects)
#   .mp3 -> ~160 kbps stereo (music)
# Looping WAVs: ffmpeg drops cue chunks, so afterwards run tools/wav_loop.py <out.wav>.
set -euo pipefail
[ $# -eq 2 ] || { sed -n '2,5p' "$0" | sed 's/^# \?//'; exit 1; }
in=$1 out=$2
case "${out,,}" in
  *.wav) ffmpeg -v error -y -i "$in" -vn -ac 1 -ar 44100 -sample_fmt s16 -c:a pcm_s16le -map_metadata -1 "$out" ;;
  *.mp3) ffmpeg -v error -y -i "$in" -vn -ac 2 -ar 44100 -c:a libmp3lame -b:a 160k -map_metadata -1 "$out" ;;
  *) echo "output must end in .wav or .mp3" >&2; exit 1 ;;
esac
echo "wrote $out"
