//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL2.
//
//=============================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "c_hl2mp_player.h"
#include "view.h"
#include "takedamageinfo.h"
#include "hl2mp_gamerules.h"
#include "kart_shareddefs.h"
#include "in_buttons.h"
#include "input.h"
#include "model_types.h"
#include "iviewrender_beams.h"			// flashlight beam
#include "r_efx.h"
#include "dlight.h"
#include "soundenvelope.h"
#include "bone_setup.h"
#include "clienteffectprecachesystem.h"

// Don't alias here
#if defined( CHL2MP_Player )
#undef CHL2MP_Player	
#endif

// misyl: Can be set to Msg if you want some info for debugging prediction
#define MsgPredTest(...)
#define MsgPredTest2(...)

ConVar sv_infinite_aux_power( "sv_infinite_aux_power", "0", FCVAR_CHEAT | FCVAR_REPLICATED );

ConVar kart_engine_pitch_min( "kart_engine_pitch_min", "85", FCVAR_ARCHIVE, "Kart engine loop pitch at a standstill (percent)." );
ConVar kart_engine_pitch_max( "kart_engine_pitch_max", "170", FCVAR_ARCHIVE, "Kart engine loop pitch at kart_max_speed (percent)." );

ConVar kart_skidmarks( "kart_skidmarks", "1", FCVAR_ARCHIVE, "Leave tire marks behind drifting karts." );
ConVar kart_skidmark_interval( "kart_skidmark_interval", "24", FCVAR_ARCHIVE, "Units a drifting kart travels between tire marks." );

extern ConVar r_decals;

// Chase camera. Client only: the camera never feeds back into movement.
ConVar kart_cam_dist( "kart_cam_dist", "220", FCVAR_ARCHIVE, "Kart chase camera distance behind the kart." );
ConVar kart_cam_height( "kart_cam_height", "70", FCVAR_ARCHIVE, "Kart chase camera height above the look-at point." );
ConVar kart_cam_target_height( "kart_cam_target_height", "30", FCVAR_ARCHIVE, "Height of the chase camera's look-at point above the kart origin." );
ConVar kart_cam_follow( "kart_cam_follow", "6", FCVAR_ARCHIVE, "How quickly the chase camera swings in behind the kart in turns. Higher is stiffer." );
ConVar kart_cam_fov( "kart_cam_fov", "95", FCVAR_ARCHIVE, "Kart chase camera field of view." );
ConVar cl_kart_steer_angle( "cl_kart_steer_angle", "25", FCVAR_ARCHIVE, "Kart front wheel steering lock at low speed, in degrees. It shrinks with the turn rate at speed." );
ConVar cl_kart_steer_ratio( "cl_kart_steer_ratio", "3", FCVAR_ARCHIVE, "Kart steering ratio: degrees the steering wheel turns per degree of the front wheels." );
ConVar cl_kart_steer_speed( "cl_kart_steer_speed", "10", FCVAR_ARCHIVE, "How quickly the kart's drawn steering follows the input. Higher is snappier." );
ConVar cl_kart_driver( "cl_kart_driver", "1", FCVAR_ARCHIVE, "Draw a driver in karts that have grip_l/grip_r attachments." );
ConVar cl_kart_driver_seat( "cl_kart_driver_seat", "-7 0 25", FCVAR_ARCHIVE, "Where the driver's pelvis sits, in kart model space (x forward, y left, z up)." );
ConVar cl_kart_driver_feet( "cl_kart_driver_feet", "24 6 12", FCVAR_ARCHIVE, "Where the driver's feet rest, in kart model space; y is mirrored for the right foot." );
ConVar cl_kart_driver_tilt( "cl_kart_driver_tilt", "5", FCVAR_ARCHIVE, "Degrees the driver's upper body leans forward towards the steering wheel." );
ConVar cl_kart_driver_lean( "cl_kart_driver_lean", "10", FCVAR_ARCHIVE, "Degrees the driver's upper body leans into a full turn." );
ConVar cl_kart_steer_drift_counter( "cl_kart_steer_drift_counter", "0.15 0.5 0.9", FCVAR_ARCHIVE, "Counter-steer in a drift as a fraction of cl_kart_steer_angle: steering into the drift, no steer, steering against it." );
ConVar kart_cam_min_dist( "kart_cam_min_dist", "80", FCVAR_ARCHIVE, "Hide the local kart when a wall pulls the chase camera closer than this to it." );
ConVar kart_boost_fov_kick( "kart_boost_fov_kick", "12", FCVAR_ARCHIVE, "Degrees the kart chase camera's field of view widens while boosting." );
ConVar kart_boost_cam_pullback( "kart_boost_cam_pullback", "30", FCVAR_ARCHIVE, "Units the kart chase camera pulls back while boosting." );
ConVar kart_boost_cam_blend( "kart_boost_cam_blend", "0.2", FCVAR_ARCHIVE, "Seconds the boost FOV kick and pull-back take to ease in and out." );
ConVar kart_boost_fx( "kart_boost_fx", "1", FCVAR_ARCHIVE, "Draw the boost exhaust flame and mini-turbo drift sparks on karts." );
ConVar kart_tilt_smooth( "kart_tilt_smooth", "10", FCVAR_ARCHIVE, "How quickly a kart's model pitches and rolls to the ground under it. Higher is snappier, 0 draws karts level." );

CLIENTEFFECT_REGISTER_BEGIN( PrecacheKartBoostFX )
CLIENTEFFECT_MATERIAL( "sprites/glow01" )
CLIENTEFFECT_MATERIAL( "sprites/light_glow02_add" )
CLIENTEFFECT_MATERIAL( "sprites/flamelet1" )
CLIENTEFFECT_MATERIAL( "sprites/flamelet2" )
CLIENTEFFECT_MATERIAL( "sprites/flamelet3" )
CLIENTEFFECT_MATERIAL( "sprites/flamelet4" )
CLIENTEFFECT_MATERIAL( "sprites/flamelet5" )
CLIENTEFFECT_REGISTER_END()

LINK_ENTITY_TO_CLASS( player, C_HL2MP_Player );

// specific to the local player
BEGIN_RECV_TABLE_NOBASE( C_HL2MP_Player, DT_HL2MPLocalPlayerExclusive )
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ) ),

	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),

	RecvPropFloat( RECVINFO( m_flKartSpeed ) ),
	RecvPropFloat( RECVINFO( m_flKartYaw ) ),
	RecvPropFloat( RECVINFO( m_flKartReverseTime ) ),
	RecvPropFloat( RECVINFO( m_flKartBumpCooldown ) ),
	RecvPropFloat( RECVINFO( m_flKartSlipAngle ) ),
	RecvPropFloat( RECVINFO( m_flKartDriftTime ) ),
	RecvPropFloat( RECVINFO( m_flKartHopTime ) ),
	RecvPropFloat( RECVINFO( m_flKartDriftCharge ) ),
	RecvPropFloat( RECVINFO( m_flKartBoostScale ) ),
	RecvPropVector( RECVINFO( m_vecKartGroundNormal ) ),
END_RECV_TABLE()

