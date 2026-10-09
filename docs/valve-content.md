# Valve content inventory

Paths into what Source SDK Base 2013 Multiplayer ships, so tickets can reference assets by path instead of guessing.
**This doc only lists paths. Never copy Valve files into the repo.**

All paths are as the engine sees them (relative to the mounted search paths). Where a file lives:

| Tag | Location under `$HOME/.local/share/Steam/steamapps/common/Source SDK Base 2013 Multiplayer` |
| --- | --- |
| `misc` | `hl2/hl2_misc_dir.vpk` (models, `.vmt` materials, scripts, resource) |
| `tex` | `hl2/hl2_textures_dir.vpk` (`.vtf` textures) |
| `snd` | `hl2/hl2_sound_misc_dir.vpk` (sounds, music) |
| `mp` | `hl2mp/hl2mp_pak_dir.vpk` |
| `loose` | plain files in `hl2/` or `hl2mp/` |

Regenerate listings with:

```sh
cd "$HOME/.local/share/Steam/steamapps/common/Source SDK Base 2013 Multiplayer"
LD_LIBRARY_PATH=bin/linux64 bin/linux64/vpk l hl2/hl2_misc_dir.vpk > /tmp/misc.txt   # same for the others
# extract (the target directories must exist first): vpk x <vpk> sound/music/hl2_song0.mp3
```

## 1. Vehicles (`misc`)

Drivable-vehicle models and their vehicle scripts:

| Vehicle | Model | Script (`loose`) | Materials |
| --- | --- | --- | --- |
| Buggy / jeep | `models/buggy.mdl` | `hl2/scripts/vehicles/jeep_test.txt` | `materials/models/buggy/` |
| Airboat | `models/airboat.mdl`, `models/airboatgun.mdl` | `hl2/scripts/vehicles/airboat.txt` | `materials/models/airboat/` |
| Combine APC | `models/combine_apc.mdl`, `models/combine_apc_dynamic.mdl`, `models/combine_apc_wheelcollision.mdl`, gibs `models/combine_apc_destroyed_gib01..06.mdl` | `hl2/scripts/vehicles/apc.txt`, `apc_npc.txt` | `materials/models/combine_apc/` |
| Prisoner pod | `models/vehicles/prisoner_pod.mdl`, `prisoner_pod_inner.mdl`, `inner_pod_arm.mdl`, `inner_pod_rotator.mdl` | `hl2/scripts/vehicles/prisoner_pod.txt` | |

Other loose vehicle scripts in `hl2/scripts/vehicles/`: `cannon.txt`, `crane.txt`, `digger.txt`, `fastdigger.txt`, `driveway.txt`,
`jetski.txt`, `reference_vehicle.txt`.

### `models/props_vehicles/` (materials in `materials/models/props_vehicles/`, 32 materials)

Cars: `car001a_hatchback`, `car001b_hatchback`, `car002a`, `car002b`, `car003a`, `car003b`, `car004a`, `car004b`, `car005a`,
`car005b`. Trucks and trailers: `truck001a`, `truck002a_cab`, `truck003a`, `van001a`, `wagon001a`, `trailer001a`,
`trailer002a`, `tanker001a`, `generatortrailer01`, `apc001`.
Wheels/tires: `apc_tire001`, `tire001a_tractor`, `tire001b_truck`, `tire001c_car`, `carparts_tire01a`, `carparts_wheel01a`.
Parts: `carparts_axel01a`, `carparts_door01a`, `carparts_muffler01a`.
(All `models/props_vehicles/<name>.mdl`.)

### Wheel models elsewhere

`models/props_wasteland/wheel01.mdl`, `wheel01a`, `wheel02a`, `wheel02b`, `wheel03a`, `wheel03b`.
No `props_c17/carparts_*` exists; the `carparts_*` models are in `props_vehicles/` above.

## 2. Track-building props (`misc`, all `.mdl`)

There are **no ramp, flag, banner or jersey-barrier models** in the SDK content. Use `props_c17/concrete_barrier001a` and
`props_wasteland/barricade*` for barriers, and build ramps from world brushes/displacements.

