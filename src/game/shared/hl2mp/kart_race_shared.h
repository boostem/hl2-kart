//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart race definitions shared by the server's race manager, the
//			client (HUD) and bots: checkpoint and lap limits, the network
//			encoding of the per-player race state and the race event and sound
//			names.
//
//			Per-player race state, networked to everyone on CHL2MP_Player:
//
//			lap				0 before crossing the line the first time, then the
//							lap being driven (1..laps). Stays at laps once the
//							player has finished.
//			next checkpoint	index of the checkpoint to hit next; 0 means the
//							finish line.
//			progress		lap + fraction of the lap driven (0..1), so it
//							orders players by how far along the race they are.
//							laps + 1 once finished.
//			position		race position, 1 for the leader; 0 when not racing.
//			finished		crossed the line at the end of the last lap.
//			lap start time	server time the current lap started.
//			best lap		fastest lap time so far, 0 before the first lap.
//			total time		sum of the completed lap times; the final race
//							time once finished. The running race time is
//							total time + ( curtime - lap start time ).
//			late join		joined while a race was running: spectates, has no
//							position and no laps until the next race.
//			wrong way		driving against the track for kart_wrongway_time
//							seconds; cleared once facing forward again.
//
//			Race flow, networked on the game rules (CHL2MPRules):
//
//			race state		see KartRaceState_t.
//			state end time	server time the current state ends (WAITING:
//							when the countdown starts, 0 while it waits for
//							karts; COUNTDOWN: GO; RACING: 0; FINISHING: when
//							the karts still racing are finished for them;
//							RESULTS: when the next race starts).
//
//=============================================================================//

#ifndef KART_RACE_SHARED_H
#define KART_RACE_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// Index of the start/finish line in the checkpoint order.
#define KART_FINISH_INDEX			0

// Checkpoint indices are 1..KART_MAX_CHECKPOINT_INDEX.
#define KART_MAX_CHECKPOINT_INDEX	63
#define KART_NET_CHECKPOINT_BITS	6

// kart_race_manager "laps" is clamped to 1..KART_MAX_LAPS.
#define KART_MAX_LAPS				31
#define KART_NET_LAP_BITS			5

// Race position, 1..MAX_PLAYERS.
#define KART_NET_POSITION_BITS		7

// Progress is 0..KART_MAX_LAPS + 1, sent with ~0.0005 lap resolution.
#define KART_NET_PROGRESS_BITS		16
#define KART_NET_PROGRESS_MAX		( KART_MAX_LAPS + 1.0f )

// Race flow states. NONE: no race runs (not kart mode, or a map without a
// kart_race_manager and a route): free drive.
enum KartRaceState_t
{
	KART_RACE_STATE_NONE = 0,
	KART_RACE_STATE_WAITING,	// karts drive freely, laps aren't counted; waits for kart_min_players or everyone ready
	KART_RACE_STATE_COUNTDOWN,	// karts frozen on the grid for kart_countdown_time
	KART_RACE_STATE_RACING,		// laps count
	KART_RACE_STATE_FINISHING,	// someone finished; the rest have kart_finish_timeout to
	KART_RACE_STATE_RESULTS,	// karts frozen for kart_results_time, then the next race or the next map

	KART_RACE_STATE_COUNT
};
#define KART_NET_RACE_STATE_BITS	3

// Race standings flags, per player on the game rules (the scoreboard).
enum
{
	KART_STANDING_RACING	= ( 1 << 0 ),	// a racer (CKartRaceManager::IsRacing): not a late joiner or spectator
	KART_STANDING_FINISHED	= ( 1 << 1 ),
	KART_STANDING_DNF		= ( 1 << 2 ),	// finished for them by kart_finish_timeout
	KART_STANDING_LATE_JOIN	= ( 1 << 3 ),
};
#define KART_NET_STANDING_FLAG_BITS	4

inline const char *KartRaceStateName( int state )
{
	static const char *s_pszNames[KART_RACE_STATE_COUNT] = { "none", "waiting", "countdown", "racing", "finishing", "results" };
	return ( state >= 0 && state < KART_RACE_STATE_COUNT ) ? s_pszNames[state] : "?";
}

// Game events (modevents.res).
#define KART_EVENT_CHECKPOINT		"kart_checkpoint"	// userid, index
#define KART_EVENT_LAP				"kart_lap"			// userid, lap, laptime
#define KART_EVENT_RACE_FINISH		"kart_race_finish"	// userid, position, totaltime, dnf
#define KART_EVENT_COUNTDOWN		"kart_countdown"	// seconds (3, 2, 1)
#define KART_EVENT_RACE_START		"kart_race_start"	// laps

// Game sounds (game_sounds_kart.txt), played to the player only.
#define KART_SOUND_CHECKPOINT		"Kart.Checkpoint"
#define KART_SOUND_LAP_COMPLETE		"Kart.LapComplete"

// Game sound played at the kart when it is put back on the track, heard by everyone.
#define KART_SOUND_RESPAWN			"Kart.Respawn"

#endif // KART_RACE_SHARED_H
