//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart hazards: items left lying on the track.
//
//			kart_hazard_oil, the Oil Slick: a puddle dropped behind the kart,
//			or thrown ahead of it, that stays for kart_oil_lifetime seconds.
//			Any kart driving through it spins out; the kart that dropped it is
//			immune for its first second. Each kart has at most
//			KART_OIL_MAX_PER_PLAYER down at once, the oldest goes first.
//
//			Server-authoritative. It flies an arc of its own (traces, no
//			physics) until it lands, then lies flat on the ground as a trigger.
//			The client draws it as a flat translucent quad (c_kart_hazards.cpp).
//
//=============================================================================//

#ifndef KART_HAZARDS_H
#define KART_HAZARDS_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "kart_hazards_shared.h"

class CHL2MP_Player;

//-----------------------------------------------------------------------------
// kart_hazard_oil
//-----------------------------------------------------------------------------
class CKartHazardOil : public CBaseEntity
{
	DECLARE_CLASS( CKartHazardOil, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CKartHazardOil();

	// Puts a new slick at vecOrigin, flying at vecVelocity until it lands.
	static CKartHazardOil *Create( CHL2MP_Player *pDropper, const Vector &vecOrigin, float flYaw, const Vector &vecVelocity );

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int UpdateTransmitState( void );

	void FlyThink( void );
	void LieThink( void );
	void OilTouch( CBaseEntity *pOther );

	// Fades it out and removes it, sooner than its lifetime.
	void Expire( void );

	CHL2MP_Player *GetDropper( void ) const;
	float GetDropTime( void ) const { return m_flDropTime; }
	float GetDieTime( void ) const { return m_flDieTime; }

private:
	void Land( const trace_t &tr );
	void DebugDraw( void );

	CNetworkVar( float, m_flDieTime );	// when it is gone; the client fades it out before
	CNetworkVar( float, m_flLandTime );	// 0 while it flies

	EHANDLE m_hDropper;
	Vector m_vecFlyVelocity;
	float m_flDropTime;
	float m_flNextDebugDraw;
};

// The Oil Slick item's use: backward drops it behind the kart, forward throws
// it ahead.
bool KartItemUse_Oil( CHL2MP_Player *pPlayer, bool bBackward );

#endif // KART_HAZARDS_H
