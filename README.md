# HL2 Kart

HL2 Kart is a kart-racing mod built on the Source SDK 2013 multiplayer code (HL2:DM base).

## Build (Linux)

```sh
cd src && ./buildallprojects
```

Every pull request is built by CI (`.github/workflows/build.yml`, hl2mp client and server in the sniper SDK container); to read a failed run, use `gh run view <id> --log-failed`.

## Run

Steam must be running and signed in.

```sh
cd game && ./mod_hl2mp_linux64
```

Playtest launch line:

```sh
cd game && ./mod_hl2mp_linux64 -windowed -w 1600 -h 900 -novid +sv_cheats 1 +map dm_lockdown
```

## Playtesting

Launch with the playtest line above, then in the console:

```
kart_debug 1          // client overlay: kart mode, speed, yaw, grounded, origin, inputs
kart_debug_server 1   // server draws each kart's heading line and hull box
cl_showerror 1        // log prediction errors
cl_showpos 1
net_graph 1
exec kart_tuning      // reload tuning from cfg/kart_tuning.cfg
```

Report findings in a comment on the ticket, with numbers (the overlay rows) and screenshots.
If karts misbehave, `kart_enabled 0; kill` respawns you as a normal HL2DM player.

## Dedicated server

The server runs headless from the Source SDK Base 2013 Dedicated Server (Steam app 244310). The Multiplayer
install has a `srcds_linux64` too, but not the server libraries it loads. Install it once:

```sh
steamcmd +login anonymous +app_update 244310 validate +quit
```

Build the mod, then:

```sh
game/run_server.sh                     # kart_arena, 12 players, port 27015
KART_MAP=kart_arena KART_MAXPLAYERS=8 game/run_server.sh +rcon_password secret
```

`run_server.sh` finds the install in the Steam libraries (or `SRCDS_DIR`), points `-game` at `game/mod_hl2mp`
and passes extra arguments on to `srcds_linux64`. Clients connect with `connect <host>` (`connect localhost` on
the same machine).

Settings are in `game/mod_hl2mp/cfg/`: `server.cfg` (hostname, `kart_laps`, `kart_races_per_map`,
`kart_bot_quota`, `sv_pure`, rates), `mapcycle.txt` and `kart_tuning.cfg`. The welcome text is
`game/mod_hl2mp/motd.txt`. `kart_bot_add` and `kart_bot_kick` work from the server console or over rcon.

## Asset policy

- Use existing Source content first, referenced by path. Never copy Valve files into the repo.
- Agent-built models are made with Blender scripts under `assets_src/`.
- Otherwise use CC0/CC-BY assets, with a line in `CREDITS.md`.
- Ask before using anything NC, ND, SA or with an unclear license.

See `assets_src/README.md`, `CREDITS.md` and `docs/README.md`.

---

# Source SDK 2013

Source code for Source SDK 2013.

Contains the game code for Half-Life 2, HL2: DM and TF2.

**Now including Team Fortress 2! ✨**

## Build instructions

Clone the repository using the following command:

`git clone https://github.com/ValveSoftware/source-sdk-2013`

### Windows

Requirements:
 - Source SDK 2013 Multiplayer installed via Steam
 - Visual Studio 2022 with the following workload and components:
   - Desktop development with C++:
     - MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
     - Windows 11 SDK (10.0.22621.0) or Windows 10 SDK (10.0.19041.1)
 - Python 3.13 or later

Inside the cloned directory, navigate to `src`, run:
```bat
createallprojects.bat
```
This will generate the Visual Studio project `everything.sln` which will be used to build your mod.

Then, on the menu bar, go to `Build > Build Solution`, and wait for everything to build.

You can then select the `Client (Mod Name)` project you wish to run, right click and select `Set as Startup Project` and hit the big green `> Local Windows Debugger` button on the tool bar in order to launch your mod.

The default launch options should be already filled in for the `Release` configuration.

### Linux

Requirements:
 - Source SDK 2013 Multiplayer installed via Steam
 - podman

Inside the cloned directory, navigate to `src`, run:
```bash
./buildallprojects
```

This will build all the projects related to the SDK and your mods automatically against the Steam Runtime.

You can then, in the root of the cloned directory, you can navigate to `game` and run your mod by launching the build launcher for your mod project, eg:
```bash
./mod_tf
```

*Mods that are distributed on Steam MUST be built against the Steam Runtime, which the above steps will automatically do for you.*

## Distributing your Mod

There is guidance on distributing your mod both on and off Steam available at the following link:

https://partner.steamgames.com/doc/sdk/uploading/distributing_source_engine

## Additional Resources

- [Valve Developer Wiki](https://developer.valvesoftware.com/wiki/Source_SDK_2013)

## License

The SDK is licensed to users on a non-commercial basis under the [SOURCE 1 SDK LICENSE](LICENSE), which is contained in the [LICENSE](LICENSE) file in the root of the repository.

For more information, see [Distributing your Mod](#markdown-header-distributing-your-mod).
