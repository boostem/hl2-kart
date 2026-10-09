//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart race map entities. See kart_race_entities.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_race_entities.h"
#include "hl2mp_player.h"
#include "kart_shareddefs.h"
#include "igamesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar kart_debug_server( "kart_debug_server", "0", 0, "Draw kart race checkpoints, the finish line and grid slots with debug overlays, and log checkpoint touches." );

// How often the debug overlays are redrawn, in seconds.
#define KART_DEBUG_DRAW_INTERVAL	0.25f

static CKartRaceManager *g_pKartRaceManager = NULL;

CKartRaceManager *KartRaceManager( void )
{
	return g_pKartRaceManager;
}

//-----------------------------------------------------------------------------
// Map queries shared by the manager, spawn selection, validation and debug.
//-----------------------------------------------------------------------------
static int CheckpointSortFunc( CKartCheckpoint * const *a, CKartCheckpoint * const *b )
{
	if ( (*a)->GetIndex() != (*b)->GetIndex() )
		return (*a)->GetIndex() - (*b)->GetIndex();
	return (*a)->entindex() - (*b)->entindex();
}

static int StartSortFunc( CKartStart * const *a, CKartStart * const *b )
{
	if ( (*a)->GetGrid() != (*b)->GetGrid() )
		return (*a)->GetGrid() - (*b)->GetGrid();
	return (*a)->entindex() - (*b)->entindex();
}

// Every kart_finish and kart_checkpoint on the map, sorted by index.
static void GatherCheckpoints( CUtlVector< CKartCheckpoint * > &list )
{
	list.RemoveAll();

	static const char *s_pszClasses[] = { "kart_finish", "kart_checkpoint" };
	for ( int i = 0; i < ARRAYSIZE( s_pszClasses ); i++ )
	{
		CBaseEntity *pEnt = NULL;
		while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, s_pszClasses[i] ) ) != NULL )
		{
			list.AddToTail( static_cast< CKartCheckpoint * >( pEnt ) );
		}
	}

	list.Sort( CheckpointSortFunc );
}

// Every kart_start on the map, sorted by grid slot.
static void GatherStarts( CUtlVector< CKartStart * > &list )
{
	list.RemoveAll();

	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "kart_start" ) ) != NULL )
	{
		list.AddToTail( static_cast< CKartStart * >( pEnt ) );
	}

	list.Sort( StartSortFunc );
}

// ##################################################################################
//	>> kart_checkpoint / kart_finish
// ##################################################################################
LINK_ENTITY_TO_CLASS( kart_checkpoint, CKartCheckpoint );
LINK_ENTITY_TO_CLASS( kart_finish, CKartFinish );

BEGIN_DATADESC( CKartCheckpoint )
	DEFINE_KEYFIELD( m_iIndex, FIELD_INTEGER, "index" ),
END_DATADESC()

CKartCheckpoint::CKartCheckpoint()
{
	m_iIndex = -1;
}

void CKartCheckpoint::Spawn( void )
{
	// Only players pass; StartTouch narrows that down to karts.
	AddSpawnFlags( SF_TRIGGER_ALLOW_CLIENTS );

	BaseClass::Spawn();

	InitTrigger();
}

void CKartCheckpoint::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	if ( !pOther->IsPlayer() || !PassesTriggerFilters( pOther ) )
		return;

	CHL2MP_Player *pPlayer = ToHL2MPPlayer( pOther );
	if ( !pPlayer || !pPlayer->IsInKart() || !pPlayer->IsAlive() )
		return;

	if ( KartRaceManager() )
	{
		KartRaceManager()->OnKartTouchedCheckpoint( pPlayer, m_iIndex );
	}
}

void CKartFinish::Spawn( void )
{
	m_iIndex = KART_FINISH_INDEX;

	BaseClass::Spawn();
}

// ##################################################################################
//	>> kart_start
// ##################################################################################
LINK_ENTITY_TO_CLASS( kart_start, CKartStart );

