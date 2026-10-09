//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef HL2MP_PLAYER_H
#define HL2MP_PLAYER_H
#pragma once

class CHL2MP_Player;

#include "basemultiplayerplayer.h"
#include "hl2_playerlocaldata.h"
#include "hl2_player.h"
#include "simtimer.h"
#include "soundenvelope.h"
#include "hl2mp_player_shared.h"
#include "kart_shareddefs.h"
#include "hl2mp_gamerules.h"
#include "utldict.h"

//=============================================================================
// >> HL2MP_Player
//=============================================================================
class CHL2MPPlayerStateInfo
{
public:
	HL2MPPlayerState m_iPlayerState;
	const char *m_pStateName;

	void (CHL2MP_Player::*pfnEnterState)();	// Init and deinit the state.
	void (CHL2MP_Player::*pfnLeaveState)();

	void (CHL2MP_Player::*pfnPreThink)();	// Do a PreThink() in this state.
};

class CHL2MP_Player : public CHL2_Player
{
public:
	DECLARE_CLASS( CHL2MP_Player, CHL2_Player );

	CHL2MP_Player();
	~CHL2MP_Player( void );
	
	static CHL2MP_Player *CreatePlayer( const char *className, edict_t *ed )
	{
		CHL2MP_Player::s_PlayerEdict = ed;
		return (CHL2MP_Player*)CreateEntityByName( className );
	}

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void PostThink( void );
	virtual void PreThink( void );
	virtual void PlayerDeathThink( void );
	virtual void SetAnimation( PLAYER_ANIM playerAnim );
	virtual bool HandleCommand_JoinTeam( int team );
	virtual bool ClientCommand( const CCommand &args );
	virtual void CreateViewModel( int viewmodelindex = 0 );
	virtual bool BecomeRagdollOnClient( const Vector &force );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual int OnTakeDamage( const CTakeDamageInfo &inputInfo );
	virtual bool WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const;
	virtual void FireBullets ( const FireBulletsInfo_t &info );
	virtual void OnMyWeaponFired( CBaseCombatWeapon* weapon );
	virtual bool Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0);
	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );
	virtual void ChangeTeam( int iTeam ) OVERRIDE;
	virtual void PickupObject ( CBaseEntity *pObject, bool bLimitMassAndSize );
	virtual void PlayerUse( void );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget = NULL, const Vector *pVelocity = NULL );
	virtual void UpdateOnRemove( void );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual CBaseEntity* EntSelectSpawnPoint( void );
		
	int FlashlightIsOn( void );
	void FlashlightTurnOn( void );
	void FlashlightTurnOff( void );
	void	PrecacheFootStepSounds( void );
	bool	ValidatePlayerModel( const char *pModel );

	QAngle GetAnimEyeAngles( void ) { return m_angEyeAngles.Get(); }

	Vector GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = NULL );

	void CheatImpulseCommands( int iImpulse );
	void CreateRagdollEntity( void );
	void GiveAllItems( void );
	void GiveDefaultItems( void );

	void NoteWeaponFired( void );

	void ResetAnimation( void );
	void SetPlayerModel( void );
	void SetPlayerTeamModel( void );
	void SetKartModel( void );
	void ApplyKartDriverModel( void );
	static const char *GetKartModelName( void );
	void ApplyKartColor( void );

	// Kart mode: the player entity is the kart. Latched from kart_enabled at spawn.
	bool IsInKart( void ) const { return m_bKartMode; }
	float GetKartSpeed( void ) const { return m_flKartSpeed; }
	float GetKartYaw( void ) const { return m_flKartYaw; }
	bool IsDrifting( void ) const { return m_nKartDriftDir != 0; }
	int GetKartDriftDir( void ) const { return m_nKartDriftDir; }
	int GetKartSteer( void ) const { return m_nKartSteer; }
	float GetKartSlipAngle( void ) const { return m_flKartSlipAngle; }
	float GetKartDriftTime( void ) const { return m_flKartDriftTime; }
	float GetKartDriftCharge( void ) const { return m_flKartDriftCharge; }
	int GetKartDriftTier( void ) const { return m_nKartDriftTier; }

	// Kart boost (drift mini-turbos, boost pads, items). Shared, see hl2mp_player_shared.cpp.
	bool IsKartBoosting( void ) const { return gpGlobals->curtime < m_flKartBoostEndTime; }
	float GetKartBoostEndTime( void ) const { return m_flKartBoostEndTime; }
	float GetKartBoostScale( void ) const { return m_flKartBoostScale; }
	void KartGiveBoost( float flDuration, float flSpeedScale );

	// Kart hit reactions (spin-out, stun). The state is predicted: the kart
	// movement plays it out and ends it. Entry is server-only, see KartApplyHit.
	int GetKartHitState( void ) const { return m_nKartHitState; }
	float GetKartHitEndTime( void ) const { return m_flKartHitEndTime; }
	bool IsKartHit( void ) const { return m_nKartHitState != KART_HIT_NONE; }
	bool IsKartSpinningOut( void ) const { return m_nKartHitState == KART_HIT_SPINOUT; }
	bool IsKartHitImmune( void ) const;
	float GetKartHitSpinYaw( void ) const;
	// Spins out or stuns the kart (pAttacker: who threw the item, may be NULL).
	// False when it can't be hit: immune, not a live kart, frozen, or its
	// buffer took the hit.
	bool KartApplyHit( KartHitType type, CBaseEntity *pAttacker );

	// Buffer item: a shield that takes the next hit, until it does or the
	// time it was raised for runs out. Server-side, networked to everyone.
	bool HasKartBuffer( void ) const { return gpGlobals->curtime < m_flKartBufferEndTime; }
	float GetKartBufferEndTime( void ) const { return m_flKartBufferEndTime; }
	void KartRaiseBuffer( float flDuration );
	void KartClearBuffer( void ) { m_flKartBufferEndTime = 0.0f; }
	// kart_max_speed multiplier of this kart's top speed: 1 for people, set
	// each tick by the kart bot (difficulty and rubber-banding). Not
	// networked: bots aren't predicted.
	float GetKartTopSpeedScale( void ) const { return m_flKartTopSpeedScale; }
	void SetKartTopSpeedScale( float flScale ) { m_flKartTopSpeedScale = flScale; }

	// Stops the kart and clears its drift, hop, drift charge and boost, heading flYaw.
	void ResetKartMovement( float flYaw );
	// Moves the kart to vecOrigin, stopped and heading flYaw (kart bots unsticking).
	void KartTeleport( const Vector &vecOrigin, float flYaw );

	// Kart race state (see kart_race_shared.h). The race manager drives it.
	int GetKartLap( void ) const { return m_nKartLap; }
	int GetKartNextCheckpoint( void ) const { return m_nKartNextCheckpoint; }
	float GetKartProgress( void ) const { return m_flKartProgress; }
	int GetKartRacePosition( void ) const { return m_nKartRacePosition; }
	bool IsKartFinished( void ) const { return m_bKartFinished; }
	float GetKartLapStartTime( void ) const { return m_flKartLapStartTime; }
	float GetKartBestLap( void ) const { return m_flKartBestLap; }
	float GetKartTotalTime( void ) const { return m_flKartTotalTime; }
	float GetKartFinishTime( void ) const { return m_flKartFinishTime; }
	bool IsKartLateJoin( void ) const { return m_bKartLateJoin; }
	bool IsKartDNF( void ) const { return m_bKartDNF; }
	bool IsKartWrongWay( void ) const { return m_bKartWrongWay; }
	void ResetKartRaceState( void );

	// Puts the kart back on the track at the last checkpoint it hit, facing the
	// next one, stopped and briefly frozen (kart_respawn_zone, kill_z and the
	// kart_respawn command). Lap and checkpoint progress are kept. False when
	// the kart can't be respawned (dead, observing, not a kart).
	bool KartRespawnAtCheckpoint( void );

	// Kart item (see kart_items.h). Server-authoritative, not predicted.
	int GetKartItem( void ) const { return m_nKartItem; }
	int GetKartItemCount( void ) const { return m_nKartItemCount; }
	int GetKartRouletteItem( void ) const { return m_nKartRouletteItem; }
	float GetKartRouletteEnd( void ) const { return m_flKartRouletteEnd; }
	bool IsKartRouletteSpinning( void ) const { return gpGlobals->curtime < m_flKartRouletteEnd; }
	void KartGiveItem( int item, int nCount, float flRouletteTime );
	void KartClearItem( void );
	void KartUseItem( bool bBackward );

	Activity TranslateTeamActivity( Activity ActToTranslate );
	
	float GetNextModelChangeTime( void ) { return m_flNextModelChangeTime; }
	float GetNextTeamChangeTime( void ) { return m_flNextTeamChangeTime; }
	void  PickDefaultSpawnTeam( void );
	void  SetupPlayerSoundsByModel( const char *pModelName );
	const char *GetPlayerModelSoundPrefix( void );
	int	  GetPlayerModelType( void ) { return m_iPlayerSoundType;	}

	int	GetMaxAmmo( int iAmmoIndex ) const;
	
	void  DetonateTripmines( void );

	void Reset();

	bool IsReady();
	void SetReady( bool bReady );

	void CheckChatText( char *p, int bufsize );

	void State_Transition( HL2MPPlayerState newState );
	void State_Enter( HL2MPPlayerState newState );
	void State_Leave();
	void State_PreThink();
	CHL2MPPlayerStateInfo *State_LookupInfo( HL2MPPlayerState state );

	void State_Enter_ACTIVE();
	void State_PreThink_ACTIVE();
	void State_Enter_OBSERVER_MODE();
	void State_PreThink_OBSERVER_MODE();


	virtual bool StartObserverMode( int mode );
	virtual void StopObserverMode( void );
	virtual bool IsValidObserverTarget( CBaseEntity *target );
	virtual void ValidateCurrentObserverTarget( void );

	// Kart spectating: a kart that has finished the race, or joined while it
	// ran, watches the karts still driving (chase cam of the leader, attack
	// keys cycle, jump to free look) until the next race respawns it.
	bool IsKartSpectating( void );
	void KartStartSpectating( void );


	Vector m_vecTotalBulletForce;	//Accumulator for bullet force in a single frame

	// Tracks our ragdoll entity.
	CNetworkHandle( CBaseEntity, m_hRagdoll );	// networked entity handle 

	virtual bool	CanHearAndReadChatFrom( CBasePlayer *pPlayer );

	bool IsThreatAimingTowardMe( CBaseEntity* threat, float cosTolerance = 0.8f ) const;
	bool IsThreatFiringAtMe( CBaseEntity* threat ) const;