**Barriers and walls**
`models/props_c17/concrete_barrier001a.mdl`, `models/props_wasteland/barricade001a.mdl`, `barricade002a.mdl`,
`models/props_debris/concrete_debris256barricade001a.mdl`, `models/props_debris/concrete_wall01a.mdl`, `concrete_wall02a.mdl`,
`concrete_section128wall001a.mdl`, `concrete_section128wall002a.mdl`, `models/props_junk/cinderblock01a.mdl`,
`models/props_debris/concrete_cynderblock001.mdl`, `models/props_pipes/concrete_pipe001a.mdl` (also `b`, `c`, `d`).

**Cones, drums, crates, pallets, dumpsters**
`models/props_junk/trafficcone001a.mdl`, `models/props_c17/oildrum001.mdl`, `models/props_c17/oildrum001_explosive.mdl`,
`models/props_borealis/bluebarrel001.mdl`, `bluebarrel002.mdl`, `models/props_junk/wood_pallet001a.mdl`,
`models/props_junk/wood_crate001a.mdl`, `wood_crate002a.mdl`, `models/props_junk/plasticcrate01a.mdl`,
`models/props_junk/trashdumpster01a.mdl`, `trashdumpster02.mdl`, `models/props_lab/scrapyarddumpster.mdl`.

**Tires (as obstacles)**
`models/props_vehicles/tire001a_tractor.mdl`, `tire001b_truck.mdl`, `tire001c_car.mdl`, `apc_tire001.mdl`.

**Fences and railings**
`models/props_c17/fence01a.mdl`, `fence01b`, `fence02a`, `fence02b`, `fence03a`, `fence04a`;
`models/props_wasteland/exterior_fence001a.mdl` (`b`, `002a`..`002e`, `003a`, `003b`), `wood_fence01a.mdl`, `wood_fence01b`,
`wood_fence01c`, `wood_fence02a`; `models/props_combine/railing_128.mdl`, `railing_256`, `railing_512`, `railing_corner_inside`,
`railing_corner_outside`; `models/props_canal/canal_bridge_railing01.mdl`, `canal_bridge_railing02`;
`models/props_wasteland/bridge_railing.mdl`.

**Lampposts, poles, signs**
`models/props_c17/lamppost03a_off.mdl`, `lamppost03a_on.mdl`, `lamp_standard_off01.mdl`, `models/props_c17/utilitypole01a.mdl`
(`01b`, `01d`, `02b`, `03a`), `models/props_c17/signpole001.mdl`, `models/props_c17/streetsign001c.mdl` (`002b`, `003b`, `004e`,
`004f`, `005b`, `005c`, `005d`), `models/props_trainstation/tracksign01.mdl` (`02`, `03`, `07`..`10`),
`models/props_junk/ravenholmsign.mdl`, `models/props_wasteland/light_spotlight01_lamp.mdl`.

**Bridges and structures**
`models/props_canal/canal_bridge01.mdl` (`01b`, `02`, `03a`..`03c`, `04`), `models/props_wasteland/bridge_middle.mdl`,
`bridge_side01.mdl`, `bridge_side02`, `bridge_side03`, `models/props_wasteland/medbridge_arch01.mdl`, `medbridge_base01`,
`models/props_docks/dockpole01a.mdl`, `models/props_wasteland/dockplank01a.mdl`, `dockplank01b`, `models/cranes/crane_frame.mdl`.

**Foliage**
`models/props_foliage/oak_tree01.mdl`, `tree_deciduous_01a.mdl` (`02a`, `03a`, `03b`), `tree_poplar_01.mdl`, `tree_cliff_01a.mdl`,
`tree_cliff_02a.mdl`, `shrub_01a.mdl`, `models/perftest/grass_tuft_001.mdl` (`001a`, `001b`, `003a`, `004a`..`004d`).

