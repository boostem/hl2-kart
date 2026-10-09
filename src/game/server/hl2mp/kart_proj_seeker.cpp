//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: kart_proj_seeker. See kart_proj_seeker.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_proj_seeker.h"
#include "hl2mp_player.h"
#include "kart_items.h"
#include "kart_race_entities.h"
#include "movevars_shared.h"
#include "collisionutils.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar kart_debug_server;

ConVar kart_seeker_speed( "kart_seeker_speed", "1100", FCVAR_NOTIFY, "Speed of a Seeker, in units per second. Karts top out at kart_max_speed.", true, 0.0f, true, 5000.0f );
ConVar kart_seeker_turn_rate( "kart_seeker_turn_rate", "240", FCVAR_NOTIFY, "How fast a Seeker turns, in degrees per second.", true, 0.0f, true, 3600.0f );
ConVar kart_seeker_lock_range( "kart_seeker_lock_range", "700", FCVAR_NOTIFY, "Within this many units of its target, with a clear line to it, a Seeker leaves the track and steers straight at it.", true, 0.0f, true, 10000.0f );
ConVar kart_seeker_lifetime( "kart_seeker_lifetime", "12", FCVAR_NOTIFY, "Seconds a Seeker chases before it breaks.", true, 0.1f, true, 60.0f );

#define KART_SEEKER_SCALE			0.6f
#define KART_SEEKER_RADIUS			8.0f
#define KART_SEEKER_HALF_HEIGHT		6.0f
#define KART_SEEKER_HOVER			6.0f	// gap kept under it while on the ground
#define KART_SEEKER_STEP			12.0f	// climbs this much per move without it being a wall
#define KART_SEEKER_HUG				24.0f	// drops this much per move to stay on the track before it falls
#define KART_SEEKER_FLOOR_NORMAL	0.7f	// steeper surfaces are walls
#define KART_SEEKER_THROWER_SAFE	0.5f	// seconds the thrower can't be hit by it
#define KART_SEEKER_LAUNCH_GAP		4.0f	// between the kart's bounds and a new seeker
#define KART_SEEKER_LAUNCH_HEIGHT	16.0f	// above the kart's origin
#define KART_SEEKER_LOOKAHEAD		256.0f	// steers at the racing line this far ahead of itself
#define KART_SEEKER_LINE_WINDOW		512.0f	// looks for itself on the racing line this far either way
#define KART_SEEKER_RETARGET_TIME	0.5f	// seconds between looks for a target while it has none
#define KART_SEEKER_FREE_CONE		0.5f	// free drive: cosine of the cone a target is picked in (60 degrees)
#define KART_SEEKER_FREE_RANGE		3000.0f	// free drive: farthest a target is picked

#define KART_SEEKER_SOUND_LAUNCH	"Kart.SeekerLaunch"
#define KART_SEEKER_SOUND_LOOP		"Kart.SeekerLoop"
#define KART_SEEKER_SOUND_LOCK		"Kart.SeekerLock"
#define KART_SEEKER_SOUND_HIT		"Kart.SeekerHit"
#define KART_SEEKER_SOUND_EXPIRE	"Kart.SeekerExpire"

static const Vector s_vecSeekerMins( -KART_SEEKER_RADIUS, -KART_SEEKER_RADIUS, -KART_SEEKER_HALF_HEIGHT );
static const Vector s_vecSeekerMaxs( KART_SEEKER_RADIUS, KART_SEEKER_RADIUS, KART_SEEKER_HALF_HEIGHT );

// The world and props only: karts are checked by hand and projectiles pass
// through each other.
class CTraceFilterSeeker : public CTraceFilterSimple
{
public:
	CTraceFilterSeeker( const IHandleEntity *pPassEntity ) : CTraceFilterSimple( pPassEntity, COLLISION_GROUP_NONE ) {}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( pEntity && ( pEntity->IsPlayer() || !Q_strncmp( pEntity->GetClassname(), "kart_proj_", 10 ) ) )
			return false;

