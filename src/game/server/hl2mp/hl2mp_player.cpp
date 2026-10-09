//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL2.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "hl2mp_player.h"
#include "globalstate.h"
#include "game.h"
#include "gamerules.h"
#include "hl2mp_player_shared.h"
#include "predicted_viewmodel.h"
#include "in_buttons.h"
#include "hl2mp_gamerules.h"
#include "kart_shareddefs.h"
#include "kart_race_entities.h"
#include "kart_race_shared.h"
#include "kart_items.h"
#include "KeyValues.h"
#include "team.h"
#include "weapon_hl2mpbase.h"
#include "grenade_satchel.h"
#include "eventqueue.h"
#include "gamestats.h"
#include "ammodef.h"
#include "NextBot.h"

#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

#include "ilagcompensationmanager.h"

int g_iLastCitizenModel = 0;
int g_iLastCombineModel = 0;

CBaseEntity	 *g_pLastCombineSpawn = NULL;
CBaseEntity	 *g_pLastRebelSpawn = NULL;
extern CBaseEntity				*g_pLastSpawn;

ConVar hl2mp_spawn_frag_fallback_radius( "hl2mp_spawn_frag_fallback_radius", "48", FCVAR_NONE, "If no spawns are available, kill players with this radius to allow new players to spawn." );

extern ConVar kart_debug_server;

#define HL2MP_COMMAND_MAX_RATE 0.3

ConVar kart_respawn_freeze( "kart_respawn_freeze", "0.75", FCVAR_NOTIFY, "Seconds a kart is held still after being put back on the track at its last checkpoint.", true, 0, true, 5 );
ConVar kart_respawn_cooldown( "kart_respawn_cooldown", "3", FCVAR_NOTIFY, "Seconds between two uses of the kart_respawn command by one player.", true, 0, false, 0 );

void DropPrimedFragGrenade( CHL2MP_Player *pPlayer, CBaseCombatWeapon *pGrenade );

LINK_ENTITY_TO_CLASS( player, CHL2MP_Player );

LINK_ENTITY_TO_CLASS( info_player_combine, CPointEntity );
LINK_ENTITY_TO_CLASS( info_player_rebel, CPointEntity );

// specific to the local player
BEGIN_SEND_TABLE_NOBASE( CHL2MP_Player, DT_HL2MPLocalPlayerExclusive )
	// send a hi-res origin to the local player for use in prediction
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),

	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),

	// full-precision kart state for the local player's prediction
	SendPropFloat( SENDINFO( m_flKartSpeed ), -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN ),
	SendPropFloat( SENDINFO( m_flKartYaw ), -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN ),
	SendPropFloat( SENDINFO( m_flKartReverseTime ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flKartBumpCooldown ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flKartSlipAngle ), -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN ),
	SendPropFloat( SENDINFO( m_flKartDriftTime ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flKartHopTime ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flKartDriftCharge ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flKartBoostScale ), -1, SPROP_NOSCALE ),

END_SEND_TABLE()

// all players except the local player
BEGIN_SEND_TABLE_NOBASE( CHL2MP_Player, DT_HL2MPNonLocalPlayerExclusive )
	// send a lo-res origin to other players
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_COORD_MP_LOWPRECISION|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_COORD_MP_LOWPRECISION|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),

	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),

	// lo-res kart state for other players: enough to draw their kart
	SendPropFloat( SENDINFO( m_flKartSpeed ), 12, SPROP_CHANGES_OFTEN, KART_NET_SPEED_MIN, KART_NET_SPEED_MAX ),
	SendPropAngle( SENDINFO( m_flKartYaw ), 13, SPROP_CHANGES_OFTEN ),

END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST(CHL2MP_Player, DT_HL2MP_Player)
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),

	// misyl:
	// m_flMaxspeed is fully predicted by the client and the client's
	// maxspeed is sent in the user message.
	// Other games like DOD, etc don't use this var at all and just fully
	// predict in GameMovement, but the HL2 codebase doesn't do that and modifies this
	// on the player.
	// So, just never send it, and don't predict it on the client either.
	SendPropExclude( "DT_BasePlayer", "m_flMaxspeed" ),


	// Data that only gets sent to the local player
	SendPropDataTable( "hl2mplocaldata", 0, &REFERENCE_SEND_TABLE( DT_HL2MPLocalPlayerExclusive ), SendProxy_SendLocalDataTable ),

	// Data that gets sent to all other players
	SendPropDataTable( "hl2mpnonlocaldata", 0, &REFERENCE_SEND_TABLE( DT_HL2MPNonLocalPlayerExclusive ), SendProxy_SendNonLocalDataTable ),

	SendPropEHandle( SENDINFO( m_hRagdoll ) ),
	SendPropInt( SENDINFO( m_iSpawnInterpCounter), 4 ),
	SendPropInt( SENDINFO( m_iPlayerSoundType), 3 ),
	SendPropBool( SENDINFO( m_bKartMode ) ),
	SendPropInt( SENDINFO( m_nKartDriftDir ), 2 ),	// signed: -1, 0, 1. Everyone gets it, for drift effects on other karts.
	SendPropInt( SENDINFO( m_nKartSteer ), 2 ),	// signed: -1, 0, 1. Everyone gets it, to turn the wheels of other karts.
	SendPropModelIndex( SENDINFO( m_nKartDriverModel ) ),
	SendPropInt( SENDINFO( m_nKartDriftTier ), 2, SPROP_UNSIGNED ),	// 0-3, for mini-turbo spark effects on every kart
	SendPropFloat( SENDINFO( m_flKartBoostEndTime ), -1, SPROP_NOSCALE ),	// full precision: the local player predicts it

	// kart race state, for everyone's HUD and the bots
	SendPropInt( SENDINFO( m_nKartLap ), KART_NET_LAP_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nKartNextCheckpoint ), KART_NET_CHECKPOINT_BITS, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flKartProgress ), KART_NET_PROGRESS_BITS, SPROP_CHANGES_OFTEN, 0.0f, KART_NET_PROGRESS_MAX ),
	SendPropInt( SENDINFO( m_nKartRacePosition ), KART_NET_POSITION_BITS, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bKartFinished ) ),
	SendPropTime( SENDINFO( m_flKartLapStartTime ) ),
	SendPropFloat( SENDINFO( m_flKartBestLap ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flKartTotalTime ), -1, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bKartLateJoin ) ),
	SendPropBool( SENDINFO( m_bKartWrongWay ) ),

	// kart item, for everyone's HUD and the bots
	SendPropInt( SENDINFO( m_nKartItem ), KART_NET_ITEM_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nKartItemCount ), KART_NET_ITEM_COUNT_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nKartRouletteItem ), KART_NET_ITEM_BITS, SPROP_UNSIGNED ),
	SendPropTime( SENDINFO( m_flKartRouletteEnd ) ),
	
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseFlex", "m_viewtarget" ),

//	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
//	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),	
END_SEND_TABLE()

BEGIN_DATADESC( CHL2MP_Player )
END_DATADESC()

BEGIN_ENT_SCRIPTDESC( CHL2MP_Player, CHL2_Player, "Half-Life 2: Deathmatch Player" )
END_SCRIPTDESC();

const char *g_ppszRandomCitizenModels[] = 
{
	"models/humans/group03/male_01.mdl",
	"models/humans/group03/male_02.mdl",
	"models/humans/group03/female_01.mdl",
	"models/humans/group03/male_03.mdl",
	"models/humans/group03/female_02.mdl",
	"models/humans/group03/male_04.mdl",
	"models/humans/group03/female_03.mdl",
	"models/humans/group03/male_05.mdl",
	"models/humans/group03/female_04.mdl",
	"models/humans/group03/male_06.mdl",
	"models/humans/group03/female_06.mdl",
	"models/humans/group03/male_07.mdl",
	"models/humans/group03/female_07.mdl",
	"models/humans/group03/male_08.mdl",
	"models/humans/group03/male_09.mdl",
};

const char *g_ppszRandomCombineModels[] =
{
	"models/combine_soldier.mdl",
	"models/combine_soldier_prisonguard.mdl",
	"models/combine_super_soldier.mdl",
	"models/police.mdl",
};


#define MAX_COMBINE_MODELS 4
#define MODEL_CHANGE_INTERVAL 5.0f
#define TEAM_CHANGE_INTERVAL 5.0f

#define HL2MPPLAYER_PHYSDAMAGE_SCALE 4.0f

#pragma warning( disable : 4355 )

