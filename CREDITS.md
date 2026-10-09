# Credits

Every non-Valve asset in this repository must have a row in the table below.

| File path | Asset | Author | Source URL | License |
| --- | --- | --- | --- | --- |
| `example/path/asset.ext` (EXAMPLE, not a real asset) | Example asset | Example Author | https://example.com/asset | CC0 |
| `game/mod_hl2mp/models/kart/kart_scrap.*`, `game/mod_hl2mp/materials/models/kart/kart_scrap.*`, `assets_src/kart_scrap/` | Scrap kart model and texture: original, made for this mod by `build_kart_scrap.py` | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/models/kart/kart_racer.*`, `game/mod_hl2mp/materials/models/kart/kart_racer.*`, `assets_src/kart_racer/` | Racer kart model and texture: original, made for this mod by `build_kart_racer.py` | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/models/kart/items/*`, `game/mod_hl2mp/materials/models/kart/items/*`, `assets_src/items/` | Item models and textures (item box, hubcap, nitro can, oil slick, seeker, buffer): original, made for this mod by `build_items.py` and `make_materials.py` | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/models/kart/props/*`, `game/mod_hl2mp/materials/models/kart/props/*`, `assets_src/props/` | Track dressing kit (tyre walls, ramps, finish gantry, start lights, signs, boost pad): original, made for this mod by `build_props.py` and `make_materials.py` | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/models/kart/driver_anims.mdl`, `assets_src/kart_driver/` | Driver animation model: original, made for this mod by `build_kart_driver.py` | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/materials/vgui/kart/items/*` | HUD icons of the items: original, made for this mod | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/maps/kart_*.bsp`, `assets_src/maps/` | Maps `kart_arena` and `kart_bg` (menu background): original, written by `kart_arena.py` and `kart_bg.py` | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/materials/console/*`, `assets_src/textures/` | Main menu background and logo: original, made for this mod (`make_logo.py`) | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/materials/effects/kart_glow.vmt` | Drift and boost glow material: original (uses Valve's glow texture by path, which is not shipped) | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/cfg/kart_tuning.cfg`, `game/mod_hl2mp/cfg/server.cfg`, `game/mod_hl2mp/cfg/listenserver.cfg`, `game/mod_hl2mp/cfg/mapcycle.txt`, `game/mod_hl2mp/motd.txt` | Kart tuning, server settings, map cycle and welcome text | HL2 Kart (the mod) | - | To be decided in M9 |
| `game/mod_hl2mp/hl2kart.fgd`, `game/mod_hl2mp/resource/ui/KartResults.res`, `game/mod_hl2mp/scripts/game_sounds_kart.txt` | Hammer entity definitions, results screen layout, kart soundscript (it references Valve sounds by path) | HL2 Kart (the mod) | - | To be decided in M9 |

## Tools

Installed into the gitignored `tools/venv` by `tools/setup_venv.sh`; not shipped in the mod.

| Tool | Used for | License |
| --- | --- | --- |
| [Pillow](https://python-pillow.org/) | Resizing and DXT compression in `tools/img2vtf.py` | MIT-CMU (HPND) |
| [Blender Source Tools](https://github.com/Artfunkel/BlenderSourceTools) 3.4.2 (Tom Edwards / Artfunkel) | Blender add-on exporting SMD/DMX, installed by `tools/blender/install_bst.sh` | GPL-2.0-or-later |
| [no_vtf](https://git.sr.ht/~b5327157/no_vtf) | Not used: it only decodes VTFs (and fails to start with current click), so `img2vtf.py` writes VTFs itself. Listed for the record. | GPL-3.0-only |