		return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

LINK_ENTITY_TO_CLASS( kart_proj_seeker, CKartProjSeeker );

BEGIN_DATADESC( CKartProjSeeker )
	DEFINE_FIELD( m_hThrower, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bLocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_flSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFallSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_bOnGround, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flLineDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_iWaypoint, FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextTargetTime, FIELD_TIME ),
	DEFINE_FIELD( m_flThrowerSafeTime, FIELD_TIME ),
	DEFINE_FIELD( m_flDieTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLastMoveTime, FIELD_TIME ),
	DEFINE_THINKFUNC( SeekerThink ),
END_DATADESC()

CKartProjSeeker::CKartProjSeeker()
{
	m_bLocked = false;
	m_flYaw = 0.0f;
	m_flSpeed = 0.0f;
	m_flFallSpeed = 0.0f;
	m_bOnGround = true;
	m_flLineDistance = -1.0f;
	m_iWaypoint = -1;
	m_flNextTargetTime = 0.0f;
	m_flThrowerSafeTime = 0.0f;
	m_flDieTime = 0.0f;
	m_flLastMoveTime = 0.0f;
}

void CKartProjSeeker::Precache( void )
{
	PrecacheModel( KART_SEEKER_MODEL );
	PrecacheScriptSound( KART_SEEKER_SOUND_LAUNCH );
	PrecacheScriptSound( KART_SEEKER_SOUND_LOOP );
	PrecacheScriptSound( KART_SEEKER_SOUND_LOCK );
	PrecacheScriptSound( KART_SEEKER_SOUND_HIT );
	PrecacheScriptSound( KART_SEEKER_SOUND_EXPIRE );
}

void CKartProjSeeker::Spawn( void )
{
	Precache();
	SetModel( KART_SEEKER_MODEL );
	SetModelScale( KART_SEEKER_SCALE );

	// Moved by hand in the think: no engine movement or collisions.
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	UTIL_SetSize( this, s_vecSeekerMins, s_vecSeekerMaxs );

	SetAbsAngles( QAngle( 0.0f, m_flYaw, 0.0f ) );

	m_bOnGround = true;
	m_flFallSpeed = 0.0f;
	m_flDieTime = gpGlobals->curtime + kart_seeker_lifetime.GetFloat();
	m_flLastMoveTime = gpGlobals->curtime;

	EmitSound( KART_SEEKER_SOUND_LOOP );

	SetThink( &CKartProjSeeker::SeekerThink );
	SetNextThink( gpGlobals->curtime );
}

void CKartProjSeeker::UpdateOnRemove( void )
{
	StopSound( KART_SEEKER_SOUND_LOOP );
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Launches a seeker from in front of the kart along its heading, and
//			picks what it chases.
//-----------------------------------------------------------------------------
bool CKartProjSeeker::Launch( CHL2MP_Player *pThrower )
{
	// Spinning out or stunned: kept for later.
	if ( pThrower->IsKartHit() )
		return false;

	float flYaw = pThrower->GetKartYaw();
	Vector vecForward;
	AngleVectors( QAngle( 0.0f, flYaw, 0.0f ), &vecForward );

	// Out of the kart's bounds, or as far as there is room for.
	Vector vecCenter = pThrower->GetAbsOrigin() + Vector( 0.0f, 0.0f, KART_SEEKER_LAUNCH_HEIGHT );
	float flOffset = pThrower->CollisionProp()->OBBSize().AsVector2D().Length() * 0.5f + KART_SEEKER_RADIUS + KART_SEEKER_LAUNCH_GAP;

	trace_t tr;
	CTraceFilterSeeker filter( pThrower );
	UTIL_TraceHull( vecCenter, vecCenter + vecForward * flOffset, s_vecSeekerMins, s_vecSeekerMaxs, MASK_SOLID, &filter, &tr );
	if ( tr.startsolid )
		return false;

	CKartProjSeeker *pSeeker = static_cast< CKartProjSeeker * >( CreateEntityByName( "kart_proj_seeker" ) );
	if ( !pSeeker )
		return false;

	pSeeker->SetAbsOrigin( tr.endpos );
	pSeeker->SetOwnerEntity( pThrower );
	pSeeker->m_hThrower = pThrower;
	pSeeker->m_flYaw = flYaw;
	pSeeker->m_flSpeed = MAX( kart_seeker_speed.GetFloat(), pThrower->GetAbsVelocity().Length2D() );
	pSeeker->m_flThrowerSafeTime = gpGlobals->curtime + KART_SEEKER_THROWER_SAFE;

	// Where it starts on the track: on the racing line next to the kart, or
	// heading for the kart's next checkpoint.
	CKartRaceManager *pManager = KartRaceManager();
	if ( pManager && pManager->HasRacingLine() )
	{
		KartRacingLinePoint_t point;
		if ( pManager->GetNearestRacingLinePoint( tr.endpos, point ) )
		{
			pSeeker->m_flLineDistance = point.distance;
		}
	}
	else if ( pManager && pManager->HasRoute() )
	{
		pSeeker->m_iWaypoint = MAX( pManager->GetRoutePosition( pThrower->GetKartNextCheckpoint() ), 0 );
	}

	pSeeker->m_hTarget = pSeeker->SelectTarget();
	pSeeker->m_flNextTargetTime = gpGlobals->curtime + KART_SEEKER_RETARGET_TIME;

	DispatchSpawn( pSeeker );

	Vector vecVelocity;
	AngleVectors( QAngle( 0.0f, flYaw, 0.0f ), &vecVelocity );
	pSeeker->SetAbsVelocity( vecVelocity * pSeeker->m_flSpeed );

	pThrower->EmitSound( KART_SEEKER_SOUND_LAUNCH );

	if ( kart_debug_server.GetBool() )
	{
		CHL2MP_Player *pTarget = pSeeker->m_hTarget.Get();
		Msg( "[kart] %s launched a seeker at %s, following the %s\n", pThrower->GetPlayerName(), pTarget ? pTarget->GetPlayerName() : "nobody",
			( pSeeker->m_flLineDistance >= 0.0f ) ? "racing line" : ( pSeeker->m_iWaypoint >= 0 ) ? "checkpoints" : "kart's heading" );
	}

	return true;
}

bool CKartProjSeeker::IsValidTarget( CHL2MP_Player *pKart ) const
{
	return pKart && pKart->IsInKart() && pKart->IsAlive() && !pKart->IsObserver() && pKart != m_hThrower.Get();
}

//-----------------------------------------------------------------------------
// Purpose: The kart one race position ahead of the thrower (the next one up
//			when that kart has finished and left the track); in free drive the
//			nearest kart in a cone ahead of the thrower. NULL for the leader.
//-----------------------------------------------------------------------------
CHL2MP_Player *CKartProjSeeker::SelectTarget( void ) const
{
	CHL2MP_Player *pThrower = m_hThrower.Get();
	if ( !pThrower )
		return NULL;

	int nPosition = pThrower->GetKartRacePosition();
	if ( nPosition == 1 )
		return NULL;

	Vector vecForward;
	AngleVectors( QAngle( 0.0f, pThrower->GetKartYaw(), 0.0f ), &vecForward );

	CHL2MP_Player *pBest = NULL;
	int nBestPosition = 0;
	float flBestDist = KART_SEEKER_FREE_RANGE;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pKart = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !IsValidTarget( pKart ) )
			continue;

		if ( nPosition > 1 )
		{
			int nKartPosition = pKart->GetKartRacePosition();
			if ( nKartPosition > nBestPosition && nKartPosition < nPosition )
			{
				nBestPosition = nKartPosition;
				pBest = pKart;
			}

			continue;
		}

		Vector vecTo = pKart->WorldSpaceCenter() - pThrower->WorldSpaceCenter();
		float flDist = VectorNormalize( vecTo );
		if ( flDist < flBestDist && DotProduct( vecTo, vecForward ) >= KART_SEEKER_FREE_CONE )
		{
			flBestDist = flDist;
			pBest = pKart;
		}
	}

