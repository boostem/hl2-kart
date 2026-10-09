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
