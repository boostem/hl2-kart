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

// Game events (modevents.res).
#define KART_EVENT_CHECKPOINT		"kart_checkpoint"	// userid, index
#define KART_EVENT_LAP				"kart_lap"			// userid, lap, laptime
#define KART_EVENT_RACE_FINISH		"kart_race_finish"	// userid, position, totaltime

// Game sounds (game_sounds_kart.txt), played to the player only.
#define KART_SOUND_CHECKPOINT		"Kart.Checkpoint"
#define KART_SOUND_LAP_COMPLETE		"Kart.LapComplete"

#endif // KART_RACE_SHARED_H
