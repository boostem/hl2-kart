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

## Compiling models and textures on Linux

The model and texture compilers only exist as Windows executables, so `tools/wine/` runs them under wine from the
Windows depot of app 243750. Needs `wine` and a prefix; paths live in `tools/wine/env.sh` and can be overridden
from the environment:

| Variable        | Default                                                                   |
| --------------- | ------------------------------------------------------------------------- |
| `SDK2013_WIN`   | `$HOME/sdk2013mp_win` (Windows depot; tools in `bin/`)                    |
| `WINEPREFIX`    | `$HOME/.wine-sdk2013`                                                     |
| `SDK2013_LINUX` | `$HOME/.local/share/Steam/steamapps/common/Source SDK Base 2013 Multiplayer` |

`env.sh` also sets `WINEDEBUG=-all` and provides `winpath` (Linux path to `Z:\...`). The scripts need no other
environment and run headless, e.g. from a clean shell: `env -i HOME=$HOME bash tools/wine/studiomdl.sh ...`.

```sh
tools/wine/studiomdl.sh assets_src/test/test_cube/test_cube.qc   # -> game/mod_hl2mp/models/<$modelname>
tools/wine/vtex.sh assets_src/test/test_cube/test_cube.tga       # -> game/mod_hl2mp/materials/models/test/test_cube.vtf
tools/wine/vtex.sh <image.tga|psd> <material dir>                # material dir relative to materials/
```

- `studiomdl.sh <file.qc> [options]` runs `studiomdl.exe -game <tools gamedir> -nop4 -verbose <qc>`. The model
  lands where the QC's `$modelname` says, under `game/mod_hl2mp/models/`. Commit the `.mdl`, `.vvd`, `.dx90.vtx`
  and `.phy`; the `.sw.vtx` and `.dx80.vtx` it also writes are gitignored (SDK 2013 MP doesn't load them).
- `vtex.sh <image> [material dir]` runs `vtex.exe -nopause -game <tools gamedir> -outdir <dir> <image>`. Without
  a material dir it reads `$basetexture` from a sibling `<image>.vmt` and copies that VMT next to the VTF. vtex
  options (`nomip 1`, `clamps 1`, `normal 1`, ...) go in a sibling `<image>.txt`, which vtex picks up itself.
  vtex skips a texture whose source hasn't changed ("is up-to-date").
- Both exit non-zero when the tool fails.
- Check the results: `head -c4 file.mdl` prints `IDST` (`file` just says "data"); for the VTF run
  `LD_LIBRARY_PATH=$SDK2013_LINUX/bin/linux64 $SDK2013_LINUX/bin/linux64/vtf2tga -i file.vtf -o out.tga`.

### The tools game dir

Without Steam running in the prefix, the `|appid_243750|` search paths in `game/mod_hl2mp/gameinfo.txt` can't
resolve. So the tools get their own game dir, `tools/wine/gamedir/`, which is **only for the compilers** (the game
never reads it). Its `gameinfo.txt` mounts the mod and the Linux SDK's `hl2mp`, `hl2_complete`, `hl2` and
`platform` folders and VPKs by absolute `Z:\` path. studiomdl writes to `<gamedir>/models/`, so the game dir's
`models` and `materials` are symlinks into `game/mod_hl2mp/`. When `SDK2013_LINUX` isn't the default path,
`env.sh` writes a copy of the game dir with your path to `~/.cache/hl2kart-tools-gamedir/` and uses that instead.

### Test model

`assets_src/test/test_cube/` is a hand-written 48-unit cube (one bone, `test_cube.smd`, `test_cube_phys.smd`,
`test_cube.qc`) with a 64x64 texture made with ImageMagick:

```sh
magick -size 64x64 xc:'#6f7a6a' -fill none -stroke '#3f463c' -strokewidth 4 -draw 'rectangle 3,3 60,60' \
  -strokewidth 2 -draw 'line 8,8 55,55' -alpha off -depth 8 -type TrueColor -compress none test_cube.tga
```

In game: `sv_cheats 1; prop_physics_create test/test_cube.mdl`.

### Windows-only (or untested under wine)

- `hammer.exe`, `hlmv.exe`, `hlfaceposer.exe`: GUI tools. Not tried here, since these scripts run headless with no
  display. They may run under wine on a desktop session, but nothing in the pipeline depends on them.
- The 64-bit builds in `bin/x64/` aren't used; the 32-bit tools in `bin/` work in the 64-bit (WoW64) prefix.
- Wine itself can print `MESA-EGL: warning: ...` lines when `DISPLAY` is set; they are harmless and don't show in
  a clean shell.
