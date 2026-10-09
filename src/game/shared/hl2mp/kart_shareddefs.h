//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared kart-mode definitions: the replicated master switch, the kart
//			collision hull and eye position, and the placeholder kart model.
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

// Below this speed (units per second, either way) the kart counts as stopped:
// holding the brake there starts the reverse delay.
#define KART_STOPPED_SPEED	1.0f

// Kart collision hull and eye position. A kart does not crouch, so the duck
// hull is the standing hull and the duck view is the standing view.
#define KART_HULL_MIN	Vector( -20, -20, 0 )
#define KART_HULL_MAX	Vector( 20, 20, 40 )
#define KART_VIEW		Vector( 0, 0, 40 )
#define KART_DEAD_VIEW	Vector( 0, 0, 14 )

// Placeholder kart model until the real one lands: HL2's jeep. Valve content,
// mounted from hl2_misc.vpk and referenced by path only.
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