CHL2MP_Player::CHL2MP_Player() : m_PlayerAnimState( this )
{
	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	m_iSpawnInterpCounter = 0;

	m_bKartMode = false;
	m_flKartSpeed = 0.0f;
	m_flKartYaw = 0.0f;
	m_flKartReverseTime = 0.0f;
	m_nKartDriverModel = -1;
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
	m_flKartTopSpeedScale = 1.0f;
	m_flKartRespawnUnfreezeTime = 0.0f;
	m_flKartNextRespawnCommand = 0.0f;

	ResetKartRaceState();

	m_nKartItem = KART_ITEM_NONE;
	m_nKartItemCount = 0;
	m_nKartRouletteItem = KART_ITEM_NONE;
	m_flKartRouletteEnd = 0.0f;
	m_flKartRouletteNextStep = 0.0f;

    m_bEnterObserver = false;
	m_bReady = false;

	BaseClass::ChangeTeam( 0 );
	
	//UseClientSideAnimation();
}

CHL2MP_Player::~CHL2MP_Player( void )
{

}

void CHL2MP_Player::UpdateOnRemove( void )
{
	if ( m_hRagdoll )
	{
		UTIL_RemoveImmediate( m_hRagdoll );
		m_hRagdoll = NULL;
	}

	BaseClass::UpdateOnRemove();
}

void CHL2MP_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel ( "sprites/glow01.vmt" );

	//Precache Citizen models
	int nHeads = ARRAYSIZE( g_ppszRandomCitizenModels );
	int i;	

	for ( i = 0; i < nHeads; ++i )
	   	 PrecacheModel( g_ppszRandomCitizenModels[i] );

	//Precache Combine Models
	nHeads = ARRAYSIZE( g_ppszRandomCombineModels );

	for ( i = 0; i < nHeads; ++i )
	   	 PrecacheModel( g_ppszRandomCombineModels[i] );

	PrecacheModel( KART_DEFAULT_MODEL );
	PrecacheModel( KART_SCRAP_MODEL );
	PrecacheModel( KART_DRIVER_ANIMS );
	PrecacheModel( KART_PLACEHOLDER_MODEL );
	if ( kart_model.GetString()[0] )
	{
		PrecacheModel( kart_model.GetString() );
	}

	PrecacheFootStepSounds();

	PrecacheScriptSound( "NPC_MetroPolice.Die" );
	PrecacheScriptSound( "NPC_CombineS.Die" );
	PrecacheScriptSound( "NPC_Citizen.die" );

	PrecacheScriptSound( "Kart.EngineIdle" );
	PrecacheScriptSound( "Kart.EngineRev" );
	PrecacheScriptSound( "Kart.Skid" );
	PrecacheScriptSound( "Kart.Impact" );
	PrecacheScriptSound( "Kart.RouletteTick" );
	PrecacheScriptSound( "Kart.Nitro" );
	PrecacheScriptSound( KART_SOUND_RESPAWN );
}

void CHL2MP_Player::GiveAllItems( void )
{
	EquipSuit();

	CBasePlayer::GiveAmmo( 255,	"Pistol");
	CBasePlayer::GiveAmmo( 255,	"AR2" );
	CBasePlayer::GiveAmmo( 5,	"AR2AltFire" );
	CBasePlayer::GiveAmmo( 255,	"SMG1");
	CBasePlayer::GiveAmmo( 1,	"smg1_grenade");
	CBasePlayer::GiveAmmo( 255,	"Buckshot");
	CBasePlayer::GiveAmmo( 32,	"357" );
	CBasePlayer::GiveAmmo( 3,	"rpg_round");
	CBasePlayer::GiveAmmo( 16,	"XBowBolt");

	CBasePlayer::GiveAmmo( 1,	"grenade" );
	CBasePlayer::GiveAmmo( 2,	"slam" );

	GiveNamedItem( "weapon_crowbar" );
	GiveNamedItem( "weapon_stunstick" );
	GiveNamedItem( "weapon_pistol" );
	GiveNamedItem( "weapon_357" );

	GiveNamedItem( "weapon_smg1" );
	GiveNamedItem( "weapon_ar2" );
	
	GiveNamedItem( "weapon_shotgun" );
	GiveNamedItem( "weapon_frag" );
	
	GiveNamedItem( "weapon_crossbow" );
	
	GiveNamedItem( "weapon_rpg" );

	GiveNamedItem( "weapon_slam" );

	GiveNamedItem( "weapon_physcannon" );
	
}

void CHL2MP_Player::GiveDefaultItems( void )
{
	EquipSuit();

	CBasePlayer::GiveAmmo( 255,	"Pistol");
	CBasePlayer::GiveAmmo( 45,	"SMG1");
	CBasePlayer::GiveAmmo( 1,	"grenade" );
	CBasePlayer::GiveAmmo( 6,	"Buckshot");
	CBasePlayer::GiveAmmo( 6,	"357" );

	if ( GetPlayerModelType() == PLAYER_SOUNDS_METROPOLICE || GetPlayerModelType() == PLAYER_SOUNDS_COMBINESOLDIER )
	{
		GiveNamedItem( "weapon_stunstick" );
	}
	else if ( GetPlayerModelType() == PLAYER_SOUNDS_CITIZEN )
	{
		GiveNamedItem( "weapon_crowbar" );
	}
	
	GiveNamedItem( "weapon_pistol" );
	GiveNamedItem( "weapon_smg1" );
	GiveNamedItem( "weapon_frag" );
	GiveNamedItem( "weapon_physcannon" );

	const char *szDefaultWeaponName = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_defaultweapon" );

	CBaseCombatWeapon *pDefaultWeapon = Weapon_OwnsThisType( szDefaultWeaponName );

	if ( pDefaultWeapon )
	{
		Weapon_Switch( pDefaultWeapon );
	}
	else
	{
		Weapon_Switch( Weapon_OwnsThisType( "weapon_physcannon" ) );
	}
}