// all players except the local player
BEGIN_RECV_TABLE_NOBASE( C_HL2MP_Player, DT_HL2MPNonLocalPlayerExclusive )
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ) ),

	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),

	RecvPropFloat( RECVINFO( m_flKartSpeed ) ),
	RecvPropFloat( RECVINFO( m_flKartYaw ) ),
	RecvPropVector( RECVINFO( m_vecKartGroundNormal ) ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_HL2MP_Player, DT_HL2MP_Player, CHL2MP_Player)
	RecvPropDataTable( "hl2mplocaldata", 0, 0, &REFERENCE_RECV_TABLE( DT_HL2MPLocalPlayerExclusive ) ),
	RecvPropDataTable( "hl2mpnonlocaldata", 0, 0, &REFERENCE_RECV_TABLE( DT_HL2MPNonLocalPlayerExclusive ) ),

	RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
	RecvPropInt( RECVINFO( m_iSpawnInterpCounter ) ),
	RecvPropInt( RECVINFO( m_iPlayerSoundType) ),
	RecvPropBool( RECVINFO( m_bKartMode ) ),
	RecvPropInt( RECVINFO( m_nKartDriftDir ) ),
	RecvPropInt( RECVINFO( m_nKartSteer ) ),
	RecvPropInt( RECVINFO( m_nKartDriverModel ) ),
	RecvPropInt( RECVINFO( m_nKartDriftTier ) ),
	RecvPropFloat( RECVINFO( m_flKartBoostEndTime ) ),

	RecvPropInt( RECVINFO( m_nKartLap ) ),
	RecvPropInt( RECVINFO( m_nKartNextCheckpoint ) ),
	RecvPropFloat( RECVINFO( m_flKartProgress ) ),
	RecvPropInt( RECVINFO( m_nKartRacePosition ) ),
	RecvPropBool( RECVINFO( m_bKartFinished ) ),
	RecvPropTime( RECVINFO( m_flKartLapStartTime ) ),
	RecvPropFloat( RECVINFO( m_flKartBestLap ) ),
	RecvPropFloat( RECVINFO( m_flKartTotalTime ) ),
	RecvPropBool( RECVINFO( m_bKartLateJoin ) ),
	RecvPropBool( RECVINFO( m_bKartWrongWay ) ),

	RecvPropInt( RECVINFO( m_nKartItem ) ),
	RecvPropInt( RECVINFO( m_nKartItemCount ) ),
	RecvPropInt( RECVINFO( m_nKartRouletteItem ) ),
	RecvPropTime( RECVINFO( m_flKartRouletteEnd ) ),

	RecvPropBool( RECVINFO( m_fIsWalking ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_HL2MP_Player )
	DEFINE_PRED_FIELD( m_fIsWalking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD( m_bKartMode, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flKartSpeed, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.5f ),
	DEFINE_PRED_FIELD_TOL( m_flKartYaw, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.125f ),
	DEFINE_PRED_FIELD_TOL( m_flKartReverseTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flKartBumpCooldown, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD( m_nKartDriftDir, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nKartSteer, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flKartSlipAngle, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.125f ),
	DEFINE_PRED_FIELD_TOL( m_flKartDriftTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flKartHopTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flKartDriftCharge, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD( m_nKartDriftTier, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flKartBoostEndTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flKartBoostScale, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_vecKartGroundNormal, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.01f ),

	// misyl: Ammo is server side entities in HL2MP. Not catastrophic to error about.
	// Just let the server stomp all over us.
	//
	// There is 1 instance in which is can be a runaway pred error, and that is if you have eg. ar2
	// with just altfire ammo, and get new ammo and we force reload. But the additional pred error sorts that out itself
	// without this for every pickup which is 1000% more common.
	DEFINE_PRED_ARRAY( m_iAmmo, FIELD_INTEGER, MAX_AMMO_TYPES, FTYPEDESC_INSENDTABLE | FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

ConVar hl2_walkspeed( "hl2_walkspeed", "150", FCVAR_REPLICATED );
ConVar hl2_normspeed( "hl2_normspeed", "190", FCVAR_REPLICATED );
ConVar hl2_sprintspeed( "hl2_sprintspeed", "320", FCVAR_REPLICATED );

#define	HL2_WALK_SPEED hl2_walkspeed.GetFloat()
#define	HL2_NORM_SPEED hl2_normspeed.GetFloat()
#define	HL2_SPRINT_SPEED hl2_sprintspeed.GetFloat()

static ConVar cl_playermodel( "cl_playermodel", "none", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "Default Player Model");
static ConVar cl_kart_color( "cl_kart_color", "0", FCVAR_USERINFO | FCVAR_ARCHIVE, "Kart color, 0-7" );
static ConVar cl_defaultweapon( "cl_defaultweapon", "weapon_physcannon", FCVAR_USERINFO | FCVAR_ARCHIVE, "Default Spawn Weapon");

void SpawnBlood (Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);

//
// SUIT POWER DEVICES
//
#define SUITPOWER_CHARGE_RATE	12.5											// 100 units in 8 seconds

#ifdef HL2MP
	CSuitPowerDevice SuitDeviceSprint( bits_SUIT_DEVICE_SPRINT, 25.0f );				// 100 units in 4 seconds
#else
	CSuitPowerDevice SuitDeviceSprint( bits_SUIT_DEVICE_SPRINT, 12.5f );				// 100 units in 8 seconds
#endif

#ifdef HL2_EPISODIC
	CSuitPowerDevice SuitDeviceFlashlight( bits_SUIT_DEVICE_FLASHLIGHT, 1.111 );	// 100 units in 90 second
#else
	CSuitPowerDevice SuitDeviceFlashlight( bits_SUIT_DEVICE_FLASHLIGHT, 2.222 );	// 100 units in 45 second
#endif
CSuitPowerDevice SuitDeviceBreather( bits_SUIT_DEVICE_BREATHER, 6.7f );		// 100 units in 15 seconds (plus three padded seconds)

C_HL2MP_Player::C_HL2MP_Player() : m_PlayerAnimState( this ), m_iv_angEyeAngles( "C_HL2MP_Player::m_iv_angEyeAngles" )
{
	m_iIDEntIndex = 0;
	m_iSpawnInterpCounterCache = 0;

	m_angEyeAngles.Init();

	m_bKartMode = false;
	m_flKartSpeed = 0.0f;
	m_flKartYaw = 0.0f;
	m_flKartReverseTime = 0.0f;
	m_flKartBumpCooldown = 0.0f;
	m_nKartDriftDir = 0;
	m_nKartSteer = 0;
	m_flKartSlipAngle = 0.0f;
	m_flKartDriftTime = 0.0f;
	m_flKartHopTime = 0.0f;
	m_flKartDriftCharge = 0.0f;
	m_nKartDriftTier = 0;
	m_flKartBoostEndTime = 0.0f;
	m_flKartBoostScale = 1.0f;
	m_vecKartGroundNormal.Init( 0.0f, 0.0f, 1.0f );
	m_vecKartTiltNormal.Init( 0.0f, 0.0f, 1.0f );
	m_flKartTiltTime = 0.0f;
	m_angKartRenderAngles.Init();

	m_nKartLap = 0;
	m_nKartNextCheckpoint = 0;
	m_flKartProgress = 0.0f;
	m_nKartRacePosition = 0;
	m_bKartFinished = false;
	m_flKartLapStartTime = 0.0f;
	m_flKartBestLap = 0.0f;
	m_flKartTotalTime = 0.0f;
	m_bKartLateJoin = false;
	m_bKartWrongWay = false;

	m_nKartItem = 0;
	m_nKartItemCount = 0;
	m_nKartRouletteItem = 0;
	m_flKartRouletteEnd = 0.0f;

	m_pKartEngineIdle = NULL;
	m_pKartEngineRev = NULL;
	m_pKartSkid = NULL;
	m_flKartSoundLastSpeed = 0.0f;

	m_vecKartSkidLastPos.Init();
	m_flKartSkidDist = 0.0f;
	m_bKartSkidActive = false;

	m_flKartExhaustAccum = 0.0f;
	m_flKartSparkAccum = 0.0f;

	m_flKartCamYaw = 0.0f;
	m_bKartCamActive = false;
	m_bKartCamTooClose = false;
	m_flKartCamBoost = 0.0f;

	m_flKartSteerAngle = 0.0f;
	m_iKartBoneSteerFL = -1;
	m_nKartDriverModel = -1;
	m_pKartDriver = NULL;
	m_flKartDriverLean = 0.0f;
	m_iKartBoneSteerFR = -1;
	m_iKartBoneSteeringWheel = -1;

	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;
	m_blinkTimer.Invalidate();

	m_pFlashlightBeam = NULL;

	SuitPower_Initialize();
}

C_HL2MP_Player::~C_HL2MP_Player( void )
{
	ReleaseFlashlight();
	StopKartSounds();
	RemoveKartDriver();
}

int C_HL2MP_Player::GetIDTarget() const
{
	return m_iIDEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target entity
//-----------------------------------------------------------------------------
void C_HL2MP_Player::UpdateIDTarget()
{
	if ( !IsLocalPlayer() )
		return;

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	// don't show IDs in chase spec mode
	if ( GetObserverMode() == OBS_MODE_CHASE || 
		 GetObserverMode() == OBS_MODE_DEATHCAM )
		 return;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), 1500, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 10,   MainViewForward(), vecStart );
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;

		if ( pEntity && (pEntity != this) )
		{
			m_iIDEntIndex = pEntity->entindex();
		}
	}
}

void C_HL2MP_Player::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	Vector vecOrigin = ptr->endpos - vecDir * 4;

	float flDistance = 0.0f;
	
	if ( info.GetAttacker() )
	{
		flDistance = (ptr->endpos - info.GetAttacker()->GetAbsOrigin()).Length();
	}

	if ( m_takedamage )
	{
		AddMultiDamage( info, this );

		int blood = BloodColor();
		
		CBaseEntity *pAttacker = info.GetAttacker();

		if ( pAttacker )
		{
			if ( HL2MPRules()->IsTeamplay() && pAttacker->InSameTeam( this ) == true )
				return;
		}

		if ( blood != DONT_BLEED )
		{
			SpawnBlood( vecOrigin, vecDir, blood, flDistance );// a little surface blood.
			TraceBleed( flDistance, vecDir, ptr, info.GetDamageType() );
		}
	}
}


C_HL2MP_Player* C_HL2MP_Player::GetLocalHL2MPPlayer()
{
	return (C_HL2MP_Player*)C_BasePlayer::GetLocalPlayer();
}

void C_HL2MP_Player::Initialize( void )
{
	m_headYawPoseParam = LookupPoseParameter( "head_yaw" );
	GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );

	CStudioHdr *hdr = GetModelPtr();
	for ( int i = 0; i < hdr->GetNumPoseParameters() ; i++ )
	{
		SetPoseParameter( hdr, i, 0.0 );
	}
}

CStudioHdr *C_HL2MP_Player::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();
	
	Initialize( );

	// Karts built with steering bones (assets_src/kart_racer); others stay rigid.
	m_iKartBoneSteerFL = LookupBone( "steer_fl" );
	m_iKartBoneSteerFR = LookupBone( "steer_fr" );
	m_iKartBoneSteeringWheel = LookupBone( "steering_wheel" );

	return hdr;
}

//-----------------------------------------------------------------------------
/**
 * Orient head and eyes towards m_lookAt.
 */
void C_HL2MP_Player::UpdateLookAt( void )
{
	// head yaw
	if (m_headYawPoseParam < 0 || m_headPitchPoseParam < 0)
		return;

	// orient eyes
	m_viewtarget = m_vLookAtTarget;

	// blinking
	if (m_blinkTimer.IsElapsed())
	{
		m_blinktoggle = !m_blinktoggle;
		m_blinkTimer.Start( RandomFloat( 1.5f, 4.0f ) );
	}

	// Figure out where we want to look in world space.
	QAngle desiredAngles;
	Vector to = m_vLookAtTarget - EyePosition();
	VectorAngles( to, desiredAngles );

	// Figure out where our body is facing in world space.
	QAngle bodyAngles( 0, 0, 0 );
	bodyAngles[YAW] = GetLocalAngles()[YAW];


	float flBodyYawDiff = bodyAngles[YAW] - m_flLastBodyYaw;
	m_flLastBodyYaw = bodyAngles[YAW];
	

	// Set the head's yaw.
	float desired = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
	desired = clamp( desired, m_headYawMin, m_headYawMax );
	m_flCurrentHeadYaw = ApproachAngle( desired, m_flCurrentHeadYaw, 130 * gpGlobals->frametime );

	// Counterrotate the head from the body rotation so it doesn't rotate past its target.
	m_flCurrentHeadYaw = AngleNormalize( m_flCurrentHeadYaw - flBodyYawDiff );
	desired = clamp( desired, m_headYawMin, m_headYawMax );
	
	SetPoseParameter( m_headYawPoseParam, m_flCurrentHeadYaw );

	
	// Set the head's yaw.
	desired = AngleNormalize( desiredAngles[PITCH] );
	desired = clamp( desired, m_headPitchMin, m_headPitchMax );
	
	m_flCurrentHeadPitch = ApproachAngle( desired, m_flCurrentHeadPitch, 130 * gpGlobals->frametime );
	m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
	SetPoseParameter( m_headPitchPoseParam, m_flCurrentHeadPitch );
}

ConVar kart_debug( "kart_debug", "0", FCVAR_CHEAT, "Draw the kart debug overlay (speed, yaw, grounded, inputs...) top-left." );

void C_HL2MP_Player::ClientThink( void )
{
	bool bFoundViewTarget = false;
	
	Vector vForward;
	AngleVectors( GetLocalAngles(), &vForward );

	for( int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( iClient );
		if(!pEnt || !pEnt->IsPlayer())
			continue;

		if ( pEnt->entindex() == entindex() )
			continue;

		Vector vTargetOrigin = pEnt->GetAbsOrigin();
		Vector vMyOrigin =  GetAbsOrigin();

		Vector vDir = vTargetOrigin - vMyOrigin;
		
		if ( vDir.Length() > 128 ) 
			continue;

		VectorNormalize( vDir );

		if ( DotProduct( vForward, vDir ) < 0.0f )
			 continue;

		m_vLookAtTarget = pEnt->EyePosition();
		bFoundViewTarget = true;
		break;
	}

	if ( bFoundViewTarget == false )
	{
		m_vLookAtTarget = GetAbsOrigin() + vForward * 512;
	}

	UpdateIDTarget();

	UpdateKartSounds();
	UpdateKartSteering();
	UpdateKartDriver();
	UpdateKartSkidmarks();
	UpdateKartBoostFX();

	if ( IsLocalPlayer() && kart_debug.GetBool() )
	{
		DrawKartDebugOverlay();
	}
}

// One labelled row of the kart debug overlay. Later milestones call this to add rows.
static int s_nKartDebugRow = 0;
static void DebugRow( const char *label, const char *fmt, ... )
{
	char value[256];
	va_list args;
	va_start( args, fmt );
	Q_vsnprintf( value, sizeof( value ), fmt, args );
	va_end( args );
	engine->Con_NPrintf( s_nKartDebugRow++, "%-12s %s", label, value );
}

