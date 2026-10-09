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

## Compiling maps on Linux

The map compilers run under wine the same way, with the same settings (`tools/wine/env.sh`) and tools game dir:

```sh
tools/wine/compile_map.sh assets_src/maps/kart_arena.vmf           # fast: vvis -fast, vrad -fast
tools/wine/compile_map.sh assets_src/maps/kart_arena.vmf --final   # full vvis, vrad -both -final (HDR too)
```

- `compile_map.sh <map.vmf> [--final]` copies the VMF to a build dir outside the repo (`$KART_MAP_BUILD`, default
  `~/.cache/hl2kart-maps/<map>/`, where the `.prt`, `.lin`, `.log` and each tool's output `vbsp.out`, `vvis.out`,
  `vrad.out` stay), runs vbsp, vvis and vrad, and copies the `.bsp` to `game/mod_hl2mp/maps/`.
- It stops with an error when vbsp reports a leak (the pointfile is the `.lin` in the build dir) or any tool fails.
  vbsp still prints `Could not locate 'GameData' key` (that is Hammer's FGD setting) and vrad
  `Couldn't open texlight file ... lights.rad` (no texture lights); both are harmless.
- The fast compile has LDR lighting only, which the game falls back to with HDR on. Use `--final` before a release.
  The kart arena takes seconds either way.
- The single tools take their options before the map, e.g. `tools/wine/vvis.sh -fast <map>.bsp` or
  `tools/wine/vrad.sh -both -final <map>.bsp`; each adds `-game <tools gamedir>` and writes next to the map.
- Commit a compiled `.bsp` only if it is under 10 MB.

### Kart test arena (`kart_arena`)

`assets_src/maps/kart_arena.vmf` is plain text, written by `assets_src/maps/kart_arena.py` (standard library only:
`python3 assets_src/maps/kart_arena.py`) so the banked curve's and ramps' planes needn't be typed by hand. Edit the
layout in the script, regenerate the VMF, compile, and commit all three. The VMF opens in Hammer too.

- A 6144 x 6144 x 1024 box sealed by `tools/toolsskybox` brushes, skybox `sky_day01_01`, one `light_environment`.
  Floor `concrete/concretefloor011a`, 128-high perimeter walls `concrete/concretewall004a`.
- A 3584 x 3584 central island, 128 high, makes a 1216-wide lane round it, driven counter-clockwise seen from above.
- **Race**: a `kart_race_manager` (3 laps, track name "Kart Arena"). The loop is driven counter-clockwise: south
  straight east, banked curve, east lane north, north lane west, west lane south, back to the line.
- **Start/finish**: `kart_finish` across the south lane at x -1536, wall to island and 512 tall, with a
  `dev/dev_hazzardstripe01a` overlay painted along it (on a floor brush of its own: vbsp allows an overlay 64 faces)
  and a cone at each end. 8 `kart_start` (grid 0-7) in a 2x4 grid, 96 apart, behind it facing east, pole on the
  island side. 8 `info_player_deathmatch` further back across the lane, for players beyond the grid.
- **Checkpoints**: 5 `kart_checkpoint` (tools/toolstrigger, full lane width, 512 tall), with a cone at each end:
  1 mid south straight (x 0), 2 east lane after the ramp (y 1280), 3 north lane before the jump (x 1536), 4 north
  lane after the landing (x -1280), 5 mid west lane (y 0). Check them with `kart_race_dump`.
- No respawn zone: there is no `kart_respawn_zone` entity yet, and the arena is walled with nothing to fall off.
- **South straight**: about 3300 units from the line to the curve, with traffic cones (physics props) down its
  middle.
- **Banked curve** (south-east corner): a quarter circle round the island's corner. The outer 416 units are banked,
  rising to 128 at the outer wall (~17 degrees); the inner 800 are flat. It is made of 12 segments, with 640-unit
  tapered pieces at each end so the bank grows from and back to flat floor.
- **Gentle ramp** (east lane, driven north): up 96 over 544 units (~10 degrees), a 384-unit plateau, down again.
- **Jump** (north lane, driven west): a kicker rising 112 over 240 (~25 degrees) that ends in a drop, a 592-unit
  gap, then a landing ramp from 64 down to the floor over 768 units, with concrete barriers along both sides.
- Concrete barriers along the outer walls every 512 units (not in the corners or on the banked curve) and at the
  island's other three corners, `lamppost03a_off` lampposts in the arena's corners and
  the middle of each island side, and two `env_cubemap` (start straight, jump). Run `buildcubemaps` in game for
  proper reflections.

```
   N                         north lane, driven west  <--
   +-------------------------------------------------------------+
   | L  b    b    b    b    b    b    b    b    b    b    b     L |
   |     c      ===barriers===                     c            |
   |    4|      [ landing   ]  gap   [K]   <-- jump |3          |
   |     c      ===barriers===          (env_cubemap) c          |
   |b       B                    L                    B         b|
   |          +---------------------------------------+          |
   |b         |                                       |   c-2-c b|
 w |          |                                       |   [up  ] | e
 e |b         |                                       |   [ 10 ] | a
 s |c-5-c   L |            central island             | L [deg ] |bs
 t |b         |              128 high                 |   [ramp] | t
   |          |                                       |          |
 | |b         |                                       |       ^ b|
 v |          |                                       |    ,/  | |
   |b       B +---------------------------------------+  ,/bank| |
   |       c                     c                     ,/curve | |
   | D     #|                    |                   ,/ 17 deg   |
   | D   GG#F >  c   c   c   c   1   c   c  (straight) (banked)  |
   | D   GG#|                    |              __/              |
   | L     c                     c       ______/               L |
   |  b    b    b    b    b    b    b                            |
   +-------------------------------------------------------------+
   F kart_finish (# hazard-stripe line)   G kart_start 2x4 grid   D deathmatch spawns
   1-5 kart_checkpoint (c cone at each end)   c cones   b, B barriers   L lampposts
```