void CHL2MP_Player::PickDefaultSpawnTeam( void )
{
	if ( GetTeamNumber() == 0 )
	{
		if ( HL2MPRules()->IsTeamplay() == false )
		{
			if ( GetModelPtr() == NULL )
			{
				const char *szModelName = NULL;
				szModelName = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_playermodel" );

				if ( ValidatePlayerModel( szModelName ) == false )
				{
					char szReturnString[512];

					Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel models/combine_soldier.mdl\n" );
					engine->ClientCommand ( edict(), szReturnString );
				}

				ChangeTeam( TEAM_UNASSIGNED );
			}
		}
		else
		{
			CTeam *pCombine = g_Teams[TEAM_COMBINE];
			CTeam *pRebels = g_Teams[TEAM_REBELS];

			if ( pCombine == NULL || pRebels == NULL )
			{
				ChangeTeam( random->RandomInt( TEAM_COMBINE, TEAM_REBELS ) );
			}
			else
			{
				if ( pCombine->GetNumPlayers() > pRebels->GetNumPlayers() )
				{
					ChangeTeam( TEAM_REBELS );
				}
				else if ( pCombine->GetNumPlayers() < pRebels->GetNumPlayers() )
				{
					ChangeTeam( TEAM_COMBINE );
				}
				else
				{
					ChangeTeam( random->RandomInt( TEAM_COMBINE, TEAM_REBELS ) );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets HL2 specific defaults.
//-----------------------------------------------------------------------------
void CHL2MP_Player::Spawn(void)
{
	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	// Latch kart mode for this life, before the team (and so the model) is picked.
	m_bKartMode = kart_enabled.GetBool();

	PickDefaultSpawnTeam();

	// A kart spectator back on the track (the next race's grid respawn).
	if ( m_iPlayerState == STATE_OBSERVER_MODE && GetTeamNumber() != TEAM_SPECTATOR )
	{
		State_Transition( STATE_ACTIVE );
	}

	BaseClass::Spawn();

	// A respawn keeps its team and so its model: re-apply the model whenever it
	// doesn't match the mode we just latched (kart_enabled was toggled, then kill).
	const char *pszModel = modelinfo->GetModelName( GetModel() );
	bool bHasKartModel = pszModel && !Q_stricmp( pszModel, GetKartModelName() );
	if ( IsInKart() != bHasKartModel )
	{
		if ( HL2MPRules()->IsTeamplay() )
		{
			SetPlayerTeamModel();
		}
		else
		{
			SetPlayerModel();
		}
	}
	
	if ( !IsObserver() )
	{
		pl.deadflag = false;
		RemoveSolidFlags( FSOLID_NOT_SOLID );

		RemoveEffects( EF_NODRAW );
		
		if ( !IsInKart() )
		{
			GiveDefaultItems();
		}
	}

	SetNumAnimOverlays( 3 );
	ResetAnimation();

	m_nRenderFX = kRenderNormal;

	m_Local.m_iHideHUD = 0;

	if ( IsInKart() )
	{
		// The kart is the player: no weapons, no suit, no damage yet (M1), and it is
		// drawn for the local player too so third person shows the kart.
		ResetKartMovement( GetAbsAngles()[YAW] );	// spawn point facing
		m_Local.m_bForceLocalPlayerDraw = true;
		m_takedamage = DAMAGE_NO;
		m_Local.m_iHideHUD |= KART_HIDEHUD_BITS;
		SetCollisionBounds( VEC_HULL_MIN, VEC_HULL_MAX );
		SetViewOffset( VEC_VIEW );
	}
	else
	{
		m_Local.m_bForceLocalPlayerDraw = false;
	}
	
	AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

	m_flKartRespawnUnfreezeTime = 0.0f;

	m_impactEnergyScale = HL2MPPLAYER_PHYSDAMAGE_SCALE;

	// Karts are also held on the grid during the countdown and at the results.
	if ( HL2MPRules()->IsIntermission() || ( IsInKart() && HL2MPRules()->IsKartRaceFrozen() ) )
	{
		AddFlag( FL_FROZEN );
	}
	else
	{
		RemoveFlag( FL_FROZEN );
	}

	if ( IsInKart() && !IsObserver() )
	{
		HL2MPRules()->OnKartSpawned( this );
	}

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);

	m_bReady = false;
}

bool CHL2MP_Player::ValidatePlayerModel( const char *pModel )
{
	int iModels = ARRAYSIZE( g_ppszRandomCitizenModels );
	int i;	

	for ( i = 0; i < iModels; ++i )
	{
		if ( !Q_stricmp( g_ppszRandomCitizenModels[i], pModel ) )
		{
			return true;
		}
	}

	iModels = ARRAYSIZE( g_ppszRandomCombineModels );

	for ( i = 0; i < iModels; ++i )
	{
	   	if ( !Q_stricmp( g_ppszRandomCombineModels[i], pModel ) )
		{
			return true;
		}
	}

	return false;
}

ConVar hl2mp_allow_pickup( "hl2mp_allow_pickup", "0", FCVAR_GAMEDLL );

void CHL2MP_Player::PickupObject( CBaseEntity* pObject, bool bLimitMassAndSize )
{
	if ( !hl2mp_allow_pickup.GetBool() )
		return;

	return BaseClass::PickupObject( pObject, bLimitMassAndSize );
}

//-----------------------------------------------------------------------------
// Purpose: +use does nothing from a kart: no object pickup, no buttons.
//-----------------------------------------------------------------------------
void CHL2MP_Player::PlayerUse( void )
{
	if ( IsInKart() )
		return;

	BaseClass::PlayerUse();
}

void CHL2MP_Player::SetPlayerTeamModel( void )
{
	if ( IsInKart() )
	{
		SetKartModel();
		return;
	}

	const char *szModelName = NULL;
	szModelName = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_playermodel" );

	int modelIndex = modelinfo->GetModelIndex( szModelName );

	if ( modelIndex == -1 || ValidatePlayerModel( szModelName ) == false )
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", szModelName );
		engine->ClientCommand ( edict(), szReturnString );
	}

	if ( GetTeamNumber() == TEAM_COMBINE )
	{
		if ( Q_stristr( szModelName, "models/human") )
		{
			int nHeads = ARRAYSIZE( g_ppszRandomCombineModels );
		
			g_iLastCombineModel = ( g_iLastCombineModel + 1 ) % nHeads;
			szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];
		}

		m_iModelType = TEAM_COMBINE;
	}
	else if ( GetTeamNumber() == TEAM_REBELS )
	{
		if ( !Q_stristr( szModelName, "models/human") )
		{
			int nHeads = ARRAYSIZE( g_ppszRandomCitizenModels );

			g_iLastCitizenModel = ( g_iLastCitizenModel + 1 ) % nHeads;
			szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];
		}

		m_iModelType = TEAM_REBELS;
	}
	
	SetModel( szModelName );
	SetupPlayerSoundsByModel( szModelName );

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetPlayerModel( void )
{
	if ( IsInKart() )
	{
		SetKartModel();
		return;
	}

	const char *szModelName = NULL;
	const char *pszCurrentModelName = modelinfo->GetModelName( GetModel());

	szModelName = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_playermodel" );

	if ( ValidatePlayerModel( szModelName ) == false )
	{
		char szReturnString[512];

		if ( ValidatePlayerModel( pszCurrentModelName ) == false )
		{
			pszCurrentModelName = "models/Combine_Soldier.mdl";
		}

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", pszCurrentModelName );
		engine->ClientCommand ( edict(), szReturnString );

		szModelName = pszCurrentModelName;
	}

	if ( GetTeamNumber() == TEAM_COMBINE )
	{
		int nHeads = ARRAYSIZE( g_ppszRandomCombineModels );
		
		g_iLastCombineModel = ( g_iLastCombineModel + 1 ) % nHeads;
		szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];

		m_iModelType = TEAM_COMBINE;
	}
	else if ( GetTeamNumber() == TEAM_REBELS )
	{
		int nHeads = ARRAYSIZE( g_ppszRandomCitizenModels );

		g_iLastCitizenModel = ( g_iLastCitizenModel + 1 ) % nHeads;
		szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];

		m_iModelType = TEAM_REBELS;
	}
	else
	{
		if ( Q_strlen( szModelName ) == 0 ) 
		{
			szModelName = g_ppszRandomCitizenModels[0];
		}

		if ( Q_stristr( szModelName, "models/human") )
		{
			m_iModelType = TEAM_REBELS;
		}
		else
		{
			m_iModelType = TEAM_COMBINE;
		}
	}

	int modelIndex = modelinfo->GetModelIndex( szModelName );

	if ( modelIndex == -1 )
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", szModelName );
		engine->ClientCommand ( edict(), szReturnString );
	}

	SetModel( szModelName );
	SetupPlayerSoundsByModel( szModelName );

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetupPlayerSoundsByModel( const char *pModelName )
{
	if ( Q_stristr( pModelName, "models/human") )
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_CITIZEN;
	}
	else if ( Q_stristr(pModelName, "police" ) )
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_METROPOLICE;
	}
	else if ( Q_stristr(pModelName, "combine" ) )
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_COMBINESOLDIER;
	}
}

//-----------------------------------------------------------------------------
// Purpose: The model kart_model names, or KART_DEFAULT_MODEL when that one wasn't
//			precached at map start (a model can't be precached later).
//-----------------------------------------------------------------------------
const char *CHL2MP_Player::GetKartModelName( void )
{
	const char *pszModel = kart_model.GetString();
	if ( pszModel[0] && modelinfo->GetModelIndex( pszModel ) >= 0 )
		return pszModel;

	static char s_szWarned[MAX_PATH];
	if ( Q_stricmp( s_szWarned, pszModel ) )
	{
		Q_strncpy( s_szWarned, pszModel, sizeof( s_szWarned ) );
		Warning( "kart_model \"%s\" is not precached (set it before the map loads); using %s\n", pszModel, KART_DEFAULT_MODEL );
	}
	return KART_DEFAULT_MODEL;
}