void C_HL2MP_Player::DrawKartDebugOverlay( void )
{
	s_nKartDebugRow = 10;	// below cl_showpos / net_graph text
	Vector vecOrigin = GetAbsOrigin();
	int nButtons = m_nButtons;
	int nThrottle = ( ( nButtons & IN_FORWARD ) ? 1 : 0 ) - ( ( nButtons & IN_BACK ) ? 1 : 0 );
	int nSteer = ( ( nButtons & IN_MOVERIGHT ) ? 1 : 0 ) - ( ( nButtons & IN_MOVELEFT ) ? 1 : 0 );

	DebugRow( "kart mode", "%s", m_bKartMode ? "on" : "off" );
	DebugRow( "speed", "%.1f", m_flKartSpeed );
	DebugRow( "|velocity|", "%.1f", GetAbsVelocity().Length() );
	DebugRow( "kart yaw", "%.1f", m_flKartYaw );
	DebugRow( "camera yaw", "%.1f", m_flKartCamYaw );
	DebugRow( "grounded", "%s", ( GetFlags() & FL_ONGROUND ) ? "yes" : "no" );
	DebugRow( "ground normal", "%.2f %.2f %.2f", m_vecKartGroundNormal.x, m_vecKartGroundNormal.y, m_vecKartGroundNormal.z );
	DebugRow( "origin", "%.1f %.1f %.1f", vecOrigin.x, vecOrigin.y, vecOrigin.z );
	DebugRow( "throttle", "%d (W=+1, S=-1)", nThrottle );
	DebugRow( "steer", "%d (D=+1, A=-1)", nSteer );
	DebugRow( "pred errors", "set cl_showerror 1 to log them" );

	// Race state
	CHL2MPRules *pRules = HL2MPRules();
	int nLaps = pRules ? pRules->GetKartLaps() : 0;
	int nRacers = pRules ? pRules->GetKartRacers() : 0;
	float flLapTime = ( m_nKartLap > 0 && !m_bKartFinished ) ? MAX( 0.0f, gpGlobals->curtime - m_flKartLapStartTime ) : 0.0f;
	DebugRow( "lap", "%d / %d", m_nKartLap, nLaps );
	DebugRow( "next cp", "%d", m_nKartNextCheckpoint );
	DebugRow( "progress", "%.3f", m_flKartProgress );
	DebugRow( "position", "%d / %d", m_nKartRacePosition, nRacers );
	DebugRow( "wrong way", "%s", m_bKartWrongWay ? "WRONG WAY" : "no" );
	DebugRow( "lap time", "%.2f", flLapTime );
	DebugRow( "best lap", "%.2f", m_flKartBestLap );
	DebugRow( "finished", "%s", m_bKartFinished ? "yes" : "no" );

	// Every kart player, for checking a second client or bots
	DebugRow( "players", "pos lap progress" );
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		C_HL2MP_Player *pKart = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pKart || !pKart->IsInKart() )
			continue;
		DebugRow( pKart->GetPlayerName(), "%d  lap %d  %.3f%s", pKart->GetKartRacePosition(), pKart->GetKartLap(),
			pKart->GetKartProgress(), pKart->IsKartFinished() ? "  finished" : "" );
	}
}

// Top speed the engine pitch is mapped to: the replicated movement convar
// from kart_shareddefs, guarded against a nonsense server value.
static float KartEngineMaxSpeed( void )
{
	float flMaxSpeed = kart_max_speed.GetFloat();
	return ( flMaxSpeed > 0.0f ) ? flMaxSpeed : 650.0f;
}

// Racer kart axle geometry, for the Ackermann angles of the front wheels
// (assets_src/kart_racer/build_kart_racer.py: FRONT and REAR).
#define KART_STEER_WHEELBASE	72.0f
#define KART_STEER_TRACK		54.0f

//-----------------------------------------------------------------------------
// Purpose: Eases the drawn steering angle towards the steer input. The lock
//			shrinks with the turn rate at speed, as the kart turns less there.
//			In a drift the front wheels counter-steer, pointing out of the
//			turn along the slide: least when steering into the drift (the
//			tightest line), most when steering against it (the widest).
//-----------------------------------------------------------------------------
void C_HL2MP_Player::UpdateKartSteering( void )
{
	if ( !IsInKart() || !IsAlive() || IsDormant() )
	{
		m_flKartSteerAngle = 0.0f;
		return;
	}

	const float flLock = cl_kart_steer_angle.GetFloat();
	float flTarget;
	if ( m_nKartDriftDir != 0 )
	{
		float flInto = 0.15f, flNone = 0.5f, flAgainst = 0.9f;
		sscanf( cl_kart_steer_drift_counter.GetString(), "%f %f %f", &flInto, &flNone, &flAgainst );
		float flCounter = flNone;
		if ( m_nKartSteer == m_nKartDriftDir )
		{
			flCounter = flInto;
		}
		else if ( m_nKartSteer != 0 )
		{
			flCounter = flAgainst;
		}
		flTarget = -m_nKartDriftDir * flCounter * flLock;
	}
	else
	{
		float flLow = MAX( kart_turn_rate_low.GetFloat(), 1.0f );
		float flSpeedScale = RemapValClamped( fabs( m_flKartSpeed ) / KartEngineMaxSpeed(), 0.0f, 1.0f, 1.0f, kart_turn_rate_high.GetFloat() / flLow );
		flTarget = m_nKartSteer * flLock * flSpeedScale;
	}

	float flBlend = clamp( gpGlobals->frametime * cl_kart_steer_speed.GetFloat(), 0.0f, 1.0f );
	m_flKartSteerAngle += ( flTarget - m_flKartSteerAngle ) * flBlend;
}

//-----------------------------------------------------------------------------
// Purpose: Turns the steering bones about their local Z (up through each front
//			wheel, along the steering column) by the drawn steering angle. The
//			front wheels follow Ackermann geometry, the inner one turning more.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::BuildTransformations( CStudioHdr *pStudioHdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	BaseClass::BuildTransformations( pStudioHdr, pos, q, cameraTransform, boneMask, boneComputed );

	if ( !IsInKart() || fabs( m_flKartSteerAngle ) < 0.01f )
		return;

	// Positive steer is right: a negative turn about an up axis.
	const float flAngle = m_flKartSteerAngle;
	const float flRadius = KART_STEER_WHEELBASE / tanf( DEG2RAD( fabs( flAngle ) ) );
	const float flInner = RAD2DEG( atanf( KART_STEER_WHEELBASE / MAX( flRadius - KART_STEER_TRACK * 0.5f, 1.0f ) ) );
	const float flOuter = RAD2DEG( atanf( KART_STEER_WHEELBASE / ( flRadius + KART_STEER_TRACK * 0.5f ) ) );
	const bool bRight = ( flAngle > 0.0f );
	const float flSign = bRight ? -1.0f : 1.0f;

	const struct { int iBone; float flTurn; } turns[] =
	{
		{ m_iKartBoneSteerFL, flSign * ( bRight ? flOuter : flInner ) },
		{ m_iKartBoneSteerFR, flSign * ( bRight ? flInner : flOuter ) },
		{ m_iKartBoneSteeringWheel, -flAngle * cl_kart_steer_ratio.GetFloat() },
	};
	for ( int i = 0; i < ARRAYSIZE( turns ); ++i )
	{
		int iBone = turns[i].iBone;
		if ( iBone < 0 || iBone >= pStudioHdr->numbones() || !( pStudioHdr->boneFlags( iBone ) & boneMask ) )
			continue;

		matrix3x4_t matTurn, matOld;
		AngleMatrix( QAngle( 0.0f, turns[i].flTurn, 0.0f ), matTurn );	// yaw: about local Z
		MatrixCopy( GetBone( iBone ), matOld );
		ConcatTransforms( matOld, matTurn, GetBoneForWrite( iBone ) );
	}
}

// A "x y z" convar as a vector, in kart model space.
static Vector KartConVarVector( const ConVar &var )
{
	Vector vec( 0.0f, 0.0f, 0.0f );
	sscanf( var.GetString(), "%f %f %f", &vec.x, &vec.y, &vec.z );
	return vec;
}

//-----------------------------------------------------------------------------
// Purpose: The driver of a kart: a client-only copy of the player's model.
//			HL2MP's models (citizens and combine, all on the ValveBiped
//			skeleton) have no seated animation, so the idle pose is posed in
//			code: moved onto the seat, the upper body tilted towards the
//			steering wheel and leaning into turns, then the legs reach for the
//			pedals and the hands for the kart's grip_l/grip_r attachments,
//			which turn with the steering wheel.
//-----------------------------------------------------------------------------
enum
{
	KART_DRIVER_LEG_L,
	KART_DRIVER_LEG_R,
	KART_DRIVER_ARM_L,
	KART_DRIVER_ARM_R,
	KART_DRIVER_LIMBS
};

class C_KartDriver : public C_BaseAnimating
{
	DECLARE_CLASS( C_KartDriver, C_BaseAnimating );
public:
	explicit C_KartDriver( C_HL2MP_Player *pKart ) : m_pKart( pKart ), m_iPelvis( -1 ), m_iSpine( -1 ) {}

	virtual CStudioHdr *OnNewModel( void ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual int DrawModel( int flags ) OVERRIDE;
	virtual void BuildTransformations( CStudioHdr *pStudioHdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed ) OVERRIDE;

private:
	C_HL2MP_Player *m_pKart;	// owns this entity and removes it before it goes
	int		m_iPelvis;
	int		m_iSpine;
	int		m_iLimb[KART_DRIVER_LIMBS][3];	// thigh, calf, foot / upper arm, forearm, hand; -1 when the model lacks one
};

CStudioHdr *C_KartDriver::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();

	static const char *s_pszLimbs[KART_DRIVER_LIMBS][3] =
	{
		{ "ValveBiped.Bip01_L_Thigh", "ValveBiped.Bip01_L_Calf", "ValveBiped.Bip01_L_Foot" },
		{ "ValveBiped.Bip01_R_Thigh", "ValveBiped.Bip01_R_Calf", "ValveBiped.Bip01_R_Foot" },
		{ "ValveBiped.Bip01_L_UpperArm", "ValveBiped.Bip01_L_Forearm", "ValveBiped.Bip01_L_Hand" },
		{ "ValveBiped.Bip01_R_UpperArm", "ValveBiped.Bip01_R_Forearm", "ValveBiped.Bip01_R_Hand" },
	};
	m_iPelvis = LookupBone( "ValveBiped.Bip01_Pelvis" );
	m_iSpine = LookupBone( "ValveBiped.Bip01_Spine" );
	for ( int i = 0; i < KART_DRIVER_LIMBS; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			m_iLimb[i][j] = LookupBone( s_pszLimbs[i][j] );
		}
	}

	return hdr;
}

bool C_KartDriver::ShouldDraw( void )
{
	return m_pKart && m_pKart->ShouldDraw();
}

int C_KartDriver::DrawModel( int flags )
{
	// Hidden with the kart when the chase camera is pulled inside it.
	if ( m_pKart && m_pKart->IsKartCamTooClose() && m_pKart->IsLocalPlayer() && !( flags & STUDIO_SHADOWDEPTHTEXTURE ) )
		return 0;

	return BaseClass::DrawModel( flags );
}

