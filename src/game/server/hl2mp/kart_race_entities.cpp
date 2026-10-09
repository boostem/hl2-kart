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
#include "GameEventListener.h"
#include "recipientfilter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar kart_debug_server( "kart_debug_server", "0", 0, "1: draw kart race checkpoints, the finish line and grid slots with debug overlays, and log checkpoint touches. 2: also draw the bots' racing line." );

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

	DEFINE_THINKFUNC( RaceThink ),

	DEFINE_OUTPUT( m_OnRaceStart, "OnRaceStart" ),
	DEFINE_OUTPUT( m_OnRaceFinish, "OnRaceFinish" ),
END_DATADESC()

CKartRaceManager::CKartRaceManager()
{
	m_iLaps = 3;
	m_iszTrackName = NULL_STRING;
	m_bSomeoneFinished = false;
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

	Precache();

	if ( m_iLaps < 1 )
	{
		Warning( "[kart] kart_race_manager laps %d is invalid, using 1.\n", m_iLaps );
		m_iLaps = 1;
	}
	else if ( m_iLaps > KART_MAX_LAPS )
	{
		Warning( "[kart] kart_race_manager laps %d is more than %d, using %d.\n", m_iLaps, KART_MAX_LAPS, KART_MAX_LAPS );
		m_iLaps = KART_MAX_LAPS;
	}
}

void CKartRaceManager::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( KART_SOUND_CHECKPOINT );
	PrecacheScriptSound( KART_SOUND_LAP_COMPLETE );
}

void CKartRaceManager::Activate( void )
{
	BaseClass::Activate();

	if ( g_pKartRaceManager != this )
		return;

	// Every map entity has spawned by now (on map load and for ent_create alike).
	CollectCheckpoints();
	BuildRacingLine();
	KartRace_ValidateMap();

	SetThink( &CKartRaceManager::RaceThink );
	SetNextThink( gpGlobals->curtime );
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

	// The route: the line, then the first trigger of each checkpoint index.
	// Broken entries (see KartRace_ValidateMap) are left out.
	m_Route.RemoveAll();
	for ( int i = 0; i < list.Count(); i++ )
	{
		int index = list[i]->GetIndex();
		bool bFinish = FClassnameIs( list[i], "kart_finish" );

		if ( m_Route.Count() == 0 )
		{
			if ( !bFinish )
				continue;
		}
		else if ( bFinish || index <= m_Route.Tail().index || index > KART_MAX_CHECKPOINT_INDEX )
		{
			continue;
		}

		RoutePoint_t &point = m_Route[m_Route.AddToTail()];
		point.index = index;
		point.center = list[i]->WorldSpaceCenter();
		point.dir.Init();
		point.length = 0.0f;
	}

	// Without a checkpoint there is no lap to drive.
	if ( m_Route.Count() < 2 )
	{
		m_Route.RemoveAll();
		return;
	}

	for ( int i = 0; i < m_Route.Count(); i++ )
	{
		RoutePoint_t &point = m_Route[i];
		point.dir = m_Route[( i + 1 ) % m_Route.Count()].center - point.center;
		point.length = VectorNormalize( point.dir );
	}
}

void CKartRaceManager::BuildRacingLine( void )
{
	m_RacingLine.Build( HasRoute() ? &m_Route[0].center : NULL );
}

CKartCheckpoint *CKartRaceManager::GetCheckpoint( int i ) const
{
	if ( i < 0 || i >= m_Checkpoints.Count() )
		return NULL;

	return m_Checkpoints[i].Get();
}

int CKartRaceManager::RoutePosition( int index ) const
{
	for ( int i = 0; i < m_Route.Count(); i++ )
	{
		if ( m_Route[i].index == index )
			return i;
	}

	return -1;
}

int CKartRaceManager::NextRouteIndex( int index ) const
{
	int p = RoutePosition( index );
	if ( p < 0 )
		return KART_FINISH_INDEX;

	return m_Route[( p + 1 ) % m_Route.Count()].index;
}

