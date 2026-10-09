# Contributing

The overview is in the [README](../README.md). This is the longer version.

## Build

Requirements: Source SDK Base 2013 Multiplayer from Steam, and podman (the build runs in the Steam Runtime container).

```sh
cd src && ./buildallprojects
```

If a change adds or removes source files in a `.vpc`, delete `src/_vpc_/ninja/sdk_everything_release.ninja` first. Never
commit build output. CI builds every pull request (`.github/workflows/build.yml`). On Windows use `src/createallprojects.bat`
and Visual Studio.

Run the mod with `cd game && ./mod_hl2mp_linux64 -windowed -novid +sv_cheats 1 +map kart_arena`, or a dedicated
server with `game/run_server.sh` (see the README for playtest commands).

## Tools

`tools/setup_venv.sh` makes a local Python environment (`tools/venv`, gitignored). Other tools:

| Path | Does |
| --- | --- |
| `tools/img2vtf.py` | PNG to VTF |
| `tools/blender/` | Blender + Blender Source Tools for model export |
| `tools/wine/` | Wine setup for the Windows-only Source compilers (`studiomdl`, `vbsp`, `vvis`, `vrad`) |
| `tools/snd_convert.sh`, `tools/wav_loop.py` | Sound conversion |
| `tools/test_assets.sh` | Checks the built assets |
| `tools/release/build_release.sh` | Builds the release zip ([`release.md`](release.md)) |
| `tools/release/credits_audit.py` | Checks `CREDITS.md` covers every non-Valve file |

Pipelines: [`asset-pipeline.md`](asset-pipeline.md), [`modeling-guide.md`](modeling-guide.md),
[`mapping-guide.md`](mapping-guide.md), [`dressing-kit.md`](dressing-kit.md).

## Asset policy

- Use existing Source content first, referenced by path. Never copy Valve files into the repo ([`valve-content.md`](valve-content.md) lists paths).
- Agent-built models are made with Blender scripts under `assets_src/`; compiled outputs go under `game/mod_hl2mp/`.
- Otherwise use CC0 or CC-BY assets.
- Ask before using anything NC, ND, SA or with an unclear license.
- **Every non-Valve file under `game/mod_hl2mp/` needs a row in `CREDITS.md`** (path or glob, asset, author, source, license).
  Check with:

  ```sh
  tools/release/credits_audit.py
  ```

  It fails for a file with no row and for a row that matches no file. Files that come from Valve's SDK are listed in the
  script and need no row; add a path there only for a genuinely Valve-derived file.

## Pull requests

Branch from `master`, keep one concern per pull request, and say how you verified it. Contributions to the SDK code are
under Valve's terms (see `CONTRIBUTING`).
