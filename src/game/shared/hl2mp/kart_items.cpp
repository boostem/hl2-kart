//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart item system. See kart_items.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_items.h"
#include "in_buttons.h"

#ifdef GAME_DLL
#include "hl2mp_player.h"
#include "kart_hazards.h"
#include "tier1/fmtstr.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GAME_DLL
static bool KartItemUse_Stub( CHL2MP_Player *pPlayer, bool bBackward );
static bool KartItemUse_Nitro( CHL2MP_Player *pPlayer, bool bBackward );
#define KART_ITEM_USE( fn )	, fn
#else
#define KART_ITEM_USE( fn )
#endif

// ##################################################################################
//	>> Item table
// ##################################################################################
// Weights by bucket: the leader gets defensive and weak items, the back of the
// pack the strong ones. Each item's own ticket replaces the stub use and tunes
// its row; the balance pass (#46) tunes the weights with kart_item_dump.
const KartItemInfo_t g_KartItems[KART_ITEM_COUNT] =
{
	//	name			display name	backward	weight: leader front middle back	count: leader front middle back
	{ "none",			"None",			false,		{  0,  0,  0,  0 },					{ 0, 0, 0, 0 }	KART_ITEM_USE( NULL ) },
	{ "hubcap",			"Hubcap",		true,		{ 30, 35, 25, 10 },					{ 1, 1, 1, 1 }	KART_ITEM_USE( KartItemUse_Stub ) },
	{ "oil_slick",		"Oil Slick",	true,		{ 45, 25, 10,  5 },					{ 1, 1, 1, 1 }	KART_ITEM_USE( KartItemUse_Oil ) },
	{ "nitro_can",		"Nitro Can",	false,		{  0, 15, 30, 35 },					{ 1, 1, 2, 3 }	KART_ITEM_USE( KartItemUse_Nitro ) },
	{ "seeker",			"Seeker",		false,		{  0, 15, 25, 30 },					{ 1, 1, 1, 1 }	KART_ITEM_USE( KartItemUse_Stub ) },
	{ "buffer",			"Buffer",		false,		{ 25, 10, 10, 20 },					{ 1, 1, 1, 1 }	KART_ITEM_USE( KartItemUse_Stub ) },
};

COMPILE_TIME_ASSERT( KART_ITEM_COUNT <= ( 1 << KART_NET_ITEM_BITS ) );

static const char *s_pszBucketNames[KART_ITEM_BUCKET_COUNT] =
{
	"leader",
	"front",
	"middle",
	"back",
};

bool KartItem_IsValid( int item )
{
	return item > KART_ITEM_NONE && item < KART_ITEM_COUNT;
}

const char *KartItem_GetName( int item )
{
	if ( item < KART_ITEM_NONE || item >= KART_ITEM_COUNT )
		return "?";

	return g_KartItems[item].pszName;
}

const char *KartItem_GetDisplayName( int item )
{
	if ( item < KART_ITEM_NONE || item >= KART_ITEM_COUNT )
		return "?";

	return g_KartItems[item].pszDisplayName;
}

int KartItem_FromName( const char *pszName )
{
	if ( !pszName || !pszName[0] )
		return KART_ITEM_COUNT;

	for ( int i = 0; i < KART_ITEM_COUNT; i++ )
	{
		if ( !Q_stricmp( pszName, g_KartItems[i].pszName ) )
			return i;
	}

	// A number, as kart_item_dump lists them.
	char *pszEnd = NULL;
	long item = strtol( pszName, &pszEnd, 10 );
	if ( pszEnd && *pszEnd == '\0' && item >= KART_ITEM_NONE && item < KART_ITEM_COUNT )
		return (int)item;

	return KART_ITEM_COUNT;
}

int KartItem_GetBucket( int nPosition, int nRacers )
{
	if ( nPosition <= 0 || nRacers < 2 )
		return KART_ITEM_BUCKET_UNIFORM;

	if ( nPosition == 1 )
		return KART_ITEM_BUCKET_LEADER;

	// The rest split in thirds by how far back they are: ceil( 3 * ( pos - 1 ) / ( racers - 1 ) ),
	// so last place is always in the back bucket.
	nPosition = MIN( nPosition, nRacers );
	int nThirds = KART_ITEM_BUCKET_COUNT - 1;
	int bucket = ( nThirds * ( nPosition - 1 ) + ( nRacers - 1 ) - 1 ) / ( nRacers - 1 );
	return clamp( bucket, (int)KART_ITEM_BUCKET_FRONT, (int)KART_ITEM_BUCKET_BACK );
}

const char *KartItem_GetBucketName( int bucket )
{
	if ( bucket < 0 || bucket >= KART_ITEM_BUCKET_COUNT )
		return "uniform";

	return s_pszBucketNames[bucket];
}