**Rocks**
`models/props_wasteland/rockgranite01a.mdl` (also `01b`..`04c`), `models/props_wasteland/rockcliff01b.mdl` (`01c`, `01e`..`01k`, `05a`,
`05b`, `05e`, `05f`, `06d`, `06i`, `07b`), `rockcliff_cluster01b.mdl` (`02a`..`03c`), `models/props_junk/rock001a.mdl`,
`models/props_lab/bigrock.mdl`, `models/props_canal/rock_riverbed01a.mdl` (`01b`..`02c`), `models/perftest/rocksground01a.mdl`
(`01b`..`01e`, `02a`..`02c`).

## 3. Surfaces and materials

Materials are `.vmt` in `misc`, textures `.vtf` in `tex`; shown here without extension, under `materials/`.
**There is no road or asphalt material.** Use concrete floors (grey), `props/hazardstrip001a` for markings, or an overlay.

- **Concrete floors**: `concrete/concretefloor001a`, `002a`, `002b`, `003c`, `005a`..`016a`, `018a`..`020b`, `022a`..`028d`, `030a`..`034a`,
  `036a`..`039b` (some with `_c17` variants). Full list: `grep '^materials/concrete/concretefloor.*vmt'`.
- **Dirt**: `nature/dirtfloor001a`, `003a`, `003b`, `004a`, `005b`, `005c`, `006a`, `008a`, `009c`, `011a`, `012a`, `013a`, `nature/dirtwall001a`.
- **Grass**: `nature/grassfloor002a`, `nature/grassfloor003a`.
- **Gravel**: `nature/gravelfloor001a`, `002a`, `002b`, `003a`, `004a`, `004a_c17`, `005a`.
- **Sand**: `nature/sandfloor005a`, `nature/sandfloor010a`.
- **Mud**: `nature/mudfloor001a`, `002a`, `004a`, `004b`, `004bs`, `005a`. **Rock**: `nature/rockfloor002a`, `003a`, `006a`.
  **Snow**: `nature/snowfloor001a`..`003a`.
- **Low-friction variants (only these two exist)**: `nature/gravelfloor002a_lowfriction`, `nature/blenddirtgrass008b_lowfriction`.
  No `_lowfriction` for dirt, sand or grass.
- **Track overlays** (`overlays/`): `dirttrack01a`, `dirttrack01b`, `sandtrack02a`, `sandtrack02b`, `gravelpath01a`, `gravelpath01b`;
  also `puddle001a`, `darkedge`, `gasoline01`, `rockslide01a`, `tideline01a`..`01c`, `shorewave001a`, `shorewave002a`.
- **Decals** (`decals/`): `decal_skidmark01`, `decal_skidmark02`, `decaltiremark001a`.
- **Hazard / caution**: `props/hazardstrip001a`, `props/signcaution002a`, `props/signcaution002b`, `props/signwarning001b`, `props/signwarning001c`,
  `decals/decalsigncaution001b`, `decals/decalsigncaution001c`.
- **Boost pad markings**: `dev/dev_hazzardstripe01a`, `effects/com_shield002a`, `effects/com_shield003a`, `effects/com_shield004a`.
- **Skyboxes** (`skybox/<name>` + `up|dn|lf|rt|ft|bk` faces): `sky_day01_01`, `sky_day01_04`..`sky_day01_09`, `sky_day02_01`..`02_07`, `02_09`,
  `02_10`, `sky_day03_01`..`03_06`, `sky_day03_06b`, `sky_borealis01`, `sky_wasteland02`, `sky_fake_white`. Most have an `_hdr` variant.

## 4. Sounds

All in `snd` (and `mp` for a few). Sounds are used via soundscripts (loose `hl2/scripts/game_sounds_vehicles.txt`) where possible.

