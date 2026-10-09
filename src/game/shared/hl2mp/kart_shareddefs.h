//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared kart-mode definitions: the replicated master switch, the kart
//			collision hull and eye position, and the kart models.
//
//			The player entity IS the kart. There is no prop_vehicle and no
//			vehicle controller: the player keeps its own (predicted) movement,
//			swaps its model for the kart and uses the kart hull below.
//
//=============================================================================//

#ifndef KART_SHAREDDEFS_H
#define KART_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "mathlib/vector.h"
#include "shareddefs.h"

// Master switch. Replicated so the client's view vectors and prediction agree
// with the server. Players latch it at spawn (CHL2MP_Player::m_bKartMode):
// change it and respawn to switch between kart and stock deathmatch.
extern ConVar kart_enabled;

// The kart model's path (KART_DEFAULT_MODEL by default). Replicated; players
// take it at spawn, like kart_enabled.
extern ConVar kart_model;

// Kart movement tuning, read by CKartGameMovement on both sides. Replicated so
// prediction matches the server; not cheats, so cfg/kart_tuning.cfg can set them.
extern ConVar kart_max_speed;
extern ConVar kart_accel;
extern ConVar kart_accel_low;
extern ConVar kart_accel_low_threshold;
extern ConVar kart_brake_decel;
extern ConVar kart_reverse_speed;
extern ConVar kart_reverse_accel;
extern ConVar kart_reverse_delay;
extern ConVar kart_coast_decel;
extern ConVar kart_turn_rate_low;
extern ConVar kart_turn_rate_high;
extern ConVar kart_turn_min_speed;
extern ConVar kart_air_turn_scale;
extern ConVar kart_air_control;
extern ConVar kart_bump_threshold;
extern ConVar kart_bump_restitution;
extern ConVar kart_bump_cooldown;
extern ConVar kart_hop_velocity;
extern ConVar kart_drift_min_speed;
extern ConVar kart_drift_slip_angle;
extern ConVar kart_drift_turn_min;
extern ConVar kart_drift_turn_max;
extern ConVar kart_drift_slip_rate;
extern ConVar kart_turbo_charge_min;
extern ConVar kart_turbo_charge_max;
extern ConVar kart_turbo_tier1_time;
extern ConVar kart_turbo_tier2_time;
extern ConVar kart_turbo_tier3_time;
extern ConVar kart_turbo_tier1_duration;
extern ConVar kart_turbo_tier2_duration;
extern ConVar kart_turbo_tier3_duration;
extern ConVar kart_boost_scale;
extern ConVar kart_boost_decay;

// Below this speed (units per second, either way) the kart counts as stopped:
// holding the brake there starts the reverse delay.
#define KART_STOPPED_SPEED	1.0f

// Kart collision hull and eye position. A kart does not crouch, so the duck
// hull is the standing hull and the duck view is the standing view.
// The hull is an axis-aligned box that doesn't turn with the kart, so it can't
// match the 112 x 70 x 50 kart model: it is as wide as the model's half-width
// allows (the sides stop at walls, the nose and tail go in about 24 units) and
// tall enough for the roll hoop. The view is at the model's vehicle_driver_eyes.
#define KART_HULL_MIN	Vector( -32, -32, 0 )
#define KART_HULL_MAX	Vector( 32, 32, 48 )
#define KART_VIEW		Vector( 0, 0, 40 )
#define KART_DEAD_VIEW	Vector( 0, 0, 14 )

// The scrap kart, built for this mod (assets_src/kart_scrap/). Its attachments:
// wheel_fl/fr/rl/rr (tyre contact patches), exhaust, vehicle_driver_eyes and
// item_hold (behind the kart).
#define KART_DEFAULT_MODEL		"models/kart/kart_scrap.mdl"

// The earlier placeholder, HL2's jeep: Valve content, mounted from
// hl2_misc.vpk and referenced by path only. Still precached for kart_model.
#define KART_PLACEHOLDER_MODEL	"models/buggy.mdl"

// Range of m_flKartSpeed as sent to other players (12 bits over this range
// gives 0.5 u/s steps). The local player gets the unscaled float.
#define KART_NET_SPEED_MIN	-1024.0f
#define KART_NET_SPEED_MAX	1024.0f

// HUD elements that mean nothing in a kart: no suit, no health, no weapons.
#define KART_HIDEHUD_BITS	( HIDEHUD_HEALTH | HIDEHUD_WEAPONSELECTION | HIDEHUD_CROSSHAIR | HIDEHUD_FLASHLIGHT )

// Kart color palette: cl_kart_color (client userinfo) indexes into it. Tint only,
// until custom models with $colortint arrive.
#define KART_COLOR_COUNT	8
extern const color32 g_KartColors[KART_COLOR_COUNT];

#endif // KART_SHAREDDEFS_H
