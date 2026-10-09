#!/usr/bin/env python3
"""Add a cue point at sample 0 to a WAV so Source loops it.

usage: wav_loop.py <in.wav> [out.wav]   (in place when out is omitted)

ffmpeg drops cue chunks, so run this after snd_convert.sh. Source loops a WAV from its first cue
point to the end. Uses only the standard library.
"""
import struct
import sys
import wave
from pathlib import Path


def add_cue(src, dst):
    with wave.open(str(src), "rb") as w:
        params = w.getparams()
        frames = w.readframes(w.getnframes())
    fmt = struct.pack("<HHIIHH", 1, params.nchannels, params.framerate,
                      params.framerate * params.nchannels * params.sampwidth,
                      params.nchannels * params.sampwidth, params.sampwidth * 8)
    # one cue point: id 1, position 0, chunk "data", chunk start 0, block start 0, sample offset 0
    cue = struct.pack("<II4sIII", 1, 0, b"data", 0, 0, 0)
    cue = struct.pack("<I", 1) + cue
    body = b"WAVE"
    for tag, data in ((b"fmt ", fmt), (b"cue ", cue), (b"data", frames)):
        body += tag + struct.pack("<I", len(data)) + data + (b"\0" if len(data) % 2 else b"")
    Path(dst).write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    add_cue(sys.argv[1], sys.argv[-1])