### Jeep (`sound/vehicles/v8/`)
`first.wav`, `second.wav`, `third.wav`, `fourth_cruise_loop2.wav`, `v8_idle_loop1.wav`, `v8_firstgear_rev_loop1.wav`, `v8_rev_short_loop1.wav`,
`v8_start_loop1.wav`, `v8_stop1.wav`, `v8_throttle_off_fast_loop1.wav`, `v8_throttle_off_slow_loop2.wav`, `v8_turbo_on_loop1.wav`,
`skid_lowfriction.wav`, `skid_normalfriction.wav`, `skid_highfriction.wav`, `vehicle_impact_medium1..4.wav`, `vehicle_impact_heavy1..4.wav`,
`vehicle_rollover1.wav`, `vehicle_rollover2.wav`.

### Airboat (`sound/vehicles/airboat/`)
`fan_motor_start1`, `fan_motor_idle_loop1`, `fan_motor_fullthrottle_loop1`, `fan_motor_shut_off1`, `fan_blade_idle_loop1`,
`fan_blade_fullthrottle_loop1`, `pontoon_fast_water_loop1`, `pontoon_fast_water_loop2`, `pontoon_stopped_water_loop1`,
`pontoon_impact_hard1/2`, `pontoon_scrape_rough1..3`, `pontoon_scrape_smooth1..3`, `pontoon_splash1/2` (all `.wav`).

### APC (`sound/vehicles/apc/`)
`apc_idle1.wav`, `apc_start_loop3.wav`, `apc_firstgear_loop1.wav`, `apc_cruise_loop3.wav`, `apc_slowdown_fast_loop5.wav`, `apc_shutdown.wav`.

### Soundscript entries (`hl2/scripts/game_sounds_vehicles.txt`, `loose`)
- `ATV_*` (jeep): `ATV_engine_idle`, `ATV_engine_start`, `ATV_engine_stop`, `ATV_engine_null`, `ATV_rev`, `ATV_reverse`, `ATV_firstgear`..`ATV_fourthgear`,
  `ATV_firstgear_noshift`..`ATV_fourthgear_noshift`, `ATV_downshift_to_2nd`, `ATV_downshift_to_1st`, `ATV_throttleoff_slowspeed`,
  `ATV_throttleoff_fastspeed`, `ATV_skid_lowfriction`, `ATV_skid_normalfriction`, `ATV_skid_highfriction`, `ATV_impact_heavy`,
  `ATV_impact_medium`, `ATV_rollover`, `ATV_turbo_on`, `ATV_turbo_off`, `ATV_start_in_water`, `ATV_stall_in_water`.
- `PropJeep.FireCannon`, `PropJeep.FireChargedCannon`, `PropJeep.AmmoOpen`, `PropJeep.AmmoClose`, `Jeep.GaussCharge`.
- `Airboat_engine_start`, `_stop`, `_idle`, `_fullthrottle`, `Airboat_fan_idle`, `Airboat_fan_fullthrottle`, `Airboat_skid_rough`, `Airboat_skid_smooth`,
  `Airboat_impact_hard`, `Airboat_impact_soft`, `Airboat_impact_splash`, `Airboat_water_stopped`, `Airboat_water_fast`, `Airboat_headlight_on`, `Airboat_headlight_off`.
- `apc_engine_idle`, `apc_engine_start`, `apc_engine_stop`, `apc_firstgear`, `apc_firstgear_resume`, `apc_throttleoff_slowspeed`, `apc_throttleoff_fastspeed`,
  `PropAPC.FireCannon`, `PropAPC.FireRocket`.

### Machines (`sound/ambient/machines/`)
`thumper_hit.wav` (used by `Kart.BoostPad`), `thumper_top.wav`, `thumper_dust.wav`, `thumper_amb.wav`, `thumper_startup1.wav`, `thumper_shutdown1.wav`.
Also `sound/ambient/energy/zap1..3.wav`, `zap5..9.wav`.

### Physics impact sets (`sound/physics/<material>/`)
Counts of files: `body` 18, `cardboard` 27, `concrete` 31, `flesh` 21, `glass` 37, `metal` 118, `nearmiss` 4, `plaster` 26, `plastic` 35, `rubber` 9,
`surfaces` 11, `wood` 71. Useful for barrier/cone hits: `physics/concrete/concrete_impact_hard1..3.wav`, `physics/concrete/concrete_block_impact_hard1..3.wav`,
`physics/concrete/boulder_impact_hard1..4.wav`, `physics/concrete/concrete_scrape_smooth_loop1.wav`, `physics/concrete/concrete_block_scrape_rough_loop1.wav`.

