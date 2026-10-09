//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: kart_proj_hubcap. See kart_proj_hubcap.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_proj_hubcap.h"
#include "hl2mp_player.h"
#include "kart_items.h"
#include "movevars_shared.h"
#include "collisionutils.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar kart_debug_server;

ConVar kart_hubcap_speed( "kart_hubcap_speed", "900", FCVAR_NOTIFY, "Speed of a thrown Hubcap, in units per second, on top of the thrower's own speed.", true, 0.0f, true, 5000.0f );
ConVar kart_hubcap_bounces( "kart_hubcap_bounces", "3", FCVAR_NOTIFY, "Wall bounces a Hubcap survives; the next wall breaks it.", true, 0.0f, true, 20.0f );
ConVar kart_hubcap_lifetime( "kart_hubcap_lifetime", "5", FCVAR_NOTIFY, "Seconds a Hubcap skims along before it breaks.", true, 0.1f, true, 60.0f );
ConVar kart_hubcap_bounce_scale( "kart_hubcap_bounce_scale", "0.9", FCVAR_NOTIFY, "Speed a Hubcap keeps on each wall bounce.", true, 0.0f, true, 1.0f );

#define KART_HUBCAP_SCALE			0.5f
#define KART_HUBCAP_RADIUS			9.0f	// the scaled wheel lies flat: ~17.5 * 0.5 across, ~5.5 * 0.5 thick
#define KART_HUBCAP_HALF_HEIGHT		3.0f
#define KART_HUBCAP_HOVER			1.0f	// gap kept under it while skimming
#define KART_HUBCAP_STEP			12.0f	// climbs this much per move without it being a wall
#define KART_HUBCAP_HUG				24.0f	// drops this much per move to stay on the track before it falls
#define KART_HUBCAP_FLOOR_NORMAL	0.7f	// steeper surfaces are walls
#define KART_HUBCAP_SPIN			1080.0f	// yaw, degrees per second
#define KART_HUBCAP_THROWER_SAFE	0.5f	// seconds the thrower can't be hit by it
#define KART_HUBCAP_MIN_SPEED_SCALE	0.5f	// a throw goes at least this much of kart_hubcap_speed
#define KART_HUBCAP_THROW_GAP		4.0f	// between the kart's bounds and a new hubcap
#define KART_HUBCAP_THROW_HEIGHT	16.0f	// above the kart's origin

#define KART_HUBCAP_SOUND_THROW		"Kart.HubcapThrow"
#define KART_HUBCAP_SOUND_BOUNCE	"Kart.HubcapBounce"
#define KART_HUBCAP_SOUND_BREAK		"Kart.HubcapBreak"

static const Vector s_vecHubcapMins( -KART_HUBCAP_RADIUS, -KART_HUBCAP_RADIUS, -KART_HUBCAP_HALF_HEIGHT );
static const Vector s_vecHubcapMaxs( KART_HUBCAP_RADIUS, KART_HUBCAP_RADIUS, KART_HUBCAP_HALF_HEIGHT );

// The world and props only: karts are checked by hand and projectiles pass
// through each other.
class CTraceFilterHubcap : public CTraceFilterSimple
{
public:
	CTraceFilterHubcap( const IHandleEntity *pPassEntity ) : CTraceFilterSimple( pPassEntity, COLLISION_GROUP_NONE ) {}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( pEntity && ( pEntity->IsPlayer() || !Q_strncmp( pEntity->GetClassname(), "kart_proj_", 10 ) ) )
			return false;

