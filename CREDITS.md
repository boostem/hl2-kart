# Credits

Every non-Valve asset in this repository must have a row in the table below.

| File path | Asset | Author | Source URL | License |
| --- | --- | --- | --- | --- |
| `example/path/asset.ext` (EXAMPLE, not a real asset) | Example asset | Example Author | https://example.com/asset | CC0 |
| `game/mod_hl2mp/models/kart/kart_scrap.*`, `game/mod_hl2mp/materials/models/kart/kart_scrap.*`, `assets_src/kart_scrap/` | Scrap kart model and texture: original, made for this mod by `build_kart_scrap.py` | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/models/kart/kart_racer.*`, `game/mod_hl2mp/materials/models/kart/kart_racer.*`, `assets_src/kart_racer/` | Racer kart model and texture: original, made for this mod by `build_kart_racer.py` | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/models/kart/items/*`, `game/mod_hl2mp/materials/models/kart/items/*`, `assets_src/items/` | Item models and textures (item box, hubcap, nitro can, oil slick, seeker, buffer): original, made for this mod by `build_items.py` and `make_materials.py` | HL2 Kart (the mod) | - | To be decided in M9 |

## Tools

Installed into the gitignored `tools/venv` by `tools/setup_venv.sh`; not shipped in the mod.

| Tool | Used for | License |
| --- | --- | --- |
| [Pillow](https://python-pillow.org/) | Resizing and DXT compression in `tools/img2vtf.py` | MIT-CMU (HPND) |
| [Blender Source Tools](https://github.com/Artfunkel/BlenderSourceTools) 3.4.2 (Tom Edwards / Artfunkel) | Blender add-on exporting SMD/DMX, installed by `tools/blender/install_bst.sh` | GPL-2.0-or-later |
| [no_vtf](https://git.sr.ht/~b5327157/no_vtf) | Not used: it only decodes VTFs (and fails to start with current click), so `img2vtf.py` writes VTFs itself. Listed for the record. | GPL-3.0-only |
