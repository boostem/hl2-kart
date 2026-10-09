//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-predicted kart movement. In kart mode the player entity is
//			the kart, and this replaces walking with a heading + speed model
//			that runs identically on the server and in client prediction.
//
//			The whole point is determinism: every input comes from the usercmd
//			(forwardmove / sidemove signs) and every piece of state is a
//			predicted player field (m_flKartSpeed, m_flKartYaw) or shared
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

	// The kart's forward direction (unit, horizontal) for a heading.
	static Vector	KartForward( float flYaw );

	CHL2MP_Player	*GetKartPlayer( void ) const { return static_cast< CHL2MP_Player * >( player ); }
};

#endif // KART_GAMEMOVEMENT_H
