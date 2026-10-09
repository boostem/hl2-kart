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

class C_HL2MP_Player;
#include "c_basehlplayer.h"
#include "hl2mp_player_shared.h"
#include "beamdraw.h"
#include "particles_simple.h"

class CSoundPatch;
class C_KartDriver;

//=============================================================================
//=============================================================================
class CSuitPowerDevice
{
public:
	CSuitPowerDevice( int bitsID, float flDrainRate ) { m_bitsDeviceID = bitsID; m_flDrainRate = flDrainRate; }
private:
	int		m_bitsDeviceID;	// tells what the device is. DEVICE_SPRINT, DEVICE_FLASHLIGHT, etc. BITMASK!!!!!
	float	m_flDrainRate;	// how quickly does this device deplete suit power? ( percent per second )

public:
	int		GetDeviceID( void ) const { return m_bitsDeviceID; }
	float	GetDeviceDrainRate( void ) const
	{
		//if ( g_pGameRules->GetSkillLevel() == SKILL_EASY && hl2_episodic.GetBool() && !( GetDeviceID() & bits_SUIT_DEVICE_SPRINT ) )
		//	return m_flDrainRate * 0.5f;
		//else
			return m_flDrainRate;
	}
};

extern ConVar hl2_sprintspeed;

//=============================================================================
// >> HL2MP_Player
//=============================================================================
class C_HL2MP_Player : public C_BaseHLPlayer
{
public:
	DECLARE_CLASS( C_HL2MP_Player, C_BaseHLPlayer );

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();


	C_HL2MP_Player();
	~C_HL2MP_Player( void );

	void ClientThink( void );

	static C_HL2MP_Player* GetLocalHL2MPPlayer();
	
	virtual int DrawModel( int flags );
	virtual void AddEntity( void );