//-----------------------------------------------------------------------------
// Purpose: Kart mode replacement for SetPlayerModel()/SetPlayerTeamModel():
//			the kart_model with the kart hull and no animation.
//-----------------------------------------------------------------------------
void CHL2MP_Player::SetKartModel( void )
{
	SetModel( GetKartModelName() );
	m_iPlayerSoundType = (int)PLAYER_SOUNDS_CITIZEN;
	SetCollisionBounds( KART_HULL_MIN, KART_HULL_MAX );
	ResetSequence( 0 );
	ApplyKartColor();
	ApplyKartDriverModel();

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

//-----------------------------------------------------------------------------
// Purpose: A kart at rest heading flYaw: no speed, drift, slip or hop.
//-----------------------------------------------------------------------------
void CHL2MP_Player::ResetKartMovement( float flYaw )
{
	m_flKartSpeed = 0.0f;
	m_flKartYaw = flYaw;
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
	m_bKartWrongWay = false;
	m_flKartWrongWayTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Puts the kart at vecOrigin, stopped and heading flYaw.
//-----------------------------------------------------------------------------
void CHL2MP_Player::KartTeleport( const Vector &vecOrigin, float flYaw )
{
	QAngle angles( 0.0f, flYaw, 0.0f );
	Teleport( &vecOrigin, &angles, &vec3_origin );
	SnapEyeAngles( angles );
	ResetKartMovement( flYaw );
}

//-----------------------------------------------------------------------------
// Purpose: The driver seated in the kart: the player's cl_playermodel (set in
//			Options > Multiplayer), or the default citizen when it is unset or
//			not a player model. Bots have no userinfo and get a citizen by
//			entity index.
//-----------------------------------------------------------------------------
void CHL2MP_Player::ApplyKartDriverModel( void )
{
	const char *pszModel = g_ppszRandomCitizenModels[0];
	if ( IsFakeClient() )
	{
		pszModel = g_ppszRandomCitizenModels[entindex() % ARRAYSIZE( g_ppszRandomCitizenModels )];
	}
	else
	{
		const char *pszPlayerModel = engine->GetClientConVarValue( entindex(), "cl_playermodel" );
		if ( ValidatePlayerModel( pszPlayerModel ) && modelinfo->GetModelIndex( pszPlayerModel ) >= 0 )
		{
			pszModel = pszPlayerModel;
		}
	}

	m_nKartDriverModel = modelinfo->GetModelIndex( pszModel );
}

//-----------------------------------------------------------------------------
// Purpose: Tint the kart with the player's cl_kart_color. Bots have no userinfo,
//			so they take a palette entry from their entity index.
//-----------------------------------------------------------------------------
void CHL2MP_Player::ApplyKartColor( void )
{
	int iColor = entindex();
	if ( !IsFakeClient() )
		iColor = atoi( engine->GetClientConVarValue( entindex(), "cl_kart_color" ) );

	iColor = abs( iColor ) % KART_COLOR_COUNT;
	const color32 &c = g_KartColors[iColor];
	SetRenderColor( c.r, c.g, c.b );
}

//-----------------------------------------------------------------------------
// Purpose: Back to the start of the race: lap 0, waiting for the line, and in
//			the next race. Kept across respawns; the race manager, the race
//			flow and kart_race_reset call this.
//-----------------------------------------------------------------------------
void CHL2MP_Player::ResetKartRaceState( void )
{
	m_nKartLap = 0;
	m_nKartNextCheckpoint = KART_FINISH_INDEX;
	m_flKartProgress = 0.0f;
	m_nKartRacePosition = 0;
	m_bKartFinished = false;
	m_flKartLapStartTime = 0.0f;
	m_flKartBestLap = 0.0f;
	m_flKartTotalTime = 0.0f;
	m_flKartFinishTime = 0.0f;
	m_bKartLateJoin = false;
	m_bKartInRace = false;
	m_bKartDNF = false;
	m_bKartWrongWay = false;
	m_flKartWrongWayTime = 0.0f;

	// A new race starts empty-handed.
	KartClearItem();
}

//-----------------------------------------------------------------------------
// Purpose: Back on the track at the last checkpoint hit (see
//			CKartRaceManager::GetRespawnPoint), or on a spawn point on a map
//			without a route. The race state is left alone, so a respawn loses
//			no lap or checkpoint, only the time it takes.
//-----------------------------------------------------------------------------
bool CHL2MP_Player::KartRespawnAtCheckpoint( void )
{
	if ( !IsInKart() || !IsAlive() || IsObserver() )
		return false;

	Vector vecOrigin;
	QAngle angFacing;
	if ( !KartRaceManager() || !KartRaceManager()->GetRespawnPoint( this, vecOrigin, angFacing ) )
	{
		CBaseEntity *pSpot = EntSelectSpawnPoint();
		if ( !pSpot )
			return false;

		vecOrigin = pSpot->GetAbsOrigin() + Vector( 0, 0, 1 );
		angFacing = QAngle( 0, pSpot->GetAbsAngles()[YAW], 0 );
	}

	Teleport( &vecOrigin, &angFacing, &vec3_origin );
	SnapEyeAngles( angFacing );
	SetGroundEntity( NULL );

	// Stopped, out of any drift or hop, facing forward.
	ResetKartMovement( angFacing[YAW] );

	// No lerp across the map on the clients.
	m_iSpawnInterpCounter = ( m_iSpawnInterpCounter + 1 ) % 8;

	if ( kart_respawn_freeze.GetFloat() > 0.0f )
	{
		AddFlag( FL_FROZEN );
		m_flKartRespawnUnfreezeTime = gpGlobals->curtime + kart_respawn_freeze.GetFloat();
	}

	color32 black = { 0, 0, 0, 255 };
	UTIL_ScreenFade( this, black, 0.5f, 0.1f, FFADE_IN | FFADE_PURGE );

	EmitSound( KART_SOUND_RESPAWN );

	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] %s respawned at %.0f %.0f %.0f facing %.0f (lap %d, next checkpoint %d)\n", GetPlayerName(),
			vecOrigin.x, vecOrigin.y, vecOrigin.z, angFacing[YAW], m_nKartLap.Get(), m_nKartNextCheckpoint.Get() );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Gives the kart an item with nCount uses. With a roulette time, the
//			roulette spins that long first and the item can't be used until it
//			stops; the item itself is already decided.
//-----------------------------------------------------------------------------
void CHL2MP_Player::KartGiveItem( int item, int nCount, float flRouletteTime )
{
	if ( !KartItem_IsValid( item ) )
	{
		KartClearItem();
		return;
	}

	m_nKartItem = item;
	m_nKartItemCount = clamp( nCount, 1, KART_MAX_ITEM_COUNT );

	if ( flRouletteTime > 0.0f )
	{
		m_flKartRouletteEnd = gpGlobals->curtime + flRouletteTime;
		m_nKartRouletteItem = RandomInt( KART_ITEM_NONE + 1, KART_ITEM_COUNT - 1 );
		m_flKartRouletteNextStep = gpGlobals->curtime + KART_ITEM_ROULETTE_STEP;
	}
	else
	{
		m_flKartRouletteEnd = gpGlobals->curtime;
		m_nKartRouletteItem = item;
	}

	IGameEvent *event = gameeventmanager->CreateEvent( KART_EVENT_ITEM_PICKUP );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "item", item );
		gameeventmanager->FireEvent( event );
	}
}

void CHL2MP_Player::KartClearItem( void )
{
	m_nKartItem = KART_ITEM_NONE;
	m_nKartItemCount = 0;
	m_nKartRouletteItem = KART_ITEM_NONE;
	m_flKartRouletteEnd = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Uses the held item once, backwards if asked and the item can be
//			thrown that way. Nothing while the roulette spins.
//-----------------------------------------------------------------------------
void CHL2MP_Player::KartUseItem( bool bBackward )
{
	int item = m_nKartItem;
	if ( !KartItem_IsValid( item ) || IsKartRouletteSpinning() )
		return;

	if ( !kart_items_enabled.GetBool() )
		return;

	const KartItemInfo_t &info = g_KartItems[item];
	bBackward = bBackward && info.bBackward;

	if ( !info.Use || !info.Use( this, bBackward ) )
		return;

	IGameEvent *event = gameeventmanager->CreateEvent( KART_EVENT_ITEM_USE );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "item", item );
		event->SetBool( "backward", bBackward );
		gameeventmanager->FireEvent( event );
	}

	// The use may have changed the item (KartGiveItem/KartClearItem); only
	// spend it if it is still the one used.
	if ( m_nKartItem != item )
		return;

	if ( m_nKartItemCount > 1 )
	{
		m_nKartItemCount--;
	}
	else
	{
		KartClearItem();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Every command: steps the roulette and reads the use input.
//-----------------------------------------------------------------------------
void CHL2MP_Player::KartItemPostThink( void )
{
	if ( m_nKartItem == KART_ITEM_NONE )
		return;

	if ( IsKartRouletteSpinning() )
	{
		// Cycle through the items, for display.
		if ( gpGlobals->curtime >= m_flKartRouletteNextStep )
		{
			m_flKartRouletteNextStep = gpGlobals->curtime + KART_ITEM_ROULETTE_STEP;
			int next = m_nKartRouletteItem + 1;
			if ( next >= KART_ITEM_COUNT )
			{
				next = KART_ITEM_NONE + 1;
			}
			m_nKartRouletteItem = next;
		}
	}
	else if ( m_nKartRouletteItem != m_nKartItem )
	{
		// Stopped: land on the real item.
		m_nKartRouletteItem = m_nKartItem;

		if ( kart_debug_server.GetBool() )
		{
			Msg( "[kart] %s's roulette landed on %s x%d\n", GetPlayerName(), KartItem_GetName( m_nKartItem ), m_nKartItemCount.Get() );
		}
	}

	// No items while held on the grid or at the results.
	if ( !IsAlive() || ( GetFlags() & FL_FROZEN ) )
		return;

	bool bBackward;
	if ( KartItem_ReadUseInput( m_nButtons, m_afButtonPressed, bBackward ) )
	{
		KartUseItem( bBackward );
	}
}

void CHL2MP_Player::ResetAnimation( void )
{
	if ( IsInKart() )
	{
		// The kart model has no player animations; keep its first sequence.
		ResetSequence( 0 );
		return;
	}

	if ( IsAlive() )
	{
		SetSequence ( -1 );
		SetActivity( ACT_INVALID );

		if (!GetAbsVelocity().x && !GetAbsVelocity().y)
			SetAnimation( PLAYER_IDLE );
		else if ((GetAbsVelocity().x || GetAbsVelocity().y) && ( GetFlags() & FL_ONGROUND ))
			SetAnimation( PLAYER_WALK );
		else if (GetWaterLevel() > 1)
			SetAnimation( PLAYER_WALK );
		else if ( ( GetFlags() & FL_ONGROUND ) != FL_ONGROUND)
			SetAnimation( PLAYER_JUMP );
		else
			SetAnimation( PLAYER_IDLE );
	}
}


bool CHL2MP_Player::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex )
{
	bool bRet = BaseClass::Weapon_Switch( pWeapon, viewmodelindex );

	if ( bRet == true )
	{
		ResetAnimation();
	}

	return bRet;
}