### UI, buttons, common
- `sound/ui/buttonclick.wav`, `buttonclickrelease.wav`, `buttonrollover.wav`.
- `sound/buttons/`: `blip1`, `button1`..`button10`, `button14`..`button19`, `button24`, `combine_button1`..`combine_button7`, `combine_button_locked`,
  `lever1`..`lever8`, `lightswitch2`.
- `sound/common/`: `warning.wav`, `wpn_select.wav`, `wpn_moveselect.wav`, `wpn_denyselect.wav`, `wpn_hudoff.wav`, `talk.wav`, `bugreporter_*.wav`, `null.wav`.

## 5. Music (`snd`, `sound/music/`)

Durations measured with `ffprobe` on extracted copies (scratch files deleted afterwards), rounded to seconds. Mood notes are inferred from
file names and in-game role only (not auditioned), so listen before committing a track to a race.

| File | Duration (s) | Note |
| --- | --- | --- |
| `sound/music/hl1_song10.mp3` | 105 | Half-Life 1 score |
| `sound/music/hl1_song11.mp3` | 35 | Half-Life 1 score |
| `sound/music/hl1_song14.mp3` | 90 | Half-Life 1 score |
| `sound/music/hl1_song15.mp3` | 121 | Half-Life 1 score |
| `sound/music/hl1_song17.mp3` | 124 | Half-Life 1 score |
| `sound/music/hl1_song19.mp3` | 116 | Half-Life 1 score |
| `sound/music/hl1_song20.mp3` | 85 | Half-Life 1 score |
| `sound/music/hl1_song21.mp3` | 85 | Half-Life 1 score |
| `sound/music/hl1_song24.mp3` | 77 | Half-Life 1 score |
| `sound/music/hl1_song25_remix3.mp3` | 61 | Half-Life 1 score |
| `sound/music/hl1_song26.mp3` | 38 | Half-Life 1 score |
| `sound/music/hl1_song3.mp3` | 132 | Half-Life 1 score |
| `sound/music/hl1_song5.mp3` | 96 | Half-Life 1 score |
| `sound/music/hl1_song6.mp3` | 100 | Half-Life 1 score |
| `sound/music/hl1_song9.mp3` | 93 | Half-Life 1 score |
| `sound/music/hl2_intro.mp3` | 85 | game intro, atmospheric |
| `sound/music/hl2_song0.mp3` | 40 | HL2 score, audition |
| `sound/music/hl2_song10.mp3` | 29 | short cue, loop-only |
| `sound/music/hl2_song11.mp3` | 35 | HL2 score, audition |
| `sound/music/hl2_song12_long.mp3` | 73 | HL2 score, audition |
| `sound/music/hl2_song13.mp3` | 54 | HL2 score, audition |
| `sound/music/hl2_song14.mp3` | 159 | HL2 score, audition |
| `sound/music/hl2_song15.mp3` | 69 | HL2 score, audition |
| `sound/music/hl2_song16.mp3` | 170 | HL2 score, audition |
| `sound/music/hl2_song17.mp3` | 61 | HL2 score, audition |
| `sound/music/hl2_song19.mp3` | 116 | HL2 score, audition |
| `sound/music/hl2_song1.mp3` | 98 | HL2 score, audition |
| `sound/music/hl2_song20_submix0.mp3` | 103 | driving-chapter-style action mix |
| `sound/music/hl2_song20_submix4.mp3` | 140 | driving-chapter-style action mix |
| `sound/music/hl2_song23_suitsong3.mp3` | 43 | suit/lab ambience |
| `sound/music/hl2_song25_teleporter.mp3` | 46 | teleporter sequence, tense |
| `sound/music/hl2_song26.mp3` | 70 | HL2 score, audition |
| `sound/music/hl2_song26_trainstation1.mp3` | 91 | City 17 station, ominous |
| `sound/music/hl2_song27_trainstation2.mp3` | 72 | City 17 station, ominous |
| `sound/music/hl2_song28.mp3` | 13 | short cue, loop-only |
| `sound/music/hl2_song29.mp3` | 136 | HL2 score, audition |
| `sound/music/hl2_song2.mp3` | 173 | HL2 score, audition |
| `sound/music/hl2_song30.mp3` | 104 | HL2 score, audition |
| `sound/music/hl2_song31.mp3` | 99 | HL2 score, audition |
| `sound/music/hl2_song32.mp3` | 43 | HL2 score, audition |
| `sound/music/hl2_song33.mp3` | 84 | HL2 score, audition |
| `sound/music/hl2_song3.mp3` | 91 | HL2 score, audition |
| `sound/music/hl2_song4.mp3` | 66 | HL2 score, audition |
| `sound/music/hl2_song6.mp3` | 45 | HL2 score, audition |
| `sound/music/hl2_song7.mp3` | 51 | HL2 score, audition |
| `sound/music/hl2_song8.mp3` | 60 | HL2 score, audition |
| `sound/music/radio1.mp3` | 40 | radio/diegetic loop |
| `sound/music/ravenholm_1.mp3` | 31 | dark, tense horror ambience |
| `sound/music/stingers/hl1_stinger_song16.mp3` | 16 | short stinger, not a track |
| `sound/music/stingers/hl1_stinger_song27.mp3` | 17 | short stinger, not a track |
| `sound/music/stingers/hl1_stinger_song28.mp3` | 7 | short stinger, not a track |
| `sound/music/stingers/hl1_stinger_song7.mp3` | 23 | short stinger, not a track |
| `sound/music/stingers/hl1_stinger_song8.mp3` | 9 | short stinger, not a track |

