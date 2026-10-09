//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-predicted kart movement. In kart mode the player entity is
//			the kart, and this replaces walking with a heading + speed model
//			that runs identically on the server and in client prediction.
//
//			The whole point is determinism: every input comes from the usercmd
//			(forwardmove / sidemove signs) and every piece of state is a
//			predicted player field (m_flKartSpeed, m_flKartYaw, ...) or shared
//			replicated convar. The client never integrates the heading outside
//			this code; it only feeds the predicted yaw back as the view angle so
//			the mouse has no effect.
//
//=============================================================================//

#ifndef KART_GAMEMOVEMENT_H
#define KART_GAMEMOVEMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "hl_gamemovement.h"

#ifdef CLIENT_DLL
#include "hl2mp/c_hl2mp_player.h"
#else
#include "hl2mp/hl2mp_player.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: hl2mp's game movement: stock HL2 movement unless the player is a
//			kart, then the kart model below.
//-----------------------------------------------------------------------------
class CKartGameMovement : public CHL2GameMovement
{
	typedef CHL2GameMovement BaseClass;
public:

	CKartGameMovement();

	virtual void	PlayerMove( void ) OVERRIDE;

protected:

	// True while the current player should drive the kart model instead of walking.
	bool			ShouldKartMove( void ) const;

	// Kart replacement for CGameMovement::PlayerMove().
	void			KartPlayerMove( void );

	// One tick of kart physics: speed and heading from the usercmd, then the move.
	void			KartMove( void );

	// The speed after one tick of throttle, signed (negative is reversing).
	float			KartUpdateSpeed( float flSpeed, float flThrottle, float flFrametime );

	// Hop and drift state for one tick: starts a hop (which takes the kart off
	// the ground, so bOnGround can change), enters, keeps or ends the drift and
	// moves the slip angle toward the drift's.
	void			KartUpdateHopAndDrift( float flSpeed, float flSteer, bool bJumpHeld, bool bJumpPressed, bool &bOnGround, float flFrametime );

	// Mini-turbo tier (0-3) for a drift charge.
	static int		KartTurboTier( float flCharge );

	// Gives the boost for releasing a drift at a mini-turbo tier (none for 0).
	void			KartReleaseTurbo( int nTier );

	// Signed steering rate (degrees per second) for a speed.
	static float	KartTurnRate( float flSpeed, bool bOnGround );

	// Upward speed for a kart that just left the ground it stood on at vecStart.
	float			KartLaunchSpeed( const Vector &vecStart );

	// Normal of the ground the kart stands on, (0,0,1) in the air.
	Vector			KartGroundNormal( void );

	// The kart's forward direction (unit, horizontal) for a heading.
	static Vector	KartForward( float flYaw );

	CHL2MP_Player	*GetKartPlayer( void ) const { return static_cast< CHL2MP_Player * >( player ); }
};

#endif // KART_GAMEMOVEMENT_H
