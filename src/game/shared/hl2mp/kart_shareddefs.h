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
