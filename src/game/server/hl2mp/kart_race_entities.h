//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Map entities that define a kart race:
//
//			kart_race_manager	one per map: lap count, track name, the ordered
//								checkpoint list and the race outputs.
//			kart_start			a grid slot karts spawn on, ordered by "grid".
//			kart_checkpoint		brush trigger, "index" 1..N in track order.
//			kart_finish			brush trigger, the start/finish line (checkpoint 0).
//			kart_path_node		the bots' racing line (kart_racing_line.h).
//			kart_respawn_zone	brush trigger, a kill plane: karts entering it are
//								put back at their last checkpoint.
//			kart_boost_pad		brush trigger, gives karts entering it a boost.
//
//			A map without them is free drive: karts spawn on the deathmatch spawns
//			and nothing is timed.
//
//=============================================================================//

#ifndef KART_RACE_ENTITIES_H
#define KART_RACE_ENTITIES_H
#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"
#include "props.h"
#include "kart_race_shared.h"
#include "kart_racing_line.h"

class CHL2MP_Player;

//-----------------------------------------------------------------------------
// kart_checkpoint: brush trigger the karts pass in track order. Only kart
// players trigger it; it reports to the race manager.
//-----------------------------------------------------------------------------
class CKartCheckpoint : public CBaseTrigger
{
	DECLARE_CLASS( CKartCheckpoint, CBaseTrigger );
	DECLARE_DATADESC();

public:
	CKartCheckpoint();

	virtual void Spawn( void );
	virtual void StartTouch( CBaseEntity *pOther );

	int GetIndex( void ) const { return m_iIndex; }

	// The entity named by "respawn_target", where karts that hit this
	// checkpoint last are put back on the track; NULL when unset or missing.
	CBaseEntity *GetRespawnTarget( void );
	bool HasRespawnTargetName( void ) const { return m_iszRespawnTarget != NULL_STRING && STRING( m_iszRespawnTarget )[0] != '\0'; }
	const char *GetRespawnTargetName( void ) const { return STRING( m_iszRespawnTarget ); }

protected:
	int m_iIndex;
	string_t m_iszRespawnTarget;
};

//-----------------------------------------------------------------------------
// kart_finish: the start/finish line, always checkpoint 0.
//-----------------------------------------------------------------------------
class CKartFinish : public CKartCheckpoint
{
	DECLARE_CLASS( CKartFinish, CKartCheckpoint );

public:
	virtual void Spawn( void );
};

//-----------------------------------------------------------------------------
// kart_respawn_zone: brush trigger under or beside the track. A kart entering
// it is put back on the track at its last checkpoint.
//-----------------------------------------------------------------------------
class CKartRespawnZone : public CBaseTrigger
{
	DECLARE_CLASS( CKartRespawnZone, CBaseTrigger );

public:
	virtual void Spawn( void );
	virtual void StartTouch( CBaseEntity *pOther );
};

//-----------------------------------------------------------------------------
// kart_boost_pad: brush trigger on the road. A kart entering it gets a boost
// (KartGiveBoost) and the pad fires OnBoost. Each kart then waits "cooldown"
// seconds before the same pad boosts it again, and only entering the pad
// counts, so a kart sitting on it is not boosted again.
//
// Server only: the boost reaches the driver's prediction through the
// networked boost fields, so the local kart sees one small correction when
// it arrives.
//-----------------------------------------------------------------------------
class CKartBoostPad : public CBaseTrigger
{
	DECLARE_CLASS( CKartBoostPad, CBaseTrigger );
	DECLARE_DATADESC();

public:
	CKartBoostPad();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void StartTouch( CBaseEntity *pOther );

private:
	float m_flBoostDuration;
	float m_flBoostScale;
	float m_flCooldown;

	// Per player slot: the time this pad may boost that kart again.
	float m_flNextBoostTime[ MAX_PLAYERS + 1 ];

	// Fired on each boost; the activator is the kart.
	COutputEvent m_OnBoost;
};

//-----------------------------------------------------------------------------
// kart_start_lights: the start-light tower (models/kart/props/start_lights.mdl
// unless the map sets another model), a solid prop_dynamic. Its skin picks the
// lit lamp: KART_START_LIGHTS_OFF, _RED, _YELLOW or _GREEN.
//-----------------------------------------------------------------------------
enum
{
	KART_START_LIGHTS_OFF = 0,
	KART_START_LIGHTS_RED,
	KART_START_LIGHTS_YELLOW,
	KART_START_LIGHTS_GREEN,
};

class CKartStartLights : public CDynamicProp
{
	DECLARE_CLASS( CKartStartLights, CDynamicProp );

public:
	virtual void Precache( void );
	virtual void Spawn( void );

	void SetLights( int iSkin ) { m_nSkin = iSkin; }
};

//-----------------------------------------------------------------------------
// kart_start: one grid slot. Karts spawn on the lowest free slot.
//-----------------------------------------------------------------------------
class CKartStart : public CPointEntity
{
	DECLARE_CLASS( CKartStart, CPointEntity );
	DECLARE_DATADESC();

public:
	CKartStart();

	int GetGrid( void ) const { return m_iGrid; }

private:
	int m_iGrid;
};

//-----------------------------------------------------------------------------
// kart_race_manager: one per map. Counts laps from the checkpoint touches,
// tracks every kart's progress along the track and ranks the karts each tick.
//
// Checkpoints are hit in index order: hitting index k when it is the player's
// next checkpoint advances to the following one. The first line crossing
// starts lap 1; crossing it again once every checkpoint was hit completes the
// lap, and completing lap "laps" finishes the race.
//-----------------------------------------------------------------------------
class CKartRaceManager : public CLogicalEntity
{
	DECLARE_CLASS( CKartRaceManager, CLogicalEntity );
	DECLARE_DATADESC();

public:
	CKartRaceManager();
	~CKartRaceManager();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate( void );

