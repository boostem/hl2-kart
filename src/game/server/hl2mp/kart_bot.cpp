//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart bots. See kart_bot.h.
//
//			Each tick, from the bot's place on the racing line:
//
//			steer		pure pursuit: aim at the line point kart_bot_lookahead
//						ahead (less at low speed) and steer toward it. Kart
//						steering is on or off, so a dead zone with hysteresis
//						stands in for how hard to steer.
//			throttle	toward a target speed: the lowest node speed_scale within
//						braking distance times top speed, capped so the steering
//						(or a drift) can follow the sharpest stretch ahead. Full
//						throttle below it, coast just above, brake well above.
//			drift		where the line turns more than kart_bot_drift_angle within
//						kart_bot_drift_distance (half that past a drift hint
//						node): hop with jump held while steering into the
//						corner, so the drift starts on landing; then steer into
//						or against it to stay on the line, and let go of jump as
//						the line straightens (the mini-turbo release).
//			unstuck		no progress along the line for kart_bot_stuck_time:
//						reverse for kart_bot_reverse_time, swinging the nose
//						toward the line. No progress for kart_bot_reset_time
//						(driving the wrong way included): back onto the line.
//
//			skill		kart_bot_difficulty 0-2 sets the bot's top speed, how
//						much its aim wanders (steering noise and a wider dead
//						zone), how many sharp corners it drifts and how quickly
//						it uses its items.
//			rubber band	while the race runs, the gap in race progress to the
//						best human still racing scales the bot's top speed by up
//						to kart_bot_rubberband: faster behind, slower ahead.
//			items		empty-handed, it steers a little toward an item box just
//						ahead. Holding an item, once the roulette has stopped and
//						its reaction time has passed: hubcap at a kart ahead in
//						line (backward at one close behind in line), seeker when
//						a kart is ahead, oil slick when a kart is close behind,
//						nitro on a straight, buffer when a kart_proj_* projectile
//						closes in or a kart close behind holds a hubcap or
//						seeker. Anything held for kart_bot_item_hold_max is
//						used anyway.
//
//			A frozen kart (countdown, results) gets an empty command and its
//			stuck timers wait. A respawn or teleport (a move too far for one
//			tick) makes the bot look for itself on the whole line again.
//
//=============================================================================//

#include "cbase.h"
#include "kart_bot.h"
#include "hl2mp_player.h"
#include "kart_race_entities.h"
#include "kart_item_box.h"
#include "kart_items.h"
#include "hl2mp_gamerules.h"
#include "kart_shareddefs.h"
#include "igamesystem.h"
#include "in_buttons.h"
#include "cdll_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar kart_debug_server;

ConVar kart_bot_quota( "kart_bot_quota", "0", FCVAR_NONE, "Kart bots to keep in the game: bots join or are kicked, one a second, to match. kart_bot_add and kart_bot_kick change it.", true, 0, true, MAX_PLAYERS );
ConVar kart_bot_lookahead( "kart_bot_lookahead", "384", FCVAR_NONE, "How far ahead along the racing line, in units, a kart bot aims at top speed. Shrinks with speed down to kart_bot_lookahead_min." );
ConVar kart_bot_lookahead_min( "kart_bot_lookahead_min", "160", FCVAR_NONE, "How far ahead along the racing line, in units, a stopped kart bot aims." );
ConVar kart_bot_steer_deadzone( "kart_bot_steer_deadzone", "3", FCVAR_NONE, "Degrees a kart bot's aim point may be off its direction of travel before it steers." );
ConVar kart_bot_drift_angle( "kart_bot_drift_angle", "55", FCVAR_NONE, "A kart bot drifts where the racing line turns more than this many degrees within kart_bot_drift_distance (half as many past a drift hint node)." );
ConVar kart_bot_drift_distance( "kart_bot_drift_distance", "512", FCVAR_NONE, "How far ahead along the racing line, in units, a kart bot looks for a corner to drift." );
ConVar kart_bot_stuck_time( "kart_bot_stuck_time", "2", FCVAR_NONE, "Seconds without progress along the racing line before a kart bot reverses out." );
ConVar kart_bot_reverse_time( "kart_bot_reverse_time", "1.5", FCVAR_NONE, "Seconds a stuck kart bot holds the brake to back up (kart_reverse_delay of it standing still)." );
ConVar kart_bot_reset_time( "kart_bot_reset_time", "8", FCVAR_NONE, "Seconds without progress along the racing line before a kart bot is put back on it. 0: never." );
ConVar kart_bot_difficulty( "kart_bot_difficulty", "1", FCVAR_NOTIFY, "Kart bot skill: 0 easy (slower, wandering aim, no drifting, slow to use items), 1 normal, 2 hard (full speed, clean lines, drifts every sharp corner).", true, 0, true, 2 );
ConVar kart_bot_rubberband( "kart_bot_rubberband", "0.15", FCVAR_NOTIFY, "How much kart bots' top speed follows their gap to the best human racing: up to this fraction faster behind them, slower ahead. 0: off.", true, 0.0f, true, 1.0f );
ConVar kart_bot_rubberband_range( "kart_bot_rubberband_range", "2000", FCVAR_NONE, "Gap to the best human, in units along the track, at which kart_bot_rubberband applies in full.", true, 1.0f, false, 0.0f );
ConVar kart_bot_items( "kart_bot_items", "1", FCVAR_NONE, "Kart bots steer toward item boxes and use their items." );
ConVar kart_bot_item_hold_max( "kart_bot_item_hold_max", "15", FCVAR_NONE, "Seconds a kart bot holds an item with no reason to use it before using it anyway. 0: never." );
ConVar kart_bot_debug( "kart_bot_debug", "0", FCVAR_NONE, "1: draw each kart bot's aim point, the racing line it plans along and what it is doing." );

// forwardmove and sidemove of a held key (cl_forwardspeed); the kart only reads their sign.
#define KART_BOT_INPUT				450.0f

// Spacing of the probes along the racing line ahead.
#define KART_BOT_PROBE_STEP			64.0f
#define KART_BOT_MAX_PROBES			64
// Probes the sharpness of the line is measured over (KART_BOT_PROBE_STEP each).
#define KART_BOT_CURVE_PROBES		4

// How far either way of its last place along the line the bot looks for itself.
#define KART_BOT_TRACK_WINDOW		768.0f
// A move farther than this in one tick is a respawn or a teleport.
#define KART_BOT_TELEPORT_DIST		256.0f
// Progress along the line that counts as getting somewhere.
#define KART_BOT_PROGRESS_STEP		48.0f

