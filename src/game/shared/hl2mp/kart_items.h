//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart item system, shared by the server, the client (HUD) and bots:
//			the item list, the item table with the pick weights by race
//			position, and the use input.
//
//			Items are server-authoritative and not predicted. Per-player item
//			state, networked to everyone on CHL2MP_Player:
//
//			item			the held item, decided when the box is taken;
//							KART_ITEM_NONE when empty-handed.
//			item count		uses left of the held item (multi-use items).
//			roulette item	what the roulette shows while it spins; display
//							only. Equals the held item once it has stopped.
//			roulette end	server time the roulette stops. The held item can
//							only be used after it.
//
//			Use input: attack uses the held item; attack2, or attack with back
//			held, throws it backwards where the item supports it.
//
//=============================================================================//

#ifndef KART_ITEMS_H
#define KART_ITEMS_H
#ifdef _WIN32
#pragma once
#endif

// Original items; each one's behaviour lands in its own ticket.
enum KartItem_t
{
	KART_ITEM_NONE = 0,
	KART_ITEM_HUBCAP,		// bouncing straight projectile
	KART_ITEM_OIL_SLICK,	// hazard dropped on the track
	KART_ITEM_NITRO_CAN,	// self boost
	KART_ITEM_SEEKER,		// homing projectile
	KART_ITEM_BUFFER,		// one-hit shield

	KART_ITEM_COUNT
};

// Network encoding of the item and its count.
#define KART_NET_ITEM_BITS			4
#define KART_NET_ITEM_COUNT_BITS	3
#define KART_MAX_ITEM_COUNT			( ( 1 << KART_NET_ITEM_COUNT_BITS ) - 1 )

// Seconds between two items shown by the spinning roulette.
#define KART_ITEM_ROULETTE_STEP		0.08f

// Race position buckets the pick weights are given for.
enum KartItemBucket_t
{
	KART_ITEM_BUCKET_UNIFORM = -1,	// position unknown: every item equally likely
	KART_ITEM_BUCKET_LEADER = 0,	// 1st
	KART_ITEM_BUCKET_FRONT,			// the front third of the rest
	KART_ITEM_BUCKET_MIDDLE,		// the middle third
	KART_ITEM_BUCKET_BACK,			// the back third, last place included

	KART_ITEM_BUCKET_COUNT
};

#ifdef GAME_DLL
class CHL2MP_Player;

// Carries out one use of the held item. Returns false when it could not be
// used right now; the item is kept.
typedef bool ( *KartItemUseFn_t )( CHL2MP_Player *pPlayer, bool bBackward );
#endif

struct KartItemInfo_t
{
	const char *pszName;		// console name, kart_give_item and kart_item_dump
	const char *pszDisplayName;
	bool bBackward;				// can be thrown backwards
	int nWeight[KART_ITEM_BUCKET_COUNT];	// relative chance in each bucket, 0 = never
	int nCount[KART_ITEM_BUCKET_COUNT];		// uses given in each bucket, 1..KART_MAX_ITEM_COUNT
#ifdef GAME_DLL
	KartItemUseFn_t Use;
#endif
};

extern const KartItemInfo_t g_KartItems[KART_ITEM_COUNT];

// Projectile items name their entities kart_proj_<item> (kart_proj_hubcap),
// so kart bots can see them coming whatever the item.
#define KART_PROJECTILE_CLASSNAMES	"kart_proj_*"

// Game events (modevents.res).
#define KART_EVENT_ITEM_PICKUP		"kart_item_pickup"	// userid, item
#define KART_EVENT_ITEM_USE			"kart_item_use"		// userid, item, backward

bool KartItem_IsValid( int item );
const char *KartItem_GetName( int item );
const char *KartItem_GetDisplayName( int item );

// Item by console name (or number); KART_ITEM_COUNT when there is none.
int KartItem_FromName( const char *pszName );

// Bucket for a race position (1 = leader) among nRacers karts.
int KartItem_GetBucket( int nPosition, int nRacers );
const char *KartItem_GetBucketName( int bucket );

// Total pick weight of a bucket.
int KartItem_GetBucketWeight( int bucket );

// Picks an item by weight for the bucket, uniformly when the bucket is
// KART_ITEM_BUCKET_UNIFORM or has no weight at all.
int KartItem_PickRandom( int bucket );

// The use input: true when this command pressed the use button. bBackward is
// set when it asks for a backward throw.
bool KartItem_ReadUseInput( int nButtons, int nButtonsPressed, bool &bBackward );

#ifdef GAME_DLL
extern ConVar kart_items_enabled;
extern ConVar kart_item_roulette_time;

// Item box hooks, called by kart_item_box.
bool KartPlayerHasItem( CHL2MP_Player *pPlayer );
void KartGiveRandomItem( CHL2MP_Player *pPlayer );

// Lag compensation for projectile items. A projectile is thrown with the
// other karts rewound to where its thrower saw them (StartLagCompensation
// around the throw), and moved straight away by the catch-up time against
// them: what the thrower aimed at is hit. From then on it moves in present
// time each tick, with its reach against each kart widened by the slop.
//
// Seconds the thrower's view of the other karts is behind the server, up to
// kart_proj_lag_max; 0 when they aren't rewound. Call between Start and
// FinishLagCompensation.
float KartProj_GetCatchUpTime( CHL2MP_Player *pThrower );

// Units a projectile's reach against this kart is widened by, for the kart's
// own latency: kart_proj_lag_slop per second, up to kart_proj_lag_max.
float KartProj_GetLagSlop( CHL2MP_Player *pTarget );
#endif

#endif // KART_ITEMS_H