private:

	CNetworkQAngle( m_angEyeAngles );
	CPlayerAnimState   m_PlayerAnimState;

	int m_iLastWeaponFireUsercmd;
	int m_iModelType;
	CNetworkVar( int, m_iSpawnInterpCounter );
	CNetworkVar( int, m_iPlayerSoundType );

	// The kart movement is the only thing that drives the kart state after spawn.
	friend class CKartGameMovement;
	CNetworkVar( bool, m_bKartMode );
	CNetworkVar( float, m_flKartSpeed );	// forward speed along the kart's yaw, u/s
	CNetworkVar( float, m_flKartYaw );		// heading of the kart body, degrees
	CNetworkVar( float, m_flKartReverseTime );	// seconds the brake has been held at a standstill
	CNetworkVar( float, m_flKartBumpCooldown );	// seconds until the next bump sound may play
	CNetworkVar( int, m_nKartDriftDir );		// drift direction, the steer sign at entry (+1 right, -1 left), 0 when not drifting
	CNetworkVar( int, m_nKartDriverModel );	// model index of the driver the client seats in the kart
	CNetworkVar( int, m_nKartSteer );		// steer input this tick (+1 right, -1 left, 0 none), for the steering animation
	CNetworkVar( float, m_flKartSlipAngle );	// heading minus velocity yaw, degrees
	CNetworkVar( float, m_flKartDriftTime );	// seconds into the current drift
	CNetworkVar( float, m_flKartHopTime );		// seconds airborne since a hop, 0 when not hopping
	CNetworkVar( float, m_flKartDriftCharge );	// mini-turbo charge of the current drift, 0 when not drifting
	CNetworkVar( int, m_nKartDriftTier );		// mini-turbo tier the charge has reached, 0-3
	CNetworkVar( float, m_flKartBoostEndTime );	// time the current boost ends, in the past when not boosting
	CNetworkVar( float, m_flKartBoostScale );	// kart_max_speed multiplier of the current boost
	CNetworkVar( int, m_nKartHitState );		// KartHitType being played out, KART_HIT_NONE when none
	CNetworkVar( float, m_flKartHitEndTime );	// time the hit ends (and kart_hit_immunity starts counting)
	CNetworkVar( float, m_flKartBufferEndTime );	// time the buffer runs out, in the past when there is none
	float m_flKartTopSpeedScale;				// kart_max_speed multiplier of the top speed, see GetKartTopSpeedScale

	// Kart race state. Only the race manager and the race flow change it.
	friend class CKartRaceManager;
	friend class CHL2MPRules;
	CNetworkVar( int, m_nKartLap );
	CNetworkVar( int, m_nKartNextCheckpoint );
	CNetworkVar( float, m_flKartProgress );
	CNetworkVar( int, m_nKartRacePosition );
	CNetworkVar( bool, m_bKartFinished );
	CNetworkVar( float, m_flKartLapStartTime );
	CNetworkVar( float, m_flKartBestLap );
	CNetworkVar( float, m_flKartTotalTime );
	float m_flKartFinishTime;	// server time the player finished, orders the finishers
	CNetworkVar( bool, m_bKartLateJoin );	// joined while the race ran: no position, races the next one
	bool m_bKartInRace;		// on the track when the current race started
	bool m_bKartDNF;		// finished for the player when kart_finish_timeout ran out
	CNetworkVar( bool, m_bKartWrongWay );	// driving against the track, see CKartRaceManager::UpdateWrongWay
	float m_flKartWrongWayTime;	// seconds the wrong-way flag has wanted to flip
	float m_flKartRespawnUnfreezeTime;	// server time a checkpoint respawn's freeze ends, 0 when not frozen by one
	float m_flKartNextRespawnCommand;	// server time kart_respawn may be used again

	// Kart item state.
	void KartItemPostThink( void );
	CNetworkVar( int, m_nKartItem );			// KartItem_t, decided when the box is taken
	CNetworkVar( int, m_nKartItemCount );		// uses left
	CNetworkVar( int, m_nKartRouletteItem );	// what the roulette shows, display only
	CNetworkVar( float, m_flKartRouletteEnd );	// server time the roulette stops
	float m_flKartRouletteNextStep;				// server time the roulette shows the next item

	float m_flNextModelChangeTime;
	float m_flNextTeamChangeTime;

	float m_flSlamProtectTime;	

	HL2MPPlayerState m_iPlayerState;
	CHL2MPPlayerStateInfo *m_pCurStateInfo;

	bool ShouldRunRateLimitedCommand( const CCommand &args );

	// This lets us rate limit the commands the players can execute so they don't overflow things like reliable buffers.
	CUtlDict<float,int>	m_RateLimitLastCommandTimes;

    bool m_bEnterObserver;
	bool m_bReady;
};

inline CHL2MP_Player *ToHL2MPPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<CHL2MP_Player*>( pEntity );
}

#endif //HL2MP_PLAYER_H
