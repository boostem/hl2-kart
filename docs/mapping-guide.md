# Mapping guide

How to build a kart track in Hammer (Windows).

## 1. Hammer setup

1. Install the Source SDK Base 2013 Multiplayer tools on Windows (Steam, the Windows depot).
2. Copy this repository's `game/mod_hl2mp` folder to Windows (or reach it through a mapped drive). This is the mod's game directory.
3. Copy `game/mod_hl2mp/hl2kart.fgd` into the SDK's `bin` folder next to `hl2mp.fgd` and `halflife2.fgd`. `hl2kart.fgd` does `@include "hl2mp.fgd"`, which only resolves when they are in the same folder.
4. In Hammer open **Tools > Options > Game Configurations**, add a configuration and set:
   - Game Data files: the `hl2kart.fgd` path (remove the stock FGDs, it includes them).
   - Game Executable Directory: the Source SDK Base 2013 Multiplayer folder.
   - Game Directory: the copied `game/mod_hl2mp` folder.
   - Compile tools (`vbsp.exe`, `vvis.exe`, `vrad.exe`): the `bin` folder of the Windows depot.
5. Copy the compiled `.bsp` into `game/mod_hl2mp/maps/`.

## 2. Kart entities

| Entity | Type | Purpose |
| --- | --- | --- |
| `kart_race_manager` | point | One per map. Keyvalues `laps`, `track_name`; outputs `OnRaceStart` (at GO), `OnRaceFinish` (first kart home). |
| `kart_start` | point | A grid slot (`grid`, lowest first). Karts spawn on the lowest free slot; at each race start they line up by the last race's finish order. |
| `kart_checkpoint` | brush trigger | Passed in `index` order (1, 2, 3...). |
| `kart_finish` | brush trigger | The start/finish line (checkpoint 0). |
| `kart_path_node` | point | A point on the bots' racing line. Keyvalues `next`, `width`, `speed_scale`, `drift`. See [Racing line for bots](#4-racing-line-for-bots). |
| `kart_item_box` | point | A floating item box. Keyvalues `respawn_time` (3 s), `scale` (0.6). Karts without an item take it; it comes back after `respawn_time`. |

## 3. Track rules

- A lap takes 60-90 seconds.
- Exactly one `kart_race_manager` and one `kart_finish`.
- 8 `kart_start` slots in a 2x4 grid, facing the track direction, with `grid` 0-7: 96 units apart across the track
  and 128 apart along it (the kart model is 112 x 70; its collision hull is 64 x 64 x 48).
- The grid sits just behind the `kart_finish`, clear of its trigger: lap 1 starts when a kart crosses the line after GO.
- Checkpoints about every 1500 units, plus one before and one after every shortcut, so a shortcut cannot skip a checkpoint.
- Checkpoint and finish triggers span the whole track width, wall to wall, and are tall, so karts cannot jump over them.
- Item boxes in rows of 3-5 across the track width, centered about 24 units above the road, a few per lap.
- Put a kill plane (`trigger_hurt`, or the M2 respawn trigger) below the track.

## 4. Racing line for bots

Bots drive along a racing line made of `kart_path_node` point entities, chained like `path_track`:

- Give every node a name and set its `next` to the node that follows it in driving direction. The last node's `next` is the first node, so the chain is one closed loop.
- Place the nodes where a good driver goes: through the apex of each corner, using the whole road. Space them about 300-500 units apart on straights and closer (100-200) through corners. Put them at kart height (about 16 units above the road).
- `width` (default 128) is how far a bot may stray from the line on either side, to overtake or avoid other karts. Keep the line plus its width inside the walls.
- `speed_scale` (0.1 to 1, default 1) is the fraction of top speed bots aim for at the node. Use about 0.5 for hairpins and 0.7-0.8 for tight corners.
- `drift` hints that bots should drift through the corner that starts at the node.

On map load the race manager builds a smooth closed spline (Catmull-Rom) through the nodes. The line starts at the node nearest the `kart_finish`. The game warns in the console when the chain breaks (a node without `next`, a `next` that doesn't exist or isn't a `kart_path_node`), when nodes lead into the loop or aren't on it, and when the loop has fewer than 3 nodes.

To check it in game: `sv_cheats 1`, then `kart_debug_server 2` draws the line in green, its width either side and each node's settings. `kart_race_dump` lists the nodes in line order with their distance along the lap.

## 5. The race flow

On a map with a `kart_race_manager`, a `kart_finish` and at least one checkpoint, the game runs races instead of deathmatch (no frag limit, no teams); a map without them is free drive.

| State | What happens | Ends |
| --- | --- | --- |
| Waiting | Karts drive freely, laps don't count. | `kart_waiting_time` (3 s) after `kart_min_players` (1) karts are in, or as soon as every kart types `mp_ready_signal` (`ready`) in chat. |
| Countdown | Map entities reset, everyone respawned on the grid and frozen; `kart_countdown` events 3, 2, 1. | After `kart_countdown_time` (3 s): `kart_race_start`, `OnRaceStart`. |
| Racing | Laps count. Karts joining now drive but have no position until the next race. | The first kart finishes. |
| Finishing | The others keep racing. | Everyone finished, or `kart_finish_timeout` (30 s): the rest are placed as they run, as DNF. |
| Results | Karts frozen, finish order in chat. | After `kart_results_time` (10 s): the next map if `mp_timelimit` has run out, else Waiting. |

Console: `kart_race_restart` starts over from Waiting, `kart_race_reset` puts every kart back to lap 0 where it stands, `kart_race_dump` prints the state.

## 6. Materials and props

Use the Source materials and models listed in [`valve-content.md`](valve-content.md). Reference them by path; never copy Valve files into the repo.

## 7. Original design

Tracks must be original designs. Do not recreate tracks, layouts or art from other racing games.

## 8. Keeping the FGD in sync

Every ticket that adds or changes a kart entity (keyvalue, input or output) must update `game/mod_hl2mp/hl2kart.fgd` and this guide in the same pull request.