	// Rebuilds the checkpoint list from the map, sorted by index, and the route
	// the progress is measured along.
	void CollectCheckpoints( void );

	int GetLaps( void ) const { return m_iLaps; }
	const char *GetTrackName( void ) const { return STRING( m_iszTrackName ); }
	int GetCheckpointCount( void ) const { return m_Checkpoints.Count(); }
	CKartCheckpoint *GetCheckpoint( int i ) const;

	// True when laps can be counted: a kart_finish and at least one checkpoint.
	bool HasRoute( void ) const { return m_Route.Count() >= 2; }
	int GetRouteCount( void ) const { return m_Route.Count(); }
	const Vector &GetRouteCenter( int i ) const { return m_Route[i].center; }

	// Rebuilds the racing line from the kart_path_node chain, starting at the
	// node nearest the start/finish line.
	void BuildRacingLine( void );
	const CKartRacingLine &GetRacingLine( void ) const { return m_RacingLine; }
	bool HasRacingLine( void ) const { return m_RacingLine.IsValid(); }
	float GetRacingLineLength( void ) const { return m_RacingLine.GetLength(); }

	// The racing line point flDistance along the lap (wrapping), and the point
	// on it nearest vecPos. False when the map has no racing line.
	bool GetRacingLinePoint( float flDistance, KartRacingLinePoint_t &point ) const { return m_RacingLine.GetPoint( flDistance, point ); }
	bool GetNearestRacingLinePoint( const Vector &vecPos, KartRacingLinePoint_t &point ) const { return m_RacingLine.GetNearestPoint( vecPos, point ); }

	// A kart player entered checkpoint 'index' (KART_FINISH_INDEX for the line).
	void OnKartTouchedCheckpoint( CHL2MP_Player *pPlayer, int index );

	// Where a kart is put back on the track: the last checkpoint it hit (the
	// one before its next), at that checkpoint's respawn_target when set, else
	// on the ground under the trigger's center, facing the next checkpoint.
	// False when the map has no route.
	bool GetRespawnPoint( CHL2MP_Player *pPlayer, Vector &vecOrigin, QAngle &angFacing ) const;

	// Karts below this height are put back on the track ("kill_z" keyvalue).
	bool HasKillZ( void ) const { return m_bHasKillZ; }
	float GetKillZ( void ) const { return m_flKillZ; }

	// Puts every player back to the start of the race (kart_race_reset).
	void ResetRace( void );

	// Every tick: each kart's progress, then the race positions.
	void RaceThink( void );

	// The race flow's GO: fires OnRaceStart.
	void StartRace( void );

	// kart_finish_timeout ran out: finishes every kart still racing, in its
	// current race order, as did-not-finish.
	void FinishStragglers( void );

	// A kart player taking part in the race: in a kart, on a team and not a
	// late joiner waiting for the next race.
	static bool IsRacing( CHL2MP_Player *pPlayer );

	// OnRaceStart is fired at the race flow's GO; OnRaceFinish when the first
	// kart finishes.
	COutputEvent m_OnRaceStart;
	COutputEvent m_OnRaceFinish;

private:

	// Route position of checkpoint 'index', or -1 when it isn't on the route.
	int RoutePosition( int index ) const;
	// Index of the checkpoint after 'index' on the route, wrapping to the line.
	int NextRouteIndex( int index ) const;

	void UpdateProgress( CHL2MP_Player *pPlayer );
	void UpdateWrongWay( CHL2MP_Player *pPlayer, int iSegment );
	void CheckKillZ( void );
	void UpdatePositions( void );
	void FinishRace( CHL2MP_Player *pPlayer, bool bDNF );

	int m_iLaps;
	string_t m_iszTrackName;
	string_t m_iszKillZ;	// "kill_z" as typed: empty for none
	bool m_bHasKillZ;
	float m_flKillZ;
	CUtlVector< CHandle< CKartCheckpoint > > m_Checkpoints;

	// The track as a loop of checkpoint centers: route position 0 is the line,
	// then one entry per checkpoint index in order. Segment p runs from point p
	// to point p + 1 (the last one back to the line).
	struct RoutePoint_t
	{
		int		index;		// checkpoint index
		CHandle< CKartCheckpoint > hTrigger;	// its trigger
		Vector	center;		// world space center of its trigger
		Vector	dir;		// unit direction of the segment starting here
		float	length;		// length of the segment starting here
	};
	CUtlVector< RoutePoint_t > m_Route;

	CKartRacingLine m_RacingLine;

	bool m_bSomeoneFinished;	// OnRaceFinish has fired
};

// The map's race manager, or NULL on a map without one (free drive).
CKartRaceManager *KartRaceManager( void );

// The grid slot a kart player should spawn on, or NULL when the map has no
// kart_start or every slot is taken (the caller falls back to stock spawns).
CBaseEntity *KartRace_SelectGridSpawn( CHL2MP_Player *pPlayer );

// Respawns these players on the grid in this order: the first on the lowest
// slot, whoever stands where. Players past the last slot spawn as usual.
void KartRace_RespawnOnGrid( const CUtlVector< CHL2MP_Player * > &order );

// Prints console warnings for a broken race setup. Silent on maps without any
// race entity.
void KartRace_ValidateMap( void );

#endif // KART_RACE_ENTITIES_H
