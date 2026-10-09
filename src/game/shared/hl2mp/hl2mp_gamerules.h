//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef HL2MP_GAMERULES_H
#define HL2MP_GAMERULES_H
#pragma once

#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "gamevars_shared.h"
#include "kart_shareddefs.h"
#include "kart_race_shared.h"

#ifndef CLIENT_DLL
#include "hl2mp_player.h"

class CHL2MP_Player;
#endif

#define VEC_CROUCH_TRACE_MIN	HL2MPRules()->GetHL2MPViewVectors()->m_vCrouchTraceMin
#define VEC_CROUCH_TRACE_MAX	HL2MPRules()->GetHL2MPViewVectors()->m_vCrouchTraceMax

enum
{
	TEAM_COMBINE = 2,
	TEAM_REBELS,
};


#ifdef CLIENT_DLL
	#define CHL2MPRules C_HL2MPRules
	#define CHL2MPGameRulesProxy C_HL2MPGameRulesProxy
#endif

class CHL2MPGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CHL2MPGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};

class HL2MPViewVectors : public CViewVectors
{
public:
	HL2MPViewVectors( 
		Vector vView,
		Vector vHullMin,
		Vector vHullMax,
		Vector vDuckHullMin,
		Vector vDuckHullMax,
		Vector vDuckView,
		Vector vObsHullMin,
		Vector vObsHullMax,
		Vector vDeadViewHeight,
		Vector vCrouchTraceMin,
		Vector vCrouchTraceMax ) :
			CViewVectors( 
				vView,
				vHullMin,
				vHullMax,
				vDuckHullMin,
				vDuckHullMax,
				vDuckView,
				vObsHullMin,
				vObsHullMax,
				vDeadViewHeight )
	{
		m_vCrouchTraceMin = vCrouchTraceMin;
		m_vCrouchTraceMax = vCrouchTraceMax;
	}

	Vector m_vCrouchTraceMin;
	Vector m_vCrouchTraceMax;	
};

class CHL2MPRules : public CTeamplayRules
{
public:
	DECLARE_CLASS( CHL2MPRules, CTeamplayRules );

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
#endif
	
	CHL2MPRules();
	virtual ~CHL2MPRules();

	virtual void Precache( void );
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );

	virtual float FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon );
	virtual float FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon );
	virtual Vector VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon );
	virtual int WeaponShouldRespawn( CBaseCombatWeapon *pWeapon );
	virtual void Think( void );
	virtual void CreateStandardEntities( void );
	virtual void ClientSettingsChanged( CBasePlayer *pPlayer );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual void GoToIntermission( void );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual const char *GetGameDescription( void );
	// derive this function if you mod uses encrypted weapon info files
	virtual const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"x9Ke0BY7"; }
	virtual const CViewVectors* GetViewVectors() const;
	const HL2MPViewVectors* GetHL2MPViewVectors() const;

	float GetMapRemainingTime();
	void CleanUpMap();
	void CheckRestartGame();
	void RestartGame();

	void OnNavMeshLoad( void );
	
#ifndef CLIENT_DLL
	virtual Vector VecItemRespawnSpot( CItem *pItem );
	virtual QAngle VecItemRespawnAngles( CItem *pItem );
	virtual float	FlItemRespawnTime( CItem *pItem );
	virtual bool	CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem );
	virtual bool	CanHaveItem( CBasePlayer *pPlayer, CItem *pItem );
	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

	void	AddLevelDesignerPlacedObject( CBaseEntity *pEntity );
	void	RemoveLevelDesignerPlacedObject( CBaseEntity *pEntity );
	void	ManageObjectRelocation( void );
	void    CheckChatForReadySignal( CHL2MP_Player *pPlayer, const char *chatmsg );
	const char *GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer );

#endif

	bool IsOfficialMap( void );

	virtual void ClientDisconnected( edict_t *pClient );

	bool CheckGameOver( void );
	bool IsIntermission( void );

	void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	
	// Karts race everyone against everyone: no teams in kart mode.
	bool	IsTeamplay( void ) { return m_bTeamPlayEnabled && !kart_enabled.GetBool(); }
	void	CheckAllPlayersReady( void );

	virtual bool IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer );

	// Kart race flow (see KartRaceState_t). Runs instead of deathmatch on a map
	// with a kart_race_manager and a route; NONE elsewhere (free drive).
	KartRaceState_t GetKartRaceState( void ) const { return (KartRaceState_t)m_nKartRaceState.Get(); }
	float GetKartStateEndTime( void ) const { return m_flKartStateEndTime; }
	// Karts are held still: on the grid during the countdown, and at the results.
	bool IsKartRaceFrozen( void ) const;
	// A race is running: checkpoints and laps count.
	bool IsKartRaceRunning( void ) const;

