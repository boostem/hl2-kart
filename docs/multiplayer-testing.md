# Multiplayer testing

Karts and items behave differently with real latency than on a listen server with no lag. Test these setups, from
the cheapest to the closest to a real game:

1. One client on a listen server, with fake lag.
2. Two clients on the same machine.
3. A dedicated server, with one client or more.

Build the mod first (`cd src && ./buildallprojects`). Steam must be running and signed in for every client.

## What to watch

In the client console:

```
kart_debug 1          // client overlay: speed, yaw, grounded, inputs
cl_showerror 1        // logs prediction errors: there should be none while driving straight
net_graph 1           // latency, loss, choke
```

On the server (the listen server's console, or the dedicated server's console or `rcon`):

```
kart_debug_server 1        // logs "[kart] hubcap N caught up X ms" for each lag compensated throw, and draws hubcap paths
sv_showlagcompensation 1   // needs sv_cheats 1: draws the rewound karts' boxes when an item is thrown
```

Server debug overlays (`kart_debug_server`, `kart_bot_debug`, `sv_showlagcompensation`) only draw for the host of a
listen server: on a dedicated server you get the console logging but no drawings. `kart_item_dump` and
`kart_race_dump` print to the server console, so run them from the host, the dedicated server's console or `rcon`.

Things to check with lag on:

- **Driving**: no `cl_showerror` errors, and no rubber-banding on straights, drifts, boost pads or ramps. A few
  errors when you crash into another kart are expected.
- **Hubcaps**: a hubcap thrown at a kart that is in front of you on your screen hits it, even with
  `net_fakelag 100`. The hit is decided by the server, so with lag the spin-out shows up a round trip after the
  hubcap reaches the kart on your screen.
- **Hit reactions**: spin-outs and stuns (hubcaps, oil) play once, without the kart snapping back.
- **Items and HUD**: the roulette, item counts, positions, laps, results and music are the same on both clients.

The lag compensation settings are server convars:

| Convar               | Default | What it does                                                                        |
| -------------------- | ------- | ----------------------------------------------------------------------------------- |
| `kart_proj_lag_max`  | `0.25`  | Most latency (seconds) projectiles make up for, for both the catch-up and the slop. |
| `kart_proj_lag_slop` | `200`   | Extra hit reach against a kart, in units per second of that kart's latency.        |

A new hubcap first catches up by its thrower's latency against the other karts rewound to where the thrower
saw them, so it hits what the thrower aimed at. After that it moves in present time, and its reach against each
kart is widened by `kart_proj_lag_slop` × that kart's latency (20 units at 100 ms).

## 1. Fake lag on a listen server

```sh
cd game && ./mod_hl2mp_linux64 -windowed -w 1600 -h 900 -novid +sv_cheats 1 +map kart_arena
```

Then in the console:

```
net_fakelag 80        // delays incoming packets by 80 ms
net_fakeloss 2        // drops 2% of incoming packets
net_fakejitter 10     // optional: +-10 ms of jitter
kart_bot_add          // something to throw at
```

`net_fakelag` and `net_fakeloss` are cheats (`sv_cheats 1`) and act on everything that process receives,
loopback included. On a listen server that delays the host's commands to its own server as well as the snapshots
back, which is a quick check of prediction, but a client connected to another process (setups 2 and 3) behaves
much more like a real slow connection. `net_fakelag 0; net_fakeloss 0` turns them off.

Bots have no latency and aren't lag compensated, but they are fine targets for a lagging thrower.

## 2. Two clients on the same machine

Source SDK 2013 refuses to start a second copy of the game unless every copy is started with `-multirun`. Each
copy needs its own client port; the second one also gets a different window position so you can see both.

Host a listen server in the first one:

```sh
cd game && ./mod_hl2mp_linux64 -multirun -windowed -w 960 -h 540 -novid +sv_lan 1 +sv_cheats 1 +maxplayers 4 +map kart_arena
```

Join it from the second one:

```sh
cd game && ./mod_hl2mp_linux64 -multirun -windowed -w 960 -h 540 -novid +clientport 27006 +connect 127.0.0.1:27015
```

Then in the **second** client's console: `net_fakelag 100; net_fakeloss 2`, and throw hubcaps at the host's kart
(`kart_give_item hubcap 3` gives you three). The second client is the one with the lag, so it is the one whose
throws and driving are being tested.

Both copies run on the same Steam account. A server only lets the same account in twice when it is a LAN server,
which is why the host starts with `+sv_lan 1` (it has to be set before the map loads). Use `-port 27016` on the host
(and connect to that port) if 27015 is already taken, for example by a dedicated server.

## 3. Dedicated server

The dedicated server comes from the Source SDK Base 2013 Dedicated Server (Steam app 244310), not from the
Multiplayer install (its `srcds_linux64` lacks the server libraries). The download needs no Steam account:

```sh
steamcmd +login anonymous +app_update 244310 validate +quit
```

Then start it with the repository script, which finds the install and points `-game` at `game/mod_hl2mp`:

```sh
game/run_server.sh +sv_cheats 1                       # kart_arena, port 27015
KART_PORT=27016 game/run_server.sh +sv_lan 1 +sv_cheats 1
```

That is the same as running it by hand from the install directory:

```sh
./srcds_linux64 -game /path/to/repo/game/mod_hl2mp -console -port 27015 +maxplayers 12 +map kart_arena
```

Join with `connect 127.0.0.1:27015` from a client on the same machine, or `connect <host>:<port>` from another one.

Steam login:

- The server itself needs no Steam login: it logs on to Steam anonymously as a game server. It needs no game
  server token either.
- Every client needs the Steam client running and signed in to an account that owns (or has installed) Source SDK
  Base 2013 Multiplayer. The server checks each client's Steam ticket unless it runs with `-insecure`.
- Two clients on one Steam account need a LAN server (`+sv_lan 1`), as in setup 2.
- To join over the internet, the server's port (UDP 27015 by default) has to be reachable, and `sv_lan` must be 0.

`net_fakelag` is a cheat, so the server needs `sv_cheats 1` for clients to use it. Keep cheats off on a public
server.

## Over the internet

The best test is a friend on a real connection. With `net_graph 1` up, each of you notes your latency, then:

- Throw hubcaps at each other's kart, forward and backward, at a standstill and at full speed, and across each
  other's path. Hits should land where the thrower aimed.
- Say when the other kart spins out on your screen without a hubcap touching it, or a hubcap passes through it.
  A little of the first is the slop at work. A lot of either means `kart_proj_lag_slop` or `kart_proj_lag_max`
  needs tuning: report your latencies with it.