void CHL2MP_Player::PreThink( void )
{
	QAngle vOldAngles = GetLocalAngles();
	QAngle vTempAngles = GetLocalAngles();

	vTempAngles = EyeAngles();

	if ( vTempAngles[PITCH] > 180.0f )
	{
		vTempAngles[PITCH] -= 360.0f;
	}

	SetLocalAngles( vTempAngles );

	BaseClass::PreThink();
	State_PreThink();

	//Reset bullet force accumulator, only lasts one frame
	m_vecTotalBulletForce = vec3_origin;
	SetLocalAngles( vOldAngles );
}

void CHL2MP_Player::PostThink( void )
{
	BaseClass::PostThink();
	
	if ( GetFlags() & FL_DUCKING )
	{
		SetCollisionBounds( VEC_CROUCH_TRACE_MIN, VEC_CROUCH_TRACE_MAX );
	}

	if ( !IsInKart() )
	{
		m_PlayerAnimState.Update();
	}
	else
	{
		KartItemPostThink();

		if ( kart_debug_server.GetBool() )
		{
			Vector vecForward;
			AngleVectors( QAngle( 0.0f, m_flKartYaw, 0.0f ), &vecForward );
			Vector vecStart = GetAbsOrigin() + Vector( 0.0f, 0.0f, 24.0f );
			NDebugOverlay::Line( vecStart, vecStart + vecForward * 128.0f, 255, 255, 0, true, 0.1f );
			NDebugOverlay::Box( GetAbsOrigin(), KART_HULL_MIN, KART_HULL_MAX, 0, 255, 255, 0, 0.1f );
		}

		// A checkpoint respawn's freeze is over, unless the race flow holds the karts.
		if ( m_flKartRespawnUnfreezeTime > 0.0f && gpGlobals->curtime >= m_flKartRespawnUnfreezeTime )
		{
			m_flKartRespawnUnfreezeTime = 0.0f;
			if ( !HL2MPRules()->IsIntermission() && !HL2MPRules()->IsKartRaceFrozen() )
			{
				RemoveFlag( FL_FROZEN );
			}
		}
	}

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );
}

void CHL2MP_Player::PlayerDeathThink()
{
	if( !IsObserver() )
	{
		BaseClass::PlayerDeathThink();
	}
}

void CHL2MP_Player::FireBullets ( const FireBulletsInfo_t &info )
{
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( this, this->GetCurrentCommand() );

	FireBulletsInfo_t modinfo = info;

	CWeaponHL2MPBase *pWeapon = dynamic_cast<CWeaponHL2MPBase *>( GetActiveWeapon() );

	if ( pWeapon )
	{
		modinfo.m_iPlayerDamage = modinfo.m_flDamage = pWeapon->GetHL2MPWpnData().m_iPlayerDamage;
	}

	NoteWeaponFired();

	BaseClass::FireBullets( modinfo );

	// Move other players back to history positions based on local player's lag
	lagcompensation->FinishLagCompensation( this );

	if ( pWeapon )
		this->OnMyWeaponFired( pWeapon );
}

void CHL2MP_Player::OnMyWeaponFired( CBaseCombatWeapon* weapon )
{
	BaseClass::OnMyWeaponFired( weapon );

	TheNextBots().OnWeaponFired( this, weapon );
}

void CHL2MP_Player::NoteWeaponFired( void )
{
	Assert( m_pCurrentCommand );
	if( m_pCurrentCommand )
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

extern ConVar sv_maxunlag;

bool CHL2MP_Player::WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if ( !( pCmd->buttons & IN_ATTACK ) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5) )
		return false;

	// If this entity hasn't been transmitted to us and acked, then don't bother lag compensating it.
	if ( pEntityTransmitBits && !pEntityTransmitBits->Get( pPlayer->entindex() ) )
		return false;

	const Vector &vMyOrigin = GetAbsOrigin();
	const Vector &vHisOrigin = pPlayer->GetAbsOrigin();

	// get max distance player could have moved within max lag compensation time, 
	// multiply by 1.5 to to avoid "dead zones"  (sqrt(2) would be the exact value)
	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	// If the player is within this distance, lag compensate them in case they're running past us.
	if ( vHisOrigin.DistTo( vMyOrigin ) < maxDistance )
		return true;

	// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
	Vector vForward;
	AngleVectors( pCmd->viewangles, &vForward );
	
	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize( vDiff );

	float flCosAngle = 0.707107f;	// 45 degree angle
	if ( vForward.Dot( vDiff ) < flCosAngle )
		return false;

	return true;
}

Activity CHL2MP_Player::TranslateTeamActivity( Activity ActToTranslate )
{
	if ( m_iModelType == TEAM_COMBINE )
		 return ActToTranslate;
	
	if ( ActToTranslate == ACT_RUN )
		 return ACT_RUN_AIM_AGITATED;

	if ( ActToTranslate == ACT_IDLE )
		 return ACT_IDLE_AIM_AGITATED;

	if ( ActToTranslate == ACT_WALK )
		 return ACT_WALK_AIM_AGITATED;

	return ActToTranslate;
}

extern ConVar hl2_normspeed;

// Set the activity based on an event or current state
void CHL2MP_Player::SetAnimation( PLAYER_ANIM playerAnim )
{
	// The kart model has none of the player activities.
	if ( IsInKart() )
		return;

	int animDesired;

	float speed;

	speed = GetAbsVelocity().Length2D();

	
	// bool bRunning = true;

	//Revisit!
/*	if ( ( m_nButtons & ( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) ) )
	{
		if ( speed > 1.0f && speed < hl2_normspeed.GetFloat() - 20.0f )
		{
			bRunning = false;
		}
	}*/

	if ( GetFlags() & ( FL_FROZEN | FL_ATCONTROLS ) )
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	Activity idealActivity = ACT_HL2MP_RUN;

	// This could stand to be redone. Why is playerAnim abstracted from activity? (sjb)
	if ( playerAnim == PLAYER_JUMP )
	{
		idealActivity = ACT_HL2MP_JUMP;
	}
	else if ( playerAnim == PLAYER_DIE )
	{
		if ( m_lifeState == LIFE_ALIVE )
		{
			return;
		}
	}
	else if ( playerAnim == PLAYER_ATTACK1 )
	{
		if ( GetActivity( ) == ACT_HOVER	|| 
			 GetActivity( ) == ACT_SWIM		||
			 GetActivity( ) == ACT_HOP		||
			 GetActivity( ) == ACT_LEAP		||
			 GetActivity( ) == ACT_DIESIMPLE )
		{
			idealActivity = GetActivity( );
		}
		else
		{
			idealActivity = ACT_HL2MP_GESTURE_RANGE_ATTACK;
		}
	}
	else if ( playerAnim == PLAYER_RELOAD )
	{
		idealActivity = ACT_HL2MP_GESTURE_RELOAD;
	}
	else if ( playerAnim == PLAYER_IDLE || playerAnim == PLAYER_WALK )
	{
		if ( !( GetFlags() & FL_ONGROUND ) && GetActivity( ) == ACT_HL2MP_JUMP )	// Still jumping
		{
			idealActivity = GetActivity( );
		}
		/*
		else if ( GetWaterLevel() > 1 )
		{
			if ( speed == 0 )
				idealActivity = ACT_HOVER;
			else
				idealActivity = ACT_SWIM;
		}
		*/
		else
		{
			if ( GetFlags() & FL_DUCKING )
			{
				if ( speed > 0 )
				{
					idealActivity = ACT_HL2MP_WALK_CROUCH;
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE_CROUCH;
				}
			}
			else
			{
				if ( speed > 0 )
				{
					/*
					if ( bRunning == false )
					{
						idealActivity = ACT_WALK;
					}
					else
					*/
					{
						idealActivity = ACT_HL2MP_RUN;
					}
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE;
				}
			}
		}

		idealActivity = TranslateTeamActivity( idealActivity );
	}
	
	if ( idealActivity == ACT_HL2MP_GESTURE_RANGE_ATTACK )
	{
		RestartGesture( Weapon_TranslateActivity( idealActivity ) );

		// FIXME: this seems a bit wacked
		//
		// misyl: it was and was causing a pred error every time.
		// the weapons already call SendWeaponAnim with the right activity.
		//Weapon_SetActivity( Weapon_TranslateActivity( ACT_RANGE_ATTACK1 ), 0 );

		return;
	}
	else if ( idealActivity == ACT_HL2MP_GESTURE_RELOAD )
	{
		RestartGesture( Weapon_TranslateActivity( idealActivity ) );
		return;
	}
	else
	{
		SetActivity( idealActivity );

		animDesired = SelectWeightedSequence( Weapon_TranslateActivity ( idealActivity ) );

		if (animDesired == -1)
		{
			animDesired = SelectWeightedSequence( idealActivity );

			if ( animDesired == -1 )
			{
				animDesired = 0;
			}
		}
	
		// Already using the desired animation?
		if ( GetSequence() == animDesired )
			return;

		m_flPlaybackRate = 1.0;
		ResetSequence( animDesired );
		SetCycle( 0 );
		return;
	}

	// Already using the desired animation?
	if ( GetSequence() == animDesired )
		return;

	//Msg( "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	ResetSequence( animDesired );
	SetCycle( 0 );
}