int KartItem_GetBucketWeight( int bucket )
{
	if ( bucket < 0 || bucket >= KART_ITEM_BUCKET_COUNT )
		return 0;

	int nTotal = 0;
	for ( int i = KART_ITEM_NONE + 1; i < KART_ITEM_COUNT; i++ )
	{
		nTotal += MAX( 0, g_KartItems[i].nWeight[bucket] );
	}
	return nTotal;
}

int KartItem_PickRandom( int bucket )
{
	int nTotal = KartItem_GetBucketWeight( bucket );
	if ( nTotal <= 0 )
		return RandomInt( KART_ITEM_NONE + 1, KART_ITEM_COUNT - 1 );

	int nRoll = RandomInt( 0, nTotal - 1 );
	for ( int i = KART_ITEM_NONE + 1; i < KART_ITEM_COUNT; i++ )
	{
		nRoll -= MAX( 0, g_KartItems[i].nWeight[bucket] );
		if ( nRoll < 0 )
			return i;
	}

	return KART_ITEM_COUNT - 1;
}

bool KartItem_ReadUseInput( int nButtons, int nButtonsPressed, bool &bBackward )
{
	bBackward = false;

	if ( nButtonsPressed & IN_ATTACK2 )
	{
		bBackward = true;
		return true;
	}

	if ( nButtonsPressed & IN_ATTACK )
	{
		bBackward = ( nButtons & IN_BACK ) != 0;
		return true;
	}

	return false;
}

#ifdef GAME_DLL
// ##################################################################################
//	>> Server: box hooks, use stubs and commands
// ##################################################################################
extern ConVar kart_debug_server;

ConVar kart_items_enabled( "kart_items_enabled", "1", FCVAR_NOTIFY, "Item boxes give karts items, and karts can use them." );
ConVar kart_item_roulette_time( "kart_item_roulette_time", "1.5", 0, "Seconds the item roulette spins after a box is taken before the item can be used.", true, 0.0f, true, 10.0f );

// Until each item's ticket lands: log the use, and the item is spent.
static bool KartItemUse_Stub( CHL2MP_Player *pPlayer, bool bBackward )
{
	Msg( "[kart] %s used %s%s (stub, not implemented yet)\n", pPlayer->GetPlayerName(), KartItem_GetName( pPlayer->GetKartItem() ), bBackward ? " backward" : "" );
	return true;
}

ConVar kart_nitro_duration( "kart_nitro_duration", "1.2", FCVAR_NOTIFY, "Seconds of boost per Nitro Can charge.", true, 0.0f, true, 10.0f );
ConVar kart_nitro_scale( "kart_nitro_scale", "1.5", FCVAR_NOTIFY, "Speed scale of the Nitro Can boost.", true, 1.0f, true, 5.0f );

// Nitro Can: an instant boost, one per charge. Not while stunned: a hit stops
// any boost, so the charge is kept for when control comes back.
static bool KartItemUse_Nitro( CHL2MP_Player *pPlayer, bool bBackward )
{
	if ( pPlayer->IsKartHit() )
		return false;

	pPlayer->KartGiveBoost( kart_nitro_duration.GetFloat(), kart_nitro_scale.GetFloat() );
	pPlayer->EmitSound( "Kart.Nitro" );
	return true;
}

bool KartPlayerHasItem( CHL2MP_Player *pPlayer )
{
	// Also true while the roulette spins: the item is already decided.
	return pPlayer->GetKartItem() != KART_ITEM_NONE;
}

// Race position and the number of karts racing, for the pick weights.
static int KartItem_GetPlayerBucket( CHL2MP_Player *pPlayer )
{
	int nRacers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pOther = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( pOther && pOther->IsInKart() && pOther->GetKartRacePosition() > 0 )
		{
			nRacers++;
		}
	}

	return KartItem_GetBucket( pPlayer->GetKartRacePosition(), nRacers );
}

void KartGiveRandomItem( CHL2MP_Player *pPlayer )
{
	if ( !kart_items_enabled.GetBool() )
		return;

	int bucket = KartItem_GetPlayerBucket( pPlayer );
	int item = KartItem_PickRandom( bucket );
	int nCount = ( bucket >= 0 ) ? g_KartItems[item].nCount[bucket] : 1;

	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] %s (position %d, %s) rolled %s x%d\n", pPlayer->GetPlayerName(), pPlayer->GetKartRacePosition(), KartItem_GetBucketName( bucket ), KartItem_GetName( item ), nCount );
	}

	pPlayer->KartGiveItem( item, nCount, kart_item_roulette_time.GetFloat() );
}

