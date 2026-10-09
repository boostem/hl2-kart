//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart bots: fake clients that drive the racing line.
//
//			A kart bot is a plain CHL2MP_Player on a fake client, so it spawns,
//			races and is ranked like anyone. Each tick the bot system builds
//			its CUserCmd (steer, throttle, IN_JUMP to hop and drift) and queues
//			it like a client's, so CKartGameMovement drives it by the same rules
//			as a human kart.
//
//			kart_bot_add [name], kart_bot_kick [name|all], kart_bot_quota.
//
//=============================================================================//

#ifndef KART_BOT_H
#define KART_BOT_H
#ifdef _WIN32
#pragma once
#endif

class CBasePlayer;
class CHL2MP_Player;

// True for a fake client the kart bot drives: not SourceTV, the replay bot or
// an hl2mp NextBot.
bool KartBot_IsKartBot( CBasePlayer *pPlayer );

// Puts a kart bot in the game, named pszName (or the next free bot name when
// NULL or empty). NULL when the server is full.
CHL2MP_Player *KartBot_Add( const char *pszName );

// Number of kart bots in the game.
int KartBot_Count( void );

#endif // KART_BOT_H