// Target speeds: never below KART_BOT_MIN_SPEED; brake when over it by more
// than KART_BOT_BRAKE_BAND, else coast; plan corners on this fraction of the
// steering rate; slow to half speed when the aim point is this far off.
#define KART_BOT_MIN_SPEED			150.0f
#define KART_BOT_BRAKE_BAND			40.0f
#define KART_BOT_TURN_MARGIN		0.85f
#define KART_BOT_WIDE_ERROR			45.0f

// Drift: speed over kart_drift_min_speed needed to start one; give up when it
// hasn't started this long after the hop, or has lasted too long; end it when
// the aim point swings this far against it, or the line turns less than
// KART_BOT_DRIFT_EXIT_ANGLE over the next KART_BOT_DRIFT_EXIT_SPAN; then wait
// a little before the next.
#define KART_BOT_DRIFT_SPEED_MARGIN	50.0f
#define KART_BOT_DRIFT_START_TIME	0.75f
#define KART_BOT_DRIFT_MAX_TIME		4.0f
#define KART_BOT_DRIFT_OVERSWING	25.0f
#define KART_BOT_DRIFT_EXIT_SPAN	256.0f
#define KART_BOT_DRIFT_EXIT_ANGLE	10.0f
#define KART_BOT_DRIFT_COOLDOWN		0.5f

// Steering noise: the aim wanders toward a new random offset this often.
#define KART_BOT_NOISE_MIN_TIME		0.4f
#define KART_BOT_NOISE_MAX_TIME		1.2f
#define KART_BOT_NOISE_RATE			20.0f	// degrees per second the offset moves

// Item boxes: steer toward one this close and this far off the aim point, by
// this much of the way, until this close to it.
#define KART_BOT_BOX_RANGE			768.0f
#define KART_BOT_BOX_ANGLE			25.0f
#define KART_BOT_BOX_PULL			0.6f
#define KART_BOT_BOX_MIN_DIST		64.0f
#define KART_BOT_BOX_LIST_INTERVAL	1.0f

// Item rules: forward throws at a kart this far ahead and this many degrees
// off the nose; backward ones and the oil slick at a kart this close behind;
// nitro where the line turns less than this ahead; the buffer against a
// projectile or an armed kart this close. Wait this long between two uses of
// a multi-use item.
#define KART_BOT_THROW_RANGE		1400.0f
#define KART_BOT_THROW_ANGLE		10.0f
#define KART_BOT_SEEKER_RANGE		3000.0f
#define KART_BOT_BEHIND_RANGE		600.0f
#define KART_BOT_BEHIND_ANGLE		12.0f
#define KART_BOT_OIL_ANGLE			40.0f
#define KART_BOT_NITRO_TURN			15.0f
#define KART_BOT_THREAT_RANGE		800.0f
#define KART_BOT_ITEM_REUSE			0.5f

#define KART_BOT_DEBUG_INTERVAL		0.1f

// What kart_bot_difficulty changes.
struct KartBotSkill_t
{
	float flTopSpeed;		// kart_max_speed multiplier, before rubber-banding
	float flSteerNoise;		// degrees the aim wanders either way
	float flDeadzoneScale;	// times kart_bot_steer_deadzone
	float flDriftChance;	// of drifting a sharp corner
	float flItemDelay;		// seconds of reaction before using an item
};

static const KartBotSkill_t s_KartBotSkills[] =
{
	{ 0.85f,	10.0f,	2.0f,	0.0f,	1.5f },	// 0 easy
	{ 0.93f,	4.0f,	1.4f,	0.5f,	0.8f },	// 1 normal
	{ 1.0f,		0.0f,	1.0f,	1.0f,	0.3f },	// 2 hard
};

static const KartBotSkill_t &KartBot_GetSkill( void )
{
	return s_KartBotSkills[clamp( kart_bot_difficulty.GetInt(), 0, (int)ARRAYSIZE( s_KartBotSkills ) - 1 )];
}

static const char *s_pszKartBotNames[] =
{
	"Alyx", "Barney", "Kleiner", "Eli", "Grigori", "Mossman", "Odessa", "Magnusson", "Uriah", "Lamarr", "Dog", "Breen",
};

//-----------------------------------------------------------------------------
// Bots in the game.
//-----------------------------------------------------------------------------
bool KartBot_IsKartBot( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsFakeClient() || pPlayer->IsHLTV() || pPlayer->IsReplay() )
		return false;

#ifdef NEXT_BOT
	if ( pPlayer->MyNextBotPointer() )
		return false;
#endif

	return true;
}

int KartBot_Count( void )
{
	int nBots = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( KartBot_IsKartBot( UTIL_PlayerByIndex( i ) ) )
		{
			nBots++;
		}
	}
	return nBots;
}

// The kart bot that joined last, or NULL.
static CHL2MP_Player *KartBot_Newest( void )
{
	CHL2MP_Player *pNewest = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( KartBot_IsKartBot( pPlayer ) && ( !pNewest || pPlayer->GetUserID() > pNewest->GetUserID() ) )
		{
			pNewest = pPlayer;
		}
	}
	return pNewest;
}

static void KartBot_Kick( CBasePlayer *pBot )
{
	engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pBot->GetUserID() ) );
}

static bool KartBot_IsNameTaken( const char *pszName )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && !Q_stricmp( pPlayer->GetPlayerName(), pszName ) )
			return true;
	}
	return false;
}

static void KartBot_PickName( char *pszName, int nSize )
{
	for ( int i = 0; i < ARRAYSIZE( s_pszKartBotNames ); i++ )
	{
		if ( !KartBot_IsNameTaken( s_pszKartBotNames[i] ) )
		{
			Q_strncpy( pszName, s_pszKartBotNames[i], nSize );
			return;
		}
	}

	for ( int i = 1; ; i++ )
	{
		Q_snprintf( pszName, nSize, "Kart Bot %d", i );
		if ( !KartBot_IsNameTaken( pszName ) )
			return;
	}
}

CHL2MP_Player *KartBot_Add( const char *pszName )
{
	char szName[MAX_PLAYER_NAME_LENGTH];
	if ( pszName && pszName[0] )
	{
		Q_strncpy( szName, pszName, sizeof( szName ) );
	}
	else
	{
		KartBot_PickName( szName, sizeof( szName ) );
	}

	// The engine puts it in the game as a stock CHL2MP_Player (ClientPutInServer,
	// then ClientActive spawns it) before it is flagged a bot here.
	edict_t *pEdict = engine->CreateFakeClient( szName );
	if ( !pEdict )
	{
		Msg( "[kart] Can't add kart bot %s: the server is full.\n", szName );
		return NULL;
	}

	CHL2MP_Player *pBot = ToHL2MPPlayer( CBaseEntity::Instance( pEdict ) );
	if ( !pBot )
		return NULL;

	pBot->AddFlag( FL_CLIENT | FL_FAKECLIENT );
	if ( pBot->IsInKart() )
	{
		// It spawned before it was a bot; bots take their color from their index.
		pBot->ApplyKartColor();
	}

	Msg( "[kart] Kart bot %s joined.\n", pBot->GetPlayerName() );
	return pBot;
}