`sound/music/hl2_ambient_1.wav`, `stingers/industrial_suspense1.wav`, `stingers/industrial_suspense2.wav` are WAV ambience, not mp3.

## 6. Fonts and UI

TrueType fonts present (no `gordin-*` font files exist in the SDK content, despite the Half-Life 2 UI using that family):
`hl2/resource/boxrocket.ttf`, `hl2/resource/halflife2.ttf`, `hl2/resource/hl2crosshairs.ttf`, `hl2/resource/hl2ep2.ttf`, `hl2/resource/marlett.ttf` (`loose`),
and `hl2mp/resource/halflife2.ttf`, `hl2mp/resource/hl2mp.ttf` (`loose`).

hl2mp UI layouts (`mp`): `resource/ui/bottomspectator.res`, `resource/ui/motd.res`, `resource/ui/scoreboard.res`, `resource/ui/spectator.res`,
`resource/ui/textwindow.res`. Other `mp` resource files: `resource/clientscheme.res`, `commandmenu.res`, `gamemenu.res`, `loadingdialog.res`,
`loadingdialognobanner.res`, `modevents.res`, `multiplayeradvanceddialog.res`, `multiplayercustomizedialog.res`, `optionssubadvanced.res`,
`optionssubmultiplayer.res`, `playerlistdialog.res`, `spectatormenu.res`, `spectatormodes.res`, `objectcontrolpanelscheme.res`,
`pdacontrolpanelscheme.res`, `g15.res`; plus `scripts/hudlayout.res` and `scripts/launcherscheme.res`. The `hl2` folder also has many loose
`resource/*.res` dialogs and `scripts/hudlayout.res`, `scripts/hud_textures.txt`.

## 7. Tier 2: needs the user's approval before use

The Episode Two jalopy (`models/vehicle.mdl`, `scripts/vehicles/jalopy.txt`) and other Valve games are not part of Source SDK Base 2013 Multiplayer:
neither path appears in any VPK listing above, and no other Valve game was found in the Steam library on this machine when this doc was written.
If they are installed later, they are tier 2: ask the user before depending on them.