	QAngle GetAnimEyeAngles( void ) { return m_angEyeAngles; }
	Vector GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = NULL );


	// Should this object cast shadows?
	virtual ShadowType_t		ShadowCastType( void );
	virtual C_BaseAnimating *BecomeRagdollOnClient();
	virtual const QAngle& GetRenderAngles();
	virtual bool ShouldDraw( void );
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual float GetFOV( void );
	virtual CStudioHdr *OnNewModel( void );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	virtual void ItemPreFrame( void );
	virtual void ItemPostFrame( void );
	virtual float GetMinFOV()	const { return 5.0f; }
	virtual Vector GetAutoaimVector( float flDelta );
	virtual void NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual void CreateLightEffects( void ) {}
	virtual bool ShouldReceiveProjectedTextures( int flags );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void PreThink( void );
	virtual void DoImpactEffect( trace_t &tr, int nDamageType );
	IRagdoll* GetRepresentativeRagdoll() const;
	virtual void CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );
	virtual const QAngle& EyeAngles( void );

	void SuitPower_Update( void );
	bool SuitPower_Drain( float flPower ); // consume some of the suit's power.
	void SuitPower_Charge( float flPower ); // add suit power.
	void SuitPower_SetCharge( float flPower ) { m_HL2Local.m_flSuitPower = flPower; }
	void SuitPower_Initialize( void );
	bool SuitPower_IsDeviceActive( const CSuitPowerDevice& device );
	bool SuitPower_AddDevice( const CSuitPowerDevice& device );
	bool SuitPower_RemoveDevice( const CSuitPowerDevice& device );
	bool SuitPower_ShouldRecharge( void );
	float SuitPower_GetCurrentPercentage( void ) { return m_HL2Local.m_flSuitPower; }
	
	bool	CanSprint( void );
	void	StartSprinting( void );
	void	StopSprinting( void );
	virtual void	HandleSpeedChanges( CMoveData *mv ) OVERRIDE;
	virtual void	ReduceTimers( CMoveData* mv ) OVERRIDE;
	void	UpdateLookAt( void );
	void	Initialize( void );
	int		GetIDTarget() const;
	void	UpdateIDTarget( void );
	void	PrecacheFootStepSounds( void );
	const char	*GetPlayerModelSoundPrefix( void );

	HL2MPPlayerState State_Get() const;

	// Walking
	void StartWalking( void );
	void StopWalking( void );
	bool IsWalking( void ) { return m_fIsWalking; }

	// Kart mode: the player entity is the kart. Latched by the server at spawn.
	bool IsInKart( void ) const { return m_bKartMode; }
	float GetKartSpeed( void ) const { return m_flKartSpeed; }
	float GetKartYaw( void ) const { return m_flKartYaw; }
	bool IsDrifting( void ) const { return m_nKartDriftDir != 0; }
	int GetKartDriftDir( void ) const { return m_nKartDriftDir; }
	int GetKartSteer( void ) const { return m_nKartSteer; }
	// Drawn steering: the front wheels' angle in degrees (+ right), smoothed, counter-steering in a drift.
	// The driver's lean and hands (grip_l/grip_r attachments) follow it.
	float GetKartSteerAngle( void ) const { return m_flKartSteerAngle; }
	float GetKartSlipAngle( void ) const { return m_flKartSlipAngle; }
	float GetKartDriftTime( void ) const { return m_flKartDriftTime; }
	float GetKartDriftCharge( void ) const { return m_flKartDriftCharge; }
	int GetKartDriftTier( void ) const { return m_nKartDriftTier; }

	// Kart boost (drift mini-turbos, boost pads, items). Shared, see hl2mp_player_shared.cpp.
	bool IsKartBoosting( void ) const { return gpGlobals->curtime < m_flKartBoostEndTime; }
	float GetKartBoostEndTime( void ) const { return m_flKartBoostEndTime; }
	float GetKartBoostScale( void ) const { return m_flKartBoostScale; }
	void KartGiveBoost( float flDuration, float flSpeedScale );

	// Kart race state (see kart_race_shared.h), networked for every player.
	int GetKartLap( void ) const { return m_nKartLap; }
	int GetKartNextCheckpoint( void ) const { return m_nKartNextCheckpoint; }
	float GetKartProgress( void ) const { return m_flKartProgress; }
	int GetKartRacePosition( void ) const { return m_nKartRacePosition; }
	bool IsKartFinished( void ) const { return m_bKartFinished; }
	float GetKartLapStartTime( void ) const { return m_flKartLapStartTime; }
	float GetKartBestLap( void ) const { return m_flKartBestLap; }
	float GetKartTotalTime( void ) const { return m_flKartTotalTime; }
	bool IsKartLateJoin( void ) const { return m_bKartLateJoin; }
	bool IsKartWrongWay( void ) const { return m_bKartWrongWay; }

	// Kart item (see kart_items.h), networked for every player.
	int GetKartItem( void ) const { return m_nKartItem; }
	int GetKartItemCount( void ) const { return m_nKartItemCount; }
	int GetKartRouletteItem( void ) const { return m_nKartRouletteItem; }
	float GetKartRouletteEnd( void ) const { return m_flKartRouletteEnd; }
	bool IsKartRouletteSpinning( void ) const { return gpGlobals->curtime < m_flKartRouletteEnd; }

	// In kart mode: locks the view to the kart heading and strips non-kart input.
	virtual bool CreateMove( float flInputSampleTime, CUserCmd *pCmd ) OVERRIDE;

	// Kart engine loops, created on the client for every kart player in PVS.
	void UpdateKartSounds( void );
	void DrawKartDebugOverlay( void );
	void StopKartSounds( void );
	float GetKartSkidSlipAngle( void );

	// Tire marks behind the rear wheels while drifting, for every kart player in PVS.
	void UpdateKartSkidmarks( void );
	void ShootKartSkidmark( const char *pszAttachment, float flSide, const Vector &vecDir );

	// Boost flame at the exhaust and mini-turbo sparks at the rear wheels, for every kart player in PVS.
	void UpdateKartBoostFX( void );
	void EmitKartExhaust( int nCount );
	void EmitKartDriftSparks( int nCount );

	// Turns the front wheels and steering wheel bones of karts that have them.
	void UpdateKartSteering( void );
	virtual void BuildTransformations( CStudioHdr *pStudioHdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed ) OVERRIDE;

	// The driver: the player's model seated in the kart, hands on grip_l/grip_r, leaning into turns.
	void UpdateKartDriver( void );
	void RemoveKartDriver( void );
	float GetKartDriverLean( void ) const { return m_flKartDriverLean; }	// -1 full left .. +1 full right
	float GetKartDriverLook( void ) const { return m_flKartDriverLook; }	// -1 full left .. +1 full right
	bool IsKartCamTooClose( void ) const { return m_bKartCamTooClose; }

	virtual void PostThink( void );

