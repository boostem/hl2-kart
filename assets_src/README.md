# assets_src

Sources for assets before compilation.

- `assets_src/test/test_cube/`: hand-written test model for the wine toolchain (see `docs/asset-pipeline.md`).
- `assets_src/kart_scrap/`: the scrap kart, the default kart model (see `docs/modeling-guide.md`). `kart_scrap.png` is
  its baked base texture.
- `assets_src/props/`: the track dressing kit, all props built by `build_props.py` (see `docs/dressing-kit.md`).
- `assets_src/<model_name>/build_<model_name>.py`: Blender script that builds the model; exported SMD/DMX and the QC file sit next to it.
- `assets_src/maps/*.vmf`: map sources. `kart_arena.vmf` and `kart_bg.vmf` (the main menu background) are written by
  `kart_arena.py` and `kart_bg.py` (see `docs/asset-pipeline.md`).
- `assets_src/textures/`: texture originals, e.g. `logo.png` (main menu logo, drawn by `make_logo.py`) and `kart_bg.png`
  (the menu background still).
- `assets_src/sounds/`: sound originals before conversion.

Compiled outputs go under `game/mod_hl2mp/{models,materials,sound,maps}`.
