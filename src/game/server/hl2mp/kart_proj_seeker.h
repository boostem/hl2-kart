//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: kart_proj_seeker, the Seeker item: a homing projectile launched
//			forward that runs along the track to the kart one place ahead in
//			the race and stuns it.
//
//			Target: the kart one race position ahead of the thrower, picked
//			again whenever that kart is gone. In free drive (no race
//			positions) the nearest kart in a cone ahead of the thrower. The
//			leader's seeker has no target and just runs the track.
//
//			Steering: it turns by yaw at kart_seeker_turn_rate toward a point
//			on the track ahead, so it follows the track around bends instead of
//			cutting across them: a lookahead point on the racing line when the
//			map has one, else the center of the next checkpoint on the route
//			(starting at the thrower's next checkpoint). Once the target is
//			within kart_seeker_lock_range with a clear line to it, it steers
//			straight at the target instead.
//
//			Moved by its own think like the hubcap: it hugs the track, steps up
//			kerbs and ramps, falls off edges and slides along walls. The first
//			kart it sweeps is stunned (KartApplyHit) and the seeker breaks; it
//			also breaks after kart_seeker_lifetime. The thrower is safe from it
//			for a moment after the launch.
//
//			Networked as a plain CBaseAnimating: the client renders the model
//			and interpolates the origin and the angles.
//
//=============================================================================//

#ifndef KART_PROJ_SEEKER_H
#define KART_PROJ_SEEKER_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"

class CHL2MP_Player;

// Placeholder model until the custom one (M8): the HL2 hopper mine, a neutral
// round shape. Valve content, referenced by path only.
#define KART_SEEKER_MODEL		"models/props_combine/combine_mine01.mdl"

//-----------------------------------------------------------------------------
// kart_proj_seeker
//-----------------------------------------------------------------------------
class CKartProjSeeker : public CBaseAnimating
{
	DECLARE_CLASS( CKartProjSeeker, CBaseAnimating );
	DECLARE_DATADESC();

public:
	CKartProjSeeker();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void UpdateOnRemove( void );

	void SeekerThink( void );

	// Launches a seeker ahead of the kart. False when there is no room to
	// launch it or the kart can't launch right now; the item is kept.
	static bool Launch( CHL2MP_Player *pThrower );

private:
	CHL2MP_Player *SelectTarget( void ) const;
	bool IsValidTarget( CHL2MP_Player *pKart ) const;
	bool GetSteerPoint( Vector &vecPoint );
	bool GetTrackPoint( Vector &vecPoint );
	void Move( float flTime, const Vector &vecSteerTo );
	CHL2MP_Player *FindKartHit( const Vector &vecStart, const Vector &vecEnd ) const;
	void Break( const char *pszSound, const char *pszReason );

	CHandle< CHL2MP_Player > m_hThrower;
	CHandle< CHL2MP_Player > m_hTarget;
	bool m_bLocked;					// steering straight at the target
	float m_flYaw;					// heading, turned toward the steer point
	float m_flSpeed;
	float m_flFallSpeed;			// downward speed while off the ground
	bool m_bOnGround;
	float m_flLineDistance;			// how far along the racing line it is, -1 when not on it
	int m_iWaypoint;				// route position of the checkpoint it heads for, -1 for none
	float m_flNextTargetTime;		// when to look for a target again while it has none
	float m_flThrowerSafeTime;		// the thrower can't be hit before this
	float m_flDieTime;
	float m_flLastMoveTime;
};

#endif // KART_PROJ_SEEKER_H
