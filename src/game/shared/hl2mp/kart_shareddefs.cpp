//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared kart-mode definitions. See kart_shareddefs.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar kart_enabled( "kart_enabled", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Players spawn as karts instead of HL2DM characters. Takes effect on respawn." );

const color32 g_KartColors[KART_COLOR_COUNT] =
{
	{ 230,  60,  60, 255 },	// red
	{  60, 120, 230, 255 },	// blue
	{  70, 200,  90, 255 },	// green
	{ 245, 210,  50, 255 },	// yellow
	{ 240, 140,  40, 255 },	// orange
	{ 170,  80, 220, 255 },	// purple
	{  60, 210, 210, 255 },	// cyan
	{ 240, 120, 190, 255 },	// pink
};