//-----------------------------------------------------------------------------
// Fake clients whose entity isn't flagged a bot (one carried over a map
// change gets a fresh entity): flag them, so the kart bot drives them again.
//-----------------------------------------------------------------------------
static void KartBot_AdoptFakeClients( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || pPlayer->IsFakeClient() || pPlayer->IsHLTV() || pPlayer->IsReplay() )
			continue;

		player_info_t info;
		if ( !engine->GetPlayerInfo( i, &info ) || !info.fakeplayer || info.ishltv )
			continue;

		pPlayer->AddFlag( FL_CLIENT | FL_FAKECLIENT );
		if ( pPlayer->IsInKart() )
		{
			pPlayer->ApplyKartColor();
		}
	}
}

//-----------------------------------------------------------------------------
// Puts the bot back on the racing line at flDistance, or a little behind it
// or to the side when a kart or the world is in the way. False when no spot
// was free.
//-----------------------------------------------------------------------------
static bool KartBot_PutOnLine( CHL2MP_Player *pBot, const CKartRacingLine &line, float flDistance )
{
	static const float s_flBack[] = { 0.0f, 96.0f, 192.0f, 288.0f, 384.0f };
	static const float s_flSide[] = { 0.0f, 0.5f, -0.5f };

	for ( int b = 0; b < ARRAYSIZE( s_flBack ); b++ )
	{
		KartRacingLinePoint_t point;
		if ( !line.GetPoint( flDistance - s_flBack[b], point ) )
			return false;

		Vector vecRight = CrossProduct( point.dir, Vector( 0, 0, 1 ) );
		if ( VectorNormalize( vecRight ) == 0.0f )
			continue;

		for ( int s = 0; s < ARRAYSIZE( s_flSide ); s++ )
		{
			// Drop the kart onto whatever is below the spot: the road, a ramp.
			Vector vecSpot = point.pos + vecRight * ( s_flSide[s] * point.width );
			trace_t tr;
			UTIL_TraceHull( vecSpot + Vector( 0, 0, 128 ), vecSpot - Vector( 0, 0, 256 ), KART_HULL_MIN, KART_HULL_MAX,
				MASK_PLAYERSOLID, pBot, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
			if ( tr.startsolid || tr.fraction == 1.0f || ( tr.m_pEnt && tr.m_pEnt->IsPlayer() ) )
				continue;

			pBot->KartTeleport( tr.endpos, UTIL_VecToYaw( point.dir ) );
			return true;
		}
	}

	return false;
}

// The error (degrees, positive left) as seen from steer sign nSteer (+1 right):
// positive when the aim point lies the way nSteer turns.
static inline float KartBot_ErrorToward( float flError, int nSteer )
{
	return ( nSteer < 0 ) ? flError : -flError;
}

// Steer sign (+1 right, -1 left, 0 straight) toward an aim point flError
// degrees to the left, holding a turn until nearly lined up.
static int KartBot_SteerToward( float flError, int nLastSteer, float flDeadzone )
{
	int nDir = ( flError > 0.0f ) ? -1 : 1;
	float flAbs = fabs( flError );
	if ( flAbs > flDeadzone || ( nLastSteer == nDir && flAbs > flDeadzone * 0.33f ) )
		return nDir;
	return 0;
}

// Fastest speed, up to flTopSpeed, at which the steering (or a drift) can
// follow a line turning flCurve degrees per unit. The steering rate follows
// the speed as a fraction of kart_max_speed (flMaxSpeed).
static float KartBot_CornerSpeed( float flCurve, bool bDrift, float flMaxSpeed, float flTopSpeed )
{
	if ( flCurve <= 0.0001f )
		return flTopSpeed;

	for ( float flSpeed = flTopSpeed; flSpeed > KART_BOT_MIN_SPEED; flSpeed -= 25.0f )
	{
		float flRate = bDrift ? kart_drift_turn_max.GetFloat()
			: RemapValClamped( flSpeed / flMaxSpeed, 0.0f, 1.0f, kart_turn_rate_low.GetFloat(), kart_turn_rate_high.GetFloat() );
		if ( flSpeed * flCurve <= KART_BOT_TURN_MARGIN * flRate )
			return flSpeed;
	}

	return KART_BOT_MIN_SPEED;
}

//-----------------------------------------------------------------------------
// One bot's driving state.
//-----------------------------------------------------------------------------
struct KartBotState_t
{
	CHandle< CHL2MP_Player > hPlayer;	// the entity this state belongs to

	// Place on the racing line.
	bool	bOnLine;			// flDistance is the bot's place
	float	flDistance;			// along the line, [0, length)
	float	flProgress;			// distance driven along the line since it was found on it
	Vector	vecLastOrigin;

	// Unstuck.
	float	flBestProgress;		// furthest flProgress so far
	float	flProgressTime;		// when it last got KART_BOT_PROGRESS_STEP further (or reversed out)
	float	flStuckSince;		// when it last got further, reversing or not
	float	flReverseEnd;		// backing up until then, 0 when not
	int		nReverseSteer;

	// Steering and drift.
	int		nSteer;				// last steer sign, for the dead zone's hysteresis
	int		nDrift;				// steer sign of the corner being drifted, 0 when not
	float	flDriftStart;		// when it hopped into the drift
	float	flNextDrift;		// no new drift before then
	int		nDriftChoice;		// for the sharp corner ahead: 1 drift it, -1 don't, 0 not decided

	// Skill.
	float	flNoise;			// degrees the aim is off, positive left
	float	flNoiseTarget;		// what it wanders toward
	float	flNextNoise;		// when it picks a new one
	float	flTopSpeedScale;	// last top speed scale, for the debug text

	// Items.
	int		nLastItem;			// held item and count last tick, to see a new one
	int		nLastItemCount;
	float	flItemHeldSince;
	float	flItemReadyTime;	// no use before then
	bool	bItemPressed;		// pressed use last tick: release it first
	const char *pszItemIntent;	// why it used its item last, for the debug text

	float	flNextDebugDraw;

	void Reset( CHL2MP_Player *pPlayer )
	{
		hPlayer = pPlayer;
		bOnLine = false;
		flDistance = 0.0f;
		vecLastOrigin = vec3_origin;
		nSteer = 0;
		nDrift = 0;
		flDriftStart = 0.0f;
		flNextDrift = 0.0f;
		nDriftChoice = 0;
		flNoise = 0.0f;
		flNoiseTarget = 0.0f;
		flNextNoise = 0.0f;
		flTopSpeedScale = 1.0f;
		nLastItem = KART_ITEM_NONE;
		nLastItemCount = 0;
		flItemHeldSince = 0.0f;
		flItemReadyTime = 0.0f;
		bItemPressed = false;
		pszItemIntent = "";
		flNextDebugDraw = 0.0f;
		ResetProgress();
	}

	// Starts the stuck timers over: just found on the line, or held still.
	void ResetProgress( void )
	{
		flProgress = 0.0f;
		flBestProgress = 0.0f;
		flProgressTime = gpGlobals->curtime;
		flStuckSince = gpGlobals->curtime;
		flReverseEnd = 0.0f;
		nReverseSteer = 0;
	}
};

//-----------------------------------------------------------------------------
// Drives the kart bots: one usercmd per bot per tick, queued before the
// players simulate. Keeps kart_bot_quota.
//-----------------------------------------------------------------------------
class CKartBotSystem : public CAutoGameSystemPerFrame
{
public:
	CKartBotSystem() : CAutoGameSystemPerFrame( "CKartBotSystem" )
	{
		m_flNextQuotaCheck = 0.0f;
		m_bWarnedNoLine = false;
		m_flNextBoxList = 0.0f;
		m_bBestHuman = false;
		m_flBestHumanProgress = 0.0f;
	}

	virtual void LevelInitPostEntity( void )
	{
		m_flNextQuotaCheck = 0.0f;
		m_bWarnedNoLine = false;
		m_flNextBoxList = 0.0f;
		m_Boxes.RemoveAll();
		m_Projectiles.RemoveAll();
		for ( int i = 0; i < ARRAYSIZE( m_Bots ); i++ )
		{
			m_Bots[i].Reset( NULL );
		}
	}

	virtual void FrameUpdatePreEntityThink( void )
	{
		if ( engine->IsPaused() )
			return;

		if ( gpGlobals->curtime >= m_flNextQuotaCheck )
		{
			m_flNextQuotaCheck = gpGlobals->curtime + 1.0f;
			UpdateQuota();
		}

		if ( KartBot_Count() == 0 )
			return;

		UpdateWorld();

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHL2MP_Player *pBot = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
			if ( !KartBot_IsKartBot( pBot ) || !pBot->IsConnected() )
				continue;

			KartBotState_t &state = m_Bots[i];
			if ( state.hPlayer != pBot )
			{
				state.Reset( pBot );
			}

			// Queued like a client's command, so the player runs it through
			// PlayerRunCommand and the kart movement when it simulates.
			CUserCmd cmd;
			BuildCommand( pBot, state, cmd );
			pBot->ProcessUsercmds( &cmd, 1, 1, 0, false );
		}
	}

private:
	void UpdateQuota( void )
	{
		KartBot_AdoptFakeClients();

		int nBots = KartBot_Count();
		int nQuota = kart_bot_quota.GetInt();
		if ( nBots < nQuota )
		{
			KartBot_Add( NULL );
		}
		else if ( nBots > nQuota )
		{
			CHL2MP_Player *pBot = KartBot_Newest();
			if ( pBot )
			{
				KartBot_Kick( pBot );
			}
		}
	}

	// What every bot looks at this tick: the best human's progress, item
	// boxes (listed once a second) and projectiles in flight.
	void UpdateWorld( void )
	{
		m_bBestHuman = false;
		m_flBestHumanProgress = 0.0f;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer || pPlayer->IsFakeClient() || !pPlayer->IsAlive() || pPlayer->IsKartFinished() || !CKartRaceManager::IsRacing( pPlayer ) )
				continue;

			if ( !m_bBestHuman || pPlayer->GetKartProgress() > m_flBestHumanProgress )
			{
				m_bBestHuman = true;
				m_flBestHumanProgress = pPlayer->GetKartProgress();
			}
		}

		if ( gpGlobals->curtime >= m_flNextBoxList )
		{
			m_flNextBoxList = gpGlobals->curtime + KART_BOT_BOX_LIST_INTERVAL;
			m_Boxes.RemoveAll();
			for ( CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "kart_item_box" ); pEnt; pEnt = gEntList.FindEntityByClassname( pEnt, "kart_item_box" ) )
			{
				m_Boxes.AddToTail( static_cast< CKartItemBox * >( pEnt ) );
			}
		}

		m_Projectiles.RemoveAll();
		for ( CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, KART_PROJECTILE_CLASSNAMES ); pEnt; pEnt = gEntList.FindEntityByClassname( pEnt, KART_PROJECTILE_CLASSNAMES ) )
		{
			m_Projectiles.AddToTail( pEnt );
		}
	}

	void BuildCommand( CHL2MP_Player *pBot, KartBotState_t &st, CUserCmd &cmd );
	float GetTopSpeedScale( CHL2MP_Player *pBot, const KartBotSkill_t &skill, float flLineLength ) const;
	bool SteerTowardBox( CHL2MP_Player *pBot, float flTravelYaw, float &flError ) const;
	void UseItem( CHL2MP_Player *pBot, KartBotState_t &st, const KartBotSkill_t &skill, float flTravelYaw, float flTurn, float flError,
		bool bReversing, CUserCmd &cmd );

	KartBotState_t m_Bots[MAX_PLAYERS + 1];	// by entity index
	float m_flNextQuotaCheck;
	bool m_bWarnedNoLine;

	bool m_bBestHuman;				// a human is racing
	float m_flBestHumanProgress;	// the furthest one's GetKartProgress, in laps
	float m_flNextBoxList;
	CUtlVector< CHandle< CKartItemBox > > m_Boxes;
	CUtlVector< EHANDLE > m_Projectiles;
};