extern int	gEvilImpulse101;
//-----------------------------------------------------------------------------
// Purpose: Player reacts to bumping a weapon. 
// Input  : pWeapon - the weapon that the player bumped into.
// Output : Returns true if player picked up the weapon
//-----------------------------------------------------------------------------
bool CHL2MP_Player::BumpWeapon( CBaseCombatWeapon *pWeapon )
{
	// Karts carry no weapons.
	if ( IsInKart() )
		return false;

	CBaseCombatCharacter *pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if ( !IsAllowedToPickupWeapons() )
		return false;

	if ( pOwner || !Weapon_CanUse( pWeapon ) || !g_pGameRules->CanHavePlayerItem( this, pWeapon ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( pWeapon );
		}
		return false;
	}

	// Don't let the player fetch weapons through walls (use MASK_SOLID so that you can't pickup through windows)
	if( !pWeapon->FVisible( this, MASK_SOLID ) && !(GetFlags() & FL_NOTARGET) )
	{
		return false;
	}

	bool bOwnsWeaponAlready = !!Weapon_OwnsThisType( pWeapon->GetClassname(), pWeapon->GetSubType());

	if ( bOwnsWeaponAlready == true ) 
	{
		//If we have room for the ammo, then "take" the weapon too.
		 if ( Weapon_EquipAmmoOnly( pWeapon ) )
		 {
			 pWeapon->CheckRespawn();

			 UTIL_Remove( pWeapon );
			 return true;
		 }
		 else
		 {
			 return false;
		 }
	}

	pWeapon->CheckRespawn();
	Weapon_Equip( pWeapon );

	return true;
}

void CHL2MP_Player::ChangeTeam( int iTeam )
{
/*	if ( GetNextTeamChangeTime() >= gpGlobals->curtime )
	{
		char szReturnString[128];
		Q_snprintf( szReturnString, sizeof( szReturnString ), "Please wait %d more seconds before trying to switch teams again.\n", (int)(GetNextTeamChangeTime() - gpGlobals->curtime) );

		ClientPrint( this, HUD_PRINTTALK, szReturnString );
		return;
	}*/

	bool bKill = false;

	if ( HL2MPRules()->IsTeamplay() != true && iTeam != TEAM_SPECTATOR )
	{
		//don't let them try to join combine or rebels during deathmatch.
		iTeam = TEAM_UNASSIGNED;
	}

	if ( HL2MPRules()->IsTeamplay() == true )
	{
		if ( iTeam != GetTeamNumber() && GetTeamNumber() != TEAM_UNASSIGNED )
		{
			bKill = true;
		}
	}

	BaseClass::ChangeTeam( iTeam );

	m_flNextTeamChangeTime = gpGlobals->curtime + TEAM_CHANGE_INTERVAL;

	if ( HL2MPRules()->IsTeamplay() == true )
	{
		SetPlayerTeamModel();
	}
	else
	{
		SetPlayerModel();
	}

	if ( iTeam == TEAM_SPECTATOR )
	{
		RemoveAllItems( true );

		State_Transition( STATE_OBSERVER_MODE );
	}

	if ( bKill == true )
	{
		CommitSuicide();
	}
}

bool CHL2MP_Player::HandleCommand_JoinTeam( int team )
{
	if ( !GetGlobalTeam( team ) || team == 0 )
	{
		Warning( "HandleCommand_JoinTeam( %d ) - invalid team index.\n", team );
		return false;
	}

	if ( team == TEAM_SPECTATOR )
	{
		// Prevent this is the cvar is set
		if ( !mp_allowspectators.GetInt() && !IsHLTV() )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Cannot_Be_Spectator" );
			return false;
		}

		if ( GetTeamNumber() != TEAM_UNASSIGNED && !IsDead() )
		{
			m_fNextSuicideTime = gpGlobals->curtime;	// allow the suicide to work

			CommitSuicide();

			// add 1 to frags to balance out the 1 subtracted for killing yourself
			IncrementFragCount( 1 );
		}

		ChangeTeam( TEAM_SPECTATOR );

		return true;
	}
	else
	{
		StopObserverMode();
		State_Transition(STATE_ACTIVE);
	}

	// Switch their actual team...
	ChangeTeam( team );

	return true;
}

bool CHL2MP_Player::ClientCommand( const CCommand &args )
{
	if ( FStrEq( args[0], "spectate" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			// instantly join spectators
			HandleCommand_JoinTeam( TEAM_SPECTATOR );	
		}
		return true;
	}
	else if ( FStrEq( args[0], "jointeam" ) ) 
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad jointeam syntax\n" );
		}

		if ( ShouldRunRateLimitedCommand( args ) )
		{
			int iTeam = atoi( args[1] );
			HandleCommand_JoinTeam( iTeam );
		}
		return true;
	}
	else if ( FStrEq( args[0], "joingame" ) )
	{
		return true;
	}
	else if ( FStrEq( args[0], "kart_respawn" ) )
	{
		// Back to the last checkpoint, for a kart that is stuck. Not while held
		// on the grid, at the results or still frozen from the last respawn.
		if ( ShouldRunRateLimitedCommand( args ) && gpGlobals->curtime >= m_flKartNextRespawnCommand
			&& IsInKart() && !( GetFlags() & FL_FROZEN ) )
		{
			if ( KartRespawnAtCheckpoint() )
			{
				m_flKartNextRespawnCommand = gpGlobals->curtime + kart_respawn_cooldown.GetFloat();
			}
		}
		return true;
	}

	return BaseClass::ClientCommand( args );
}

void CHL2MP_Player::CheatImpulseCommands( int iImpulse )
{
	switch ( iImpulse )
	{
		case 101:
			{
				if( sv_cheats->GetBool() )
				{
					GiveAllItems();
				}
			}
			break;

		default:
			BaseClass::CheatImpulseCommands( iImpulse );
	}
}

bool CHL2MP_Player::ShouldRunRateLimitedCommand( const CCommand &args )
{
	int i = m_RateLimitLastCommandTimes.Find( args[0] );
	if ( i == m_RateLimitLastCommandTimes.InvalidIndex() )
	{
		m_RateLimitLastCommandTimes.Insert( args[0], gpGlobals->curtime );
		return true;
	}
	else if ( (gpGlobals->curtime - m_RateLimitLastCommandTimes[i]) < HL2MP_COMMAND_MAX_RATE )
	{
		// Too fast.
		return false;
	}
	else
	{
		m_RateLimitLastCommandTimes[i] = gpGlobals->curtime;
		return true;
	}
}

void CHL2MP_Player::CreateViewModel( int index /*=0*/ )
{
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	if ( GetViewModel( index ) )
		return;

	CPredictedViewModel *vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( index );
		DispatchSpawn( vm );
		vm->FollowEntity( this, false );
		m_hViewModel.Set( index, vm );
	}
}

bool CHL2MP_Player::BecomeRagdollOnClient( const Vector &force )
{
	return true;
}

// -------------------------------------------------------------------------------- //
// Ragdoll entities.
// -------------------------------------------------------------------------------- //

class CHL2MPRagdoll : public CBaseAnimatingOverlay
{
public:
	DECLARE_CLASS( CHL2MPRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

public:
	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle( CBaseEntity, m_hPlayer );	// networked entity handle 
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
};

LINK_ENTITY_TO_CLASS( hl2mp_ragdoll, CHL2MPRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CHL2MPRagdoll, DT_HL2MPRagdoll )
	SendPropVector( SENDINFO(m_vecRagdollOrigin), -1,  SPROP_COORD ),
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropModelIndex( SENDINFO( m_nModelIndex ) ),
	SendPropInt		( SENDINFO(m_nForceBone), 8, 0 ),
	SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vecRagdollVelocity ) )
END_SEND_TABLE()