bool CKartRaceManager::IsRacing( CHL2MP_Player *pPlayer )
{
	return pPlayer && pPlayer->IsConnected() && pPlayer->IsInKart() && !pPlayer->IsHLTV()
		&& pPlayer->GetTeamNumber() != TEAM_SPECTATOR;
}

static void KartRace_PlaySound( CHL2MP_Player *pPlayer, const char *pszSound )
{
	CSingleUserRecipientFilter filter( pPlayer );
	CBaseEntity::EmitSound( filter, pPlayer->entindex(), pszSound );
}

static void KartRace_FireCheckpointEvent( CHL2MP_Player *pPlayer, int index )
{
	IGameEvent *event = gameeventmanager->CreateEvent( KART_EVENT_CHECKPOINT );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "index", index );
		gameeventmanager->FireEvent( event );
	}
}

void CKartRaceManager::OnKartTouchedCheckpoint( CHL2MP_Player *pPlayer, int index )
{
	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] %s touched checkpoint %d at %.2f (next %d)\n", pPlayer->GetPlayerName(), index, gpGlobals->curtime, pPlayer->m_nKartNextCheckpoint.Get() );
	}
	else
	{
		DevMsg( "[kart] %s touched checkpoint %d at %.2f (next %d)\n", pPlayer->GetPlayerName(), index, gpGlobals->curtime, pPlayer->m_nKartNextCheckpoint.Get() );
	}

	if ( !HasRoute() || pPlayer->m_bKartFinished )
		return;

	// Out of order (skipped, driven backwards or touched twice): nothing.
	if ( index != pPlayer->m_nKartNextCheckpoint )
		return;

	if ( index != KART_FINISH_INDEX )
	{
		pPlayer->m_nKartNextCheckpoint = NextRouteIndex( index );
		KartRace_FireCheckpointEvent( pPlayer, index );
		KartRace_PlaySound( pPlayer, KART_SOUND_CHECKPOINT );
		return;
	}

	// First time over the line: lap 1 starts.
	if ( pPlayer->m_nKartLap == 0 )
	{
		pPlayer->m_nKartLap = 1;
		pPlayer->m_flKartLapStartTime = gpGlobals->curtime;
		pPlayer->m_nKartNextCheckpoint = NextRouteIndex( KART_FINISH_INDEX );
		KartRace_FireCheckpointEvent( pPlayer, index );
		KartRace_PlaySound( pPlayer, KART_SOUND_CHECKPOINT );
		return;
	}

	// Over the line after every checkpoint: the lap is complete.
	float flLapTime = gpGlobals->curtime - pPlayer->m_flKartLapStartTime;
	pPlayer->m_flKartTotalTime += flLapTime;
	if ( pPlayer->m_flKartBestLap <= 0.0f || flLapTime < pPlayer->m_flKartBestLap )
	{
		pPlayer->m_flKartBestLap = flLapTime;
	}

	IGameEvent *event = gameeventmanager->CreateEvent( KART_EVENT_LAP );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "lap", pPlayer->m_nKartLap );
		event->SetFloat( "laptime", flLapTime );
		gameeventmanager->FireEvent( event );
	}

	KartRace_PlaySound( pPlayer, KART_SOUND_LAP_COMPLETE );

	DevMsg( "[kart] %s completed lap %d/%d in %.2f\n", pPlayer->GetPlayerName(), pPlayer->m_nKartLap.Get(), m_iLaps, flLapTime );

	if ( pPlayer->m_nKartLap >= m_iLaps )
	{
		FinishRace( pPlayer );
		return;
	}

	pPlayer->m_nKartLap++;
	pPlayer->m_flKartLapStartTime = gpGlobals->curtime;
	pPlayer->m_nKartNextCheckpoint = NextRouteIndex( KART_FINISH_INDEX );
}

void CKartRaceManager::FinishRace( CHL2MP_Player *pPlayer )
{
	// Finishers are ranked by when they crossed the line.
	int nPosition = 1;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pOther = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( pOther && pOther != pPlayer && IsRacing( pOther ) && pOther->m_bKartFinished )
		{
			nPosition++;
		}
	}

	pPlayer->m_bKartFinished = true;
	pPlayer->m_flKartFinishTime = gpGlobals->curtime;
	pPlayer->m_flKartProgress = pPlayer->m_nKartLap + 1.0f;
	pPlayer->m_nKartRacePosition = nPosition;

	IGameEvent *event = gameeventmanager->CreateEvent( KART_EVENT_RACE_FINISH );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "position", nPosition );
		event->SetFloat( "totaltime", pPlayer->m_flKartTotalTime );
		gameeventmanager->FireEvent( event );
	}

	Msg( "[kart] %s finished %d in %.2f\n", pPlayer->GetPlayerName(), nPosition, pPlayer->m_flKartTotalTime.Get() );

	// Movement keeps working; the race flow (later tickets) decides what's next.
	if ( !m_bSomeoneFinished )
	{
		m_bSomeoneFinished = true;
		m_OnRaceFinish.FireOutput( pPlayer, this );
	}
}

void CKartRaceManager::ResetRace( void )
{
	m_bSomeoneFinished = false;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			pPlayer->ResetKartRaceState();
		}
	}

	RaceThink();
}

void CKartRaceManager::RaceThink( void )
{
	SetNextThink( gpGlobals->curtime );

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( IsRacing( pPlayer ) )
		{
			UpdateProgress( pPlayer );
		}
	}

	UpdatePositions();
}

//-----------------------------------------------------------------------------
// Purpose: progress = lap + ( prev + t ) / count, where prev is the route
//			position of the last checkpoint hit and t how far the kart is
//			along the segment from it to the next one (its projection onto the
//			segment, clamped to 0..1). Straight segments make it monotonic
//			along the track; it only steps forward when a checkpoint is hit.
//-----------------------------------------------------------------------------
void CKartRaceManager::UpdateProgress( CHL2MP_Player *pPlayer )
{
	// Finishers keep the value FinishRace gave them; the dead keep their last.
	if ( !HasRoute() || pPlayer->m_bKartFinished || !pPlayer->IsAlive() )
		return;

	const int nCount = m_Route.Count();
	int iNext = RoutePosition( pPlayer->m_nKartNextCheckpoint );
	if ( iNext < 0 )
	{
		iNext = 0;
	}
	int iPrev = ( iNext + nCount - 1 ) % nCount;

	const RoutePoint_t &seg = m_Route[iPrev];
	float t = 1.0f;
	if ( seg.length > 1.0f )
	{
		t = clamp( DotProduct( pPlayer->WorldSpaceCenter() - seg.center, seg.dir ) / seg.length, 0.0f, 1.0f );
	}

	pPlayer->m_flKartProgress = pPlayer->m_nKartLap + ( iPrev + t ) / nCount;
}

// Finishers first, by finish time, then everyone else by progress.
static int RaceOrderSortFunc( CHL2MP_Player * const *a, CHL2MP_Player * const *b )
{
	const CHL2MP_Player *pA = *a;
	const CHL2MP_Player *pB = *b;

	if ( pA->IsKartFinished() != pB->IsKartFinished() )
		return pA->IsKartFinished() ? -1 : 1;

	if ( pA->IsKartFinished() )
	{
		if ( pA->GetKartFinishTime() != pB->GetKartFinishTime() )
			return pA->GetKartFinishTime() < pB->GetKartFinishTime() ? -1 : 1;
	}
	else if ( pA->GetKartProgress() != pB->GetKartProgress() )
	{
		return pA->GetKartProgress() > pB->GetKartProgress() ? -1 : 1;
	}

	return pA->entindex() - pB->entindex();
}

void CKartRaceManager::UpdatePositions( void )
{
	CUtlVectorFixed< CHL2MP_Player *, MAX_PLAYERS > racers;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer )
			continue;

		if ( HasRoute() && IsRacing( pPlayer ) )
		{
			racers.AddToTail( pPlayer );
		}
		else
		{
			pPlayer->m_nKartRacePosition = 0;
		}
	}

	racers.Sort( RaceOrderSortFunc );

	for ( int i = 0; i < racers.Count(); i++ )
	{
		racers[i]->m_nKartRacePosition = i + 1;
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
	if ( !KartRaceManager() && !checkpoints.Count() && !starts.Count() && !CKartRacingLine::CountMapNodes() )
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

		if ( index > KART_MAX_CHECKPOINT_INDEX )
		{
			Warning( "[kart] kart_checkpoint '%s' index %d is above the maximum of %d; it is ignored.\n", checkpoints[i]->GetDebugName(), index, KART_MAX_CHECKPOINT_INDEX );
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
	else if ( iExpected == 1 )
	{
		Warning( "[kart] Map has no kart_checkpoint: laps can't be counted.\n" );
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

		CKartRaceManager *pManager = KartRaceManager();
		if ( !pManager )
			return;

		if ( kart_debug_server.GetInt() >= 2 )
		{
			DrawRacingLine( pManager->GetRacingLine(), flDuration );
		}

		if ( !pManager->HasRoute() )
			return;

		// The route progress is measured along.
		for ( int i = 0; i < pManager->GetRouteCount(); i++ )
		{
			int iNext = ( i + 1 ) % pManager->GetRouteCount();
			NDebugOverlay::Line( pManager->GetRouteCenter( i ), pManager->GetRouteCenter( iNext ), 255, 200, 0, true, flDuration );
		}

		// Each kart's race state above it.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer || !pPlayer->IsInKart() || !pPlayer->IsAlive() )
				continue;

			Q_snprintf( szText, sizeof( szText ), "P%d  lap %d/%d  next %d  %.3f%s", pPlayer->GetKartRacePosition(), pPlayer->GetKartLap(), pManager->GetLaps(),
				pPlayer->GetKartNextCheckpoint(), pPlayer->GetKartProgress(), pPlayer->IsKartFinished() ? "  finished" : "" );
			NDebugOverlay::EntityTextAtPosition( pPlayer->GetAbsOrigin() + Vector( 0, 0, KART_HULL_MAX.z + 16.0f ), 0, szText, flDuration );
		}
	}

	// The spline in green, the width bounds either side of it and each node.
	void DrawRacingLine( const CKartRacingLine &line, float flDuration )
	{
		if ( !line.IsValid() )
			return;

		char szText[96];
		const Vector vecUp( 0, 0, 1 );

		for ( int i = 0; i < line.GetSampleCount(); i++ )
		{
			const KartRacingLinePoint_t &a = line.GetSample( i );
			const KartRacingLinePoint_t &b = line.GetSample( ( i + 1 ) % line.GetSampleCount() );

			Vector vecRightA = CrossProduct( a.dir, vecUp );
			Vector vecRightB = CrossProduct( b.dir, vecUp );
			VectorNormalize( vecRightA );
			VectorNormalize( vecRightB );

			NDebugOverlay::Line( a.pos, b.pos, 0, 255, 0, true, flDuration );
			NDebugOverlay::Line( a.pos + vecRightA * a.width, b.pos + vecRightB * b.width, 0, 128, 0, true, flDuration );
			NDebugOverlay::Line( a.pos - vecRightA * a.width, b.pos - vecRightB * b.width, 0, 128, 0, true, flDuration );
		}

		for ( int i = 0; i < line.GetNodeCount(); i++ )
		{
			CKartPathNode *pNode = line.GetNode( i );
			if ( !pNode )
				continue;

			KartRacingLinePoint_t point;
			line.GetPoint( line.GetNodeDistance( i ), point );
			Vector vecRight = CrossProduct( point.dir, vecUp );
			VectorNormalize( vecRight );

			NDebugOverlay::Cross3D( pNode->GetAbsOrigin(), 12.0f, 0, 255, 0, true, flDuration );
			NDebugOverlay::Line( pNode->GetAbsOrigin() - vecRight * pNode->GetWidth(), pNode->GetAbsOrigin() + vecRight * pNode->GetWidth(), 0, 255, 0, true, flDuration );

			Q_snprintf( szText, sizeof( szText ), "%s%s  w %.0f  speed %.2f%s", i == 0 ? "start  " : "", pNode->GetDebugName(), pNode->GetWidth(),
				pNode->GetSpeedScale(), pNode->IsDriftHint() ? "  drift" : "" );
			NDebugOverlay::Text( pNode->GetAbsOrigin() + Vector( 0, 0, 24 ), szText, false, flDuration );
		}
	}

	float m_flNextDebugDraw;
};

