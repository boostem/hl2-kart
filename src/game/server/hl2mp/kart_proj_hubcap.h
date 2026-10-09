//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: kart_proj_hubcap, the Hubcap item: a hubcap thrown forward or
//			backward that skims along the track, bounces off walls and spins
//			out the first kart it touches.
//
//			Moved by its own think, one step per tick: a hull trace along its
//			velocity against the world (karts and other projectiles ignored),
//			raised by a step while on the ground so it climbs kerbs and ramps,
//			then a trace down that keeps it hugging the track. Off an edge it
//			falls until it lands again. Walls (steeper than
//			KART_HUBCAP_FLOOR_NORMAL) reflect it with some speed loss, up to
//			kart_hubcap_bounces times. Karts are checked against each step:
//			the first one swept spins out (KartApplyHit) and the hubcap breaks.
//			The thrower is safe from its own hubcap for a moment after the throw.
//
//			Networked as a plain CBaseAnimating: the client renders the model
//			and interpolates the origin and the spin.
//
//=============================================================================//

#ifndef KART_PROJ_HUBCAP_H
#define KART_PROJ_HUBCAP_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"

class CHL2MP_Player;

// Placeholder model until the custom one (M8): an HL2 car wheel at half size.
// Valve content, referenced by path only.
#define KART_HUBCAP_MODEL		"models/props_vehicles/carparts_wheel01a.mdl"

//-----------------------------------------------------------------------------
// kart_proj_hubcap
//-----------------------------------------------------------------------------
class CKartProjHubcap : public CBaseAnimating
{
	DECLARE_CLASS( CKartProjHubcap, CBaseAnimating );
	DECLARE_DATADESC();

public:
	CKartProjHubcap();

	virtual void Spawn( void );
	virtual void Precache( void );

	void HubcapThink( void );

	// Throws a hubcap from the kart. False when there is no room to throw it
	// or the kart can't throw right now; the item is kept.
	static bool Throw( CHL2MP_Player *pThrower, bool bBackward );

private:
	void Move( float flTime );
	CHL2MP_Player *FindKartHit( const Vector &vecStart, const Vector &vecEnd ) const;
	void Bounce( const Vector &vecNormal, const Vector &vecPos );
	void Break( const char *pszReason );

	CHandle< CHL2MP_Player > m_hThrower;
	Vector m_vecVelocity;			// moved by hand, so kept here and mirrored to the abs velocity
	bool m_bOnGround;
	int m_nBounces;
	float m_flThrowerSafeTime;		// the thrower can't be hit before this
	float m_flDieTime;
	float m_flLastMoveTime;
};

#endif // KART_PROJ_HUBCAP_H