private:
	
	C_HL2MP_Player( const C_HL2MP_Player & );

	CPlayerAnimState m_PlayerAnimState;

	QAngle	m_angEyeAngles;

	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	EHANDLE	m_hRagdoll;

	int	m_headYawPoseParam;
	int	m_headPitchPoseParam;
	float m_headYawMin;
	float m_headYawMax;
	float m_headPitchMin;
	float m_headPitchMax;

	bool m_isInit;
	Vector m_vLookAtTarget;

	float m_flLastBodyYaw;
	float m_flCurrentHeadYaw;
	float m_flCurrentHeadPitch;

	int	  m_iIDEntIndex;

	CountdownTimer m_blinkTimer;

	int	  m_iSpawnInterpCounter;
	int	  m_iSpawnInterpCounterCache;

	int	  m_iPlayerSoundType;

	void ReleaseFlashlight( void );
	Beam_t	*m_pFlashlightBeam;

	CNetworkVar( HL2MPPlayerState, m_iPlayerState );	

	bool m_fIsWalking = false;

	// The kart movement is the only thing that drives the kart state in prediction.
	friend class CKartGameMovement;
	bool	m_bKartMode;
	float	m_flKartSpeed;		// forward speed along the kart's yaw, u/s
	float	m_flKartYaw;		// heading of the kart body, degrees
	float	m_flKartReverseTime;	// seconds the brake has been held at a standstill
	float	m_flKartBumpCooldown;	// seconds until the next bump sound may play
	int		m_nKartDriftDir;	// drift direction, the steer sign at entry (+1 right, -1 left), 0 when not drifting
	int		m_nKartSteer;		// steer input this tick (+1 right, -1 left, 0 none)
	float	m_flKartSlipAngle;	// heading minus velocity yaw, degrees
	float	m_flKartDriftTime;	// seconds into the current drift
	float	m_flKartHopTime;	// seconds airborne since a hop, 0 when not hopping
	float	m_flKartDriftCharge;	// mini-turbo charge of the current drift, 0 when not drifting
	int		m_nKartDriftTier;	// mini-turbo tier the charge has reached, 0-3
	float	m_flKartBoostEndTime;	// time the current boost ends, in the past when not boosting
	float	m_flKartBoostScale;	// kart_max_speed multiplier of the current boost
	QAngle	m_angKartRenderAngles;	// what GetRenderAngles() returns in kart mode

	// Kart race state, from the server's race manager.
	int		m_nKartLap;
	int		m_nKartNextCheckpoint;
	float	m_flKartProgress;
	int		m_nKartRacePosition;
	bool	m_bKartFinished;
	float	m_flKartLapStartTime;
	float	m_flKartBestLap;
	float	m_flKartTotalTime;
	bool	m_bKartLateJoin;
	bool	m_bKartWrongWay;

	// Kart item, from the server.
	int		m_nKartItem;
	int		m_nKartItemCount;
	int		m_nKartRouletteItem;
	float	m_flKartRouletteEnd;

	CSoundPatch	*m_pKartEngineIdle;
	CSoundPatch	*m_pKartEngineRev;
	CSoundPatch	*m_pKartSkid;		// drift screech, only while drifting
	float	m_flKartSoundLastSpeed;	// |m_flKartSpeed| at the last think, for remote throttle

	Vector	m_vecKartSkidLastPos;	// origin at the last skidmark think
	float	m_flKartSkidDist;		// units travelled while drifting since the last skidmark
	bool	m_bKartSkidActive;		// m_vecKartSkidLastPos is valid

	// Boost and drift spark particles (client only). They are short-lived, so
	// stopping the emission is all it takes to end the effect.
	CSmartPtr<CSimpleEmitter>	m_pKartFXEmitter;
	float	m_flKartExhaustAccum;	// exhaust particles owed to the emission rate
	float	m_flKartSparkAccum;		// drift spark particles owed to the emission rate

	// Chase camera (local player only, never predicted or networked).
	void	CalcKartView( Vector &eyeOrigin, QAngle &eyeAngles, float &fov );
	float	m_flKartCamYaw;		// lagged camera heading, chasing m_flKartYaw
	bool	m_bKartCamActive;	// the chase camera ran last frame; otherwise snap m_flKartCamYaw
	bool	m_bKartCamTooClose;	// a wall pulled the camera into the kart: hide the local model
	float	m_flKartCamBoost;	// 0-1, eases the boost FOV kick and pull-back in and out

	// Steering animation (client only): bone indices, -1 when the kart model has no such bone.
	float	m_flKartSteerAngle;	// see GetKartSteerAngle
	int		m_iKartBoneSteerFL;
	int		m_iKartBoneSteerFR;
	int		m_iKartBoneSteeringWheel;

	// Driver (client only), drawn with m_nKartDriverModel, the server's pick from cl_playermodel.
	int		m_nKartDriverModel;
	C_KartDriver	*m_pKartDriver;
	float	m_flKartDriverLean;	// see GetKartDriverLean
	float	m_flKartDriverLook;	// see GetKartDriverLook
};

inline C_HL2MP_Player *ToHL2MPPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<C_HL2MP_Player*>( pEntity );
}


class C_HL2MPRagdoll : public C_BaseAnimatingOverlay
{
public:
	DECLARE_CLASS( C_HL2MPRagdoll, C_BaseAnimatingOverlay );
	DECLARE_CLIENTCLASS();
	
	C_HL2MPRagdoll();
	~C_HL2MPRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;

	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );
	void UpdateOnRemove( void );
	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	
private:
	
	C_HL2MPRagdoll( const C_HL2MPRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pDestinationEntity );
	void CreateHL2MPRagdoll( void );

private:

	EHANDLE	m_hPlayer;
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
};

#endif //HL2MP_PLAYER_H
