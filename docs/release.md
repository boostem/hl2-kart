# Releases

HL2 Kart ships as a sourcemod: a folder players drop into Steam's `sourcemods/` that runs on **Source SDK Base 2013
Multiplayer** (Steam app 243750, free in the Steam library under Tools). The package holds only HL2 Kart's own files;
the HL2 and HL2MP content it uses comes from the player's SDK install.

## Building

```sh
cd src && ./buildallprojects && cd ..
tools/release/build_release.sh
```

The script needs the SDK installed (it uses the SDK's `bin/linux64/vpk` tool and lists the SDK's VPKs), `strip` and
`python3`. It finds the SDK in the Steam libraries; set `SDK_DIR` to point elsewhere. Output, all under `release/`
(gitignored):

- `release/build/hl2kart/`: the staged mod folder.
- `release/hl2kart-<version>.zip`: that folder zipped. This is what you upload.

The version comes from `VERSION` at the repository root. Bump it before a release; the script writes it into
`steam.inf` and `version.txt` (with the git commit).

## What goes in

| Path | Contents |
| --- | --- |
| `gameinfo.txt` | The dev one, with paths made relative to the mod folder and `hl2kart_pak.vpk` mounted |
| `steam.inf`, `version.txt` | Version string |
| `hl2kart_pak.vpk` | `materials/`, `models/`, `sound/`, `particles/`, `shaders/` |
| `maps/` | `kart_*.bsp` |
| `cfg/`, `resource/`, `scripts/`, `motd.txt` | Loose, as in `game/mod_hl2mp/` |
| `bin/linux64/` | `client.so`, `server.so`, shader library (debug info stripped) |

Only files tracked in git go in, so local `config.cfg`, logs, screenshots and caches stay out. `resource/mod_hl2mp_*.txt`
is renamed to `hl2kart_*.txt`, since the localisation file is looked up by the mod folder's name.

Left out on purpose: Valve's `dm_lockdown.bsp` and the SDK's example shader (`example_model_*`) files, which are Valve
paths, and `hl2kart.fgd`, which is for mappers.

**No Valve files.** After packing, the script lists `hl2kart_pak.vpk` and fails if any path in it is also in one of the
SDK's VPKs (`hl2/`, `hl2mp/`, `hl2_complete/`, `platform/`, ...). To check a zip by hand:

```sh
SDK="$HOME/.local/share/Steam/steamapps/common/Source SDK Base 2013 Multiplayer"
LD_LIBRARY_PATH="$SDK/bin/linux64" "$SDK/bin/linux64/vpk" l release/build/hl2kart/hl2kart_pak.vpk
```

**Windows.** The package has Linux binaries only. Windows players need `client.dll` and `server.dll` in `bin/x64/`, built
on Windows (`src/createallprojects.bat`, then Visual Studio); `bin/README.txt` in the package says so.

## Uploading

Upload `release/hl2kart-<version>.zip` (for example as a GitHub release asset, tagged `v<version>`). Don't upload
`release/build/`.

## Installing (players)

1. Install **Source SDK Base 2013 Multiplayer** from the Steam library (Tools).
2. Unzip the release into Steam's `sourcemods/` folder so it holds `sourcemods/hl2kart/gameinfo.txt`:
   - Linux: `~/.local/share/Steam/steamapps/sourcemods/`
   - Windows: `C:\Program Files (x86)\Steam\steamapps\sourcemods\`
3. Restart Steam. **HL2 Kart** appears in the library; launch it from there.
