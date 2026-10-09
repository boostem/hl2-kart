# Modeling guide

Models are built by Blender Python scripts in background mode, exported to SMD with Blender Source Tools (BST),
and compiled with `tools/wine/studiomdl.sh`. See [asset-pipeline.md](asset-pipeline.md) for the rest of the pipeline.

## Setup

```sh
tools/blender/install_bst.sh    # user add-on dir, no sudo, enables the add-on
```

## Scale and style

- 1 Blender unit = 1 Source unit = 1 inch. The player ("citizen") is 72 units tall; the buggy is about 120 long,
  70 wide and 50 tall.
- HL2-era low-poly props: hard-surface shapes, modest polycount (a few hundred to a few thousand triangles), 512px
  textures at most, baked AO optional.
- The object origin is the model origin (BST exports relative to it). Put it on the floor, centred, facing +X.
- studiomdl turns every model 90 degrees about Z unless the QC says `$origin 0 0 0 -90`, so put that in the QC
  of a model that must face +X in game (karts, like the player). Check the result: the compiled `.mdl`'s hull
  (six floats at byte 104) should be longest along X.
- Asset tiers, Source-first: use Valve's own assets where they fit, then freely licensed ones recorded in
  `CREDITS.md`, and only then models built here.

## Required pieces

- A reference mesh and a collision mesh (`$collisionmodel`, convex pieces, simple). studiomdl finds the convex
  pieces by shared vertices, so shade the collision mesh smooth and give it no UVs; otherwise every triangle is its
  own piece and it warns "Model has 2-dimensional geometry". Several pieces need `$concave` in the block.
- `$surfaceprop` (e.g. `metal`, `wood`) and `$cdmaterials "models/<name>/"`; material names in Blender become the
  SMD material names looked up under `$cdmaterials`.
- Naming: lowercase, underscores. Folder `assets_src/<model>/` holds `build_<model>.py`, `<model>.smd`,
  `<model>_phys.smd` and `<model>.qc`. The Blender collection `<model>` is the reference mesh and `<model>_phys` the
  collision mesh.

## Building

```sh
blender -b -P assets_src/<m>/build_<m>.py          # builds the scene, exports SMDs, writes the QC
tools/wine/studiomdl.sh assets_src/<m>/<m>.qc      # compiles into game/mod_hl2mp/models/
```

`tools/blender/export_smd.py` has `export_smd(out_dir)` for build scripts (see
`assets_src/test/bst_cube/build_bst_cube.py` for a minimal example), and also runs on a `.blend`:
`blender -b m.blend -P tools/blender/export_smd.py -- --out assets_src/<m>/`.

## Renders for model PRs

Every model PR attaches four renders: `front`, `side`, `3q` and `scale` (the model beside a 72-unit "citizen" box,
and with `--buggy` the buggy's bounding box).

```sh
blender -b -P tools/blender/render_model.py -- --smd assets_src/<m>/<m>.smd --out /tmp/<m>-renders --buggy
blender -b m.blend -P tools/blender/render_model.py -- --out /tmp/<m>-renders
```

`--texture assets_src/<m>/<m>.png` shows the base texture on the model instead of flat material colours.

Attach the PNGs to the ticket; don't commit them.

## Models

- `assets_src/kart_racer/`: the racer kart (`models/kart/kart_racer.mdl`), the default `kart_model`: a cartoon racer
  go-kart with a rounded tub, wide bumpers, fat tyres, a rear engine with twin pipes and a wing. About 123 x 71 x 43,
  ~5100 triangles, near-white paint so `cl_kart_color` tints it. Same attachments, texture bake and build steps as
  the scrap kart below, with `kart_racer` for `kart_scrap`. It also has steering bones: `steer_fl` and `steer_fr`
  (pivoting up through each front wheel) and `steering_wheel` (along the column), each turning about its local Z.
  The client turns them as the kart steers (`C_HL2MP_Player::BuildTransformations`). The front wheels use Ackermann
  angles, the steering wheel follows at `cl_kart_steer_ratio`, and in a drift the wheels counter-steer. Attachments
  `grip_l` and `grip_r` on the steering wheel rim are where the driver's hands go. Tuning: `cl_kart_steer_angle`,
  `cl_kart_steer_speed`, `cl_kart_steer_drift_counter`. A model without these bones stays rigid.
- `assets_src/kart_scrap/`: the scrap kart (`models/kart/kart_scrap.mdl`), the earlier default, still available as
  `kart_model models/kart/kart_scrap.mdl`. About 112 x 70 x 50, ~3300 triangles, one 512 texture baked from procedural materials and AO in Cycles. Attachments `wheel_fl`,
  `wheel_fr`, `wheel_rl`, `wheel_rr` (tyre contact patches on the floor), `exhaust` (pointing out of the pipe),
  `vehicle_driver_eyes` and `item_hold` (behind the kart). `kart_debug_server 1` draws them in game. Rebuild:

  ```sh
  blender -b -P assets_src/kart_scrap/build_kart_scrap.py
  tools/venv/bin/python tools/img2vtf.py assets_src/kart_scrap/kart_scrap.png models/kart/kart_scrap
  git checkout game/mod_hl2mp/materials/models/kart/kart_scrap.vmt   # if the committed VMT has edits
  tools/wine/studiomdl.sh assets_src/kart_scrap/kart_scrap.qc
  ```