static CKartBotSystem g_KartBotSystem;

//-----------------------------------------------------------------------------
// Purpose: The bot's command for this tick.
//-----------------------------------------------------------------------------
void CKartBotSystem::BuildCommand( CHL2MP_Player *pBot, KartBotState_t &st, CUserCmd &cmd )
{
	const float flNow = gpGlobals->curtime;

	cmd.Reset();
	cmd.command_number = gpGlobals->tickcount;
	cmd.tick_count = gpGlobals->tickcount;
	cmd.random_seed = RandomInt( 0, 0x7fffffff );
	cmd.viewangles = pBot->EyeAngles();

	if ( !pBot->IsAlive() )
	{
		// Killed from the console: tap jump to respawn.
		cmd.buttons = ( ( gpGlobals->tickcount / 8 ) & 1 ) ? IN_JUMP : 0;
		st.bOnLine = false;
		return;
	}

	// On foot (kart_enabled 0) it just stands there.
	if ( !pBot->IsInKart() )
		return;

	cmd.viewangles = QAngle( 0.0f, pBot->GetKartYaw(), 0.0f );

	// Held on the grid or at the results: no input, and it isn't stuck.
	if ( pBot->GetFlags() & FL_FROZEN )
	{
		st.nDrift = 0;
		st.ResetProgress();
		return;
	}

	CKartRaceManager *pManager = KartRaceManager();
	if ( !pManager || !pManager->HasRacingLine() )
	{
		if ( !m_bWarnedNoLine )
		{
			Warning( "[kart] This map has no racing line (kart_path_node), so kart bots don't drive.\n" );
			m_bWarnedNoLine = true;
		}
		return;
	}

	const CKartRacingLine &line = pManager->GetRacingLine();
	const float flLength = line.GetLength();
	const Vector vecOrigin = pBot->GetAbsOrigin();

	// Where it is along the line. Near its last place, so it can't jump to a
	// stretch of track passing close by; anywhere after a respawn or teleport.
	KartRacingLinePoint_t here;
	if ( !st.bOnLine || ( vecOrigin - st.vecLastOrigin ).Length2DSqr() > Square( KART_BOT_TELEPORT_DIST ) )
	{
		line.GetNearestPoint( vecOrigin, here );
		st.nDrift = 0;
		st.ResetProgress();
	}
	else
	{
		line.GetNearestPointNear( vecOrigin, st.flDistance, KART_BOT_TRACK_WINDOW, here );

		float flDelta = here.distance - st.flDistance;
		if ( flDelta > 0.5f * flLength )
		{
			flDelta -= flLength;
		}
		else if ( flDelta < -0.5f * flLength )
		{
			flDelta += flLength;
		}
		st.flProgress += flDelta;
	}
	st.bOnLine = true;
	st.flDistance = here.distance;
	st.vecLastOrigin = vecOrigin;

	if ( st.flProgress > st.flBestProgress + KART_BOT_PROGRESS_STEP )
	{
		st.flBestProgress = st.flProgress;
		st.flProgressTime = flNow;
		st.flStuckSince = flNow;
	}

	// Nowhere for too long (stuck for good, or driving the wrong way): back onto the line.
	float flResetTime = kart_bot_reset_time.GetFloat();
	if ( flResetTime > 0.0f && flNow - st.flStuckSince > flResetTime )
	{
		if ( KartBot_PutOnLine( pBot, line, st.flDistance ) )
		{
			DevMsg( "[kart] Kart bot %s was stuck; put it back on the racing line.\n", pBot->GetPlayerName() );
			st.bOnLine = false;
			return;
		}
		st.flStuckSince = flNow;	// no free spot: try again later
	}

	// Top speed: the difficulty's, rubber-banded.
	const KartBotSkill_t &skill = KartBot_GetSkill();
	st.flTopSpeedScale = GetTopSpeedScale( pBot, skill, flLength );
	pBot->SetKartTopSpeedScale( st.flTopSpeedScale );

	const float flMaxSpeed = MAX( kart_max_speed.GetFloat(), 1.0f );
	const float flTopSpeed = flMaxSpeed * st.flTopSpeedScale;
	const float flSpeed = pBot->GetKartSpeed();
	const float flDeadzone = kart_bot_steer_deadzone.GetFloat() * skill.flDeadzoneScale;

	// Pure pursuit: the aim point ahead on the line, and how far it is off the
	// direction the kart travels (its heading less the drift's slip).
	float flLookahead = Lerp( clamp( fabs( flSpeed ) / flMaxSpeed, 0.0f, 1.0f ), kart_bot_lookahead_min.GetFloat(), kart_bot_lookahead.GetFloat() );
	KartRacingLinePoint_t aim;
	line.GetPoint( st.flDistance + flLookahead, aim );
	float flTravelYaw = pBot->GetKartYaw() - pBot->GetKartSlipAngle();
	float flError = AngleDiff( UTIL_VecToYaw( aim.pos - vecOrigin ), flTravelYaw );	// positive: to the left

	// Steering noise: the aim wanders slowly, more at lower difficulty.
	if ( flNow >= st.flNextNoise )
	{
		st.flNextNoise = flNow + RandomFloat( KART_BOT_NOISE_MIN_TIME, KART_BOT_NOISE_MAX_TIME );
		st.flNoiseTarget = RandomFloat( -skill.flSteerNoise, skill.flSteerNoise );
	}
	st.flNoise = Approach( st.flNoiseTarget, st.flNoise, KART_BOT_NOISE_RATE * gpGlobals->frametime );
	flError += st.flNoise;

	// Empty-handed: lean toward an item box just ahead.
	bool bBoxSteer = false;
	if ( st.nDrift == 0 && kart_bot_items.GetBool() && kart_items_enabled.GetBool() && pBot->GetKartItem() == KART_ITEM_NONE )
	{
		bBoxSteer = SteerTowardBox( pBot, flTravelYaw, flError );
	}

	// The line ahead: how far it turns within the drift distance and just
	// ahead (the drift exit), its sharpest stretch and lowest speed scale
	// within braking distance, and drift hints.
	const float flBrakeSpan = 128.0f + MAX( flSpeed, 0.0f ) * 0.8f;
	const float flDriftSpan = MAX( kart_bot_drift_distance.GetFloat(), KART_BOT_PROBE_STEP );
	const int nProbes = MIN( (int)( ( MAX( flBrakeSpan, flDriftSpan ) + KART_BOT_PROBE_STEP * KART_BOT_CURVE_PROBES ) / KART_BOT_PROBE_STEP ) + 1, KART_BOT_MAX_PROBES );

	float flHeadings[KART_BOT_MAX_PROBES];
	float flSpeedScale = 1.0f;
	bool bDriftHint = false;
	for ( int k = 0; k < nProbes; k++ )
	{
		KartRacingLinePoint_t probe;
		line.GetPoint( st.flDistance + k * KART_BOT_PROBE_STEP, probe );
		flHeadings[k] = UTIL_VecToYaw( probe.dir );

		if ( k * KART_BOT_PROBE_STEP <= flBrakeSpan )
		{
			flSpeedScale = MIN( flSpeedScale, probe.speedScale );
		}
		if ( k * KART_BOT_PROBE_STEP <= flDriftSpan && probe.drift )
		{
			bDriftHint = true;
		}
	}

	float flTurn = 0.0f;		// within the drift distance, degrees, positive left
	float flExitTurn = 0.0f;	// within KART_BOT_DRIFT_EXIT_SPAN
	float flCurve = 0.0f;		// sharpest, degrees per unit
	for ( int k = 0; k + 1 < nProbes; k++ )
	{
		float flStep = AngleDiff( flHeadings[k + 1], flHeadings[k] );
		if ( ( k + 1 ) * KART_BOT_PROBE_STEP <= flDriftSpan )
		{
			flTurn += flStep;
		}
		if ( ( k + 1 ) * KART_BOT_PROBE_STEP <= KART_BOT_DRIFT_EXIT_SPAN )
		{
			flExitTurn += flStep;
		}

		if ( k * KART_BOT_PROBE_STEP <= flBrakeSpan && k + KART_BOT_CURVE_PROBES < nProbes )
		{
			float flWindow = AngleDiff( flHeadings[k + KART_BOT_CURVE_PROBES], flHeadings[k] );
			flCurve = MAX( flCurve, fabs( flWindow ) / ( KART_BOT_CURVE_PROBES * KART_BOT_PROBE_STEP ) );
		}
	}

	const float flDriftMinSpeed = kart_drift_min_speed.GetFloat();
	const float flDriftAngle = kart_bot_drift_angle.GetFloat() * ( bDriftHint ? 0.5f : 1.0f );
	const bool bSharpCorner = fabs( flTurn ) >= flDriftAngle;

	// Whether to drift this corner, decided once as it comes up.
	if ( !bSharpCorner )
	{
		st.nDriftChoice = 0;
	}
	else if ( st.nDriftChoice == 0 )
	{
		st.nDriftChoice = ( RandomFloat( 0.0f, 1.0f ) < skill.flDriftChance ) ? 1 : -1;
	}
	const bool bDriftCorner = bSharpCorner && st.nDriftChoice > 0;
	const int nCornerSteer = ( flTurn > 0.0f ) ? -1 : 1;	// the steer sign that follows it
	const int nKartDrift = pBot->GetKartDriftDir();

	// Stuck: back up for a while, swinging the nose toward the aim point (in
	// reverse the steering turns the kart the other way).
	if ( st.flReverseEnd > 0.0f && flNow >= st.flReverseEnd )
	{
		st.flReverseEnd = 0.0f;
		st.flProgressTime = flNow;
	}
	if ( st.flReverseEnd == 0.0f && flNow - st.flProgressTime > kart_bot_stuck_time.GetFloat() )
	{
		st.flReverseEnd = flNow + kart_bot_reverse_time.GetFloat();
		st.nReverseSteer = ( flError > 0.0f ) ? 1 : -1;
	}
	const bool bReversing = ( st.flReverseEnd > 0.0f );

	// Drift: start one into a sharp corner once the aim point turns into it,
	// end it as the corner straightens out.
	if ( st.nDrift == 0 )
	{
		if ( bDriftCorner && !bReversing && flNow >= st.flNextDrift && pBot->GetGroundEntity() != NULL
			&& flSpeed >= flDriftMinSpeed + KART_BOT_DRIFT_SPEED_MARGIN && KartBot_ErrorToward( flError, nCornerSteer ) > flDeadzone )
		{
			st.nDrift = nCornerSteer;
			st.flDriftStart = flNow;
		}
	}
	else
	{
		bool bEnd;
		if ( bReversing || flNow - st.flDriftStart > KART_BOT_DRIFT_MAX_TIME )
		{
			bEnd = true;
		}
		else if ( nKartDrift == 0 )
		{
			// Still hopping, or the drift didn't start (too slow) or a bump ended it.
			bEnd = ( flNow - st.flDriftStart > KART_BOT_DRIFT_START_TIME );
		}
		else if ( nKartDrift != st.nDrift )
		{
			bEnd = true;
		}
		else
		{
			float flToward = KartBot_ErrorToward( flError, st.nDrift );
			bool bCornerDone = KartBot_ErrorToward( flExitTurn, st.nDrift ) < KART_BOT_DRIFT_EXIT_ANGLE;
			bEnd = ( flToward < -KART_BOT_DRIFT_OVERSWING ) || ( bCornerDone && flToward < flDeadzone );
		}

		if ( bEnd )
		{
			st.nDrift = 0;
			st.flNextDrift = flNow + KART_BOT_DRIFT_COOLDOWN;
		}
	}

	// Target speed.
	float flTarget = MIN( flTopSpeed * flSpeedScale, KartBot_CornerSpeed( flCurve, bDriftCorner || st.nDrift != 0, flMaxSpeed, flTopSpeed ) );
	if ( st.nDrift == 0 && fabs( flError ) > KART_BOT_WIDE_ERROR )
	{
		flTarget = MIN( flTarget, 0.5f * flMaxSpeed );
	}
	flTarget = MAX( flTarget, KART_BOT_MIN_SPEED );
	if ( st.nDrift != 0 || bDriftCorner )
	{
		flTarget = MAX( flTarget, flDriftMinSpeed + KART_BOT_DRIFT_SPEED_MARGIN );
	}

	int nThrottle = 0;
	if ( flSpeed < flTarget - 10.0f )
	{
		nThrottle = 1;
	}
	else if ( flSpeed > flTarget + KART_BOT_BRAKE_BAND && st.nDrift == 0 )
	{
		nThrottle = -1;
	}

	int nSteer;
	if ( bReversing )
	{
		nThrottle = -1;
		nSteer = st.nReverseSteer;
	}
	else if ( st.nDrift != 0 )
	{
		cmd.buttons |= IN_JUMP;	// the first tick is the hop, holding it is the drift
		if ( nKartDrift == 0 )
		{
			// Hopping: steer into the corner, so the drift starts that way on
			// landing, but not past the aim point: a hop steers at the full rate.
			// Landing without steering, the drift starts as soon as it steers in.
			nSteer = ( KartBot_ErrorToward( flError, st.nDrift ) > -flDeadzone ) ? st.nDrift : 0;
		}
		else
		{
			// Drifting: into the drift tightens it, against it widens it.
			float flToward = KartBot_ErrorToward( flError, st.nDrift );
			nSteer = ( flToward > flDeadzone ) ? st.nDrift : ( ( flToward < -flDeadzone ) ? -st.nDrift : 0 );
		}
	}
	else
	{
		nSteer = KartBot_SteerToward( flError, st.nSteer, flDeadzone );
	}
	st.nSteer = bReversing ? 0 : nSteer;

	cmd.forwardmove = nThrottle * KART_BOT_INPUT;
	cmd.sidemove = nSteer * KART_BOT_INPUT;

	UseItem( pBot, st, skill, flTravelYaw, flTurn, flError, bReversing, cmd );

	if ( kart_bot_debug.GetBool() && flNow >= st.flNextDebugDraw )
	{
		st.flNextDebugDraw = flNow + KART_BOT_DEBUG_INTERVAL;
		const float flDuration = KART_BOT_DEBUG_INTERVAL + 0.05f;

		// The aim point: yellow driving, orange drifting, red backing up.
		int r = 255, g = 255, b = 0;
		if ( bReversing )
		{
			g = 0;
		}
		else if ( st.nDrift != 0 )
		{
			g = 140;
		}
		NDebugOverlay::Line( vecOrigin + Vector( 0, 0, 24 ), aim.pos, r, g, b, true, flDuration );
		NDebugOverlay::Cross3D( aim.pos, 16.0f, r, g, b, true, flDuration );

		// The line it plans along, from its place to the end of the probes.
		KartRacingLinePoint_t from = here;
		for ( int k = 1; k < nProbes; k++ )
		{
			KartRacingLinePoint_t to;
			line.GetPoint( st.flDistance + k * KART_BOT_PROBE_STEP, to );
			NDebugOverlay::Line( from.pos, to.pos, 0, 255, 255, true, flDuration );
			from = to;
		}

		static const char *s_pszSteer[] = { "left", "-", "right" };
		static const char *s_pszThrottle[] = { "brake", "coast", "gas" };
		char szText[160];
		Q_snprintf( szText, sizeof( szText ), "%s  %.0f/%.0f u/s  %s  steer %s  turn %.0f%s%s", pBot->GetPlayerName(), flSpeed, flTarget,
			s_pszThrottle[nThrottle + 1], s_pszSteer[nSteer + 1], flTurn,
			st.nDrift ? ( st.nDrift > 0 ? "  drift R" : "  drift L" ) : "", bReversing ? "  REVERSE" : "" );
		NDebugOverlay::Text( vecOrigin + Vector( 0, 0, KART_HULL_MAX.z + 48.0f ), szText, false, flDuration );

		Q_snprintf( szText, sizeof( szText ), "skill %d  top x%.2f  noise %+.0f  item %s%s  %s", kart_bot_difficulty.GetInt(), st.flTopSpeedScale, st.flNoise,
			KartItem_GetName( pBot->GetKartItem() ), bBoxSteer ? "  -> box" : "", st.pszItemIntent );
		NDebugOverlay::Text( vecOrigin + Vector( 0, 0, KART_HULL_MAX.z + 32.0f ), szText, false, flDuration );

		if ( flNow - st.flProgressTime > 0.5f )
		{
			Q_snprintf( szText, sizeof( szText ), "no progress %.1f s", flNow - st.flStuckSince );
			NDebugOverlay::Text( vecOrigin + Vector( 0, 0, KART_HULL_MAX.z + 64.0f ), szText, false, flDuration );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The bot's top speed, as a kart_max_speed multiplier: the
//			difficulty's, then rubber-banded by its gap in race progress to the
//			best human still racing.
//-----------------------------------------------------------------------------
float CKartBotSystem::GetTopSpeedScale( CHL2MP_Player *pBot, const KartBotSkill_t &skill, float flLineLength ) const
{
	float flScale = skill.flTopSpeed;

	float flRubberband = kart_bot_rubberband.GetFloat();
	if ( flRubberband > 0.0f && m_bBestHuman && !pBot->IsKartFinished() && HL2MPRules() && HL2MPRules()->IsKartRaceRunning() )
	{
		// Progress is in laps; positive gap: the human is ahead.
		float flGap = ( m_flBestHumanProgress - pBot->GetKartProgress() ) * flLineLength;
		flScale *= 1.0f + flRubberband * clamp( flGap / kart_bot_rubberband_range.GetFloat(), -1.0f, 1.0f );
	}

	return flScale;
}

//-----------------------------------------------------------------------------
// Purpose: Bends flError (degrees, positive left) part of the way toward the
//			nearest available item box ahead, when one is close and near the
//			aim point. True when it did.
//-----------------------------------------------------------------------------
bool CKartBotSystem::SteerTowardBox( CHL2MP_Player *pBot, float flTravelYaw, float &flError ) const
{
	const Vector vecOrigin = pBot->GetAbsOrigin();

	float flBestDist = KART_BOT_BOX_RANGE;
	float flBoxError = 0.0f;
	bool bFound = false;
	for ( int i = 0; i < m_Boxes.Count(); i++ )
	{
		CKartItemBox *pBox = m_Boxes[i];
		if ( !pBox || !pBox->IsAvailable() )
			continue;

		Vector vecTo = pBox->GetAbsOrigin() - vecOrigin;
		float flDist = vecTo.Length2D();
		if ( flDist < KART_BOT_BOX_MIN_DIST || flDist >= flBestDist || fabs( vecTo.z ) > 128.0f )
			continue;

		// Near where it is heading anyway: it only leans off the line a little.
		float flToBox = AngleDiff( UTIL_VecToYaw( vecTo ), flTravelYaw );
		if ( fabs( flToBox - flError ) > KART_BOT_BOX_ANGLE )
			continue;

		flBestDist = flDist;
		flBoxError = flToBox;
		bFound = true;
	}

	if ( bFound )
	{
		flError = Lerp( KART_BOT_BOX_PULL, flError, flBoxError );
	}
	return bFound;
}

// The nearest other kart in a cone around flYaw (degrees either way) within
// flRange, in clear sight. NULL when none.
static CHL2MP_Player *KartBot_FindKartInCone( CHL2MP_Player *pBot, float flYaw, float flAngle, float flRange )
{
	const Vector vecEye = pBot->GetAbsOrigin() + Vector( 0, 0, 24 );

	CHL2MP_Player *pBest = NULL;
	float flBestDist = flRange;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pOther = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pOther || pOther == pBot || !pOther->IsAlive() || !pOther->IsInKart() )
			continue;

		Vector vecTo = pOther->GetAbsOrigin() - pBot->GetAbsOrigin();
		float flDist = vecTo.Length2D();
		if ( flDist >= flBestDist || fabs( AngleDiff( UTIL_VecToYaw( vecTo ), flYaw ) ) > flAngle )
			continue;

		trace_t tr;
		UTIL_TraceLine( vecEye, pOther->GetAbsOrigin() + Vector( 0, 0, 24 ), MASK_SOLID_BRUSHONLY, pBot, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f )
			continue;

		pBest = pOther;
		flBestDist = flDist;
	}
	return pBest;
}

// A kart holding a hubcap or seeker, its roulette stopped.
static bool KartBot_IsArmed( CHL2MP_Player *pPlayer )
{
	int item = pPlayer->GetKartItem();
	return ( item == KART_ITEM_HUBCAP || item == KART_ITEM_SEEKER ) && !pPlayer->IsKartRouletteSpinning();
}

//-----------------------------------------------------------------------------
// Purpose: Uses the held item when its rule says so: presses attack (forward)
//			or attack2 (backward) for one tick.
//-----------------------------------------------------------------------------
void CKartBotSystem::UseItem( CHL2MP_Player *pBot, KartBotState_t &st, const KartBotSkill_t &skill, float flTravelYaw, float flTurn, float flError,
	bool bReversing, CUserCmd &cmd )
{
	const float flNow = gpGlobals->curtime;
	const int item = pBot->GetKartItem();
	const int nCount = pBot->GetKartItemCount();

	// A new item, or one use of it spent: react to it after a while.
	if ( item != st.nLastItem || nCount != st.nLastItemCount )
	{
		if ( item != st.nLastItem )
		{
			st.flItemHeldSince = flNow;
		}
		float flDelay = skill.flItemDelay * RandomFloat( 0.75f, 1.25f );
		if ( item == st.nLastItem )
		{
			flDelay += KART_BOT_ITEM_REUSE;
		}
		st.flItemReadyTime = MAX( pBot->GetKartRouletteEnd(), flNow ) + flDelay;
		st.nLastItem = item;
		st.nLastItemCount = nCount;
	}

	// Let go of the button for a tick after a press, so the next is a new press.
	if ( st.bItemPressed )
	{
		st.bItemPressed = false;
		return;
	}

	if ( !kart_bot_items.GetBool() || !kart_items_enabled.GetBool() || !KartItem_IsValid( item ) || bReversing
		|| pBot->IsKartRouletteSpinning() || flNow < st.flItemReadyTime )
		return;

	const float flBackYaw = AngleNormalize( flTravelYaw + 180.0f );
	const float flMaxSpeed = MAX( kart_max_speed.GetFloat(), 1.0f );

	bool bUse = false;
	bool bBackward = false;
	const char *pszIntent = "";
	switch ( item )
	{
	case KART_ITEM_HUBCAP:
		if ( KartBot_FindKartInCone( pBot, flTravelYaw, KART_BOT_THROW_ANGLE, KART_BOT_THROW_RANGE ) )
		{
			bUse = true;
			pszIntent = "hubcap at kart ahead";
		}
		else if ( KartBot_FindKartInCone( pBot, flBackYaw, KART_BOT_BEHIND_ANGLE, KART_BOT_BEHIND_RANGE ) )
		{
			bUse = bBackward = true;
			pszIntent = "hubcap at kart behind";
		}
		break;

	case KART_ITEM_SEEKER:
		// It homes on the kart ahead in the race, so any kart ahead will do.
		if ( pBot->GetKartRacePosition() > 1 || KartBot_FindKartInCone( pBot, flTravelYaw, 60.0f, KART_BOT_SEEKER_RANGE ) )
		{
			bUse = true;
			pszIntent = "seeker, kart ahead";
		}
		break;

	case KART_ITEM_OIL_SLICK:
		if ( KartBot_FindKartInCone( pBot, flBackYaw, KART_BOT_OIL_ANGLE, KART_BOT_BEHIND_RANGE ) )
		{
			bUse = bBackward = true;
			pszIntent = "oil, kart behind";
		}
		break;

	case KART_ITEM_NITRO_CAN:
		if ( fabs( flTurn ) < KART_BOT_NITRO_TURN && fabs( flError ) < KART_BOT_NITRO_TURN && !pBot->IsDrifting() && !pBot->IsKartBoosting()
			&& pBot->GetGroundEntity() != NULL && pBot->GetKartSpeed() > 0.5f * flMaxSpeed )
		{
			bUse = true;
			pszIntent = "nitro on a straight";
		}
		break;

	case KART_ITEM_BUFFER:
		{
			// A projectile closing in, or an armed kart close behind.
			const Vector vecOrigin = pBot->GetAbsOrigin();
			for ( int i = 0; i < m_Projectiles.Count() && !bUse; i++ )
			{
				CBaseEntity *pProj = m_Projectiles[i];
				if ( !pProj || pProj->GetOwnerEntity() == pBot )
					continue;

				Vector vecTo = vecOrigin - pProj->GetAbsOrigin();
				if ( vecTo.LengthSqr() < Square( KART_BOT_THREAT_RANGE ) && DotProduct( pProj->GetAbsVelocity() - pBot->GetAbsVelocity(), vecTo ) > 0.0f )
				{
					bUse = true;
					pszIntent = "buffer, projectile";
				}
			}

			CHL2MP_Player *pBehind = bUse ? NULL : KartBot_FindKartInCone( pBot, flBackYaw, 30.0f, KART_BOT_THREAT_RANGE );
			if ( pBehind && KartBot_IsArmed( pBehind ) )
			{
				bUse = true;
				pszIntent = "buffer, armed kart behind";
			}
		}
		break;
	}

	// Held too long with no reason: use it anyway (the oil slick dropped behind).
	float flHoldMax = kart_bot_item_hold_max.GetFloat();
	if ( !bUse && flHoldMax > 0.0f && flNow - st.flItemHeldSince > flHoldMax )
	{
		bUse = true;
		bBackward = ( item == KART_ITEM_OIL_SLICK );
		pszIntent = "held too long";
	}

	if ( !bUse )
		return;

	cmd.buttons |= bBackward ? IN_ATTACK2 : IN_ATTACK;
	st.bItemPressed = true;
	st.flItemReadyTime = flNow + KART_BOT_ITEM_REUSE;	// when the item couldn't be used right now
	st.pszItemIntent = pszIntent;

	if ( kart_debug_server.GetBool() || kart_bot_debug.GetBool() )
	{
		Msg( "[kart] Kart bot %s uses %s%s: %s\n", pBot->GetPlayerName(), KartItem_GetName( item ), bBackward ? " backward" : "", pszIntent );
	}
}

// ##################################################################################
//	>> Commands
// ##################################################################################
CON_COMMAND_F( kart_bot_add, "Add a kart bot: kart_bot_add [name]. Raises kart_bot_quota by one.", FCVAR_GAMEDLL )
{
	// Listen server host or rcon only.
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( KartBot_Add( args.ArgC() > 1 ? args.Arg( 1 ) : NULL ) )
	{
		kart_bot_quota.SetValue( kart_bot_quota.GetInt() + 1 );
	}
}

CON_COMMAND_F( kart_bot_kick, "Kick kart bots: kart_bot_kick [name|all], the newest one without a name. Lowers kart_bot_quota to match.", FCVAR_GAMEDLL )
{
	// Listen server host or rcon only.
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *pszWhich = ( args.ArgC() > 1 ) ? args.Arg( 1 ) : NULL;
	bool bAll = pszWhich && !Q_stricmp( pszWhich, "all" );

	int nKicked = 0;
	if ( !pszWhich )
	{
		CHL2MP_Player *pBot = KartBot_Newest();
		if ( pBot )
		{
			KartBot_Kick( pBot );
			nKicked++;
		}
	}
	else
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( KartBot_IsKartBot( pPlayer ) && ( bAll || !Q_stricmp( pPlayer->GetPlayerName(), pszWhich ) ) )
			{
				KartBot_Kick( pPlayer );
				nKicked++;
			}
		}
	}

	if ( nKicked == 0 )
	{
		Msg( "kart_bot_kick: no kart bot%s%s.\n", ( pszWhich && !bAll ) ? " named " : "", ( pszWhich && !bAll ) ? pszWhich : "" );
		return;
	}

	kart_bot_quota.SetValue( MAX( kart_bot_quota.GetInt() - nKicked, 0 ) );
}