void C_KartDriver::BuildTransformations( CStudioHdr *pStudioHdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	BaseClass::BuildTransformations( pStudioHdr, pos, q, cameraTransform, boneMask, boneComputed );

	const int nBones = pStudioHdr->numbones();
	if ( !m_pKart || nBones > MAXSTUDIOBONES )
		return;

	// Every bone posed here must have been set up for this mask.
	if ( m_iPelvis < 0 || m_iSpine < 0 || !( pStudioHdr->boneFlags( m_iPelvis ) & boneMask ) || !( pStudioHdr->boneFlags( m_iSpine ) & boneMask ) )
		return;
	for ( int i = 0; i < KART_DRIVER_LIMBS; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			if ( m_iLimb[i][j] < 0 || !( pStudioHdr->boneFlags( m_iLimb[i][j] ) & boneMask ) )
				return;
		}
	}

	int iGrip[2] = { m_pKart->LookupAttachment( "grip_l" ), m_pKart->LookupAttachment( "grip_r" ) };
	Vector vecGrip[2];
	if ( iGrip[0] <= 0 || iGrip[1] <= 0 || !m_pKart->GetAttachment( iGrip[0], vecGrip[0] ) || !m_pKart->GetAttachment( iGrip[1], vecGrip[1] ) )
		return;

	matrix3x4_t *pBones = m_BoneAccessor.GetBoneArrayForWrite();

	matrix3x4_t matKart;
	AngleMatrix( m_pKart->GetRenderAngles(), m_pKart->GetRenderOrigin(), matKart );
	Vector vecForward, vecLeft, vecUp;
	MatrixGetColumn( matKart, 0, vecForward );
	MatrixGetColumn( matKart, 1, vecLeft );
	MatrixGetColumn( matKart, 2, vecUp );

	// Onto the seat: move the whole pose so the pelvis is there.
	Vector vecSeat, vecPelvis;
	VectorTransform( KartConVarVector( cl_kart_driver_seat ), matKart, vecSeat );
	MatrixPosition( pBones[m_iPelvis], vecPelvis );
	const Vector vecShift = vecSeat - vecPelvis;
	for ( int i = 0; i < nBones; ++i )
	{
		for ( int k = 0; k < 3; ++k )
		{
			pBones[i][k][3] += vecShift[k];
		}
	}

	// The upper body (the spine and everything on it) tilts forward and leans
	// into the turn about the pelvis. Positive turns about the left axis
	// pitch forward, about the forward axis roll right.
	matrix3x4_t matTilt, matLean, matUpper;
	MatrixBuildRotationAboutAxis( vecLeft, cl_kart_driver_tilt.GetFloat(), matTilt );
	MatrixBuildRotationAboutAxis( vecForward, m_pKart->GetKartDriverLean() * cl_kart_driver_lean.GetFloat(), matLean );
	ConcatTransforms( matLean, matTilt, matUpper );
	Vector vecPivot;
	VectorRotate( vecSeat, matUpper, vecPivot );
	MatrixSetColumn( vecSeat - vecPivot, 3, matUpper );

	bool bUpper[MAXSTUDIOBONES];
	for ( int i = 0; i < nBones; ++i )
	{
		int iParent = pStudioHdr->boneParent( i );
		bUpper[i] = ( i == m_iSpine ) || ( iParent >= 0 && bUpper[iParent] );
		if ( bUpper[i] )
		{
			matrix3x4_t matOld;
			MatrixCopy( pBones[i], matOld );
			ConcatTransforms( matUpper, matOld, pBones[i] );
		}
	}

	matrix3x4_t matBefore[MAXSTUDIOBONES];
	memcpy( matBefore, pBones, nBones * sizeof( matrix3x4_t ) );

	// Feet on the pedals, knees up; hands on the grips, elbows out and down.
	Vector vecFeet = KartConVarVector( cl_kart_driver_feet );
	Vector vecTarget[KART_DRIVER_LIMBS], vecBend[KART_DRIVER_LIMBS];
	VectorTransform( Vector( vecFeet.x, vecFeet.y, vecFeet.z ), matKart, vecTarget[KART_DRIVER_LEG_L] );
	VectorTransform( Vector( vecFeet.x, -vecFeet.y, vecFeet.z ), matKart, vecTarget[KART_DRIVER_LEG_R] );
	vecTarget[KART_DRIVER_ARM_L] = vecGrip[0];
	vecTarget[KART_DRIVER_ARM_R] = vecGrip[1];
	vecBend[KART_DRIVER_LEG_L] = vecBend[KART_DRIVER_LEG_R] = vecUp;
	vecBend[KART_DRIVER_ARM_L] = ( vecLeft - vecUp ).Normalized();
	vecBend[KART_DRIVER_ARM_R] = ( -vecLeft - vecUp ).Normalized();

	bool bMoved[MAXSTUDIOBONES] = {};
	for ( int i = 0; i < KART_DRIVER_LIMBS; ++i )
	{
		const int *iBone = m_iLimb[i];
		Vector vecJoint;
		MatrixPosition( pBones[iBone[1]], vecJoint );
		if ( !Studio_SolveIK( iBone[0], iBone[1], iBone[2], vecTarget[i], vecJoint, vecBend[i], pBones ) )
			continue;

		if ( i == KART_DRIVER_ARM_L || i == KART_DRIVER_ARM_R )
		{
			// A straight wrist, carrying on from the forearm.
			Vector vecForearm, vecHand;
			MatrixPosition( pBones[iBone[1]], vecForearm );
			MatrixPosition( pBones[iBone[2]], vecHand );
			Studio_AlignIKMatrix( pBones[iBone[2]], vecHand - vecForearm );
		}
		bMoved[iBone[0]] = bMoved[iBone[1]] = bMoved[iBone[2]] = true;
	}

	// Everything hanging off the posed limbs (twist bones, fingers, toes)
	// follows its parent.
	for ( int i = 0; i < nBones; ++i )
	{
		int iParent = pStudioHdr->boneParent( i );
		if ( bMoved[i] || iParent < 0 || !bMoved[iParent] )
			continue;

		matrix3x4_t matParentInv, matLocal;
		MatrixInvert( matBefore[iParent], matParentInv );
		ConcatTransforms( matParentInv, matBefore[i], matLocal );
		ConcatTransforms( pBones[iParent], matLocal, pBones[i] );
		bMoved[i] = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Keeps the driver in the kart: created for karts with grips once the
//			server has picked a model, replaced when the model changes, gone
//			with the kart. Eases the lean towards the turn, which grows with
//			speed: into the drift while drifting, else the way it is steering.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::UpdateKartDriver( void )
{
	const model_t *pModel = ( m_nKartDriverModel > 0 ) ? modelinfo->GetModel( m_nKartDriverModel ) : NULL;
	if ( !pModel || !cl_kart_driver.GetBool() || !IsInKart() || !IsAlive() || IsDormant()
		|| LookupAttachment( "grip_l" ) <= 0 || LookupAttachment( "grip_r" ) <= 0 )
	{
		RemoveKartDriver();
		m_flKartDriverLean = 0.0f;
		return;
	}

	float flTurn = ( m_nKartDriftDir != 0 ) ? m_nKartDriftDir : m_nKartSteer;
	float flTarget = flTurn * RemapValClamped( fabs( m_flKartSpeed ), 0.0f, KartEngineMaxSpeed() * 0.5f, 0.0f, 1.0f );
	float flBlend = clamp( gpGlobals->frametime * cl_kart_steer_speed.GetFloat() * 0.5f, 0.0f, 1.0f );
	m_flKartDriverLean += ( flTarget - m_flKartDriverLean ) * flBlend;

	if ( m_pKartDriver && m_pKartDriver->GetModel() != pModel )
	{
		RemoveKartDriver();
	}

	if ( !m_pKartDriver )
	{
		C_KartDriver *pDriver = new C_KartDriver( this );
		if ( !pDriver->InitializeAsClientEntity( modelinfo->GetModelName( pModel ), RENDER_GROUP_OPAQUE_ENTITY ) )
		{
			pDriver->Release();
			return;
		}

		int iSequence = pDriver->SelectWeightedSequence( ACT_HL2MP_IDLE );
		pDriver->SetSequence( ( iSequence >= 0 ) ? iSequence : 0 );
		pDriver->SetCycle( 0.0f );
		pDriver->SetPlaybackRate( 0.0f );
		m_pKartDriver = pDriver;
	}

	m_pKartDriver->SetAbsOrigin( GetRenderOrigin() );
	m_pKartDriver->SetAbsAngles( GetRenderAngles() );
	m_pKartDriver->UpdateVisibility();
}

void C_HL2MP_Player::RemoveKartDriver( void )
{
	if ( m_pKartDriver )
	{
		m_pKartDriver->Release();
		m_pKartDriver = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Engine idle loop pitched by speed, plus a rev loop that fades in
//			on throttle. Runs for every kart player the client knows about.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::UpdateKartSounds( void )
{
	if ( !IsInKart() || !IsAlive() || IsDormant() )
	{
		StopKartSounds();
		return;
	}

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	float flSpeed = fabs( m_flKartSpeed );
	float flPitch = RemapValClamped( flSpeed, 0.0f, KartEngineMaxSpeed(), kart_engine_pitch_min.GetFloat(), kart_engine_pitch_max.GetFloat() );

	if ( !m_pKartEngineIdle )
	{
		CPASAttenuationFilter filter( this );
		m_pKartEngineIdle = controller.SoundCreate( filter, entindex(), "Kart.EngineIdle" );
		controller.Play( m_pKartEngineIdle, 1.0f, flPitch );
	}

	if ( !m_pKartEngineRev )
	{
		CPASAttenuationFilter filter( this );
		m_pKartEngineRev = controller.SoundCreate( filter, entindex(), "Kart.EngineRev" );
		controller.Play( m_pKartEngineRev, 0.0f, flPitch );
	}

	// Throttle: the local player has its buttons; for everyone else, speed going up.
	bool bThrottle;
	if ( IsLocalPlayer() )
	{
		bThrottle = ( m_nButtons & IN_FORWARD ) != 0;
	}
	else
	{
		bThrottle = flSpeed > m_flKartSoundLastSpeed + 0.5f;
	}
	m_flKartSoundLastSpeed = flSpeed;

	controller.SoundChangePitch( m_pKartEngineIdle, flPitch, 0.1f );
	controller.SoundChangePitch( m_pKartEngineRev, flPitch, 0.1f );
	controller.SoundChangeVolume( m_pKartEngineIdle, bThrottle ? 0.6f : 1.0f, 0.25f );
	controller.SoundChangeVolume( m_pKartEngineRev, bThrottle ? 1.0f : 0.0f, 0.25f );

	// Skid screech: starts on drift entry, louder the further the kart slides
	// sideways, fades out when the drift ends.
	if ( IsDrifting() )
	{
		float flMaxSlip = MAX( kart_drift_slip_angle.GetFloat(), 1.0f );
		float flSkidVolume = RemapValClamped( fabs( GetKartSkidSlipAngle() ), 0.0f, flMaxSlip, 0.2f, 1.0f );

		if ( !m_pKartSkid )
		{
			CPASAttenuationFilter filter( this );
			m_pKartSkid = controller.SoundCreate( filter, entindex(), "Kart.Skid" );
			controller.Play( m_pKartSkid, 0.0f, 100 );
		}

		controller.SoundChangeVolume( m_pKartSkid, flSkidVolume, 0.1f );
	}
	else if ( m_pKartSkid )
	{
		controller.SoundFadeOut( m_pKartSkid, 0.15f, true );
		m_pKartSkid = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: The drift slip angle in degrees. The local player has it predicted;
//			for everyone else it isn't networked, so it comes from the
//			networked kart heading against the interpolated velocity.
//-----------------------------------------------------------------------------
float C_HL2MP_Player::GetKartSkidSlipAngle( void )
{
	if ( IsLocalPlayer() )
		return m_flKartSlipAngle;

	Vector vecVelocity;
	EstimateAbsVelocity( vecVelocity );
	vecVelocity.z = 0.0f;
	if ( vecVelocity.LengthSqr() < 50.0f * 50.0f )
		return 0.0f;

	return AngleDiff( m_flKartYaw, UTIL_VecToYaw( vecVelocity ) );
}

void C_HL2MP_Player::StopKartSounds( void )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	if ( m_pKartEngineIdle )
	{
		controller.SoundDestroy( m_pKartEngineIdle );
		m_pKartEngineIdle = NULL;
	}

	if ( m_pKartEngineRev )
	{
		controller.SoundDestroy( m_pKartEngineRev );
		m_pKartEngineRev = NULL;
	}

	if ( m_pKartSkid )
	{
		controller.SoundDestroy( m_pKartSkid );
		m_pKartSkid = NULL;
	}

	m_flKartSoundLastSpeed = 0.0f;
}

// The skid decals are 512 px long at $decalscale .5: 256 units, centred on
// where they are shot. Shooting them half that behind the wheel keeps the mark
// from reaching ahead of the kart.
#define KART_SKIDMARK_HALF_LENGTH	128.0f

//-----------------------------------------------------------------------------
// Purpose: Every kart_skidmark_interval units a drifting kart travels, a tire
//			mark under each rear wheel, along the direction of travel.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::UpdateKartSkidmarks( void )
{
	if ( !kart_skidmarks.GetBool() || !r_decals.GetBool() || !IsInKart() || !IsAlive() || IsDormant() || !IsDrifting() )
	{
		m_bKartSkidActive = false;
		return;
	}

	Vector vecOrigin = GetAbsOrigin();
	if ( !m_bKartSkidActive )
	{
		// Drift entry: the first marks go down straight away.
		m_bKartSkidActive = true;
		m_vecKartSkidLastPos = vecOrigin;
		m_flKartSkidDist = MAX( kart_skidmark_interval.GetFloat(), 1.0f );
	}
	else
	{
		m_flKartSkidDist += ( vecOrigin - m_vecKartSkidLastPos ).Length2D();
		m_vecKartSkidLastPos = vecOrigin;
	}

	if ( m_flKartSkidDist < MAX( kart_skidmark_interval.GetFloat(), 1.0f ) )
		return;

	Vector vecDir;
	EstimateAbsVelocity( vecDir );
	vecDir.z = 0.0f;
	if ( VectorNormalize( vecDir ) < 1.0f )
		return;

	m_flKartSkidDist = 0.0f;
	ShootKartSkidmark( "wheel_rl", 1.0f, vecDir );
	ShootKartSkidmark( "wheel_rr", -1.0f, vecDir );
}

//-----------------------------------------------------------------------------
// Purpose: One tire mark on the ground under a rear wheel. The wheel comes
//			from its attachment; models without one use a corner of the kart
//			hull (flSide +1 is left, -1 right).
//-----------------------------------------------------------------------------
void C_HL2MP_Player::ShootKartSkidmark( const char *pszAttachment, float flSide, const Vector &vecDir )
{
	Vector vecWheel;
	QAngle angWheel;
	int iAttachment = LookupAttachment( pszAttachment );
	if ( iAttachment <= 0 || !GetAttachment( iAttachment, vecWheel, angWheel ) )
	{
		Vector vecForward, vecLeft;
		AngleVectors( QAngle( 0, m_flKartYaw, 0 ), &vecForward, &vecLeft, NULL );
		vecLeft.Negate();
		vecWheel = GetAbsOrigin() + vecForward * ( KART_HULL_MIN.x * 0.75f ) + vecLeft * ( flSide * KART_HULL_MAX.y * 0.75f );
	}

	// Short trace: an airborne kart leaves no mark.
	trace_t tr;
	UTIL_TraceLine( vecWheel + Vector( 0, 0, 16 ), vecWheel - Vector( 0, 0, 24 ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction == 1.0f || tr.startsolid || !tr.m_pEnt || tr.plane.normal.z < 0.7f )
		return;

	C_BaseEntity *pHit = tr.m_pEnt;
	if ( !pHit->GetModel() || modelinfo->GetModelType( pHit->GetModel() ) != mod_brush )
		return;

	static const char *s_pszSkidDecals[] = { "decals/decal_skidmark01", "decals/decal_skidmark02" };
	int iDecal = effects->Draw_DecalIndexFromName( (char *)s_pszSkidDecals[ RandomInt( 0, ARRAYSIZE( s_pszSkidDecals ) - 1 ) ] );

	// The decal textures are long in V: putting S across the direction of
	// travel lays the streak along it.
	Vector vecRight = CrossProduct( vecDir, tr.plane.normal );
	if ( VectorNormalize( vecRight ) < 0.001f )
		return;

	Vector vecPos = tr.endpos - vecDir * KART_SKIDMARK_HALF_LENGTH;
	effects->DecalShoot( iDecal, pHit->entindex(), pHit->GetModel(), pHit->GetAbsOrigin(), pHit->GetAbsAngles(), vecPos, &vecRight, 0 );
}

// Exhaust and spark emission rates, particles per second (sparks per rear wheel).
#define KART_EXHAUST_RATE		60.0f
#define KART_SPARK_RATE_BASE	20.0f
#define KART_SPARK_RATE_TIER	15.0f
// Most particles one think may emit, so a hitch doesn't dump a burst.
#define KART_FX_MAX_PER_THINK	6

// Mini-turbo spark colors by drift tier (index 0 unused): ice blue, amber, magenta.
static const color24 s_KartSparkColors[] =
{
	{ 255, 255, 255 },
	{ 90, 180, 255 },
	{ 255, 160, 40 },
	{ 255, 60, 200 },
};

//-----------------------------------------------------------------------------
// Purpose: Boost flame at the exhaust while boosting, and sparks at the rear
//			wheels colored by the mini-turbo tier while drifting. Both come
//			from networked state, so remote karts get them too. The particles
//			live a fraction of a second: when the boost or drift ends (or the
//			kart dies, respawns or leaves the PVS) the emission stops and the
//			effect is gone a moment later.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::UpdateKartBoostFX( void )
{
	bool bActive = kart_boost_fx.GetBool() && IsInKart() && IsAlive() && !IsDormant();
	bool bExhaust = bActive && IsKartBoosting();
	bool bSparks = bActive && IsDrifting() && GetKartDriftTier() > 0;

	if ( !bExhaust )
		m_flKartExhaustAccum = 0.0f;
	if ( !bSparks )
		m_flKartSparkAccum = 0.0f;
	if ( !bExhaust && !bSparks )
		return;

	if ( !m_pKartFXEmitter )
	{
		m_pKartFXEmitter = CSimpleEmitter::Create( "C_HL2MP_Player::KartBoostFX" );
		if ( !m_pKartFXEmitter )
			return;
	}
	m_pKartFXEmitter->SetSortOrigin( GetAbsOrigin() );

	if ( bExhaust )
	{
		m_flKartExhaustAccum += gpGlobals->frametime * KART_EXHAUST_RATE;
		int nCount = (int)m_flKartExhaustAccum;
		m_flKartExhaustAccum -= nCount;
		if ( nCount > 0 )
		{
			EmitKartExhaust( MIN( nCount, KART_FX_MAX_PER_THINK ) );
		}
	}

	if ( bSparks )
	{
		m_flKartSparkAccum += gpGlobals->frametime * ( KART_SPARK_RATE_BASE + KART_SPARK_RATE_TIER * GetKartDriftTier() );
		int nCount = (int)m_flKartSparkAccum;
		m_flKartSparkAccum -= nCount;
		if ( nCount > 0 )
		{
			EmitKartDriftSparks( MIN( nCount, KART_FX_MAX_PER_THINK ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Boost flame: a glow on the exhaust pipe and flame puffs blown out
//			of it. Models without an exhaust attachment use a point behind the
//			kart hull. The particles carry the kart's velocity so the flame
//			stays on the pipe at speed.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::EmitKartExhaust( int nCount )
{
	Vector vecForward, vecRight, vecUp;
	AngleVectors( QAngle( 0, m_flKartYaw, 0 ), &vecForward, &vecRight, &vecUp );

	Vector vecPos, vecDir;
	QAngle angExhaust;
	int iAttachment = LookupAttachment( "exhaust" );
	if ( iAttachment > 0 && GetAttachment( iAttachment, vecPos, angExhaust ) )
	{
		AngleVectors( angExhaust, &vecDir );
	}
	else
	{
		vecPos = GetAbsOrigin() + vecForward * ( KART_HULL_MIN.x - 4.0f ) + vecUp * 24.0f;
		vecDir = -vecForward;
	}

	Vector vecKartVel;
	EstimateAbsVelocity( vecKartVel );

	PMaterialHandle hGlow = m_pKartFXEmitter->GetPMaterial( "sprites/light_glow02_add" );
	PMaterialHandle hHalo = m_pKartFXEmitter->GetPMaterial( "sprites/glow01" );

	for ( int i = 0; i < nCount; i++ )
	{
		// Hot core on the pipe.
		SimpleParticle *pParticle = m_pKartFXEmitter->AddSimpleParticle( hGlow, vecPos, 0.05f, 22 );
		if ( pParticle )
		{
			pParticle->m_vecVelocity = vecKartVel;
			pParticle->m_uchColor[0] = 255;
			pParticle->m_uchColor[1] = 230;
			pParticle->m_uchColor[2] = 180;
			pParticle->m_uchStartAlpha = 255;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = RandomInt( 18, 24 );
			pParticle->m_uchEndSize = 12;
		}

		// Wider orange halo.
		pParticle = m_pKartFXEmitter->AddSimpleParticle( hHalo, vecPos + vecDir * 6.0f, 0.06f, 40 );
		if ( pParticle )
		{
			pParticle->m_vecVelocity = vecKartVel;
			pParticle->m_uchColor[0] = 255;
			pParticle->m_uchColor[1] = 140;
			pParticle->m_uchColor[2] = 50;
			pParticle->m_uchStartAlpha = 160;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = RandomInt( 34, 44 );
			pParticle->m_uchEndSize = 24;
		}

		// Flame puff blown out of the pipe.
		char szFlame[32];
		Q_snprintf( szFlame, sizeof( szFlame ), "sprites/flamelet%d", RandomInt( 1, 5 ) );
		pParticle = m_pKartFXEmitter->AddSimpleParticle( m_pKartFXEmitter->GetPMaterial( szFlame ), vecPos, RandomFloat( 0.08f, 0.14f ), 12 );
		if ( pParticle )
		{
			pParticle->m_vecVelocity = vecKartVel + vecDir * RandomFloat( 180.0f, 280.0f ) + vecRight * RandomFloat( -25.0f, 25.0f ) + vecUp * RandomFloat( -10.0f, 30.0f );
			pParticle->m_uchColor[0] = 255;
			pParticle->m_uchColor[1] = 255;
			pParticle->m_uchColor[2] = 255;
			pParticle->m_uchStartAlpha = 255;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = RandomInt( 10, 14 );
			pParticle->m_uchEndSize = RandomInt( 2, 4 );
			pParticle->m_flRoll = RandomFloat( 0.0f, 360.0f );
			pParticle->m_flRollDelta = RandomFloat( -4.0f, 4.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Mini-turbo sparks thrown back off each rear wheel, with a glow on
//			the wheel, in the color of the drift tier the charge has reached.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::EmitKartDriftSparks( int nCount )
{
	int nTier = clamp( GetKartDriftTier(), 1, (int)ARRAYSIZE( s_KartSparkColors ) - 1 );
	const color24 &color = s_KartSparkColors[ nTier ];

	Vector vecForward, vecLeft, vecUp;
	AngleVectors( QAngle( 0, m_flKartYaw, 0 ), &vecForward, &vecLeft, &vecUp );
	vecLeft.Negate();

	Vector vecKartVel;
	EstimateAbsVelocity( vecKartVel );

	PMaterialHandle hSpark = m_pKartFXEmitter->GetPMaterial( "sprites/light_glow02_add" );
	PMaterialHandle hGlow = m_pKartFXEmitter->GetPMaterial( "sprites/glow01" );

	static const char *s_pszWheels[] = { "wheel_rl", "wheel_rr" };
	for ( int iWheel = 0; iWheel < 2; iWheel++ )
	{
		float flSide = iWheel == 0 ? 1.0f : -1.0f;

		Vector vecWheel;
		QAngle angWheel;
		int iAttachment = LookupAttachment( s_pszWheels[iWheel] );
		if ( iAttachment <= 0 || !GetAttachment( iAttachment, vecWheel, angWheel ) )
		{
			vecWheel = GetAbsOrigin() + vecForward * ( KART_HULL_MIN.x * 0.75f ) + vecLeft * ( flSide * KART_HULL_MAX.y * 0.75f );
		}
		vecWheel += vecUp * 3.0f;

		// Glow on the wheel, bigger with the tier.
		SimpleParticle *pParticle = m_pKartFXEmitter->AddSimpleParticle( hGlow, vecWheel, 0.08f, 16 );
		if ( pParticle )
		{
			pParticle->m_vecVelocity = vecKartVel;
			pParticle->m_uchColor[0] = color.r;
			pParticle->m_uchColor[1] = color.g;
			pParticle->m_uchColor[2] = color.b;
			pParticle->m_uchStartAlpha = 200;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = 10 + 5 * nTier;
			pParticle->m_uchEndSize = 6 + 3 * nTier;
		}

		for ( int i = 0; i < nCount; i++ )
		{
			pParticle = m_pKartFXEmitter->AddSimpleParticle( hSpark, vecWheel, RandomFloat( 0.15f, 0.3f ), 4 );
			if ( !pParticle )
				break;

			pParticle->m_vecVelocity = vecKartVel * 0.7f
				- vecForward * RandomFloat( 60.0f, 160.0f )
				+ vecLeft * ( flSide * RandomFloat( 0.0f, 80.0f ) )
				+ vecUp * RandomFloat( 40.0f, 140.0f );
			pParticle->m_uchColor[0] = color.r;
			pParticle->m_uchColor[1] = color.g;
			pParticle->m_uchColor[2] = color.b;
			pParticle->m_uchStartAlpha = 255;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = RandomInt( 3, 5 );
			pParticle->m_uchEndSize = 1;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_Player::DrawModel( int flags )
{
	if ( !m_bReadyToDraw )
		return 0;

	// The chase camera was pulled into the kart by a wall: don't draw it from
	// inside. Checked per draw rather than in ShouldDraw, which is cached in the
	// leaf system. Its shadows still render.
	if ( m_bKartCamTooClose && IsLocalPlayer() && !( flags & STUDIO_SHADOWDEPTHTEXTURE ) )
		return 0;

    return BaseClass::DrawModel(flags);
}

//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::ShouldReceiveProjectedTextures( int flags )
{
	Assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( IsEffectActive( EF_NODRAW ) )
		 return false;

	if( flags & SHADOW_FLAGS_FLASHLIGHT )
	{
		return true;
	}

	return BaseClass::ShouldReceiveProjectedTextures( flags );
}

void C_HL2MP_Player::DoImpactEffect( trace_t &tr, int nDamageType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->DoImpactEffect( tr, nDamageType );
		return;
	}

	BaseClass::DoImpactEffect( tr, nDamageType );
}

void C_HL2MP_Player::PreThink( void )
{
	QAngle vTempAngles = GetLocalAngles();

	if ( GetLocalPlayer() == this )
	{
		vTempAngles[PITCH] = EyeAngles()[PITCH];
	}
	else
	{
		vTempAngles[PITCH] = m_angEyeAngles[PITCH];
	}

	if ( vTempAngles[YAW] < 0.0f )
	{
		vTempAngles[YAW] += 360.0f;
	}

	SetLocalAngles( vTempAngles );

	BaseClass::PreThink();
}

const QAngle &C_HL2MP_Player::EyeAngles()
{
	if( IsLocalPlayer() )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		return m_angEyeAngles;
	}
}

//-----------------------------------------------------------------------------
// Charge battery fully, turn off all devices.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::SuitPower_Initialize( void )
{
	m_HL2Local.m_bitsActiveDevices = 0x00000000;
	m_HL2Local.m_flSuitPower = 100.0;
	m_HL2Local.m_flSuitPowerLoad = 0.0;
}


//-----------------------------------------------------------------------------
// Purpose: Interface to drain power from the suit's power supply.
// Input:	Amount of charge to remove (expressed as percentage of full charge)
// Output:	Returns TRUE if successful, FALSE if not enough power available.
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::SuitPower_Drain( float flPower )
{
	// Suitpower cheat on?
	if ( sv_infinite_aux_power.GetBool() )
		return true;

	m_HL2Local.m_flSuitPower -= flPower;

	if ( m_HL2Local.m_flSuitPower < 0.01 )
	{
		// Power is depleted!
		// Clamp and fail
		m_HL2Local.m_flSuitPower = 0.0;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Interface to add power to the suit's power supply
// Input:	Amount of charge to add
//-----------------------------------------------------------------------------
void C_HL2MP_Player::SuitPower_Charge( float flPower )
{
	m_HL2Local.m_flSuitPower += flPower;

	if( m_HL2Local.m_flSuitPower > 100.0 )
	{
		// Full charge, clamp.
		m_HL2Local.m_flSuitPower = 100.0;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::SuitPower_IsDeviceActive( const CSuitPowerDevice &device )
{
	return (m_HL2Local.m_bitsActiveDevices & device.GetDeviceID()) != 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::SuitPower_AddDevice( const CSuitPowerDevice &device )
{
	// Make sure this device is NOT active!!
	if( m_HL2Local.m_bitsActiveDevices & device.GetDeviceID() )
		return false;

	if( !IsSuitEquipped() )
		return false;

	m_HL2Local.m_bitsActiveDevices |= device.GetDeviceID();
	m_HL2Local.m_flSuitPowerLoad += device.GetDeviceDrainRate();
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::SuitPower_RemoveDevice( const CSuitPowerDevice &device )
{
	// Make sure this device is active!!
	if( ! (m_HL2Local.m_bitsActiveDevices & device.GetDeviceID()) )
		return false;

	if( !IsSuitEquipped() )
		return false;

	// Take a little bit of suit power when you disable a device. If the device is shutting off
	// because the battery is drained, no harm done, the battery charge cannot go below 0. 
	// This code in combination with the delay before the suit can start recharging are a defense
	// against exploits where the player could rapidly tap sprint and never run out of power.
	MsgPredTest2( "[Client %d] [A REMOVE] m_HL2Local.m_flSuitPower: %f\n", gpGlobals->tickcount, m_HL2Local.m_flSuitPower );
	SuitPower_Drain( device.GetDeviceDrainRate() * 0.1f );
	MsgPredTest2( "[Client %d] [B REMOVE] m_HL2Local.m_flSuitPower: %f\n", gpGlobals->tickcount, m_HL2Local.m_flSuitPower );

	m_HL2Local.m_bitsActiveDevices &= ~device.GetDeviceID();
	m_HL2Local.m_flSuitPowerLoad -= device.GetDeviceDrainRate();

	if( m_HL2Local.m_bitsActiveDevices == 0x00000000 )
	{
		// With this device turned off, we can set this timer which tells us when the
		// suit power system entered a no-load state.
		m_HL2Local.m_flTimeAllSuitDevicesOff = gpGlobals->curtime;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define SUITPOWER_BEGIN_RECHARGE_DELAY	0.5f
bool C_HL2MP_Player::SuitPower_ShouldRecharge( void )
{
	// Make sure all devices are off.
	if( m_HL2Local.m_bitsActiveDevices != 0x00000000 )
		return false;

	// Is the system fully charged?
	if( m_HL2Local.m_flSuitPower >= 100.0f )
		return false; 

	// Has the system been in a no-load state for long enough
	// to begin recharging?
	if( gpGlobals->curtime < m_HL2Local.m_flTimeAllSuitDevicesOff + SUITPOWER_BEGIN_RECHARGE_DELAY )
		return false;

	return true;
}

void C_HL2MP_Player::SuitPower_Update( void )
{
	if( SuitPower_ShouldRecharge() )
	{
		SuitPower_Charge( SUITPOWER_CHARGE_RATE * gpGlobals->frametime );
	}
	else if( m_HL2Local.m_bitsActiveDevices )
	{
		float flPowerLoad = m_HL2Local.m_flSuitPowerLoad;

		//Since stickysprint quickly shuts off sprint if it isn't being used, this isn't an issue.
		// misyl: no sticky sprint for hl2mp.
		//if ( !sv_stickysprint.GetBool() )
		{
			if( SuitPower_IsDeviceActive(SuitDeviceSprint) )
			{
				if( CloseEnough(fabs(GetAbsVelocity().x), 0.0f) && CloseEnough(fabs(GetAbsVelocity().y), 0.0f) )
				{
					if ( CloseEnough( m_HL2Local.m_flSuitPowerLoad, SuitDeviceSprint.GetDeviceDrainRate() ) )
					{
						flPowerLoad = 0.0f;
					}
					else
					{
						// If player's not moving, don't drain sprint juice.
						flPowerLoad -= SuitDeviceSprint.GetDeviceDrainRate();
					}
				}
			}
		}

		if( SuitPower_IsDeviceActive(SuitDeviceFlashlight) )
		{
			//float factor;

			//factor = 1.0f / m_flFlashlightPowerDrainScale;

			float factor = 1.0f;

			flPowerLoad -= ( SuitDeviceFlashlight.GetDeviceDrainRate() * (1.0f - factor) );
		}

		SuitPower_Drain( flPowerLoad * gpGlobals->frametime );

	}
	MsgPredTest2( "[Client %d] m_HL2Local.m_flSuitPower: %f m_fIsSprinting: %d\n", gpGlobals->tickcount, m_HL2Local.m_flSuitPower, m_fIsSprinting ? 1 : 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_HL2MP_Player::AddEntity( void )
{
	BaseClass::AddEntity();

	QAngle vTempAngles = GetLocalAngles();
	vTempAngles[PITCH] = m_angEyeAngles[PITCH];

	SetLocalAngles( vTempAngles );
		
	if ( !IsInKart() )
	{
		m_PlayerAnimState.Update();
	}

	// Zero out model pitch, blending takes care of all of it.
	SetLocalAnglesDim( X_INDEX, 0 );

	if( this != C_BasePlayer::GetLocalPlayer() )
	{
		if ( IsEffectActive( EF_DIMLIGHT ) )
		{
			int iAttachment = LookupAttachment( "anim_attachment_RH" );

			if ( iAttachment < 0 )
				return;

			Vector vecOrigin;
			QAngle eyeAngles = m_angEyeAngles;
	
			GetAttachment( iAttachment, vecOrigin, eyeAngles );

			Vector vForward;
			AngleVectors( eyeAngles, &vForward );
				
			trace_t tr;
			UTIL_TraceLine( vecOrigin, vecOrigin + (vForward * 200), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

			if( !m_pFlashlightBeam )
			{
				BeamInfo_t beamInfo;
				beamInfo.m_nType = TE_BEAMPOINTS;
				beamInfo.m_vecStart = tr.startpos;
				beamInfo.m_vecEnd = tr.endpos;
				beamInfo.m_pszModelName = "sprites/glow01.vmt";
				beamInfo.m_pszHaloName = "sprites/glow01.vmt";
				beamInfo.m_flHaloScale = 3.0;
				beamInfo.m_flWidth = 8.0f;
				beamInfo.m_flEndWidth = 35.0f;
				beamInfo.m_flFadeLength = 300.0f;
				beamInfo.m_flAmplitude = 0;
				beamInfo.m_flBrightness = 60.0;
				beamInfo.m_flSpeed = 0.0f;
				beamInfo.m_nStartFrame = 0.0;
				beamInfo.m_flFrameRate = 0.0;
				beamInfo.m_flRed = 255.0;
				beamInfo.m_flGreen = 255.0;
				beamInfo.m_flBlue = 255.0;
				beamInfo.m_nSegments = 8;
				beamInfo.m_bRenderable = true;
				beamInfo.m_flLife = 0.5;
				beamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM;
				
				m_pFlashlightBeam = beams->CreateBeamPoints( beamInfo );
			}

			if( m_pFlashlightBeam )
			{
				BeamInfo_t beamInfo;
				beamInfo.m_vecStart = tr.startpos;
				beamInfo.m_vecEnd = tr.endpos;
				beamInfo.m_flRed = 255.0;
				beamInfo.m_flGreen = 255.0;
				beamInfo.m_flBlue = 255.0;

				beams->UpdateBeamInfo( m_pFlashlightBeam, beamInfo );

				dlight_t *el = effects->CL_AllocDlight( 0 );
				el->origin = tr.endpos;
				el->radius = 50; 
				el->color.r = 200;
				el->color.g = 200;
				el->color.b = 200;
				el->die = gpGlobals->curtime + 0.1;
			}
		}
		else if ( m_pFlashlightBeam )
		{
			ReleaseFlashlight();
		}
	}
}

ShadowType_t C_HL2MP_Player::ShadowCastType( void ) 
{
	if ( !IsVisible() )
		 return SHADOWS_NONE;

	return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
}


const QAngle& C_HL2MP_Player::GetRenderAngles()
{
	if ( IsRagdoll() )
	{
		return vec3_angle;
	}
	else if ( IsInKart() )
	{
		// The kart body faces its own heading, not the eyes. The local player's is
		// predicted; for everyone else the server's angles are all we have.
		float flYaw = IsLocalPlayer() ? m_flKartYaw : GetAbsAngles()[YAW];

		// The karts built for this mod face +X; HL2's jeep faces -Y and would
		// drive sideways.
		const model_t *pModel = GetModel();
		if ( pModel && !V_stricmp( modelinfo->GetModelName( pModel ), KART_PLACEHOLDER_MODEL ) )
		{
			flYaw += KART_PLACEHOLDER_YAW;
		}

		m_angKartRenderAngles.Init( 0.0f, flYaw, 0.0f );

		// Pitch and roll with the ground: the heading's frame turned so its up is
		// the (smoothed) ground normal. Called several times a frame; only time
		// passing moves the smoothing.
		float flRate = kart_tilt_smooth.GetFloat();
		if ( flRate > 0.0f )
		{
			float flDelta = gpGlobals->curtime - m_flKartTiltTime;
			m_flKartTiltTime = gpGlobals->curtime;
			if ( flDelta < 0.0f || flDelta > 0.5f )
			{
				m_vecKartTiltNormal = m_vecKartGroundNormal;	// first frame, teleport or a long pause
			}
			else if ( flDelta > 0.0f )
			{
				m_vecKartTiltNormal = Lerp( 1.0f - expf( -flRate * flDelta ), m_vecKartTiltNormal, m_vecKartGroundNormal );
			}

			Vector vecUp = m_vecKartTiltNormal;
			if ( VectorNormalize( vecUp ) > 0.5f && vecUp.z > 0.5f )
			{
				Vector vecForward;
				AngleVectors( m_angKartRenderAngles, &vecForward );
				vecForward -= DotProduct( vecForward, vecUp ) * vecUp;
				VectorNormalize( vecForward );
				VectorAngles( vecForward, vecUp, m_angKartRenderAngles );
			}
		}

		return m_angKartRenderAngles;
	}
	else
	{
		return m_PlayerAnimState.GetRenderAngles();
	}
}

bool C_HL2MP_Player::ShouldDraw( void )
{
	// If we're dead, our ragdoll will be drawn for us instead.
	if ( !IsAlive() )
		return false;

//	if( GetTeamNumber() == TEAM_SPECTATOR )
//		return false;

	if( IsLocalPlayer() && IsRagdoll() )
		return true;
	
	if ( IsRagdoll() )
		return false;

	return BaseClass::ShouldDraw();
}

void C_HL2MP_Player::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	if ( state == SHOULDTRANSMIT_END )
	{
		if( m_pFlashlightBeam != NULL )
		{
			ReleaseFlashlight();
		}

		StopKartSounds();
		RemoveKartDriver();
	}

	BaseClass::NotifyShouldTransmit( state );
}

void C_HL2MP_Player::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	UpdateVisibility();
}

void C_HL2MP_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	if ( m_iSpawnInterpCounter != m_iSpawnInterpCounterCache )
	{
		MoveToLastReceivedPosition( true );
		ResetLatched();
		m_iSpawnInterpCounterCache = m_iSpawnInterpCounter;
	}

	BaseClass::PostDataUpdate( updateType );
}

void C_HL2MP_Player::ReleaseFlashlight( void )
{
	if( m_pFlashlightBeam )
	{
		m_pFlashlightBeam->flags = 0;
		m_pFlashlightBeam->die = gpGlobals->curtime - 1;

		m_pFlashlightBeam = NULL;
	}
}

float C_HL2MP_Player::GetFOV( void )
{
	//Find our FOV with offset zoom value
	float flFOVOffset = C_BasePlayer::GetFOV() + GetZoom();

	// Clamp FOV in MP
	int min_fov = GetMinFOV();
	
	// Don't let it go too low
	flFOVOffset = MAX( min_fov, flFOVOffset );

	return flFOVOffset;
}

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector C_HL2MP_Player::GetAutoaimVector( float flDelta )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( EyeAngles() + m_Local.m_vecPunchAngle, &forward );
	return	forward;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we are allowed to sprint now.
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::CanSprint( void )
{
	return ( (!m_Local.m_bDucked && !m_Local.m_bDucking) && (GetWaterLevel() != 3) );
}

extern ConVar sv_maxspeed;

void C_HL2MP_Player::HandleSpeedChanges( CMoveData *mv )
{
	int nChangedButtons = mv->m_nButtons ^ mv->m_nOldButtons;

	bool bJustPressedSpeed = !!( nChangedButtons & IN_SPEED );

	const bool bWantSprint = ( CanSprint() && IsSuitEquipped() && ( mv->m_nButtons & IN_SPEED ) );
	const bool bWantsToChangeSprinting = ( m_HL2Local.m_bNewSprinting != bWantSprint ) && ( nChangedButtons & IN_SPEED ) != 0;

	bool bSprinting = m_HL2Local.m_bNewSprinting;
	if ( bWantsToChangeSprinting )
	{
		if ( bWantSprint )
		{
			if ( m_HL2Local.m_flSuitPower < 10.0f )
			{
				if ( bJustPressedSpeed )
				{
					CPASAttenuationFilter filter( this );
					filter.UsePredictionRules();
					EmitSound( filter, entindex(), "HL2Player.SprintNoPower" );
				}
			}
			else
			{
				bSprinting = true;
			}
		}
		else
		{
			bSprinting = false;
		}
	}

	if ( m_HL2Local.m_flSuitPower < 0.01 )
	{
		bSprinting = false;
	}

	bool bWantWalking;

	if ( IsSuitEquipped() )
	{
		bWantWalking = ( mv->m_nButtons & IN_WALK ) && !bSprinting && !( mv->m_nButtons & IN_DUCK );
	}
	else
	{
		bWantWalking = true;
	}

	if ( bWantWalking )
	{
		bSprinting = false;
	}

	m_HL2Local.m_bNewSprinting = bSprinting;

	if ( bSprinting )
	{
		if ( bJustPressedSpeed )
		{
			CPASAttenuationFilter filter( this );
			filter.UsePredictionRules();
			EmitSound( filter, entindex(), "HL2Player.SprintStart" );
		}
		mv->m_flClientMaxSpeed = HL2_SPRINT_SPEED;
	}
	else if ( bWantWalking )
	{
		mv->m_flClientMaxSpeed = HL2_WALK_SPEED;
	}
	else
	{
		mv->m_flClientMaxSpeed = HL2_NORM_SPEED;
	}

	mv->m_flMaxSpeed = sv_maxspeed.GetFloat();
}

void C_HL2MP_Player::ReduceTimers( CMoveData* mv )
{
	bool bSprinting = mv->m_flClientMaxSpeed == HL2_SPRINT_SPEED;

	if ( bSprinting )
	{
		SuitPower_AddDevice( SuitDeviceSprint );
	}
	else
	{
		SuitPower_RemoveDevice( SuitDeviceSprint );
	}

	SuitPower_Update();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_HL2MP_Player::StartWalking( void )
{
	SetMaxSpeed( HL2_WALK_SPEED );
	m_fIsWalking = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_HL2MP_Player::StopWalking( void )
{
	SetMaxSpeed( HL2_NORM_SPEED );
	m_fIsWalking = false;
}

void C_HL2MP_Player::ItemPreFrame( void )
{
	if ( GetFlags() & FL_FROZEN )
		 return;

	// Disallow shooting while zooming
	if ( m_nButtons & IN_ZOOM )
	{
		//FIXME: Held weapons like the grenade get sad when this happens
		m_nButtons &= ~(IN_ATTACK|IN_ATTACK2);
	}

	BaseClass::ItemPreFrame();

}
	
void C_HL2MP_Player::ItemPostFrame( void )
{
	if ( GetFlags() & FL_FROZEN )
		 return;

	BaseClass::ItemPostFrame();
}

C_BaseAnimating *C_HL2MP_Player::BecomeRagdollOnClient()
{
	// Let the C_CSRagdoll entity do this.
	// m_builtRagdoll = true;
	return NULL;
}

void C_HL2MP_Player::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{
	if ( IsInKart() && IsAlive() && !IsObserver() )
	{
		BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );
		CalcKartView( eyeOrigin, eyeAngles, fov );
		return;
	}

	m_bKartCamActive = false;
	m_bKartCamTooClose = false;

	if ( m_lifeState != LIFE_ALIVE && !IsObserver() )
	{
		Vector origin = EyePosition();			

		IRagdoll *pRagdoll = GetRepresentativeRagdoll();

		if ( pRagdoll )
		{
			origin = pRagdoll->GetRagdollOrigin();
			origin.z += VEC_DEAD_VIEWHEIGHT_SCALED( this ).z; // look over ragdoll, not through
		}

		BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );

		eyeOrigin = origin;
		
		Vector vForward; 
		AngleVectors( eyeAngles, &vForward );

		VectorNormalize( vForward );
		VectorMA( origin, -CHASE_CAM_DISTANCE_MAX, vForward, eyeOrigin );

		Vector WALL_MIN( -WALL_OFFSET, -WALL_OFFSET, -WALL_OFFSET );
		Vector WALL_MAX( WALL_OFFSET, WALL_OFFSET, WALL_OFFSET );

		trace_t trace; // clip against world
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();

		if (trace.fraction < 1.0)
		{
			eyeOrigin = trace.endpos;
		}
		
		return;
	}

	BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );
}

//-----------------------------------------------------------------------------
// Purpose: Chase camera behind and above the kart. It looks at a point above the
//			kart's origin (smoothed like the first-person eye, so prediction
//			corrections don't jerk it) from kart_cam_dist behind the kart's
//			heading. The camera heading lags the predicted kart heading in turns
//			(the TF2 halloween kart formula: the bigger the gap, the faster it
//			closes) and +lookback flips it with no lag while held. A hull trace
//			toward the camera keeps it out of walls: it slides in toward the kart.
//-----------------------------------------------------------------------------
void C_HL2MP_Player::CalcKartView( Vector &eyeOrigin, QAngle &eyeAngles, float &fov )
{
	Vector vecSmooth;
	GetPredictionErrorSmoothingVector( vecSmooth );
	Vector vecTarget = GetAbsOrigin() + Vector( 0.0f, 0.0f, kart_cam_target_height.GetFloat() ) + vecSmooth;

	if ( !m_bKartCamActive )
	{
		m_flKartCamYaw = m_flKartYaw;
		m_flKartCamBoost = 0.0f;
		m_bKartCamActive = true;
	}
	else
	{
		float flDelta = AngleDiff( m_flKartYaw, m_flKartCamYaw );
		float flGap = MAX( 2.0f, fabs( flDelta ) );
		float flSpeed = gpGlobals->frametime * flGap * flGap * kart_cam_follow.GetFloat();
		m_flKartCamYaw = AngleNormalize( m_flKartCamYaw + Approach( flDelta, 0.0f, flSpeed ) );
	}

	// The lagged heading keeps tracking while looking back, so letting go snaps
	// straight back to where the camera would be.
	float flCamYaw = m_flKartCamYaw;
	if ( input->GetButtonBits( 0 ) & IN_LOOKBACK )
	{
		flCamYaw = AngleNormalize( m_flKartYaw + 180.0f );
	}

	// Boost: ease the FOV kick and the pull-back in and out over kart_boost_cam_blend.
	float flBoostTarget = IsKartBoosting() ? 1.0f : 0.0f;
	float flBlendTime = kart_boost_cam_blend.GetFloat();
	m_flKartCamBoost = flBlendTime > 0.0f ? Approach( flBoostTarget, m_flKartCamBoost, gpGlobals->frametime / flBlendTime ) : flBoostTarget;
	float flBoost = SimpleSpline( m_flKartCamBoost );

	Vector vecForward;
	AngleVectors( QAngle( 0.0f, flCamYaw, 0.0f ), &vecForward );
	float flCamDist = kart_cam_dist.GetFloat() + flBoost * kart_boost_cam_pullback.GetFloat();
	Vector vecCamera = vecTarget - vecForward * flCamDist + Vector( 0.0f, 0.0f, kart_cam_height.GetFloat() );

	Vector WALL_MIN( -WALL_OFFSET, -WALL_OFFSET, -WALL_OFFSET );
	Vector WALL_MAX( WALL_OFFSET, WALL_OFFSET, WALL_OFFSET );

	trace_t trace; // clip against world
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( vecTarget, vecCamera, WALL_MIN, WALL_MAX, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

	eyeOrigin = trace.endpos;

	Vector vecLook = vecTarget - eyeOrigin;
	m_bKartCamTooClose = vecLook.Length() < kart_cam_min_dist.GetFloat();
	if ( vecLook.IsZero() )
	{
		// Trace started solid: look along the kart's heading.
		vecLook = vecForward;
	}

	VectorAngles( vecLook, eyeAngles );
	eyeAngles[ROLL] = 0.0f;

	fov = kart_cam_fov.GetFloat() + flBoost * kart_boost_fov_kick.GetFloat();
}

IRagdoll* C_HL2MP_Player::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_HL2MPRagdoll *pRagdoll = (C_HL2MPRagdoll*)m_hRagdoll.Get();

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}

//HL2MPRAGDOLL


IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_HL2MPRagdoll, DT_HL2MPRagdoll, CHL2MPRagdoll )
	RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_nModelIndex ) ),
	RecvPropInt( RECVINFO(m_nForceBone) ),
	RecvPropVector( RECVINFO(m_vecForce) ),
	RecvPropVector( RECVINFO( m_vecRagdollVelocity ) )
END_RECV_TABLE()



C_HL2MPRagdoll::C_HL2MPRagdoll()
{

}

C_HL2MPRagdoll::~C_HL2MPRagdoll()
{
	PhysCleanupFrictionSounds( this );

	if ( m_hPlayer )
	{
		m_hPlayer->CreateModelInstance();
	}
}

void C_HL2MPRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;
	
	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();
    	
	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		const char *pszName = pDestEntry->watcher->GetDebugName();
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(), pszName ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

void C_HL2MPRagdoll::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;

	Vector dir = pTrace->endpos - pTrace->startpos;

	if ( iDamageType == DMG_BLAST )
	{
		dir *= 4000;  // adjust impact strenght
				
		// apply force at object mass center
		pPhysicsObject->ApplyForceCenter( dir );
	}
	else
	{
		Vector hitpos;  
	
		VectorMA( pTrace->startpos, pTrace->fraction, dir, hitpos );
		VectorNormalize( dir );

		dir *= 4000;  // adjust impact strenght

		// apply force where we hit it
		pPhysicsObject->ApplyForceOffset( dir, hitpos );	

		// Blood spray!
//		FX_CS_BloodSpray( hitpos, dir, 10 );
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();
}


void C_HL2MPRagdoll::CreateHL2MPRagdoll( void )
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_HL2MP_Player *pPlayer = dynamic_cast< C_HL2MP_Player* >( m_hPlayer.Get() );
	
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// Copy all the interpolated vars from the player entity.
		// The entity uses the interpolated history to get bone velocity.
		bool bRemotePlayer = (pPlayer != C_BasePlayer::GetLocalPlayer());			
		if ( bRemotePlayer )
		{
			Interp_Copy( pPlayer );

			SetAbsAngles( pPlayer->GetRenderAngles() );
			GetRotationInterpolator().Reset();

			m_flAnimTime = pPlayer->m_flAnimTime;
			SetSequence( pPlayer->GetSequence() );
			m_flPlaybackRate = pPlayer->GetPlaybackRate();
		}
		else
		{
			// This is the local player, so set them in a default
			// pose and slam their velocity, angles and origin
			SetAbsOrigin( m_vecRagdollOrigin );
			
			SetAbsAngles( pPlayer->GetRenderAngles() );

			SetAbsVelocity( m_vecRagdollVelocity );

			int iSeq = pPlayer->GetSequence();
			if ( iSeq == -1 )
			{
				Assert( false );	// missing walk_lower?
				iSeq = 0;
			}
			
			SetSequence( iSeq );	// walk_lower, basic pose
			SetCycle( 0.0 );

			Interp_Reset( varMap );
		}		
	}
	else
	{
		// overwrite network origin so later interpolation will
		// use this position
		SetNetworkOrigin( m_vecRagdollOrigin );

		SetAbsOrigin( m_vecRagdollOrigin );
		SetAbsVelocity( m_vecRagdollVelocity );

		Interp_Reset( GetVarMapping() );
		
	}

	SetModelIndex( m_nModelIndex );

	// Make us a ragdoll..
	m_nRenderFX = kRenderFxRagdoll;

	matrix3x4_t boneDelta0[MAXSTUDIOBONES];
	matrix3x4_t boneDelta1[MAXSTUDIOBONES];
	matrix3x4_t currentBones[MAXSTUDIOBONES];
	const float boneDt = 0.05f;

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
	}
	else
	{
		GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
	}

	InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
}


void C_HL2MPRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		CreateHL2MPRagdoll();
	}
}

IRagdoll* C_HL2MPRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

void C_HL2MPRagdoll::UpdateOnRemove( void )
{
	VPhysicsSetObject( NULL );

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: clear out any face/eye values stored in the material system
//-----------------------------------------------------------------------------
void C_HL2MPRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );

	static float destweight[128];
	static bool bIsInited = false;

	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
		return;

	int nFlexDescCount = hdr->numflexdesc();
	if ( nFlexDescCount )
	{
		Assert( !pFlexDelayedWeights );
		memset( pFlexWeights, 0, nFlexWeightCount * sizeof(float) );
	}

	if ( m_iEyeAttachment > 0 )
	{
		matrix3x4_t attToWorld;
		if (GetAttachment( m_iEyeAttachment, attToWorld ))
		{
			Vector local, tmp;
			local.Init( 1000.0f, 0.0f, 0.0f );
			VectorTransform( local, attToWorld, tmp );
			modelrender->SetViewTarget( GetModelPtr(), GetBody(), tmp );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handling. In kart mode the mouse does nothing and most keys
//			mean nothing: the view angle sent with every command is the kart's
//			predicted heading (so the engine's view angles, which in_main.cpp
//			sets from cmd->viewangles right after this, drop the mouse deltas),
//			and only the driving keys survive. forwardmove/sidemove stay as the
//			input system made them; the movement only reads their signs.
//
//			Only a live, walking kart is locked: a dead kart spectates and a
//			noclipping one flies with the normal view, matching
//			CKartGameMovement::ShouldKartMove().
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	bool bResult = BaseClass::CreateMove( flInputSampleTime, pCmd );

	if ( IsInKart() && IsAlive() && GetMoveType() == MOVETYPE_WALK )
	{
		pCmd->buttons &= ( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP | IN_ATTACK | IN_ATTACK2 | IN_SCORE | IN_LOOKBACK );
		pCmd->weaponselect = 0;
		pCmd->impulse = 0;
		pCmd->viewangles.Init( 0.0f, m_flKartYaw, 0.0f );
	}

	return bResult;
}

void C_HL2MP_Player::PostThink( void )
{
	BaseClass::PostThink();

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

	if ( GetFlags() & FL_DUCKING )
	{
		SetCollisionBounds( VEC_CROUCH_TRACE_MIN, VEC_CROUCH_TRACE_MAX );
	}
}