// ##################################################################################
//	>> kart_item_dump
// ##################################################################################
CON_COMMAND( kart_item_dump, "Print the kart item table: each item's weight and uses by race position bucket, and every kart's held item." )
{
	Msg( "[kart] Items (%s), roulette %.2fs. Weight%% (uses) by bucket:\n", kart_items_enabled.GetBool() ? "enabled" : "disabled", kart_item_roulette_time.GetFloat() );

	CUtlString line;
	line.Format( "  %2s  %-10s %-8s", "#", "name", "backward" );
	for ( int bucket = 0; bucket < KART_ITEM_BUCKET_COUNT; bucket++ )
	{
		line.Append( CFmtStr( " %14s", KartItem_GetBucketName( bucket ) ) );
	}
	Msg( "%s\n", line.Get() );

	for ( int i = KART_ITEM_NONE + 1; i < KART_ITEM_COUNT; i++ )
	{
		const KartItemInfo_t &info = g_KartItems[i];
		line.Format( "  %2d  %-10s %-8s", i, info.pszName, info.bBackward ? "yes" : "no" );
		for ( int bucket = 0; bucket < KART_ITEM_BUCKET_COUNT; bucket++ )
		{
			int nTotal = KartItem_GetBucketWeight( bucket );
			float flPercent = nTotal > 0 ? 100.0f * MAX( 0, info.nWeight[bucket] ) / nTotal : 0.0f;
			line.Append( CFmtStr( " %3d %5.1f%% (%d)", info.nWeight[bucket], flPercent, info.nCount[bucket] ) );
		}
		Msg( "%s\n", line.Get() );
	}

	Msg( "  Buckets: 1st is the leader; the rest split in thirds front/middle/back, last place always back.\n" );
	Msg( "  Uniform (%.1f%% each, 1 use) when the position is unknown or fewer than 2 karts race.\n", 100.0f / ( KART_ITEM_COUNT - 1 ) );

	Msg( "[kart] Karts:\n" );
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsInKart() )
			continue;

		float flRoulette = MAX( 0.0f, pPlayer->GetKartRouletteEnd() - gpGlobals->curtime );
		Msg( "  %-24s position %2d (%-7s) item %-10s x%d  roulette %.2fs\n", pPlayer->GetPlayerName(), pPlayer->GetKartRacePosition(),
			KartItem_GetBucketName( KartItem_GetPlayerBucket( pPlayer ) ), KartItem_GetName( pPlayer->GetKartItem() ), pPlayer->GetKartItemCount(), flRoulette );
	}
}

// ##################################################################################
//	>> kart_give_item
// ##################################################################################
static int KartGiveItemAutocomplete( const char *pszPartial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH] )
{
	const char *pszCommand = "kart_give_item";
	const char *pszArg = Q_strstr( pszPartial, " " );
	pszArg = pszArg ? pszArg + 1 : "";
	int nArgLen = Q_strlen( pszArg );

	int nMatches = 0;
	for ( int i = 0; i < KART_ITEM_COUNT && nMatches < COMMAND_COMPLETION_MAXITEMS; i++ )
	{
		if ( !Q_strnicmp( g_KartItems[i].pszName, pszArg, nArgLen ) )
		{
			Q_snprintf( commands[nMatches++], COMMAND_COMPLETION_ITEM_LENGTH, "%s %s", pszCommand, g_KartItems[i].pszName );
		}
	}
	return nMatches;
}

CON_COMMAND_F_COMPLETION( kart_give_item, "Give your kart an item, without the roulette: kart_give_item <name|number> [uses]. 'none' clears it. sv_cheats only.", FCVAR_CHEAT, KartGiveItemAutocomplete )
{
	if ( !sv_cheats || !sv_cheats->GetBool() )
	{
		Warning( "[kart] kart_give_item needs sv_cheats 1.\n" );
		return;
	}

	CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_GetCommandClient() );
	if ( !pPlayer )
	{
		Warning( "[kart] kart_give_item: run it as a player.\n" );
		return;
	}

	if ( args.ArgC() < 2 )
	{
		Msg( "Usage: kart_give_item <name|number> [uses]. Items:" );
		for ( int i = 0; i < KART_ITEM_COUNT; i++ )
		{
			Msg( " %s", g_KartItems[i].pszName );
		}
		Msg( "\n" );
		return;
	}

	int item = KartItem_FromName( args.Arg( 1 ) );
	if ( item == KART_ITEM_COUNT )
	{
		Warning( "[kart] Unknown item '%s'; kart_item_dump lists them.\n", args.Arg( 1 ) );
		return;
	}

	if ( !pPlayer->IsInKart() )
	{
		Warning( "[kart] %s is not in a kart.\n", pPlayer->GetPlayerName() );
		return;
	}

	if ( item == KART_ITEM_NONE )
	{
		pPlayer->KartClearItem();
		Msg( "[kart] %s: item cleared\n", pPlayer->GetPlayerName() );
		return;
	}

	int nCount = args.ArgC() >= 3 ? atoi( args.Arg( 2 ) ) : 1;
	pPlayer->KartGiveItem( item, nCount, 0.0f );
	Msg( "[kart] %s: given %s x%d\n", pPlayer->GetPlayerName(), KartItem_GetName( item ), pPlayer->GetKartItemCount() );
}
#endif // GAME_DLL