BEGIN_DATADESC( CKartStart )
	DEFINE_KEYFIELD( m_iGrid, FIELD_INTEGER, "grid" ),
END_DATADESC()

CKartStart::CKartStart()
{
	m_iGrid = 0;
}

// True when no other live kart overlaps a kart hull standing on this slot.
static bool IsGridSlotFree( CKartStart *pStart, CHL2MP_Player *pPlayer )
{
	const Vector vecSize = KART_HULL_MAX - KART_HULL_MIN;
	const Vector &vecSpot = pStart->GetAbsOrigin();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pOther = UTIL_PlayerByIndex( i );
		if ( !pOther || pOther == pPlayer || !pOther->IsAlive() || pOther->IsObserver() )
			continue;

		Vector vecDelta = pOther->GetAbsOrigin() - vecSpot;
		if ( fabsf( vecDelta.x ) < vecSize.x && fabsf( vecDelta.y ) < vecSize.y && fabsf( vecDelta.z ) < vecSize.z )
			return false;
	}

	return true;
}

CBaseEntity *KartRace_SelectGridSpawn( CHL2MP_Player *pPlayer )
{
	CUtlVector< CKartStart * > starts;
	GatherStarts( starts );

	for ( int i = 0; i < starts.Count(); i++ )
	{
		if ( IsGridSlotFree( starts[i], pPlayer ) )
			return starts[i];
	}

	if ( starts.Count() )
	{
		Warning( "[kart] All %d kart_start slots are taken, spawning %s on a deathmatch spawn.\n", starts.Count(), pPlayer->GetPlayerName() );
	}

	return NULL;
}

// ##################################################################################
//	>> kart_race_manager
// ##################################################################################
LINK_ENTITY_TO_CLASS( kart_race_manager, CKartRaceManager );

BEGIN_DATADESC( CKartRaceManager )
	DEFINE_KEYFIELD( m_iLaps, FIELD_INTEGER, "laps" ),
	DEFINE_KEYFIELD( m_iszTrackName, FIELD_STRING, "track_name" ),

	DEFINE_OUTPUT( m_OnRaceStart, "OnRaceStart" ),
	DEFINE_OUTPUT( m_OnRaceFinish, "OnRaceFinish" ),
END_DATADESC()

CKartRaceManager::CKartRaceManager()
{
	m_iLaps = 3;
	m_iszTrackName = NULL_STRING;
}

CKartRaceManager::~CKartRaceManager()
{
	if ( g_pKartRaceManager == this )
	{
		g_pKartRaceManager = NULL;
	}
}

void CKartRaceManager::Spawn( void )
{
	BaseClass::Spawn();

	if ( g_pKartRaceManager && g_pKartRaceManager != this )
	{
		Warning( "[kart] More than one kart_race_manager on the map; removing the extra one ('%s').\n", GetDebugName() );
		UTIL_Remove( this );
		return;
	}

	g_pKartRaceManager = this;

	if ( m_iLaps < 1 )
	{
		Warning( "[kart] kart_race_manager laps %d is invalid, using 1.\n", m_iLaps );
		m_iLaps = 1;
	}
}

void CKartRaceManager::Activate( void )
{
	BaseClass::Activate();

	if ( g_pKartRaceManager != this )
		return;

	// Every map entity has spawned by now (on map load and for ent_create alike).
	CollectCheckpoints();
	KartRace_ValidateMap();
}

void CKartRaceManager::CollectCheckpoints( void )
{
	CUtlVector< CKartCheckpoint * > list;
	GatherCheckpoints( list );

	m_Checkpoints.RemoveAll();
	for ( int i = 0; i < list.Count(); i++ )
	{
		m_Checkpoints.AddToTail( list[i] );
	}
}

CKartCheckpoint *CKartRaceManager::GetCheckpoint( int i ) const
{
	if ( i < 0 || i >= m_Checkpoints.Count() )
		return NULL;

	return m_Checkpoints[i].Get();
}

void CKartRaceManager::OnKartTouchedCheckpoint( CHL2MP_Player *pPlayer, int index )
{
	// Lap logic comes with the next ticket; log the touch for now.
	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] %s touched checkpoint %d at %.2f\n", pPlayer->GetPlayerName(), index, gpGlobals->curtime );
	}
	else
	{
		DevMsg( "[kart] %s touched checkpoint %d at %.2f\n", pPlayer->GetPlayerName(), index, gpGlobals->curtime );
	}
}

// ##################################################################################
//	>> Validation
// ##################################################################################
void KartRace_ValidateMap( void )
{
	CUtlVector< CKartCheckpoint * > checkpoints;
	CUtlVector< CKartStart * > starts;
	GatherCheckpoints( checkpoints );
	GatherStarts( starts );

	// No race entities at all: a free-drive map, nothing to say.
	if ( !KartRaceManager() && !checkpoints.Count() && !starts.Count() )
		return;

	if ( !KartRaceManager() )
	{
		Warning( "[kart] Map has race entities but no kart_race_manager: no race will run.\n" );
	}

	int nFinish = 0;
	int iExpected = 1;
	for ( int i = 0; i < checkpoints.Count(); i++ )
	{
		int index = checkpoints[i]->GetIndex();
		if ( index == KART_FINISH_INDEX )
		{
			if ( FClassnameIs( checkpoints[i], "kart_finish" ) )
			{
				nFinish++;
			}
			else
			{
				Warning( "[kart] kart_checkpoint '%s' has index 0, which is the kart_finish's; checkpoints start at 1.\n", checkpoints[i]->GetDebugName() );
			}
			continue;
		}

		if ( index < 0 )
		{
			Warning( "[kart] kart_checkpoint '%s' has no valid index (%d); checkpoints start at 1.\n", checkpoints[i]->GetDebugName(), index );
			continue;
		}

		if ( index < iExpected )
		{
			Warning( "[kart] Duplicate kart_checkpoint index %d ('%s').\n", index, checkpoints[i]->GetDebugName() );
			continue;
		}

		if ( index > iExpected )
		{
			if ( index == iExpected + 1 )
				Warning( "[kart] Gap in kart_checkpoint indices: %d is missing.\n", iExpected );
			else
				Warning( "[kart] Gap in kart_checkpoint indices: %d to %d are missing.\n", iExpected, index - 1 );
		}

		iExpected = index + 1;
	}

	if ( nFinish == 0 )
	{
		Warning( "[kart] Map has no kart_finish: laps can't be counted.\n" );
	}
	else if ( nFinish > 1 )
	{
		Warning( "[kart] Map has %d kart_finish; only one start/finish line is supported.\n", nFinish );
	}

	for ( int i = 1; i < starts.Count(); i++ )
	{
		if ( starts[i]->GetGrid() == starts[i - 1]->GetGrid() )
		{
			Warning( "[kart] Duplicate kart_start grid slot %d.\n", starts[i]->GetGrid() );
		}
	}

	if ( starts.Count() < gpGlobals->maxClients )
	{
		Warning( "[kart] Map has %d kart_start for %d maxplayers; the rest spawn on deathmatch spawns.\n", starts.Count(), gpGlobals->maxClients );
	}
}

// ##################################################################################
//	>> Level hooks and debug drawing
// ##################################################################################
class CKartRaceSystem : public CAutoGameSystemPerFrame
{
public:
	CKartRaceSystem() : CAutoGameSystemPerFrame( "CKartRaceSystem" )
	{
		m_flNextDebugDraw = 0.0f;
	}

	virtual void LevelInitPostEntity( void )
	{
		m_flNextDebugDraw = 0.0f;

		// With a manager, its Activate has validated already.
		if ( !KartRaceManager() )
		{
			KartRace_ValidateMap();
		}
	}

