//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-predicted kart movement. See kart_gamemovement.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_gamemovement.h"
#include "kart_shareddefs.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: -1, 0 or +1 for an analog usercmd axis. mathlib's Sign() returns +1
//			for zero, which would drive the kart with no key held.
//-----------------------------------------------------------------------------
static inline float KartInputSign( float flMove )
{
	if ( flMove > 0.0f )
		return 1.0f;
	if ( flMove < 0.0f )
		return -1.0f;
	return 0.0f;
}

CKartGameMovement::CKartGameMovement()
{
}

//-----------------------------------------------------------------------------
// Purpose: Horizontal unit vector for a kart heading.
//-----------------------------------------------------------------------------
Vector CKartGameMovement::KartForward( float flYaw )
{
	Vector vecForward;
	AngleVectors( QAngle( 0.0f, flYaw, 0.0f ), &vecForward );
	vecForward.z = 0.0f;
	VectorNormalize( vecForward );
	return vecForward;
}

//-----------------------------------------------------------------------------
// Purpose: Kart movement only applies to a live, walking kart. Dead players
//			(observer / none), noclip and anything else keep the stock movement
//			so spectating, "kill" and sv_cheats noclip keep working in kart mode.
//-----------------------------------------------------------------------------
bool CKartGameMovement::ShouldKartMove( void ) const
{
	CHL2MP_Player *pKart = GetKartPlayer();
	if ( !pKart || !pKart->IsInKart() )
		return false;

	if ( player->pl.deadflag )
		return false;

	return player->GetMoveType() == MOVETYPE_WALK;
}

//-----------------------------------------------------------------------------
// Purpose: Entry point for one usercmd, on the server and in client prediction.
//-----------------------------------------------------------------------------
void CKartGameMovement::PlayerMove( void )
{
	if ( !ShouldKartMove() )
	{
		BaseClass::PlayerMove();
		return;
	}

	KartPlayerMove();
}