		return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

LINK_ENTITY_TO_CLASS( kart_proj_hubcap, CKartProjHubcap );

BEGIN_DATADESC( CKartProjHubcap )
	DEFINE_FIELD( m_hThrower, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_bOnGround, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nBounces, FIELD_INTEGER ),
	DEFINE_FIELD( m_flThrowerSafeTime, FIELD_TIME ),
	DEFINE_FIELD( m_flDieTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLastMoveTime, FIELD_TIME ),
	DEFINE_THINKFUNC( HubcapThink ),
END_DATADESC()

CKartProjHubcap::CKartProjHubcap()
{
	m_vecVelocity.Init();
	m_bOnGround = true;
	m_nBounces = 0;
	m_flThrowerSafeTime = 0.0f;
	m_flDieTime = 0.0f;
	m_flLastMoveTime = 0.0f;
}

void CKartProjHubcap::Precache( void )
{
	PrecacheModel( KART_HUBCAP_MODEL );
	PrecacheScriptSound( KART_HUBCAP_SOUND_THROW );
	PrecacheScriptSound( KART_HUBCAP_SOUND_BOUNCE );
	PrecacheScriptSound( KART_HUBCAP_SOUND_BREAK );
}

void CKartProjHubcap::Spawn( void )
{
	Precache();
	SetModel( KART_HUBCAP_MODEL );
	SetModelScale( KART_HUBCAP_SCALE );

	// Moved by hand in the think: no engine movement or collisions.
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	UTIL_SetSize( this, s_vecHubcapMins, s_vecHubcapMaxs );

	// The wheel's axle is its Y axis: rolled 90 it lies flat, and the yaw
	// spins it like a thrown disc.
	SetAbsAngles( QAngle( 0.0f, RandomFloat( 0.0f, 360.0f ), 90.0f ) );

	m_bOnGround = true;
	m_nBounces = 0;
	m_flDieTime = gpGlobals->curtime + kart_hubcap_lifetime.GetFloat();
	m_flLastMoveTime = gpGlobals->curtime;

	SetThink( &CKartProjHubcap::HubcapThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Throws a hubcap from in front of (or behind) the kart along its
//			heading, at kart_hubcap_speed plus the kart's own speed that way.
//-----------------------------------------------------------------------------
bool CKartProjHubcap::Throw( CHL2MP_Player *pThrower, bool bBackward )
{
	// Spinning out or stunned: the throw would go anywhere. Kept for later.
	if ( pThrower->IsKartHit() )
		return false;

	Vector vecForward;
	AngleVectors( QAngle( 0.0f, pThrower->GetKartYaw(), 0.0f ), &vecForward );
	Vector vecDir = bBackward ? -vecForward : vecForward;

	Vector vecKartVel = pThrower->GetAbsVelocity();
	vecKartVel.z = 0.0f;
	float flBaseSpeed = kart_hubcap_speed.GetFloat();
	float flSpeed = MAX( flBaseSpeed + DotProduct( vecKartVel, vecDir ), flBaseSpeed * KART_HUBCAP_MIN_SPEED_SCALE );

	// Out of the kart's bounds, or as far as there is room for.
	Vector vecCenter = pThrower->GetAbsOrigin() + Vector( 0.0f, 0.0f, KART_HUBCAP_THROW_HEIGHT );
	float flOffset = pThrower->CollisionProp()->OBBSize().AsVector2D().Length() * 0.5f + KART_HUBCAP_RADIUS + KART_HUBCAP_THROW_GAP;

	trace_t tr;
	CTraceFilterHubcap filter( pThrower );
	UTIL_TraceHull( vecCenter, vecCenter + vecDir * flOffset, s_vecHubcapMins, s_vecHubcapMaxs, MASK_SOLID, &filter, &tr );
	if ( tr.startsolid )
		return false;

	CKartProjHubcap *pHubcap = static_cast< CKartProjHubcap * >( CreateEntityByName( "kart_proj_hubcap" ) );
	if ( !pHubcap )
		return false;

	pHubcap->SetAbsOrigin( tr.endpos );
	pHubcap->SetOwnerEntity( pThrower );
	pHubcap->m_hThrower = pThrower;
	pHubcap->m_vecVelocity = vecDir * flSpeed;
	pHubcap->m_flThrowerSafeTime = gpGlobals->curtime + KART_HUBCAP_THROWER_SAFE;
	DispatchSpawn( pHubcap );
	pHubcap->SetAbsVelocity( pHubcap->m_vecVelocity );

	pThrower->EmitSound( KART_HUBCAP_SOUND_THROW );

	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] %s threw a hubcap %s at %.0f u/s\n", pThrower->GetPlayerName(), bBackward ? "backward" : "forward", flSpeed );
	}

	return true;
}

void CKartProjHubcap::HubcapThink( void )
{
	if ( gpGlobals->curtime >= m_flDieTime )
	{
		Break( "expired" );
		return;
	}

	float flTime = gpGlobals->curtime - m_flLastMoveTime;
	m_flLastMoveTime = gpGlobals->curtime;

	// Move may break it.
	Move( flTime );
	if ( IsMarkedForDeletion() )
		return;

	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: One step of flTime seconds: skims, climbs, falls, bounces, and hits
//			the first kart in the way.
//-----------------------------------------------------------------------------
void CKartProjHubcap::Move( float flTime )
{
	CTraceFilterHubcap filter( this );
	trace_t tr;

	const Vector vecStart = GetAbsOrigin();
	Vector vecPos = vecStart;
	Vector vecVel = m_vecVelocity;

	if ( m_bOnGround )
	{
		vecVel.z = 0.0f;
	}
	else
	{
		vecVel.z -= sv_gravity.GetFloat() * flTime;
	}

	// A few passes, so a wall or a landing doesn't eat the rest of the step.
	float flLeft = flTime;
	for ( int i = 0; i < 4 && flLeft > 0.0f; i++ )
	{
		Vector vecFrom = vecPos;
		if ( m_bOnGround )
		{
			vecFrom.z += KART_HUBCAP_STEP;
		}
		UTIL_TraceHull( vecFrom, vecFrom + vecVel * flLeft, s_vecHubcapMins, s_vecHubcapMaxs, MASK_SOLID, &filter, &tr );

		// No room for the step (a low ceiling): straight along instead.
		if ( tr.startsolid && m_bOnGround )
		{
			vecFrom = vecPos;
			UTIL_TraceHull( vecFrom, vecFrom + vecVel * flLeft, s_vecHubcapMins, s_vecHubcapMaxs, MASK_SOLID, &filter, &tr );
		}

		if ( tr.allsolid )
		{
			Break( "stuck" );
			return;
		}

		vecPos = tr.endpos;
		flLeft *= 1.0f - tr.fraction;
		if ( tr.fraction >= 1.0f )
			break;

		if ( tr.plane.normal.z >= KART_HUBCAP_FLOOR_NORMAL )
		{
			// A floor: lands on it, or a ramp too steep for one step that the
			// next pass climbs from here.
			m_bOnGround = true;
			vecVel.z = 0.0f;
		}
		else if ( tr.plane.normal.z <= -KART_HUBCAP_FLOOR_NORMAL )
		{
			// A ceiling.
			vecVel.z = MIN( vecVel.z, 0.0f );
		}
		else
		{
			// A wall: off it horizontally.
			Vector vecNormal( tr.plane.normal.x, tr.plane.normal.y, 0.0f );
			VectorNormalize( vecNormal );
			vecVel -= 2.0f * DotProduct( vecVel, vecNormal ) * vecNormal;
			vecVel *= kart_hubcap_bounce_scale.GetFloat();
			Bounce( vecNormal, vecPos );
			if ( IsMarkedForDeletion() )
				return;
		}
	}

	// Hugs the track: down onto the floor below, or falls off the edge.
	if ( m_bOnGround )
	{
		// From wherever the passes' steps raised it to a little under where it started.
		Vector vecDown( vecPos.x, vecPos.y, MIN( vecPos.z, vecStart.z ) - KART_HUBCAP_HUG );
		UTIL_TraceHull( vecPos, vecDown, s_vecHubcapMins, s_vecHubcapMaxs, MASK_SOLID, &filter, &tr );
		if ( !tr.startsolid && tr.fraction < 1.0f && tr.plane.normal.z >= KART_HUBCAP_FLOOR_NORMAL )
		{
			vecPos = tr.endpos + Vector( 0.0f, 0.0f, KART_HUBCAP_HOVER );
		}
		else
		{
			m_bOnGround = false;
		}
	}

	CHL2MP_Player *pKart = FindKartHit( vecStart, vecPos );

	SetAbsOrigin( vecPos );
	m_vecVelocity = vecVel;
	SetAbsVelocity( vecVel );

	QAngle angles = GetAbsAngles();
	angles.y = anglemod( angles.y + KART_HUBCAP_SPIN * flTime );
	SetAbsAngles( angles );

	if ( kart_debug_server.GetBool() )
	{
		NDebugOverlay::Line( vecStart, vecPos, 255, 160, 0, true, 2.0f );
	}

	if ( pKart )
	{
		// Spent on the kart even when it can't be hit right now.
		CBaseEntity *pThrower = m_hThrower.Get();
		pKart->KartApplyHit( KART_HIT_SPINOUT, pThrower );
		Break( pKart->GetPlayerName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The first kart whose bounds the hubcap swept from vecStart to vecEnd.
//-----------------------------------------------------------------------------
CHL2MP_Player *CKartProjHubcap::FindKartHit( const Vector &vecStart, const Vector &vecEnd ) const
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
		vecMins += s_vecHubcapMins;
		vecMaxs += s_vecHubcapMaxs;
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

void CKartProjHubcap::Bounce( const Vector &vecNormal, const Vector &vecPos )
{
	m_nBounces++;
	if ( m_nBounces > kart_hubcap_bounces.GetInt() )
	{
		SetAbsOrigin( vecPos );
		Break( "out of bounces" );
		return;
	}

	EmitSound( KART_HUBCAP_SOUND_BOUNCE );
	g_pEffects->MetalSparks( vecPos - vecNormal * KART_HUBCAP_RADIUS, vecNormal );

	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] hubcap %d: bounce %d of %d\n", entindex(), m_nBounces, kart_hubcap_bounces.GetInt() );
	}
}

void CKartProjHubcap::Break( const char *pszReason )
{
	EmitSound( KART_HUBCAP_SOUND_BREAK );
	g_pEffects->Sparks( GetAbsOrigin() );

	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] hubcap %d broke: %s\n", entindex(), pszReason );
	}

	SetThink( NULL );
	UTIL_Remove( this );
}