#ifndef CLIENT_DLL
	// kart_race_flow.cpp
	void OnKartSpawned( CHL2MP_Player *pPlayer );
	void OnKartFinished( CHL2MP_Player *pPlayer );
	void OnKartRaceReset( void );
	// kart_race_restart: a new race from WAITING, on a clean map and the grid.
	bool RequestKartRaceRestart( void );
	// The kart spectators watch: the best placed one still driving.
	CHL2MP_Player *GetKartLeader( void );
#endif

	// Kart race: laps in the race (0 without a kart_race_manager) and how many
	// karts are racing. Set by the race manager, read by the HUD.
	int		GetKartLaps( void ) const { return m_nKartLaps; }
	int		GetKartRacers( void ) const { return m_nKartRacers; }
#ifndef CLIENT_DLL
	void	SetKartLaps( int nLaps ) { m_nKartLaps = nLaps; }
	void	SetKartRacers( int nRacers ) { m_nKartRacers = nRacers; }
#endif

	// Kart race standings by player index (1..maxClients), for the scoreboard.
	// Copied from the players' race state every think and sent with the game
	// rules, so unlike the race state on the players they don't go stale for
	// karts outside the PVS. Flags: KART_STANDING_*.
	int		GetKartStandingPosition( int iPlayer ) const;
	int		GetKartStandingLap( int iPlayer ) const;
	int		GetKartStandingFlags( int iPlayer ) const;
	float	GetKartStandingBestLap( int iPlayer ) const;
	// The final time once finished, else the running race time (0 before lap 1).
	float	GetKartStandingRaceTime( int iPlayer ) const;
	
private:

#ifndef CLIENT_DLL
	void KartRaceThink( void );
	void KartSetState( KartRaceState_t state, float flEndTime );
	void KartNewRace( KartRaceState_t state );
	void KartStartRace( void );
	void KartMarkInRace( void );
	void KartShowResults( void );
	int KartCountRacers( int *pnFinished = NULL, bool *pbAllReady = NULL );
	void KartGetGridOrder( CUtlVector< CHL2MP_Player * > &order );
	void KartPlayersThink( bool bRaceMap );
	void KartRespawnSpectators( void );

	int m_iKartRacesDone;				// races finished on this map
	int m_iKartCountdownTick;			// last countdown second announced
	bool m_bKartRestartPending;			// kart_race_restart, done on the next think
	CUtlVector< int > m_KartGridOrder;	// userids by the last race's finish order
#endif

	CNetworkVar( int, m_nKartRaceState );
	CNetworkVar( float, m_flKartStateEndTime );
	
	CNetworkVar( bool, m_bTeamPlayEnabled );
	CNetworkVar( int, m_nKartLaps );
	CNetworkVar( int, m_nKartRacers );
	CNetworkArray( int, m_nKartStandingPosition, MAX_PLAYERS_ARRAY_SAFE );
	CNetworkArray( int, m_nKartStandingLap, MAX_PLAYERS_ARRAY_SAFE );
	CNetworkArray( int, m_nKartStandingFlags, MAX_PLAYERS_ARRAY_SAFE );
	CNetworkArray( float, m_flKartStandingBestLap, MAX_PLAYERS_ARRAY_SAFE );
	CNetworkArray( float, m_flKartStandingTotalTime, MAX_PLAYERS_ARRAY_SAFE );	// sum of the completed laps
	CNetworkArray( float, m_flKartStandingLapStartTime, MAX_PLAYERS_ARRAY_SAFE );
	CNetworkVar( float, m_flGameStartTime );
	CUtlVector<EHANDLE> m_hRespawnableItemsAndWeapons;
	float m_tmNextPeriodicThink;
	float m_flRestartGameTime;
	bool m_bCompleteReset;
	bool m_bAwaitingReadyRestart;
	bool m_bHeardAllPlayersReady;

#ifndef CLIENT_DLL
	bool m_bChangelevelDone;
#endif
};

inline CHL2MPRules* HL2MPRules()
{
	return static_cast<CHL2MPRules*>(g_pGameRules);
}

#endif //HL2MP_GAMERULES_H
