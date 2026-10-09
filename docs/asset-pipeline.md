# Asset pipeline

One-time setup (no sudo; creates `tools/venv`, which is gitignored):

```sh
tools/setup_venv.sh
```

Needs `python3`, `ffmpeg` and `lame` on the PATH. `tools/test_assets.sh` is a self-test of the tools below
(set `VTF2TGA` to the Linux SDK's `bin/linux64/vtf2tga` to check the VTF with Valve's reader too).

## Textures

```sh
tools/venv/bin/python tools/img2vtf.py <image> <material path> [--type model|world] [--normal <image>] [--surfaceprop NAME] [--metal]
# e.g.
tools/venv/bin/python tools/img2vtf.py crate.png props/crate01 --type model --normal crate_n.png
```

- `<material path>` is relative to `game/mod_hl2mp/materials/`, without extension.
- Writes `<path>.vtf` and `<path>.vmt` there. Images are resized to a power of two (max 1024).
- VTF 7.2, full mip chain, DXT1 when the image has no alpha, DXT5 when it does.
- VMT shader: `VertexLitGeneric` for `--type model` (default), `LightmappedGeneric` for `--type world`, with
  `$basetexture` and `$surfaceprop`. `--normal` writes `<path>_normal.vtf` and sets `$bumpmap`; `--metal` adds
  `$envmap env_cubemap` and `$envmaptint`. Colour choices are up to the artist; edit the VMT as needed.
- Check a result: `vtf2tga -i <file>.vtf -o out.tga` (Linux SDK, `bin/linux64`).

## Sounds

```sh
tools/snd_convert.sh <in> <out.wav>   # SFX: 44.1 kHz, 16-bit PCM, mono
tools/snd_convert.sh <in> <out.mp3>   # music: ~160 kbps, 44.1 kHz stereo
```

Verify with `ffprobe`. Source loops a WAV from its `cue` chunk, and ffmpeg drops those, so for a looping sound run
afterwards:

```sh
tools/venv/bin/python tools/wav_loop.py <out.wav>   # adds a cue point at sample 0
```
