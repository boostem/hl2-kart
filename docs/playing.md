# Playing HL2 Kart

HL2 Kart is a kart-racing mod for Half-Life 2: Deathmatch. Up to 12 players race laps, collect items and hit each
other with them; bots fill empty slots.

## Requirements

- Steam, signed in.
- **Source SDK Base 2013 Multiplayer** (Steam app 243750). It is free: in the Steam library, switch the filter to
  *Tools* and install it. HL2 Kart uses its HL2 and HL2:DM content, so the package is small.
- A 64-bit Linux or Windows PC. The release zip has the Linux binaries; Windows players need the Windows build of
  `client.dll` and `server.dll` in `bin/x64/` (see `bin/README.txt` in the zip and [`release.md`](release.md)).

## Install

1. Download `hl2kart-<version>.zip` from the releases page.
2. Unzip it into Steam's `sourcemods/` folder so you get `sourcemods/hl2kart/gameinfo.txt`:
   - Linux: `~/.local/share/Steam/steamapps/sourcemods/`
   - Windows: `C:\Program Files (x86)\Steam\steamapps\sourcemods\`
3. Restart Steam. **HL2 Kart** shows up in your library; launch it from there.

## Controls

The default Half-Life 2 keys, all rebindable in the options:

| Key | Does |
| --- | --- |
| `W` / `S` | Accelerate / brake and reverse |
| `A` / `D` | Steer |
| `Space` | Hop; hold it in a turn to drift, release to get a boost |
| Left mouse | Use the item (hold `S` to use it backward) |
| Right mouse | Use the item backward |
| `Tab` | Scoreboard |
| `` ` `` | Console (enable it in Options, Keyboard, Advanced) |

## Playing

Start a game from the main menu (*Create server*), or join one with `connect <host>` in the console. Drive through the
checkpoints in order to finish laps. Drive through item boxes to get an item. After the race the results screen shows the times.

## Console variables

Open the console and type the name alone to see its value, or the name and a value to set it. Server settings
(change them as the host) live in `cfg/server.cfg`.

| Variable | Default | Does |
| --- | --- | --- |
| `kart_laps` | 3 | Laps per race (server) |
| `kart_races_per_map` | 2 | Races before the map changes (server) |
| `kart_bot_quota` | 4 | Bots to keep in the game (server) |
| `kart_bot_difficulty` | 1 | Bot skill: 0 easy, 1 normal, 2 hard (server) |
| `kart_items_enabled` | 1 | Item boxes on or off (server) |
| `kart_enabled` | 1 | `0; kill` respawns you as a normal HL2DM player |
| `kart_model` | | Kart model; takes effect on respawn |
| `kart_hud` | 1 | Show the kart HUD |
| `kart_cam_dist`, `kart_cam_height`, `kart_cam_fov` | | Chase camera |
| `kart_music_volume` | 0.6 | Race music volume, 0 to 1 (0 is off) |
| `kart_debug` | 0 | Overlay with speed, yaw and inputs |

Commands: `kart_bot_add`, `kart_bot_kick`, `kart_results`. Tuning values (speed, acceleration, drift) are in
`cfg/kart_tuning.cfg`; reload with `exec kart_tuning`.

## Problems

- *HL2 Kart is missing from Steam:* the folder must be `sourcemods/hl2kart/` with `gameinfo.txt` directly inside; restart Steam.
- *Fails to start:* check that Source SDK Base 2013 Multiplayer is installed and has been run once.
- Bugs: open an issue with your console output (`console.log` in the mod folder).
