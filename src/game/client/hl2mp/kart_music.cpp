//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart race music, played by each client on its own.
//
//			The track is the kart_race_manager's "music" (a Kart.Music.* game
//			sound, networked on the game rules). It starts at GO, loops while
//			the race runs, and fades out when the local kart finishes (with
//			the finish sting) or when the race ends for everyone.
//			kart_music_volume scales it; 0 silences it.
//
//=============================================================================//

#include "cbase.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "kart_race_shared.h"
#include "igamesystem.h"
#include "soundenvelope.h"
#include "soundchars.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

extern ISoundEmitterSystemBase *soundemitterbase;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar kart_music_volume( "kart_music_volume", "0.6", FCVAR_ARCHIVE, "Volume of the kart race music and finish sting, 0 to 1. 0 turns the music off.", true, 0.0f, true, 1.0f );
ConVar kart_music_fade_time( "kart_music_fade_time", "2", FCVAR_ARCHIVE, "Seconds the kart race music takes to fade out at the finish.", true, 0.0f, true, 10.0f );

class CKartMusic : public CAutoGameSystemPerFrame
{
public:
	CKartMusic() : CAutoGameSystemPerFrame( "CKartMusic" )
	{
		m_pTrack = NULL;
		m_pSting = NULL;
		m_flTrackDuration = 0.0f;
		m_flTrackEndTime = 0.0f;
		m_bDoneThisRace = false;
		m_flVolume = -1.0f;
	}

	virtual void LevelShutdownPreEntity( void )
	{
		Stop( 0.0f );
		StopSting();
		m_bDoneThisRace = false;
	}

	virtual void Update( float frametime );

private:
	void Start( const char *pszSound );
	void Stop( float flFadeTime );
	void StopSting( void );
	void PlaySting( void );
	CSoundPatch *CreatePatch( const char *pszSound );

	CSoundPatch *m_pTrack;
	CSoundPatch *m_pSting;
	char m_szTrack[KART_MUSIC_NAME_LENGTH];
	float m_flTrackDuration;	// seconds, 0 when unknown (then it doesn't loop)
	float m_flTrackEndTime;		// realtime the track ends and starts over
	bool m_bDoneThisRace;		// the local kart finished: no music until the next race
	float m_flVolume;			// kart_music_volume the patches were last set to
};

static CKartMusic g_KartMusic;

CSoundPatch *CKartMusic::CreatePatch( const char *pszSound )
{
	C_BaseEntity::PrecacheScriptSound( pszSound );

	CLocalPlayerFilter filter;
	return CSoundEnvelopeController::GetController().SoundCreate( filter, SOUND_FROM_LOCAL_PLAYER, pszSound );
}

void CKartMusic::Start( const char *pszSound )
{
	Stop( 0.0f );

	m_pTrack = CreatePatch( pszSound );
	if ( !m_pTrack )
		return;

	Q_strncpy( m_szTrack, pszSound, sizeof( m_szTrack ) );
	m_flVolume = kart_music_volume.GetFloat();
	CSoundEnvelopeController::GetController().Play( m_pTrack, m_flVolume, 100 );

	// mp3s don't loop by themselves: start the track over once it has played.
	m_flTrackDuration = 0.0f;
	CSoundParameters params;
	if ( soundemitterbase->GetParametersForSound( pszSound, params, GENDER_NONE ) )
	{
		m_flTrackDuration = enginesound->GetSoundDuration( PSkipSoundChars( params.soundname ) );
	}
	m_flTrackEndTime = gpGlobals->realtime + m_flTrackDuration;
}

void CKartMusic::Stop( float flFadeTime )
{
	if ( !m_pTrack )
		return;

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( flFadeTime > 0.0f )
	{
		controller.SoundFadeOut( m_pTrack, flFadeTime, true );
	}
	else
	{
		controller.SoundDestroy( m_pTrack );
	}
	m_pTrack = NULL;
}

void CKartMusic::StopSting( void )
{
	if ( m_pSting )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pSting );
		m_pSting = NULL;
	}
}

void CKartMusic::PlaySting( void )
{
	StopSting();
	if ( kart_music_volume.GetFloat() <= 0.0f )
		return;

	m_pSting = CreatePatch( KART_SOUND_MUSIC_FINISH );
	if ( m_pSting )
	{
		CSoundEnvelopeController::GetController().Play( m_pSting, kart_music_volume.GetFloat(), 100 );
	}
}

void CKartMusic::Update( float frametime )
{
	CHL2MPRules *pRules = HL2MPRules();
	if ( !pRules )
		return;

	KartRaceState_t state = pRules->GetKartRaceState();
	bool bRaceRunning = ( state == KART_RACE_STATE_RACING || state == KART_RACE_STATE_FINISHING );
	const char *pszMusic = pRules->GetKartMusic();

	if ( !bRaceRunning )
	{
		// The race is over for everyone, or hasn't started: the next GO starts the music.
		Stop( kart_music_fade_time.GetFloat() );
		m_bDoneThisRace = false;
		return;
	}

	C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
	if ( pPlayer && pPlayer->IsKartFinished() && !m_bDoneThisRace )
	{
		m_bDoneThisRace = true;
		Stop( kart_music_fade_time.GetFloat() );
		PlaySting();
	}

	float flVolume = kart_music_volume.GetFloat();
	if ( m_bDoneThisRace || flVolume <= 0.0f || !pszMusic[0] )
	{
		Stop( 0.0f );
		return;
	}

	if ( !m_pTrack || Q_stricmp( m_szTrack, pszMusic ) || ( m_flTrackDuration > 0.0f && gpGlobals->realtime >= m_flTrackEndTime ) )
	{
		Start( pszMusic );
	}
	else if ( flVolume != m_flVolume )
	{
		m_flVolume = flVolume;
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pTrack, flVolume, 0.1f );
	}
}