void CHL2MP_Player::CreateRagdollEntity( void )
{
	if ( m_hRagdoll )
	{
		UTIL_RemoveImmediate( m_hRagdoll );
		m_hRagdoll = NULL;
	}

	// If we already have a ragdoll, don't make another one.
	CHL2MPRagdoll *pRagdoll = dynamic_cast< CHL2MPRagdoll* >( m_hRagdoll.Get() );
	
	if ( !pRagdoll )
	{
		// create a new one
		pRagdoll = dynamic_cast< CHL2MPRagdoll* >( CreateEntityByName( "hl2mp_ragdoll" ) );
	}

	if ( pRagdoll )
	{
		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->m_vecForce = m_vecTotalBulletForce;
		pRagdoll->SetAbsOrigin( GetAbsOrigin() );
	}

	// ragdolls will be removed on round restart automatically
	m_hRagdoll = pRagdoll;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL2MP_Player::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2MP_Player::FlashlightTurnOn( void )
{
	if( flashlight.GetInt() > 0 && IsAlive() )
	{
		AddEffects( EF_DIMLIGHT );
		EmitSound( "HL2Player.FlashlightOn" );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2MP_Player::FlashlightTurnOff( void )
{
	RemoveEffects( EF_DIMLIGHT );
	
	if( IsAlive() )
	{
		EmitSound( "HL2Player.FlashlightOff" );
	}
}

void CHL2MP_Player::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget, const Vector *pVelocity )
{
	//Drop a grenade if it's primed.
	if ( GetActiveWeapon() )
	{
		CBaseCombatWeapon *pGrenade = Weapon_OwnsThisType("weapon_frag");

		if ( GetActiveWeapon() == pGrenade )
		{
			if ( ( m_nButtons & IN_ATTACK ) || (m_nButtons & IN_ATTACK2) )
			{
				DropPrimedFragGrenade( this, pGrenade );
				return;
			}
		}
	}

	BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );
}

int CHL2MP_Player::GetMaxAmmo( int iAmmoIndex ) const
{
	if ( iAmmoIndex == -1 )
		return 0;

	if ( GetAmmoDef()->MaxCarry( iAmmoIndex ) == INFINITE_AMMO )
		return 999;

	return GetAmmoDef()->MaxCarry( iAmmoIndex );
}

void CHL2MP_Player::DetonateTripmines( void )
{
	CBaseEntity *pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "npc_satchel" )) != NULL)
	{
		CSatchelCharge *pSatchel = dynamic_cast<CSatchelCharge *>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() == this )
		{
			g_EventQueue.AddEvent( pSatchel, "Explode", 0.20, this, this );
		}
	}

	// Play sound for pressing the detonator
	EmitSound( "Weapon_SLAM.SatchelDetonate" );
}

void CHL2MP_Player::Event_Killed( const CTakeDamageInfo &info )
{
	//update damage info with our accumulated physics force
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce( m_vecTotalBulletForce );

	SetNumAnimOverlays( 0 );

	// Note: since we're dead, it won't draw us on the client, but we don't set EF_NODRAW
	// because we still want to transmit to the clients in our PVS.
	CreateRagdollEntity();

	DetonateTripmines();

	BaseClass::Event_Killed( subinfo );

	if ( info.GetDamageType() & DMG_DISSOLVE )
	{
		if ( m_hRagdoll )
		{
			m_hRagdoll->GetBaseAnimating()->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
		}
	}

	CBaseEntity *pAttacker = info.GetAttacker();

	if ( pAttacker )
	{
		int iScoreToAdd = 1;

		if ( pAttacker == this )
		{
			iScoreToAdd = -1;
		}

		GetGlobalTeam( pAttacker->GetTeamNumber() )->AddScore( iScoreToAdd );
	}

	FlashlightTurnOff();

	m_lifeState = LIFE_DEAD;

	RemoveEffects( EF_NODRAW );	// still draw player body
	StopZooming();
}

int CHL2MP_Player::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	//return here if the player is in the respawn grace period vs. slams.
	if ( gpGlobals->curtime < m_flSlamProtectTime &&  (inputInfo.GetDamageType() == DMG_BLAST ) )
		return 0;

	m_vecTotalBulletForce += inputInfo.GetDamageForce();
	
	gamestats->Event_PlayerDamage( this, inputInfo );

	return BaseClass::OnTakeDamage( inputInfo );
}

void CHL2MP_Player::DeathSound( const CTakeDamageInfo &info )
{
	if ( m_hRagdoll && m_hRagdoll->GetBaseAnimating()->IsDissolving() )
		 return;

	char szStepSound[128];

	Q_snprintf( szStepSound, sizeof( szStepSound ), "%s.Die", GetPlayerModelSoundPrefix() );

	const char *pModelName = STRING( GetModelName() );

	CSoundParameters params;
	if ( GetParametersForSound( szStepSound, params, pModelName ) == false )
		return;

	Vector vecOrigin = GetAbsOrigin();
	
	CRecipientFilter filter;
	filter.AddRecipientsByPAS( vecOrigin );

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep );
}

CBaseEntity* CHL2MP_Player::EntSelectSpawnPoint( void )
{
	CBaseEntity *pSpot = NULL;
	CBaseEntity *pLastSpawnPoint = g_pLastSpawn;
	edict_t		*player = edict();
	const char *pSpawnpointName = "info_player_deathmatch";

	// Karts line up on the race grid when the map has one. The deathmatch
	// spawn bookkeeping below is left alone so free drive is unchanged.
	if ( IsInKart() )
	{
		pSpot = KartRace_SelectGridSpawn( this );
		if ( pSpot )
		{
			m_flSlamProtectTime = gpGlobals->curtime + 0.5;
			return pSpot;
		}
	}

	if ( HL2MPRules()->IsTeamplay() == true )
	{
		if ( GetTeamNumber() == TEAM_COMBINE )
		{
			pSpawnpointName = "info_player_combine";
			pLastSpawnPoint = g_pLastCombineSpawn;
		}
		else if ( GetTeamNumber() == TEAM_REBELS )
		{
			pSpawnpointName = "info_player_rebel";
			pLastSpawnPoint = g_pLastRebelSpawn;
		}

		if ( gEntList.FindEntityByClassname( NULL, pSpawnpointName ) == NULL )
		{
			pSpawnpointName = "info_player_deathmatch";
			pLastSpawnPoint = g_pLastSpawn;
		}
	}

	pSpot = pLastSpawnPoint;
	// Randomize the start spot
	for ( int i = random->RandomInt(1,5); i > 0; i-- )
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
	if ( !pSpot )  // skip over the null point
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );

	CBaseEntity *pFirstSpot = pSpot;

	do 
	{
		if ( pSpot )
		{
			// check if pSpot is valid
			if ( g_pGameRules->IsSpawnPointValid( pSpot, this ) )
			{
				if ( pSpot->GetLocalOrigin() == vec3_origin )
				{
					pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
					continue;
				}

				// if so, go to pSpot
				goto ReturnSpot;
			}
		}
		// increment pSpot
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
	} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

	// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
	if ( pSpot )
	{
		CBaseEntity *ent = NULL;
		for ( CEntitySphereQuery sphere( pSpot->GetAbsOrigin(), hl2mp_spawn_frag_fallback_radius.GetFloat() ); (ent = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
		{
			// if ent is a client, kill em (unless they are ourselves)
			if ( ent->IsPlayer() && !(ent->edict() == player) )
				ent->TakeDamage( CTakeDamageInfo( GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), 300, DMG_GENERIC ) );
		}
		goto ReturnSpot;
	}

	if ( !pSpot  )
	{
		pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_start" );

		if ( pSpot )
			goto ReturnSpot;
	}

ReturnSpot:

	if ( HL2MPRules()->IsTeamplay() == true )
	{
		if ( GetTeamNumber() == TEAM_COMBINE )
		{
			g_pLastCombineSpawn = pSpot;
		}
		else if ( GetTeamNumber() == TEAM_REBELS ) 
		{
			g_pLastRebelSpawn = pSpot;
		}
	}

	g_pLastSpawn = pSpot;

	m_flSlamProtectTime = gpGlobals->curtime + 0.5;

	return pSpot;
} 


CON_COMMAND( timeleft, "prints the time remaining in the match" )
{
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_GetCommandClient() );

	int iTimeRemaining = (int)HL2MPRules()->GetMapRemainingTime();
    
	if ( iTimeRemaining == 0 )
	{
		if ( pPlayer )
		{
			ClientPrint( pPlayer, HUD_PRINTTALK, "This game has no timelimit." );
		}
		else
		{
			Msg( "* No Time Limit *\n" );
		}
	}
	else
	{
		int iMinutes, iSeconds;
		iMinutes = iTimeRemaining / 60;
		iSeconds = iTimeRemaining % 60;

		char minutes[8];
		char seconds[8];

		Q_snprintf( minutes, sizeof(minutes), "%d", iMinutes );
		Q_snprintf( seconds, sizeof(seconds), "%2.2d", iSeconds );

		if ( pPlayer )
		{
			ClientPrint( pPlayer, HUD_PRINTTALK, "Time left in map: %s1:%s2", minutes, seconds );
		}
		else
		{
			Msg( "Time Remaining:  %s:%s\n", minutes, seconds );
		}
	}	
}