static CKartRaceSystem g_KartRaceSystem;

// ##################################################################################
//	>> kart_race_dump
// ##################################################################################
CON_COMMAND( kart_race_dump, "Print the kart race setup parsed from the map (laps, track name, checkpoint order) and every kart's race state." )
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

	pManager->BuildRacingLine();
	const CKartRacingLine &line = pManager->GetRacingLine();
	if ( line.IsValid() )
	{
		Msg( "[kart] Racing line: %d nodes, %.0f units, %d samples:\n", line.GetNodeCount(), line.GetLength(), line.GetSampleCount() );
		for ( int i = 0; i < line.GetNodeCount(); i++ )
		{
			CKartPathNode *pNode = line.GetNode( i );
			if ( !pNode )
				continue;

			const Vector &vecOrigin = pNode->GetAbsOrigin();
			Msg( "  %2d: '%s'  at %6.0f  width %4.0f  speed %.2f%s  (%.0f %.0f %.0f)\n", i, pNode->GetDebugName(), line.GetNodeDistance( i ),
				pNode->GetWidth(), pNode->GetSpeedScale(), pNode->IsDriftHint() ? "  drift" : "", vecOrigin.x, vecOrigin.y, vecOrigin.z );
		}
	}
	else
	{
		Msg( "[kart] No racing line (%d kart_path_node on the map).\n", CKartRacingLine::CountMapNodes() );
	}

	KartRace_ValidateMap();

	Msg( "[kart] Karts:\n" );
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsInKart() )
			continue;

		float flLapTime = 0.0f;
		if ( pPlayer->GetKartLap() > 0 && !pPlayer->IsKartFinished() )
		{
			flLapTime = gpGlobals->curtime - pPlayer->GetKartLapStartTime();
		}

		Msg( "  pos %2d  %-24s lap %2d/%d  next %2d  progress %7.3f  lap %7.2fs  best %7.2fs  total %7.2fs%s\n",
			pPlayer->GetKartRacePosition(), pPlayer->GetPlayerName(), pPlayer->GetKartLap(), pManager->GetLaps(), pPlayer->GetKartNextCheckpoint(),
			pPlayer->GetKartProgress(), flLapTime, pPlayer->GetKartBestLap(), pPlayer->GetKartTotalTime(), pPlayer->IsKartFinished() ? "  finished" : "" );
	}
}

// ##################################################################################
//	>> kart_race_reset
// ##################################################################################
CON_COMMAND( kart_race_reset, "Put every kart back to the start of the race: lap 0, waiting for the start/finish line." )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CKartRaceManager *pManager = KartRaceManager();
	if ( !pManager )
	{
		Warning( "[kart] No kart_race_manager on this map.\n" );
		return;
	}

	pManager->ResetRace();
	Msg( "[kart] Race state reset.\n" );
}
