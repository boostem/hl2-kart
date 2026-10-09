# assets_src

Sources for assets before compilation.

- `assets_src/test/test_cube/`: hand-written test model for the wine toolchain (see `docs/asset-pipeline.md`).
- `assets_src/kart_scrap/`: the scrap kart, the default kart model (see `docs/modeling-guide.md`). `kart_scrap.png` is
  its baked base texture.
- `assets_src/<model_name>/build_<model_name>.py`: Blender script that builds the model; exported SMD/DMX and the QC file sit next to it.
- `assets_src/maps/*.vmf`: map sources. `kart_arena.vmf` is written by `kart_arena.py` (see `docs/asset-pipeline.md`).
- `assets_src/textures/`: texture originals.
- `assets_src/sounds/`: sound originals before conversion.

Compiled outputs go under `game/mod_hl2mp/{models,materials,sound,maps}`.