	virtual void FrameUpdatePostEntityThink( void )
	{
		if ( !kart_debug_server.GetBool() || gpGlobals->curtime < m_flNextDebugDraw )
			return;

		m_flNextDebugDraw = gpGlobals->curtime + KART_DEBUG_DRAW_INTERVAL;
		DrawDebug( KART_DEBUG_DRAW_INTERVAL + 0.05f );
	}

private:
	void DrawDebug( float flDuration )
	{
		char szText[64];

		CUtlVector< CKartCheckpoint * > checkpoints;
		GatherCheckpoints( checkpoints );
		for ( int i = 0; i < checkpoints.Count(); i++ )
		{
			CKartCheckpoint *pCheckpoint = checkpoints[i];
			bool bFinish = ( pCheckpoint->GetIndex() == KART_FINISH_INDEX && FClassnameIs( pCheckpoint, "kart_finish" ) );

			if ( bFinish )
			{
				NDebugOverlay::EntityBounds( pCheckpoint, 255, 255, 255, 24, flDuration );
				Q_snprintf( szText, sizeof( szText ), "finish (0)" );
			}
			else
			{
				NDebugOverlay::EntityBounds( pCheckpoint, 255, 200, 0, 24, flDuration );
				Q_snprintf( szText, sizeof( szText ), "checkpoint %d", pCheckpoint->GetIndex() );
			}
			NDebugOverlay::EntityTextAtPosition( pCheckpoint->WorldSpaceCenter(), 0, szText, flDuration );
		}

		CUtlVector< CKartStart * > starts;
		GatherStarts( starts );
		for ( int i = 0; i < starts.Count(); i++ )
		{
			CKartStart *pStart = starts[i];
			Vector vecForward;
			AngleVectors( pStart->GetAbsAngles(), &vecForward );

			NDebugOverlay::BoxAngles( pStart->GetAbsOrigin(), KART_HULL_MIN, KART_HULL_MAX, pStart->GetAbsAngles(), 0, 200, 255, 24, flDuration );
			NDebugOverlay::Line( pStart->GetAbsOrigin(), pStart->GetAbsOrigin() + vecForward * 48.0f, 0, 200, 255, true, flDuration );
			Q_snprintf( szText, sizeof( szText ), "grid %d", pStart->GetGrid() );
			NDebugOverlay::EntityTextAtPosition( pStart->GetAbsOrigin() + Vector( 0, 0, KART_HULL_MAX.z ), 0, szText, flDuration );
		}
	}

	float m_flNextDebugDraw;
};

static CKartRaceSystem g_KartRaceSystem;

// ##################################################################################
//	>> kart_race_dump
// ##################################################################################
CON_COMMAND( kart_race_dump, "Print the kart race setup parsed from the map: laps, track name and the checkpoint order." )
{
	CKartRaceManager *pManager = KartRaceManager();
	if ( !pManager )
	{
		Warning( "[kart] No kart_race_manager on this map.\n" );
		return;
	}

	// Pick up checkpoints created since the manager activated (ent_create).
	pManager->CollectCheckpoints();

	const char *pszTrack = pManager->GetTrackName();
	Msg( "[kart] Track '%s', %d laps, %d checkpoints in order:\n", ( pszTrack && pszTrack[0] ) ? pszTrack : "(unnamed)", pManager->GetLaps(), pManager->GetCheckpointCount() );

	for ( int i = 0; i < pManager->GetCheckpointCount(); i++ )
	{
		CKartCheckpoint *pCheckpoint = pManager->GetCheckpoint( i );
		if ( !pCheckpoint )
			continue;

		const Vector &vecCenter = pCheckpoint->WorldSpaceCenter();
		Msg( "  %2d: %-15s index %2d  '%s'  at (%.0f %.0f %.0f)\n", i, pCheckpoint->GetClassname(), pCheckpoint->GetIndex(),
			pCheckpoint->GetEntityName() != NULL_STRING ? STRING( pCheckpoint->GetEntityName() ) : "",
			vecCenter.x, vecCenter.y, vecCenter.z );
	}

	KartRace_ValidateMap();
}