void CHL2MP_Player::Reset()
{	
	ResetDeathCount();
	ResetFragCount();
}

bool CHL2MP_Player::IsReady()
{
	return m_bReady;
}

void CHL2MP_Player::SetReady( bool bReady )
{
	m_bReady = bReady;
}

void CHL2MP_Player::CheckChatText( char *p, int bufsize )
{
	//Look for escape sequences and replace

	char *buf = new char[bufsize];
	int pos = 0;

	// Parse say text for escape sequences
	for ( char *pSrc = p; pSrc != NULL && *pSrc != 0 && pos < bufsize-1; pSrc++ )
	{
		// copy each char across
		buf[pos] = *pSrc;
		pos++;
	}

	buf[pos] = '\0';

	// copy buf back into p
	Q_strncpy( p, buf, bufsize );

	delete[] buf;	

	const char *pReadyCheck = p;

	HL2MPRules()->CheckChatForReadySignal( this, pReadyCheck );
}

void CHL2MP_Player::State_Transition( HL2MPPlayerState newState )
{
	State_Leave();
	State_Enter( newState );
}


void CHL2MP_Player::State_Enter( HL2MPPlayerState newState )
{
	m_iPlayerState = newState;
	m_pCurStateInfo = State_LookupInfo( newState );

	// Initialize the new state.
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnEnterState )
		(this->*m_pCurStateInfo->pfnEnterState)();
}


void CHL2MP_Player::State_Leave()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState )
	{
		(this->*m_pCurStateInfo->pfnLeaveState)();
	}
}


void CHL2MP_Player::State_PreThink()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnPreThink )
	{
		(this->*m_pCurStateInfo->pfnPreThink)();
	}
}


CHL2MPPlayerStateInfo *CHL2MP_Player::State_LookupInfo( HL2MPPlayerState state )
{
	// This table MUST match the 
	static CHL2MPPlayerStateInfo playerStateInfos[] =
	{
		{ STATE_ACTIVE,			"STATE_ACTIVE",			&CHL2MP_Player::State_Enter_ACTIVE, NULL, &CHL2MP_Player::State_PreThink_ACTIVE },
		{ STATE_OBSERVER_MODE,	"STATE_OBSERVER_MODE",	&CHL2MP_Player::State_Enter_OBSERVER_MODE,	NULL, &CHL2MP_Player::State_PreThink_OBSERVER_MODE }
	};

	for ( int i=0; i < ARRAYSIZE( playerStateInfos ); i++ )
	{
		if ( playerStateInfos[i].m_iPlayerState == state )
			return &playerStateInfos[i];
	}

	return NULL;
}

bool CHL2MP_Player::StartObserverMode(int mode)
{
	//we only want to go into observer mode if the player asked to, not on a death timeout
	if ( m_bEnterObserver == true )
	{
		VPhysicsDestroyObject();
		return BaseClass::StartObserverMode( mode );
	}
	return false;
}

void CHL2MP_Player::StopObserverMode()
{
	m_bEnterObserver = false;
	BaseClass::StopObserverMode();
}

bool CHL2MP_Player::IsKartSpectating( void )
{
	return IsInKart() && IsObserver() && GetTeamNumber() != TEAM_SPECTATOR;
}

//-----------------------------------------------------------------------------
// Purpose: The kart leaves the track and watches the race leader. From the race
//			flow, for karts that finished and for late joiners.
//-----------------------------------------------------------------------------
void CHL2MP_Player::KartStartSpectating( void )
{
	if ( !IsInKart() || IsObserver() || GetTeamNumber() == TEAM_SPECTATOR )
		return;

	KartClearItem();
	RemoveFlag( FL_FROZEN );

	State_Transition( STATE_OBSERVER_MODE );
	if ( !IsObserver() )
		return;

	// Chase cam whatever cl_spec_mode says: jump switches to free look.
	CHL2MP_Player *pLeader = HL2MPRules()->GetKartLeader();
	if ( pLeader )
	{
		SetObserverTarget( pLeader );
	}
	SetObserverMode( OBS_MODE_CHASE );

	m_Local.m_iHideHUD |= KART_HIDEHUD_BITS;
}

//-----------------------------------------------------------------------------
// Purpose: Kart spectators only watch karts still on the track.
//-----------------------------------------------------------------------------
bool CHL2MP_Player::IsValidObserverTarget( CBaseEntity *target )
{
	if ( !IsKartSpectating() )
		return BaseClass::IsValidObserverTarget( target );

	CHL2MP_Player *pKart = ToHL2MPPlayer( target );
	if ( !pKart || pKart == this || !pKart->IsInKart() || !pKart->IsAlive() || pKart->IsObserver() )
		return false;

	return !pKart->IsEffectActive( EF_NODRAW );
}

//-----------------------------------------------------------------------------
// Purpose: When the kart being watched finishes (or leaves), a kart spectator
//			moves on to the leader; with nobody left driving, it looks around.
//-----------------------------------------------------------------------------
void CHL2MP_Player::ValidateCurrentObserverTarget( void )
{
	if ( !IsKartSpectating() || IsValidObserverTarget( m_hObserverTarget.Get() ) )
	{
		BaseClass::ValidateCurrentObserverTarget();
		return;
	}

	CHL2MP_Player *pLeader = HL2MPRules()->GetKartLeader();
	if ( pLeader && SetObserverTarget( pLeader ) )
		return;

	ForceObserverMode( OBS_MODE_ROAMING );
}

void CHL2MP_Player::State_Enter_OBSERVER_MODE()
{
	int observerMode = m_iObserverLastMode;
	if ( IsNetClient() )
	{
		const char *pIdealMode = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_spec_mode" );
		if ( pIdealMode )
		{
			observerMode = atoi( pIdealMode );
			if ( observerMode <= OBS_MODE_FIXED || observerMode > OBS_MODE_ROAMING )
			{
				observerMode = m_iObserverLastMode;
			}
		}
	}
	m_bEnterObserver = true;
	StartObserverMode( observerMode );
}

void CHL2MP_Player::State_PreThink_OBSERVER_MODE()
{
	// Make sure nobody has changed any of our state.
	//	Assert( GetMoveType() == MOVETYPE_FLY );
	Assert( m_takedamage == DAMAGE_NO );
	Assert( IsSolidFlagSet( FSOLID_NOT_SOLID ) );
	//	Assert( IsEffectActive( EF_NODRAW ) );

	// Must be dead.
	Assert( m_lifeState == LIFE_DEAD );
	Assert( pl.deadflag );
}


void CHL2MP_Player::State_Enter_ACTIVE()
{
	SetMoveType( MOVETYPE_WALK );
	
	// md 8/15/07 - They'll get set back to solid when they actually respawn. If we set them solid now and mp_forcerespawn
	// is false, then they'll be spectating but blocking live players from moving.
	// RemoveSolidFlags( FSOLID_NOT_SOLID );
	
	m_Local.m_iHideHUD = 0;

	if ( IsInKart() )
	{
		m_Local.m_iHideHUD |= KART_HIDEHUD_BITS;
	}
}


void CHL2MP_Player::State_PreThink_ACTIVE()
{
	//we don't really need to do anything here. 
	//This state_prethink structure came over from CS:S and was doing an assert check that fails the way hl2dm handles death
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2MP_Player::CanHearAndReadChatFrom( CBasePlayer *pPlayer )
{
	// can always hear the console unless we're ignoring all chat
	if ( !pPlayer )
		return false;

	return true;
}

//-----------------------------------------------------------------------------------------------------
// Return true if the given threat is aiming in our direction
bool CHL2MP_Player::IsThreatAimingTowardMe( CBaseEntity* threat, float cosTolerance ) const
{
	CHL2MP_Player* player = ToHL2MPPlayer( threat );
	Vector to = GetAbsOrigin() - threat->GetAbsOrigin();
	Vector forward;

	if ( player == NULL )
	{
		return false;
	}

	// is the player pointing at me?
	player->EyeVectors( &forward );

	if ( DotProduct( to, forward ) > cosTolerance )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
// Return true if the given threat is aiming in our direction and firing its weapon
bool CHL2MP_Player::IsThreatFiringAtMe( CBaseEntity* threat ) const
{
	if ( IsThreatAimingTowardMe( threat ) )
	{
		CHL2MP_Player* player = ToHL2MPPlayer( threat );

		if ( player )
		{
			return player->IsFiringWeapon();
		}
	}

	return false;
}
