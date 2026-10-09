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
//			Inputs: the sign of forwardmove (throttle) and sidemove (steer), and
//			IN_JUMP (hop and drift), from the usercmd. Never client-only convars:
//			the server has to reproduce this exactly.
//			State: m_flKartSpeed, m_flKartYaw, m_flKartReverseTime,
//			m_flKartBumpCooldown and the hop and drift state (m_nKartDriftDir,
//			m_flKartSlipAngle, m_flKartDriftTime, m_flKartHopTime), the
//			mini-turbo charge (m_flKartDriftCharge, m_nKartDriftTier) and the boost
//			(m_flKartBoostEndTime, m_flKartBoostScale) on the player, all predicted.
//			Tuning: replicated convars, the same value on both sides.
//			Time: gpGlobals->frametime, which is TICK_INTERVAL on both sides, and
//			gpGlobals->curtime (the boost's end), the predicted tick's time.
//-----------------------------------------------------------------------------
void CKartGameMovement::KartMove( void )
{
	VPROF( "CKartGameMovement::KartMove" );

	CHL2MP_Player *pKart = GetKartPlayer();
	const float flFrametime = gpGlobals->frametime;
	bool bOnGround = ( player->GetGroundEntity() != NULL );	// a hop clears it below

	pKart->m_flKartBumpCooldown = MAX( 0.0f, pKart->m_flKartBumpCooldown - flFrametime );

	// Inputs. A frozen kart gets none and coasts to a stop.
	float flThrottle = 0.0f;
	float flSteer = 0.0f;
	bool bJumpHeld = false;
	bool bJumpPressed = false;
	if ( !( player->GetFlags() & FL_FROZEN ) )
	{
		flThrottle = KartInputSign( mv->m_flForwardMove );
		flSteer = KartInputSign( mv->m_flSideMove );
		bJumpHeld = ( mv->m_nButtons & IN_JUMP ) != 0;
		bJumpPressed = bJumpHeld && !( mv->m_nOldButtons & IN_JUMP );
	}
	else
	{
		// Frozen mid-drift (round end, finish): the drift ends without a
		// mini-turbo, and any boost stops so the kart really coasts.
		pKart->m_flKartDriftCharge = 0.0f;
		pKart->m_nKartDriftTier = 0;
		pKart->m_flKartBoostEndTime = 0.0f;
	}

	pKart->m_nKartSteer = (int)flSteer;	// only drawn: the wheels and steering wheel turn with it

	float flSpeed = KartUpdateSpeed( pKart->m_flKartSpeed, flThrottle, flFrametime );

	KartUpdateHopAndDrift( flSpeed, flSteer, bJumpHeld, bJumpPressed, bOnGround, flFrametime );

	// Boost (a mini-turbo given just above, boost pads, items): the speed jumps
	// straight up to the boost's, whatever the throttle. When it ends,
	// KartUpdateSpeed brings the speed back down to top speed.
	if ( pKart->IsKartBoosting() )
	{
		flSpeed = MAX( flSpeed, kart_max_speed.GetFloat() * pKart->m_flKartBoostScale );
	}

	// Heading: positive yaw is left, so D (positive sidemove) decreases yaw.
	float flYaw = pKart->m_flKartYaw;
	const int nDriftDir = pKart->m_nKartDriftDir;
	if ( nDriftDir != 0 )
	{
		// Drifting: the kart always turns the drift's way. Steering into the drift
		// tightens the turn, steering against it widens it, no steer is in between.
		float flRate = RemapVal( flSteer * nDriftDir, -1.0f, 1.0f, kart_drift_turn_min.GetFloat(), kart_drift_turn_max.GetFloat() );
		if ( !bOnGround )
		{
			flRate *= kart_air_turn_scale.GetFloat();
		}
		flYaw = AngleNormalize( flYaw - nDriftDir * flRate * flFrametime );
	}
	else if ( flSteer != 0.0f )
	{
		// A hop steers like the ground, so it can swing the nose into a corner.
		bool bFullSteer = bOnGround || pKart->m_flKartHopTime > 0.0f;
		flYaw = AngleNormalize( flYaw - flSteer * KartTurnRate( flSpeed, bFullSteer ) * flFrametime );
	}

	// The kart moves along its velocity yaw, which lags the heading by the slip
	// angle in and just after a drift: the kart slides with its nose pointing
	// into the turn. The body (render) yaw stays the heading.
	Vector vecMoveDir = KartForward( flYaw - pKart->m_flKartSlipAngle );

	// Keep the base movement angles consistent with the heading for any helper
	// that reads them (the view angles are never used for movement in a kart).
	AngleVectors( QAngle( 0.0f, flYaw, 0.0f ), &m_vecForward, &m_vecRight, &m_vecUp );

	Vector vecStart = mv->GetAbsOrigin();

	// The speed along the velocity yaw the move would keep with nothing in the way:
	// whatever the collisions take off it is what the bump check looks at.
	float flExpected;

	if ( bOnGround )
	{
		// On the ground the velocity is the velocity yaw times the speed, no
		// momentum sideways: the kart goes where it points, less the slip.
		mv->m_vecVelocity.x = vecMoveDir.x * flSpeed;
		mv->m_vecVelocity.y = vecMoveDir.y * flSpeed;
		mv->m_vecVelocity.z = 0.0f;
		flExpected = flSpeed;

		Vector dest;
		dest.x = vecStart.x + mv->m_vecVelocity.x * flFrametime;
		dest.y = vecStart.y + mv->m_vecVelocity.y * flFrametime;
		dest.z = vecStart.z;

		trace_t pm;
		TracePlayerBBox( vecStart, dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );

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
		// Air control: the momentum turns toward the velocity yaw, a little each tick,
		// so the kart can line up a landing without steering like on the ground.
		float flBlend = clamp( kart_air_control.GetFloat() * flFrametime, 0.0f, 1.0f );
		mv->m_vecVelocity.x = Lerp( flBlend, mv->m_vecVelocity.x, vecMoveDir.x * flSpeed );
		mv->m_vecVelocity.y = Lerp( flBlend, mv->m_vecVelocity.y, vecMoveDir.y * flSpeed );
		flExpected = mv->m_vecVelocity.x * vecMoveDir.x + mv->m_vecVelocity.y * vecMoveDir.y;

		StartGravity();
		TryPlayerMove();
		FinishGravity();
	}

	// Set final flags.
	CategorizePosition();

	// Driving off the ground keeps the climb of the slope it left, so a ramp
	// gives a little air instead of the kart dropping off its lip.
	if ( bOnGround && player->GetGroundEntity() == NULL )
	{
		mv->m_vecVelocity.z = KartLaunchSpeed( vecStart );
	}

	// Make sure velocity is valid.
	CheckVelocity();

	// If we are on ground, no downward velocity.
	if ( player->GetGroundEntity() != NULL )
	{
		mv->m_vecVelocity.z = 0.0f;
	}

	CheckFalling();

	// Wall contact: the speed state follows what the collision left of the
	// velocity along the velocity yaw, so a glancing hit bleeds speed. A hit that
	// takes more than kart_bump_threshold off is a bump: the kart keeps only part
	// of what is left, thuds and drops out of any drift.
	float flAlong = mv->m_vecVelocity.x * vecMoveDir.x + mv->m_vecVelocity.y * vecMoveDir.y;

	// On the ground flExpected is flSpeed, so this is flAlong. In the air the
	// velocity trails it (air control); only the collision's share comes off the speed.
	flSpeed += flAlong - flExpected;

	if ( fabs( flExpected ) - fabs( flAlong ) > kart_bump_threshold.GetFloat() )
	{
		flSpeed *= kart_bump_restitution.GetFloat();

		// The drift's charge is lost: no mini-turbo off a wall.
		pKart->m_nKartDriftDir = 0;
		pKart->m_flKartDriftTime = 0.0f;
		pKart->m_flKartDriftCharge = 0.0f;
		pKart->m_nKartDriftTier = 0;

		if ( pKart->m_flKartBumpCooldown <= 0.0f )
		{
			pKart->m_flKartBumpCooldown = kart_bump_cooldown.GetFloat();

			// Predicted: the client plays it the first time it predicts the bump,
			// the server sends it to everyone else.
			CPASAttenuationFilter filter( player );
			filter.UsePredictionRules();
			player->EmitSound( filter, player->entindex(), "Kart.Impact" );
		}
	}

	pKart->m_flKartSpeed = flSpeed;
	pKart->m_flKartYaw = flYaw;

	// Only drawn: the client tilts the kart model with it.
	pKart->m_vecKartGroundNormal = KartGroundNormal();
}

//-----------------------------------------------------------------------------
// Purpose: Hop and drift state for one tick, before the heading and the move.
//
//			Hop: jump pressed (not held over from an earlier tick) on the ground
//			throws the kart up at kart_hop_velocity; m_flKartHopTime counts its
//			airtime and clears on landing. There is no hop in the air.
//
//			Drift: on the ground (including the tick a hop lands) with jump held,
//			a steer key down and at least kart_drift_min_speed forward, the kart
//			drifts the way it is steering, and keeps that direction until jump is
//			released, the speed drops below the minimum or it bumps a wall (see
//			KartMove). The direction is the steer sign: +1 right, -1 left.
//
//			Mini-turbo: a drift charges m_flKartDriftCharge, faster steering into
//			it (kart_turbo_charge_max) than against it (kart_turbo_charge_min), and
//			m_nKartDriftTier is the kart_turbo_tierN_time it has passed. Releasing
//			jump ends the drift with a boost of that tier's duration. A drift that
//			ends below the minimum speed (here) or on a bump (KartMove) gives none.
//
//			Slip: the slip angle (heading minus velocity yaw) moves toward the
//			drift's at kart_drift_slip_rate, and back to zero the same way once
//			the drift ends, so the kart straightens out instead of snapping.
//-----------------------------------------------------------------------------
void CKartGameMovement::KartUpdateHopAndDrift( float flSpeed, float flSteer, bool bJumpHeld, bool bJumpPressed, bool &bOnGround, float flFrametime )
{
	CHL2MP_Player *pKart = GetKartPlayer();

	if ( bOnGround )
	{
		pKart->m_flKartHopTime = 0.0f;

		if ( bJumpPressed )
		{
			mv->m_vecVelocity.z = kart_hop_velocity.GetFloat();
			SetGroundEntity( NULL );
			bOnGround = false;
		}
	}

	if ( !bOnGround && ( bJumpPressed || pKart->m_flKartHopTime > 0.0f ) )
	{
		pKart->m_flKartHopTime += flFrametime;
	}

	int nDir = pKart->m_nKartDriftDir;
	const float flMinSpeed = kart_drift_min_speed.GetFloat();

	if ( nDir != 0 && ( !bJumpHeld || flSpeed < flMinSpeed ) )
	{
		if ( !bJumpHeld )
		{
			KartReleaseTurbo( pKart->m_nKartDriftTier );
		}
		nDir = 0;
	}
	else if ( nDir == 0 && bOnGround && bJumpHeld && flSteer != 0.0f && flSpeed >= flMinSpeed )
	{
		nDir = ( flSteer > 0.0f ) ? 1 : -1;
	}

	pKart->m_nKartDriftDir = nDir;
	pKart->m_flKartDriftTime = ( nDir != 0 ) ? pKart->m_flKartDriftTime + flFrametime : 0.0f;

	if ( nDir != 0 )
	{
		float flChargeRate = RemapVal( flSteer * nDir, -1.0f, 1.0f, kart_turbo_charge_min.GetFloat(), kart_turbo_charge_max.GetFloat() );
		pKart->m_flKartDriftCharge += flChargeRate * flFrametime;
	}
	else
	{
		pKart->m_flKartDriftCharge = 0.0f;
	}
	pKart->m_nKartDriftTier = KartTurboTier( pKart->m_flKartDriftCharge );

	// Positive yaw is left and +1 is a right drift, which turns the heading to the
	// right of the velocity: a negative slip angle.
	float flSlipTarget = -nDir * kart_drift_slip_angle.GetFloat();
	pKart->m_flKartSlipAngle = Approach( flSlipTarget, pKart->m_flKartSlipAngle, kart_drift_slip_rate.GetFloat() * flFrametime );
}

//-----------------------------------------------------------------------------
// Purpose: Mini-turbo tier (0 for none, up to 3) a drift charge has reached.
//-----------------------------------------------------------------------------
int CKartGameMovement::KartTurboTier( float flCharge )
{
	if ( flCharge >= kart_turbo_tier3_time.GetFloat() )
		return 3;
	if ( flCharge >= kart_turbo_tier2_time.GetFloat() )
		return 2;
	if ( flCharge >= kart_turbo_tier1_time.GetFloat() )
		return 1;
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: The boost for releasing a drift at nTier (none for tier 0).
//-----------------------------------------------------------------------------
void CKartGameMovement::KartReleaseTurbo( int nTier )
{
	float flDuration;
	switch ( nTier )
	{
	case 1:		flDuration = kart_turbo_tier1_duration.GetFloat(); break;
	case 2:		flDuration = kart_turbo_tier2_duration.GetFloat(); break;
	case 3:		flDuration = kart_turbo_tier3_duration.GetFloat(); break;
	default:	return;
	}

	GetKartPlayer()->KartGiveBoost( flDuration, kart_boost_scale.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: The kart's speed after one tick of throttle (+1 W, -1 S, 0 none).
//			Signed: negative is reversing. Also runs the reverse delay timer.
//			Above top speed (a boost just ended) it slows back down to it at
//			kart_boost_decay whatever the throttle, or harder when braking.
//-----------------------------------------------------------------------------
float CKartGameMovement::KartUpdateSpeed( float flSpeed, float flThrottle, float flFrametime )
{
	CHL2MP_Player *pKart = GetKartPlayer();

	const float flMaxSpeed = kart_max_speed.GetFloat();
	if ( flSpeed > flMaxSpeed )
	{
		pKart->m_flKartReverseTime = 0.0f;

		float flRate = kart_boost_decay.GetFloat();
		if ( flThrottle < 0.0f )
		{
			flRate = MAX( flRate, kart_brake_decel.GetFloat() );
		}

		return Approach( flMaxSpeed, flSpeed, flRate * flFrametime );
	}

	if ( flThrottle > 0.0f )
	{
		pKart->m_flKartReverseTime = 0.0f;

		// Still rolling backwards: brake. Otherwise a punchy launch, then a slower
		// climb to top speed (and a slow-down to it if above).
		float flRate;
		if ( flSpeed < 0.0f )
			flRate = kart_brake_decel.GetFloat();
		else if ( flSpeed < kart_accel_low_threshold.GetFloat() )
			flRate = kart_accel_low.GetFloat();
		else
			flRate = kart_accel.GetFloat();

		return Approach( flMaxSpeed, flSpeed, flRate * flFrametime );
	}

	if ( flThrottle < 0.0f )
	{
		if ( flSpeed > KART_STOPPED_SPEED )
		{
			// Rolling forward: brake.
			pKart->m_flKartReverseTime = 0.0f;
			return Approach( 0.0f, flSpeed, kart_brake_decel.GetFloat() * flFrametime );
		}

		if ( flSpeed >= -KART_STOPPED_SPEED && pKart->m_flKartReverseTime < kart_reverse_delay.GetFloat() )
		{
			// Stopped: hold still for kart_reverse_delay before backing up, so
			// braking to a stop does not roll straight into reverse.
			pKart->m_flKartReverseTime += flFrametime;
			return Approach( 0.0f, flSpeed, kart_brake_decel.GetFloat() * flFrametime );
		}

		return Approach( -kart_reverse_speed.GetFloat(), flSpeed, kart_reverse_accel.GetFloat() * flFrametime );
	}

	pKart->m_flKartReverseTime = 0.0f;
	return Approach( 0.0f, flSpeed, kart_coast_decel.GetFloat() * flFrametime );
}

//-----------------------------------------------------------------------------
// Purpose: Signed steering rate in degrees per second for a speed: tight at low
//			speed, wide at top speed, none when (nearly) stopped, reduced in the
//			air, and mirrored in reverse like a car.
//-----------------------------------------------------------------------------
float CKartGameMovement::KartTurnRate( float flSpeed, bool bOnGround )
{
	float flAbsSpeed = fabs( flSpeed );
	if ( flAbsSpeed < kart_turn_min_speed.GetFloat() )
		return 0.0f;

	float flMaxSpeed = MAX( kart_max_speed.GetFloat(), 1.0f );
	float flRate = RemapValClamped( flAbsSpeed / flMaxSpeed, 0.0f, 1.0f, kart_turn_rate_low.GetFloat(), kart_turn_rate_high.GetFloat() );

	if ( !bOnGround )
	{
		flRate *= kart_air_turn_scale.GetFloat();
	}

	return ( flSpeed < 0.0f ) ? -flRate : flRate;
}

//-----------------------------------------------------------------------------
// Purpose: Upward speed for a kart that just drove off the ground at vecStart:
//			its ground velocity projected onto the slope it was on. Zero on flat
//			or downhill ground, and only walkable slopes count, so a step edge
//			never throws the kart.
//-----------------------------------------------------------------------------
float CKartGameMovement::KartLaunchSpeed( const Vector &vecStart )
{
	trace_t pm;
	TracePlayerBBox( vecStart, vecStart - Vector( 0.0f, 0.0f, 2.0f ), PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );

	if ( pm.fraction == 1.0f || pm.startsolid || pm.plane.normal.z < 0.7f )
		return 0.0f;

	const Vector &n = pm.plane.normal;
	float flClimb = -( n.x * mv->m_vecVelocity.x + n.y * mv->m_vecVelocity.y ) / n.z;
	return MAX( flClimb, 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: The normal of the ground under the kart, from the same short hull
//			trace down CategorizePosition grounds it with. Straight up in the air
//			and on anything too steep to stand on.
//-----------------------------------------------------------------------------
Vector CKartGameMovement::KartGroundNormal( void )
{
	if ( player->GetGroundEntity() == NULL )
		return Vector( 0.0f, 0.0f, 1.0f );

	trace_t pm;
	const Vector &vecOrigin = mv->GetAbsOrigin();
	TracePlayerBBox( vecOrigin, vecOrigin - Vector( 0.0f, 0.0f, 4.0f ), PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );

	if ( pm.fraction == 1.0f || pm.startsolid || pm.plane.normal.z < 0.7f )
		return Vector( 0.0f, 0.0f, 1.0f );

	return pm.plane.normal;
}

//-----------------------------------------------------------------------------
// Expose our interface. hl2mp's singleton lives here instead of
// hl_gamemovement.cpp (wrapped there in #if !defined( HL2MP )).
//-----------------------------------------------------------------------------
static CKartGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );
