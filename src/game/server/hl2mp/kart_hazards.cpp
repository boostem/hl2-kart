//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart hazards. See kart_hazards.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_hazards.h"
#include "hl2mp_player.h"
#include "kart_shareddefs.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar kart_debug_server;

ConVar kart_oil_lifetime( "kart_oil_lifetime", "20", FCVAR_NOTIFY, "Seconds an Oil Slick stays on the track.", true, 1.0f, true, 300.0f );
ConVar kart_oil_throw_speed( "kart_oil_throw_speed", "700", FCVAR_NOTIFY, "Forward speed of a thrown Oil Slick, on top of the kart's own.", true, 0.0f, true, 3000.0f );
ConVar kart_oil_throw_up( "kart_oil_throw_up", "300", FCVAR_NOTIFY, "Upward speed of a thrown Oil Slick.", true, 0.0f, true, 2000.0f );

#define KART_OIL_MAX_PER_PLAYER		3
#define KART_OIL_DROPPER_IMMUNE		1.0f	// seconds the dropper drives through its own slick unharmed
#define KART_OIL_GRAVITY			800.0f
#define KART_OIL_MAX_FLY_TIME		4.0f	// fell off the map: gone
#define KART_OIL_FLY_THINK			0.03f
#define KART_OIL_LIE_THINK			0.1f
#define KART_OIL_MAX_SLOPE			0.7f	// lands on ground at least this flat (normal z), slides off walls
#define KART_OIL_GROUND_OFFSET		1.0f
#define KART_OIL_DROP_BACK			64.0f	// dropped this far behind the kart's origin
#define KART_OIL_THROW_AHEAD		48.0f
#define KART_OIL_START_HEIGHT		24.0f
#define KART_OIL_DEBUG_DRAW			0.25f

static const Vector s_vecOilFlyMins( -4, -4, 0 );
static const Vector s_vecOilFlyMaxs( 4, 4, 4 );

LINK_ENTITY_TO_CLASS( kart_hazard_oil, CKartHazardOil );

IMPLEMENT_SERVERCLASS_ST( CKartHazardOil, DT_KartHazardOil )
	SendPropTime( SENDINFO( m_flDieTime ) ),
	SendPropTime( SENDINFO( m_flLandTime ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CKartHazardOil )
	DEFINE_FIELD( m_flDieTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLandTime, FIELD_TIME ),
	DEFINE_FIELD( m_hDropper, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecFlyVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_flDropTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextDebugDraw, FIELD_TIME ),
	DEFINE_THINKFUNC( FlyThink ),
	DEFINE_THINKFUNC( LieThink ),
	DEFINE_ENTITYFUNC( OilTouch ),
END_DATADESC()

CKartHazardOil::CKartHazardOil()
{
	m_flDieTime = 0.0f;
	m_flLandTime = 0.0f;
	m_vecFlyVelocity.Init();
	m_flDropTime = 0.0f;
	m_flNextDebugDraw = 0.0f;
}

CKartHazardOil *CKartHazardOil::Create( CHL2MP_Player *pDropper, const Vector &vecOrigin, float flYaw, const Vector &vecVelocity )
{
	CKartHazardOil *pOil = static_cast<CKartHazardOil *>( CreateEntityByName( "kart_hazard_oil" ) );
	if ( !pOil )
		return NULL;

	pOil->SetAbsOrigin( vecOrigin );
	pOil->SetAbsAngles( QAngle( 0, flYaw, 0 ) );
	pOil->m_hDropper = pDropper;
	pOil->m_vecFlyVelocity = vecVelocity;
	DispatchSpawn( pOil );
	return pOil;
}

void CKartHazardOil::Precache( void )
{
	PrecacheMaterial( KART_OIL_MATERIAL );
	PrecacheScriptSound( "Kart.OilThrow" );
	PrecacheScriptSound( "Kart.OilLand" );
}

void CKartHazardOil::Spawn( void )
{
	Precache();

	// No model: the client draws the quad. A trigger once it has landed,
	// sized for the PVS check from the start.
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );
	UTIL_SetSize( this, Vector( -KART_OIL_RADIUS, -KART_OIL_RADIUS, -4.0f ), Vector( KART_OIL_RADIUS, KART_OIL_RADIUS, KART_OIL_TRIGGER_HEIGHT ) );

	m_flDropTime = gpGlobals->curtime;
	m_flDieTime = gpGlobals->curtime + KART_OIL_MAX_FLY_TIME + kart_oil_lifetime.GetFloat();
	m_flLandTime = 0.0f;

	SetTouch( NULL );
	SetThink( &CKartHazardOil::FlyThink );
	SetNextThink( gpGlobals->curtime );
}

int CKartHazardOil::UpdateTransmitState( void )
{
	// It has no model, which CBaseEntity would not send.
	return SetTransmitState( FL_EDICT_PVSCHECK );
}

CHL2MP_Player *CKartHazardOil::GetDropper( void ) const
{
	return ToHL2MPPlayer( m_hDropper.Get() );
}

//-----------------------------------------------------------------------------
// Purpose: The arc: gravity, landing on flat enough ground, sliding down walls.
//-----------------------------------------------------------------------------
void CKartHazardOil::FlyThink( void )
{
	SetNextThink( gpGlobals->curtime + KART_OIL_FLY_THINK );

	if ( gpGlobals->curtime - m_flDropTime > KART_OIL_MAX_FLY_TIME )
	{
		UTIL_Remove( this );
		return;
	}

	float flDelta = KART_OIL_FLY_THINK;
	Vector vecStart = GetAbsOrigin();
	m_vecFlyVelocity.z -= KART_OIL_GRAVITY * flDelta;

	// A wall hit slides along it; a few tries per step at most.
	for ( int i = 0; i < 3 && flDelta > 0.0f; i++ )
	{
		trace_t tr;
		UTIL_TraceHull( vecStart, vecStart + m_vecFlyVelocity * flDelta, s_vecOilFlyMins, s_vecOilFlyMaxs, MASK_PLAYERSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.startsolid )
		{
			UTIL_Remove( this );
			return;
		}

		vecStart = tr.endpos;
		if ( tr.fraction >= 1.0f )
			break;

		if ( tr.plane.normal.z >= KART_OIL_MAX_SLOPE )
		{
			SetAbsOrigin( vecStart );
			Land( tr );
			return;
		}

		flDelta *= 1.0f - tr.fraction;
		m_vecFlyVelocity -= DotProduct( m_vecFlyVelocity, tr.plane.normal ) * tr.plane.normal;
		m_vecFlyVelocity.x *= 0.5f;
		m_vecFlyVelocity.y *= 0.5f;
	}

	SetAbsOrigin( vecStart );
	DebugDraw();
}

void CKartHazardOil::Land( const trace_t &tr )
{
	// Lie flat on the ground, the yaw kept.
	Vector vecForward;
	AngleVectors( QAngle( 0, GetAbsAngles().y, 0 ), &vecForward );
	vecForward -= DotProduct( vecForward, tr.plane.normal ) * tr.plane.normal;
	if ( vecForward.NormalizeInPlace() < 0.01f )
	{
		vecForward = CrossProduct( Vector( 0, 1, 0 ), tr.plane.normal );
		vecForward.NormalizeInPlace();
	}
	QAngle angles;
	VectorAngles( vecForward, tr.plane.normal, angles );

	SetAbsOrigin( tr.endpos + tr.plane.normal * KART_OIL_GROUND_OFFSET );
	SetAbsAngles( angles );

	// Rides along on whatever it landed on (a moving platform).
	if ( tr.m_pEnt && !tr.m_pEnt->IsWorld() && tr.m_pEnt->GetMoveType() == MOVETYPE_PUSH )
	{
		SetParent( tr.m_pEnt );
	}

	m_vecFlyVelocity.Init();
	m_flLandTime = gpGlobals->curtime;
	m_flDieTime = gpGlobals->curtime + kart_oil_lifetime.GetFloat();
	EmitSound( "Kart.OilLand" );

	if ( kart_debug_server.GetBool() )
	{
		CHL2MP_Player *pDropper = GetDropper();
		Msg( "[kart] oil slick %d by %s landed at %.0f %.0f %.0f, %.0fs\n", entindex(), pDropper ? pDropper->GetPlayerName() : "nobody",
			GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z, kart_oil_lifetime.GetFloat() );
	}

	SetTouch( &CKartHazardOil::OilTouch );
	SetThink( &CKartHazardOil::LieThink );
	SetNextThink( gpGlobals->curtime + KART_OIL_LIE_THINK );
}

void CKartHazardOil::LieThink( void )
{
	if ( gpGlobals->curtime >= m_flDieTime )
	{
		UTIL_Remove( this );
		return;
	}

	SetNextThink( gpGlobals->curtime + KART_OIL_LIE_THINK );
	DebugDraw();
}

void CKartHazardOil::OilTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() || gpGlobals->curtime >= m_flDieTime - KART_OIL_FADE_TIME )
		return;

	CHL2MP_Player *pPlayer = ToHL2MPPlayer( pOther );
	if ( !pPlayer || !pPlayer->IsInKart() )
		return;

	CHL2MP_Player *pDropper = GetDropper();
	if ( pPlayer == pDropper && gpGlobals->curtime < m_flDropTime + KART_OIL_DROPPER_IMMUNE )
		return;

	// The trigger box is upright; on a slope only spin out karts down on the puddle.
	Vector vecUp;
	AngleVectors( GetAbsAngles(), NULL, NULL, &vecUp );
	if ( DotProduct( pPlayer->GetAbsOrigin() - GetAbsOrigin(), vecUp ) > KART_OIL_TRIGGER_HEIGHT )
		return;

	if ( pPlayer->KartApplyHit( KART_HIT_SPINOUT, pDropper ) && kart_debug_server.GetBool() )
	{
		Msg( "[kart] %s drove into oil slick %d\n", pPlayer->GetPlayerName(), entindex() );
	}
}

void CKartHazardOil::Expire( void )
{
	float flDieTime = gpGlobals->curtime + KART_OIL_FADE_TIME;
	if ( flDieTime >= m_flDieTime )
		return;

	m_flDieTime = flDieTime;
	if ( !m_flLandTime )
	{
		// Still flying: just gone.
		UTIL_Remove( this );
	}
}

void CKartHazardOil::DebugDraw( void )
{
	if ( !kart_debug_server.GetBool() || gpGlobals->curtime < m_flNextDebugDraw )
		return;

	m_flNextDebugDraw = gpGlobals->curtime + KART_OIL_DEBUG_DRAW;
	float flDuration = KART_OIL_DEBUG_DRAW + 0.05f;

	if ( m_flLandTime )
	{
		Vector vecMins, vecMaxs;
		CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
		NDebugOverlay::Box( vec3_origin, vecMins, vecMaxs, 40, 40, 40, 16, flDuration );
		NDebugOverlay::EntityTextAtPosition( GetAbsOrigin(), 0, CFmtStr( "oil (%.1fs)", m_flDieTime - gpGlobals->curtime ), flDuration );
	}
	else
	{
		NDebugOverlay::Box( GetAbsOrigin(), s_vecOilFlyMins, s_vecOilFlyMaxs, 40, 40, 40, 64, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The Oil Slick item. Backward drops it behind the kart, forward
//			throws it ahead in an arc. The kart's oldest slick goes when it
//			would have more than KART_OIL_MAX_PER_PLAYER down.
//-----------------------------------------------------------------------------
bool KartItemUse_Oil( CHL2MP_Player *pPlayer, bool bBackward )
{
	float flYaw = pPlayer->GetKartYaw();
	Vector vecForward;
	AngleVectors( QAngle( 0, flYaw, 0 ), &vecForward );

	Vector vecKartVelocity = pPlayer->GetAbsVelocity();
	vecKartVelocity.z = 0.0f;

	Vector vecTarget, vecVelocity;
	if ( bBackward )
	{
		// Dropped: it falls where it is let go, the kart drives away from it.
		vecTarget = pPlayer->GetAbsOrigin() - vecForward * KART_OIL_DROP_BACK + Vector( 0, 0, KART_OIL_START_HEIGHT );
		vecVelocity.Init();
	}
	else
	{
		vecTarget = pPlayer->GetAbsOrigin() + vecForward * KART_OIL_THROW_AHEAD + Vector( 0, 0, KART_OIL_START_HEIGHT );
		vecVelocity = vecKartVelocity + vecForward * kart_oil_throw_speed.GetFloat() + Vector( 0, 0, kart_oil_throw_up.GetFloat() );
	}

	// Not through a wall: start from the kart and stop short of anything in the way.
	Vector vecFrom = pPlayer->GetAbsOrigin() + Vector( 0, 0, KART_OIL_START_HEIGHT );
	trace_t tr;
	UTIL_TraceHull( vecFrom, vecTarget, s_vecOilFlyMins, s_vecOilFlyMaxs, MASK_PLAYERSOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.startsolid )
		return false;

	// Make room: the oldest of this kart's slicks goes.
	CUtlVector<CKartHazardOil *> slicks;
	for ( CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "kart_hazard_oil" ); pEnt; pEnt = gEntList.FindEntityByClassname( pEnt, "kart_hazard_oil" ) )
	{
		CKartHazardOil *pOil = static_cast<CKartHazardOil *>( pEnt );
		if ( pOil->GetDropper() == pPlayer && !pOil->IsMarkedForDeletion() && pOil->GetDieTime() > gpGlobals->curtime + KART_OIL_FADE_TIME )
		{
			slicks.AddToTail( pOil );
		}
	}
	while ( slicks.Count() >= KART_OIL_MAX_PER_PLAYER )
	{
		int iOldest = 0;
		for ( int i = 1; i < slicks.Count(); i++ )
		{
			if ( slicks[i]->GetDropTime() < slicks[iOldest]->GetDropTime() )
			{
				iOldest = i;
			}
		}
		slicks[iOldest]->Expire();
		slicks.Remove( iOldest );
	}

	CKartHazardOil *pOil = CKartHazardOil::Create( pPlayer, tr.endpos, flYaw, vecVelocity );
	if ( !pOil )
		return false;

	pPlayer->EmitSound( "Kart.OilThrow" );

	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] %s %s an oil slick (%d down)\n", pPlayer->GetPlayerName(), bBackward ? "dropped" : "threw", slicks.Count() + 1 );
	}
	return true;
}
