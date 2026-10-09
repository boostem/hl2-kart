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
- Asset tiers, Source-first: use Valve's own assets where they fit, then freely licensed ones recorded in
  `CREDITS.md`, and only then models built here.

## Required pieces

- A reference mesh and a collision mesh (`$collisionmodel`, convex pieces, simple).
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

Attach the PNGs to the ticket; don't commit them.