//-----------------------------------------------------------------------------
// Purpose: Kart replacement for CGameMovement::PlayerMove(). Deliberately not
//			called: CheckParameters (sprint logic, forwardmove crop, view-angle
//			roll), Duck, LadderMove, UpdateStepSound. The kart never crouches,
//			climbs or plays footsteps, and its angles are its heading.
//-----------------------------------------------------------------------------
void CKartGameMovement::KartPlayerMove( void )
{
	VPROF( "CKartGameMovement::KartPlayerMove" );

	CHL2MP_Player *pKart = GetKartPlayer();

	// clear output applied velocity
	mv->m_outWishVel.Init();
	mv->m_outJumpVel.Init();

	DecayPunchAngle();

	MoveHelper()->ResetTouchList();				// Assume we don't touch anything

	ReduceTimers();

	// Always try and unstick us (we are alive and walking here, see ShouldKartMove).
	bool bStuck = false;
	if ( CheckInterval( STUCK ) )
	{
		bStuck = CheckStuck() != 0;
	}

	if ( !bStuck )
	{
		// Now that we are "unstuck", see where we are (water level, ground entity).
		// Unconditional: the optimized-movement shortcut in the base relies on
		// WalkMove's ground handling, which the kart does not run.
		CategorizePosition();

		// Store off the starting water level
		m_nOldWaterLevel = player->GetWaterLevel();

		// If we are not on ground, store off how fast we are moving down
		if ( player->GetGroundEntity() == NULL )
		{
			player->m_Local.m_flFallVelocity = -mv->m_vecVelocity[ 2 ];
		}

		m_nOnLadder = 0;

		// A kart does not crouch: keep the duck state cleared on both sides so the
		// hull, view offset and FL_DUCKING never diverge from the spawn state.
		player->m_Local.m_bDucked = false;
		player->m_Local.m_bDucking = false;
		player->m_Local.m_bInDuckJump = false;
		if ( player->GetFlags() & FL_DUCKING )
		{
			player->RemoveFlag( FL_DUCKING );
		}

		KartMove();
	}

	// The entity faces its heading: the server's FinishMove writes these to the
	// entity angles, so the body yaw is the predicted kart yaw and nothing else.
	mv->m_vecAngles.Init( 0.0f, pKart->m_flKartYaw, 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: One tick of kart physics.
//
//			Inputs: the sign of forwardmove (throttle) and sidemove (steer) from
//			the usercmd. Never client-only convars: the server has to reproduce
//			this exactly.
//			State: m_flKartSpeed and m_flKartYaw on the player, both predicted.
//			Tuning: replicated convars, the same value on both sides.
//			Time: gpGlobals->frametime, which is TICK_INTERVAL on both sides.
//-----------------------------------------------------------------------------
void CKartGameMovement::KartMove( void )
{
	VPROF( "CKartGameMovement::KartMove" );

	CHL2MP_Player *pKart = GetKartPlayer();
	const float flFrametime = gpGlobals->frametime;

	// Inputs
	float flThrottle = 0.0f;
	float flSteer = 0.0f;
	if ( !( player->GetFlags() & FL_FROZEN ) )
	{
		flThrottle = KartInputSign( mv->m_flForwardMove );
		flSteer = KartInputSign( mv->m_flSideMove );
	}

	// Speed: constant top speed with W held, coast to a stop otherwise.
	// (Feel curves, reverse and braking are the next ticket.)
	float flSpeed = pKart->m_flKartSpeed;
	float flTargetSpeed = ( flThrottle > 0.0f ) ? kart_max_speed.GetFloat() : 0.0f;
	flSpeed = Approach( flTargetSpeed, flSpeed, kart_accel.GetFloat() * flFrametime );

	// Heading: positive yaw is left, so D (positive sidemove) decreases yaw.
	// No turning from a standstill.
	float flYaw = pKart->m_flKartYaw;
	if ( flSteer != 0.0f && fabs( flSpeed ) >= kart_turn_min_speed.GetFloat() )
	{
		flYaw = AngleNormalize( flYaw - flSteer * kart_turn_rate.GetFloat() * flFrametime );
	}

	Vector vecForward = KartForward( flYaw );

	// Keep the base movement angles consistent with the heading for any helper
	// that reads them (the view angles are never used for movement in a kart).
	AngleVectors( QAngle( 0.0f, flYaw, 0.0f ), &m_vecForward, &m_vecRight, &m_vecUp );

	if ( player->GetGroundEntity() != NULL )
	{
		// On the ground the velocity is the heading times the speed, no momentum
		// sideways: the kart goes where it points.
		mv->m_vecVelocity.x = vecForward.x * flSpeed;
		mv->m_vecVelocity.y = vecForward.y * flSpeed;
		mv->m_vecVelocity.z = 0.0f;

		Vector dest;
		dest.x = mv->GetAbsOrigin().x + mv->m_vecVelocity.x * flFrametime;
		dest.y = mv->GetAbsOrigin().y + mv->m_vecVelocity.y * flFrametime;
		dest.z = mv->GetAbsOrigin().z;

		trace_t pm;
		TracePlayerBBox( mv->GetAbsOrigin(), dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );

		if ( pm.fraction == 1 )
		{
			mv->SetAbsOrigin( pm.endpos );
		}
		else
		{
			// Stairs, ramps, walls: step up or slide, whichever goes farther.
			StepMove( dest, pm );
		}

		StayOnGround();
	}
	else
	{
		// In the air the kart keeps its horizontal momentum and falls.
		StartGravity();
		TryPlayerMove();
		FinishGravity();
	}

	// Set final flags.
	CategorizePosition();

	// Make sure velocity is valid.
	CheckVelocity();

	// If we are on ground, no downward velocity.
	if ( player->GetGroundEntity() != NULL )
	{
		mv->m_vecVelocity.z = 0.0f;
	}

	CheckFalling();

	// Wall contact: the speed state follows what the collision left of the
	// velocity along the heading, so running into a wall bleeds speed instead of
	// the kart pretending it still moves. (Bump sound/restitution are the next
	// ticket.)
	float flAlong = mv->m_vecVelocity.x * vecForward.x + mv->m_vecVelocity.y * vecForward.y;

	pKart->m_flKartSpeed = flAlong;
	pKart->m_flKartYaw = flYaw;
}

//-----------------------------------------------------------------------------
// Expose our interface. hl2mp's singleton lives here instead of
// hl_gamemovement.cpp (wrapped there in #if !defined( HL2MP )).
//-----------------------------------------------------------------------------
static CKartGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );
