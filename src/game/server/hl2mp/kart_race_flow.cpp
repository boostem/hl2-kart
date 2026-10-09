//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The kart race flow on CHL2MPRules: on a map with a kart_race_manager
//			and a route, one race after the other instead of deathmatch.
//
//			WAITING		karts drive freely, laps aren't counted. Once
//						kart_min_players karts are in, the countdown starts
//						kart_waiting_time later; right away once every kart
//						has said mp_ready_signal in chat.
//			COUNTDOWN	clean map, every kart respawned on the grid (the last
//						race's finish order, newcomers by join order) and
//						frozen. kart_countdown ticks 3, 2, 1, then
//						kart_race_start.
//			RACING		laps count. Karts joining now are late joiners: they
//						drive, but have no position and race the next one.
//			FINISHING	the first kart has finished; the others have
//						kart_finish_timeout, then they are finished for them.
//			RESULTS		karts frozen for kart_results_time, then the next map
//						if mp_timelimit has run out, else WAITING again on a
//						clean map and the grid.
//
//=============================================================================//

#include "cbase.h"
#include "hl2mp_gamerules.h"
#include "hl2mp_player.h"
#include "kart_race_entities.h"
#include "kart_race_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar kart_min_players( "kart_min_players", "1", FCVAR_NOTIFY, "Karts needed before a race starts (unless every kart says mp_ready_signal in chat).", true, 1, true, MAX_PLAYERS );
ConVar kart_waiting_time( "kart_waiting_time", "3", FCVAR_NOTIFY, "Seconds to wait once kart_min_players karts are in before the countdown, so karts still loading make the grid.", true, 0, false, 0 );
ConVar kart_countdown_time( "kart_countdown_time", "3", FCVAR_NOTIFY, "Seconds the karts are held on the grid before the race starts.", true, 0, true, 10 );
ConVar kart_finish_timeout( "kart_finish_timeout", "30", FCVAR_NOTIFY, "Seconds the other karts have to finish once the first one has; then they are finished for them.", true, 0, false, 0 );
ConVar kart_races_per_map( "kart_races_per_map", "1", FCVAR_NOTIFY, "Races run on a map before the server goes on to the next map of the mapcycle (a one-entry cycle restarts the map).", true, 1, false, 0 );
ConVar kart_results_time( "kart_results_time", "10", FCVAR_NOTIFY, "Seconds the results are shown before the next race (or the next map once mp_timelimit has run out).", true, 0, false, 0 );

static void KartFormatTime( float flSeconds, char *pszOut, int nSize )
{
	int nCentis = (int)( MAX( flSeconds, 0.0f ) * 100.0f + 0.5f );
	Q_snprintf( pszOut, nSize, "%d:%02d.%02d", nCentis / 6000, ( nCentis / 100 ) % 60, nCentis % 100 );
}

//-----------------------------------------------------------------------------
// Purpose: Every frame, from Think: runs the race flow when the map has a race,
//			drops back to NONE (free drive) when it hasn't.
//-----------------------------------------------------------------------------
void CHL2MPRules::KartRaceThink( void )
{
	CKartRaceManager *pManager = KartRaceManager();
	bool bRaceMap = kart_enabled.GetBool() && pManager && pManager->HasRoute();

	if ( !bRaceMap )
	{
		if ( GetKartRaceState() != KART_RACE_STATE_NONE )
		{
			KartSetState( KART_RACE_STATE_NONE, 0.0f );
		}
		m_bKartRestartPending = false;
		return;
	}

	// Map start (karts spawn on the grid by themselves), or kart mode just came on.
	if ( GetKartRaceState() == KART_RACE_STATE_NONE )
	{
		KartSetState( KART_RACE_STATE_WAITING, 0.0f );
	}

	if ( m_bKartRestartPending )
	{
		m_bKartRestartPending = false;
		KartNewRace( KART_RACE_STATE_WAITING );
		return;
	}

	int nFinished = 0;
	bool bAllReady = false;
	int nRacers = KartCountRacers( &nFinished, &bAllReady );

	switch ( GetKartRaceState() )
	{
	case KART_RACE_STATE_WAITING:
		{
			if ( nRacers == 0 || ( nRacers < kart_min_players.GetInt() && !bAllReady ) )
			{
				// Not enough karts: wait without a deadline.
				if ( m_flKartStateEndTime != 0.0f )
				{
					m_flKartStateEndTime = 0.0f;
				}
				break;
			}

			if ( m_flKartStateEndTime == 0.0f )
			{
				m_flKartStateEndTime = gpGlobals->curtime + kart_waiting_time.GetFloat();
			}

			if ( bAllReady || gpGlobals->curtime >= m_flKartStateEndTime )
			{
				KartNewRace( KART_RACE_STATE_COUNTDOWN );
			}
		}
		break;

	case KART_RACE_STATE_COUNTDOWN:
		{
			if ( nRacers == 0 )
			{
				KartSetState( KART_RACE_STATE_WAITING, 0.0f );
				break;
			}

			if ( gpGlobals->curtime >= m_flKartStateEndTime )
			{
				KartStartRace();
				break;
			}

			int nSeconds = (int)ceil( m_flKartStateEndTime - gpGlobals->curtime );
			if ( nSeconds < m_iKartCountdownTick )
			{
				m_iKartCountdownTick = nSeconds;

				IGameEvent *event = gameeventmanager->CreateEvent( KART_EVENT_COUNTDOWN );
				if ( event )
				{
					event->SetInt( "seconds", nSeconds );
					gameeventmanager->FireEvent( event );
				}

				// Until the countdown HUD draws it.
				char szSeconds[8];
				Q_snprintf( szSeconds, sizeof( szSeconds ), "%d", nSeconds );
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "%s1", szSeconds );
			}
		}
		break;

	case KART_RACE_STATE_RACING:
		// The first finisher moves on to FINISHING (OnKartFinished).
		if ( nRacers == 0 )
		{
			KartNewRace( KART_RACE_STATE_WAITING );
		}
		break;

	case KART_RACE_STATE_FINISHING:
		if ( nRacers == 0 )
		{
			KartNewRace( KART_RACE_STATE_WAITING );
		}
		else if ( nFinished >= nRacers )
		{
			KartShowResults();
		}
		else if ( gpGlobals->curtime >= m_flKartStateEndTime )
		{
			Msg( "[kart] kart_finish_timeout ran out with %d kart(s) still racing.\n", nRacers - nFinished );
			pManager->FinishStragglers();
			KartShowResults();
		}
		break;

	case KART_RACE_STATE_RESULTS:
		if ( gpGlobals->curtime < m_flKartStateEndTime )
			break;

		m_iKartRacesDone++;

		if ( GetMapRemainingTime() < 0 || m_iKartRacesDone >= kart_races_per_map.GetInt() )
		{
			GoToIntermission();
			break;
		}

		KartNewRace( KART_RACE_STATE_WAITING );
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Switches the race state and freezes or frees every kart to match.
//-----------------------------------------------------------------------------
void CHL2MPRules::KartSetState( KartRaceState_t state, float flEndTime )
{
	if ( state != GetKartRaceState() )
	{
		Msg( "[kart] Race state %s -> %s\n", KartRaceStateName( GetKartRaceState() ), KartRaceStateName( state ) );
	}

	m_nKartRaceState = state;
	m_flKartStateEndTime = flEndTime;

	if ( state == KART_RACE_STATE_COUNTDOWN )
	{
		// The next think announces the first second.
		m_iKartCountdownTick = (int)ceil( MAX( flEndTime - gpGlobals->curtime, 0.0f ) ) + 1;
	}

	bool bFreeze = IsKartRaceFrozen();
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsInKart() )
			continue;

		if ( bFreeze )
		{
			pPlayer->AddFlag( FL_FROZEN );
		}
		else if ( !IsIntermission() )
		{
			pPlayer->RemoveFlag( FL_FROZEN );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Racers (CKartRaceManager::IsRacing), how many have finished, and
//			whether every one of them has said mp_ready_signal (bots always
//			have).
//-----------------------------------------------------------------------------
int CHL2MPRules::KartCountRacers( int *pnFinished, bool *pbAllReady )
{
	int nRacers = 0;
	int nFinished = 0;
	bool bAllReady = true;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !CKartRaceManager::IsRacing( pPlayer ) )
			continue;

		nRacers++;

		if ( pPlayer->IsKartFinished() )
		{
			nFinished++;
		}

		if ( !pPlayer->IsFakeClient() && !pPlayer->IsReady() )
		{
			bAllReady = false;
		}
	}

	if ( pnFinished )
	{
		*pnFinished = nFinished;
	}
	if ( pbAllReady )
	{
		*pbAllReady = ( nRacers > 0 && bAllReady );
	}

	return nRacers;
}