	return pBest;
}

void CKartProjSeeker::SeekerThink( void )
{
	if ( gpGlobals->curtime >= m_flDieTime )
	{
		Break( KART_SEEKER_SOUND_EXPIRE, "expired" );
		return;
	}

	float flTime = gpGlobals->curtime - m_flLastMoveTime;
	m_flLastMoveTime = gpGlobals->curtime;

	// The target left the race: the kart ahead of the thrower now, if any.
	if ( !IsValidTarget( m_hTarget.Get() ) )
	{
		m_hTarget = NULL;
		if ( gpGlobals->curtime >= m_flNextTargetTime )
		{
			m_hTarget = SelectTarget();
			m_flNextTargetTime = gpGlobals->curtime + KART_SEEKER_RETARGET_TIME;
		}
	}

	Vector vecSteerTo;
	if ( !GetSteerPoint( vecSteerTo ) )
	{
		// Nothing to follow: straight on.
		Vector vecForward;
		AngleVectors( QAngle( 0.0f, m_flYaw, 0.0f ), &vecForward );
		vecSteerTo = GetAbsOrigin() + vecForward * KART_SEEKER_LOOKAHEAD;
	}

	// Move may break it.
	Move( flTime, vecSteerTo );
	if ( IsMarkedForDeletion() )
		return;

	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Where to steer: the target once it is locked on, else the track
//			ahead. False when there is neither.
//-----------------------------------------------------------------------------
bool CKartProjSeeker::GetSteerPoint( Vector &vecPoint )
{
	// Keeps its place on the track up to date even while locked on, so it
	// can go back to the track if it loses the target.
	Vector vecTrack;
	bool bTrack = GetTrackPoint( vecTrack );

	CHL2MP_Player *pTarget = m_hTarget.Get();
	bool bLocked = false;
	if ( pTarget )
	{
		Vector vecTarget = pTarget->WorldSpaceCenter();
		bLocked = !bTrack;
		if ( !bLocked && ( vecTarget - GetAbsOrigin() ).LengthSqr() < Square( kart_seeker_lock_range.GetFloat() ) )
		{
			trace_t tr;
			CTraceFilterSeeker filter( this );
			UTIL_TraceLine( GetAbsOrigin(), vecTarget, MASK_SOLID, &filter, &tr );
			bLocked = ( tr.fraction >= 1.0f );
		}

		if ( bLocked )
		{
			vecPoint = vecTarget;
		}
	}

	if ( bLocked && !m_bLocked )
	{
		EmitSound( KART_SEEKER_SOUND_LOCK );
		if ( kart_debug_server.GetBool() )
		{
			Msg( "[kart] seeker %d locked on %s\n", entindex(), pTarget->GetPlayerName() );
		}
	}
	m_bLocked = bLocked;

	if ( bLocked )
		return true;

	vecPoint = vecTrack;
	return bTrack;
}

//-----------------------------------------------------------------------------
// Purpose: The point on the track it heads for: KART_SEEKER_LOOKAHEAD along
//			the racing line, or the center of the next checkpoint it hasn't
//			passed. False on a map with neither.
//-----------------------------------------------------------------------------
bool CKartProjSeeker::GetTrackPoint( Vector &vecPoint )
{
	CKartRaceManager *pManager = KartRaceManager();
	if ( !pManager )
		return false;

	const Vector vecPos = GetAbsOrigin();

	if ( m_flLineDistance >= 0.0f && pManager->HasRacingLine() )
	{
		const CKartRacingLine &line = pManager->GetRacingLine();
		KartRacingLinePoint_t point;
		if ( !line.GetNearestPointNear( vecPos, m_flLineDistance, KART_SEEKER_LINE_WINDOW, point ) )
			return false;

		m_flLineDistance = point.distance;
		if ( !line.GetPoint( m_flLineDistance + KART_SEEKER_LOOKAHEAD, point ) )
			return false;

		vecPoint = point.pos;
		return true;
	}

	if ( m_iWaypoint >= 0 && pManager->HasRoute() )
	{
		// Passed when it is in the checkpoint's trigger or beyond its plane,
		// facing along the segment leading to it.
		const int nCount = pManager->GetRouteCount();
		for ( int i = 0; i < nCount; i++ )
		{
			m_iWaypoint %= nCount;
			const Vector &vecCenter = pManager->GetRouteCenter( m_iWaypoint );
			const Vector &vecIn = pManager->GetRouteDir( ( m_iWaypoint + nCount - 1 ) % nCount );
			bool bPassed = DotProduct( vecPos - vecCenter, vecIn ) >= 0.0f;

			CKartCheckpoint *pTrigger = pManager->GetRouteTrigger( m_iWaypoint );
			if ( !bPassed && pTrigger )
			{
				Vector vecMins, vecMaxs;
				pTrigger->CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
				bPassed = IsBoxIntersectingBox( vecMins, vecMaxs, vecPos + s_vecSeekerMins, vecPos + s_vecSeekerMaxs );
			}

			if ( !bPassed )
				break;

			m_iWaypoint++;
		}

		vecPoint = pManager->GetRouteCenter( m_iWaypoint % pManager->GetRouteCount() );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: One step of flTime seconds: turns toward vecSteerTo, skims, climbs,
//			falls, slides along walls, and hits the first kart in the way.
//-----------------------------------------------------------------------------
void CKartProjSeeker::Move( float flTime, const Vector &vecSteerTo )
{
	CTraceFilterSeeker filter( this );
	trace_t tr;

	const Vector vecStart = GetAbsOrigin();
	Vector vecPos = vecStart;

	Vector vecTo = vecSteerTo - vecStart;
	if ( vecTo.AsVector2D().LengthSqr() > 1.0f )
	{
		m_flYaw = ApproachAngle( UTIL_VecToYaw( vecTo ), m_flYaw, kart_seeker_turn_rate.GetFloat() * flTime );
	}

	Vector vecForward;
	AngleVectors( QAngle( 0.0f, m_flYaw, 0.0f ), &vecForward );
	Vector vecVel = vecForward * m_flSpeed;

	if ( !m_bOnGround )
	{
		m_flFallSpeed += sv_gravity.GetFloat() * flTime;
		vecVel.z = -m_flFallSpeed;
	}

	// A few passes, so a wall or a landing doesn't eat the rest of the step.
	float flLeft = flTime;
	for ( int i = 0; i < 4 && flLeft > 0.0f; i++ )
	{
		Vector vecFrom = vecPos;
		if ( m_bOnGround )
		{
			vecFrom.z += KART_SEEKER_STEP;
		}
		UTIL_TraceHull( vecFrom, vecFrom + vecVel * flLeft, s_vecSeekerMins, s_vecSeekerMaxs, MASK_SOLID, &filter, &tr );

		// No room for the step (a low ceiling): straight along instead.
		if ( tr.startsolid && m_bOnGround )
		{
			vecFrom = vecPos;
			UTIL_TraceHull( vecFrom, vecFrom + vecVel * flLeft, s_vecSeekerMins, s_vecSeekerMaxs, MASK_SOLID, &filter, &tr );
		}

		if ( tr.allsolid )
		{
			Break( KART_SEEKER_SOUND_EXPIRE, "stuck" );
			return;
		}

		vecPos = tr.endpos;
		flLeft *= 1.0f - tr.fraction;
		if ( tr.fraction >= 1.0f )
			break;

		if ( tr.plane.normal.z >= KART_SEEKER_FLOOR_NORMAL )
		{
			// A floor: lands on it, or a ramp too steep for one step that the
			// next pass climbs from here.
			m_bOnGround = true;
			m_flFallSpeed = 0.0f;
			vecVel.z = 0.0f;
		}
		else if ( tr.plane.normal.z <= -KART_SEEKER_FLOOR_NORMAL )
		{
			// A ceiling.
			vecVel.z = MIN( vecVel.z, 0.0f );
		}
		else
		{
			// A wall: slides along it; the steering turns it away.
			Vector vecNormal( tr.plane.normal.x, tr.plane.normal.y, 0.0f );
			VectorNormalize( vecNormal );
			vecVel -= MIN( DotProduct( vecVel, vecNormal ), 0.0f ) * vecNormal;
		}
	}

	// Hugs the track: down onto the floor below, or falls off the edge.
	if ( m_bOnGround )
	{
		// From wherever the passes' steps raised it to a little under where it started.
		Vector vecDown( vecPos.x, vecPos.y, MIN( vecPos.z, vecStart.z ) - KART_SEEKER_HUG );
		UTIL_TraceHull( vecPos, vecDown, s_vecSeekerMins, s_vecSeekerMaxs, MASK_SOLID, &filter, &tr );
		if ( !tr.startsolid && tr.fraction < 1.0f && tr.plane.normal.z >= KART_SEEKER_FLOOR_NORMAL )
		{
			// Up to its hover height, as far as there is room.
			Vector vecFloor = tr.endpos;
			UTIL_TraceHull( vecFloor, vecFloor + Vector( 0.0f, 0.0f, KART_SEEKER_HOVER ), s_vecSeekerMins, s_vecSeekerMaxs, MASK_SOLID, &filter, &tr );
			vecPos = tr.endpos;
		}
		else
		{
			m_bOnGround = false;
			m_flFallSpeed = 0.0f;
		}
	}

	CHL2MP_Player *pKart = FindKartHit( vecStart, vecPos );

	SetAbsOrigin( vecPos );
	SetAbsVelocity( vecVel );
	SetAbsAngles( QAngle( 0.0f, m_flYaw, 0.0f ) );

	if ( kart_debug_server.GetBool() )
	{
		NDebugOverlay::Line( vecStart, vecPos, 0, 200, 255, true, 2.0f );
		NDebugOverlay::Line( vecPos, vecSteerTo, m_bLocked ? 255 : 0, m_bLocked ? 0 : 255, 0, true, 0.1f );
	}

	if ( pKart )
	{
		// Spent on the kart even when it can't be hit right now (immune, or
		// its buffer takes it).
		CBaseEntity *pThrower = m_hThrower.Get();
		pKart->KartApplyHit( KART_HIT_STUN, pThrower );
		Break( KART_SEEKER_SOUND_HIT, pKart->GetPlayerName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The first kart whose bounds the seeker swept from vecStart to vecEnd.
//-----------------------------------------------------------------------------
CHL2MP_Player *CKartProjSeeker::FindKartHit( const Vector &vecStart, const Vector &vecEnd ) const
{
	CHL2MP_Player *pBest = NULL;
	float flBestDistSqr = FLT_MAX;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsInKart() || !pPlayer->IsAlive() || pPlayer->IsObserver() )
			continue;

		if ( pPlayer == m_hThrower.Get() && gpGlobals->curtime < m_flThrowerSafeTime )
			continue;

		Vector vecMins, vecMaxs;
		pPlayer->CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
		vecMins += s_vecSeekerMins;
		vecMaxs += s_vecSeekerMaxs;
		if ( !IsBoxIntersectingRay( vecMins, vecMaxs, vecStart, vecEnd - vecStart ) )
			continue;

		float flDistSqr = ( pPlayer->WorldSpaceCenter() - vecStart ).LengthSqr();
		if ( flDistSqr < flBestDistSqr )
		{
			flBestDistSqr = flDistSqr;
			pBest = pPlayer;
		}
	}

	return pBest;
}

void CKartProjSeeker::Break( const char *pszSound, const char *pszReason )
{
	EmitSound( pszSound );
	g_pEffects->Sparks( GetAbsOrigin() );

	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] seeker %d broke: %s\n", entindex(), pszReason );
	}

	SetThink( NULL );
	UTIL_Remove( this );
}
