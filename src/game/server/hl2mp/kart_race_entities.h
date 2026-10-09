//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Map entities that define a kart race:
//
//			kart_race_manager	one per map: lap count, track name, the ordered
//								checkpoint list and the race outputs.
//			kart_start			a grid slot karts spawn on, ordered by "grid".
//			kart_checkpoint		brush trigger, "index" 1..N in track order.
//			kart_finish			brush trigger, the start/finish line (checkpoint 0).
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

class CHL2MP_Player;

// Index of the start/finish line in the checkpoint order.
#define KART_FINISH_INDEX	0

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

protected:
	int m_iIndex;
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
// kart_race_manager: one per map.
//-----------------------------------------------------------------------------
class CKartRaceManager : public CLogicalEntity
{
	DECLARE_CLASS( CKartRaceManager, CLogicalEntity );
	DECLARE_DATADESC();

public:
	CKartRaceManager();
	~CKartRaceManager();

	virtual void Spawn( void );
	virtual void Activate( void );

	// Rebuilds the checkpoint list from the map, sorted by index.
	void CollectCheckpoints( void );

	int GetLaps( void ) const { return m_iLaps; }
	const char *GetTrackName( void ) const { return STRING( m_iszTrackName ); }
	int GetCheckpointCount( void ) const { return m_Checkpoints.Count(); }
	CKartCheckpoint *GetCheckpoint( int i ) const;

	// A kart player entered checkpoint 'index' (KART_FINISH_INDEX for the line).
	void OnKartTouchedCheckpoint( CHL2MP_Player *pPlayer, int index );

	// Fired by the race flow (later tickets).
	COutputEvent m_OnRaceStart;
	COutputEvent m_OnRaceFinish;

private:
	int m_iLaps;
	string_t m_iszTrackName;
	CUtlVector< CHandle< CKartCheckpoint > > m_Checkpoints;
};

// The map's race manager, or NULL on a map without one (free drive).
CKartRaceManager *KartRaceManager( void );

// The grid slot a kart player should spawn on, or NULL when the map has no
// kart_start or every slot is taken (the caller falls back to stock spawns).
CBaseEntity *KartRace_SelectGridSpawn( CHL2MP_Player *pPlayer );

// Prints console warnings for a broken race setup. Silent on maps without any
// race entity.
void KartRace_ValidateMap( void );

#endif // KART_RACE_ENTITIES_H