//-----------------------------------------------------------------------------
// Purpose: Everyone who will race, in grid order: by the last race's finish
//			order, then by join order (userids only grow).
//-----------------------------------------------------------------------------
struct KartGridEntry_t
{
	CHL2MP_Player *pPlayer;
	int iLastRace;	// place in the last race's finish order, or past the end
	int iUserID;
};

static int KartGridSortFunc( const KartGridEntry_t *a, const KartGridEntry_t *b )
{
	if ( a->iLastRace != b->iLastRace )
		return a->iLastRace - b->iLastRace;

	return a->iUserID - b->iUserID;
}

void CHL2MPRules::KartGetGridOrder( CUtlVector< CHL2MP_Player * > &order )
{
	CUtlVector< KartGridEntry_t > entries;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsHLTV() || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
			continue;

		KartGridEntry_t entry;
		entry.pPlayer = pPlayer;
		entry.iUserID = pPlayer->GetUserID();
		entry.iLastRace = m_KartGridOrder.Find( entry.iUserID );
		if ( entry.iLastRace < 0 )
		{
			entry.iLastRace = m_KartGridOrder.Count();
		}
		entries.AddToTail( entry );
	}

	entries.Sort( KartGridSortFunc );

	order.RemoveAll();
	for ( int i = 0; i < entries.Count(); i++ )
	{
		order.AddToTail( entries[i].pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A new race in WAITING or COUNTDOWN: the map cleaned up (item boxes,
//			hazards, projectiles), every kart's race state reset and everyone
//			respawned on the grid.
//-----------------------------------------------------------------------------
void CHL2MPRules::KartNewRace( KartRaceState_t state )
{
	Assert( state == KART_RACE_STATE_WAITING || state == KART_RACE_STATE_COUNTDOWN );

	CUtlVector< CHL2MP_Player * > order;
	KartGetGridOrder( order );

	// Recreates the map entities, the race manager among them.
	CleanUpMap();

	CKartRaceManager *pManager = KartRaceManager();
	if ( pManager )
	{
		pManager->ResetRace();
	}
	else
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer )
			{
				pPlayer->ResetKartRaceState();
			}
		}
	}

	// Before the respawns, so they spawn frozen for the countdown.
	float flEndTime = 0.0f;
	if ( state == KART_RACE_STATE_COUNTDOWN )
	{
		flEndTime = gpGlobals->curtime + kart_countdown_time.GetFloat();
	}
	KartSetState( state, flEndTime );

	KartRace_RespawnOnGrid( order );
}

//-----------------------------------------------------------------------------
// Purpose: Everyone racing now takes part in the current race; anyone joining
//			later is a late joiner (OnKartSpawned).
//-----------------------------------------------------------------------------
void CHL2MPRules::KartMarkInRace( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( CKartRaceManager::IsRacing( pPlayer ) )
		{
			pPlayer->m_bKartInRace = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: GO.
//-----------------------------------------------------------------------------
void CHL2MPRules::KartStartRace( void )
{
	KartSetState( KART_RACE_STATE_RACING, 0.0f );
	KartMarkInRace();

	CKartRaceManager *pManager = KartRaceManager();
	if ( pManager )
	{
		pManager->StartRace();
	}

	IGameEvent *event = gameeventmanager->CreateEvent( KART_EVENT_RACE_START );
	if ( event )
	{
		event->SetInt( "laps", pManager ? pManager->GetLaps() : 0 );
		gameeventmanager->FireEvent( event );
	}

	// Until the countdown HUD draws it.
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "GO!" );
}

static int KartPositionSortFunc( CHL2MP_Player * const *a, CHL2MP_Player * const *b )
{
	return (*a)->GetKartRacePosition() - (*b)->GetKartRacePosition();
}

//-----------------------------------------------------------------------------
// Purpose: Every racer has finished: hold the karts, keep the finish order for
//			the next grid and print it (until the results panel shows it).
//-----------------------------------------------------------------------------
void CHL2MPRules::KartShowResults( void )
{
	KartSetState( KART_RACE_STATE_RESULTS, gpGlobals->curtime + kart_results_time.GetFloat() );

	CUtlVectorFixed< CHL2MP_Player *, MAX_PLAYERS > finishers;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( CKartRaceManager::IsRacing( pPlayer ) && pPlayer->IsKartFinished() )
		{
			finishers.AddToTail( pPlayer );
		}
	}
	finishers.Sort( KartPositionSortFunc );

	m_KartGridOrder.RemoveAll();
	UTIL_ClientPrintAll( HUD_PRINTTALK, "Race results:" );
	Msg( "[kart] Race results:\n" );

	for ( int i = 0; i < finishers.Count(); i++ )
	{
		CHL2MP_Player *pPlayer = finishers[i];
		m_KartGridOrder.AddToTail( pPlayer->GetUserID() );

		char szTime[32];
		char szBest[32];
		KartFormatTime( pPlayer->GetKartTotalTime(), szTime, sizeof( szTime ) );
		KartFormatTime( pPlayer->GetKartBestLap(), szBest, sizeof( szBest ) );

		char szLine[128];
		if ( pPlayer->IsKartDNF() )
		{
			Q_snprintf( szLine, sizeof( szLine ), "%d. %s  DNF", i + 1, pPlayer->GetPlayerName() );
		}
		else
		{
			Q_snprintf( szLine, sizeof( szLine ), "%d. %s  %s  (best lap %s)", i + 1, pPlayer->GetPlayerName(), szTime, szBest );
		}

		UTIL_ClientPrintAll( HUD_PRINTTALK, "%s1", szLine );
		Msg( "  %s\n", szLine );
	}
}

//-----------------------------------------------------------------------------
// Purpose: From CHL2MP_Player::Spawn. Joining while a race runs makes the kart
//			a late joiner: it drives, but races the next one.
//-----------------------------------------------------------------------------
void CHL2MPRules::OnKartSpawned( CHL2MP_Player *pPlayer )
{
	if ( !IsKartRaceRunning() || pPlayer->m_bKartInRace || pPlayer->m_bKartLateJoin )
		return;

	pPlayer->m_bKartLateJoin = true;
	Msg( "[kart] %s joined during the race and races the next one.\n", pPlayer->GetPlayerName() );
	ClientPrint( pPlayer, HUD_PRINTCENTER, "Race in progress: you race the next one" );
}

//-----------------------------------------------------------------------------
// Purpose: From the race manager: a kart completed its last lap. The first
//			one starts the finish timeout.
//-----------------------------------------------------------------------------
void CHL2MPRules::OnKartFinished( CHL2MP_Player *pPlayer )
{
	if ( GetKartRaceState() == KART_RACE_STATE_RACING )
	{
		KartSetState( KART_RACE_STATE_FINISHING, gpGlobals->curtime + kart_finish_timeout.GetFloat() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: kart_race_reset put every kart back to lap 0 where it stands. A
//			race that was running, finishing or over runs again from here;
//			waiting and the countdown carry on.
//-----------------------------------------------------------------------------
void CHL2MPRules::OnKartRaceReset( void )
{
	if ( GetKartRaceState() == KART_RACE_STATE_RACING || GetKartRaceState() == KART_RACE_STATE_FINISHING
		|| GetKartRaceState() == KART_RACE_STATE_RESULTS )
	{
		KartSetState( KART_RACE_STATE_RACING, 0.0f );
		KartMarkInRace();
	}
}

bool CHL2MPRules::RequestKartRaceRestart( void )
{
	if ( GetKartRaceState() == KART_RACE_STATE_NONE )
		return false;

	m_bKartRestartPending = true;
	return true;
}

// ##################################################################################
//	>> kart_race_restart
// ##################################################################################
CON_COMMAND( kart_race_restart, "Start a new kart race: clean map, every kart back on the grid, then waiting and the countdown." )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !HL2MPRules() || !HL2MPRules()->RequestKartRaceRestart() )
	{
		Warning( "[kart] No race on this map: it needs kart_enabled 1 and a kart_race_manager with a kart_finish and checkpoints.\n" );
		return;
	}

	Msg( "[kart] Restarting the race.\n" );
}
