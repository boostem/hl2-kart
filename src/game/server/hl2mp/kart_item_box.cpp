//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: kart_item_box. See kart_item_box.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_item_box.h"
#include "hl2mp_player.h"
#include "kart_items.h"
#include "Sprite.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar kart_debug_server;

ConVar kart_item_box_always( "kart_item_box_always", "0", 0, "Item boxes give an item even to a kart that already holds one." );

#define KART_ITEM_BOX_FLASH_SPRITE	"sprites/glow01.vmt"

// Pickup area around the model's bounds, like CItem's ITEM_PICKUP_BOX_BLOAT.
#define KART_ITEM_BOX_BLOAT			24.0f

#define KART_ITEM_BOX_SPIN			90.0f	// yaw, degrees per second
#define KART_ITEM_BOX_BOB_HEIGHT	4.0f	// units above and below the placed position
#define KART_ITEM_BOX_BOB_PERIOD	2.0f	// seconds per bob
#define KART_ITEM_BOX_THINK			0.1f
#define KART_ITEM_BOX_GROW_TIME		0.4f	// the materialize grows the box from nothing
#define KART_ITEM_BOX_DEBUG_DRAW	0.25f

LINK_ENTITY_TO_CLASS( kart_item_box, CKartItemBox );

BEGIN_DATADESC( CKartItemBox )
	DEFINE_KEYFIELD( m_flRespawnTime, FIELD_FLOAT, "respawn_time" ),
	DEFINE_KEYFIELD( m_flScale, FIELD_FLOAT, "scale" ),
	DEFINE_FIELD( m_bTaken, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flMaterializeTime, FIELD_TIME ),
	DEFINE_FIELD( m_vecBaseOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_flBobPhase, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextDebugDraw, FIELD_TIME ),
	DEFINE_ENTITYFUNC( BoxTouch ),
	DEFINE_THINKFUNC( BoxThink ),
END_DATADESC()

CKartItemBox::CKartItemBox()
{
	m_flRespawnTime = 3.0f;
	m_flScale = 1.0f;
	m_bTaken = false;
	m_flMaterializeTime = 0.0f;
	m_flBobPhase = 0.0f;
	m_flNextDebugDraw = 0.0f;
}

void CKartItemBox::Precache( void )
{
	PrecacheModel( KartItem_GetModel( KART_ITEM_NONE ) );
	PrecacheModel( KART_ITEM_BOX_FLASH_SPRITE );
	PrecacheScriptSound( "Kart.ItemPickup" );
	PrecacheScriptSound( "Kart.ItemRespawn" );
}

void CKartItemBox::Spawn( void )
{
	Precache();
	SetModel( KartItem_GetModel( KART_ITEM_NONE ) );

	if ( m_flRespawnTime < 0.0f )
	{
		m_flRespawnTime = 0.0f;
	}
	if ( m_flScale <= 0.0f )
	{
		m_flScale = 1.0f;
	}

	// Floats in place: noclip so the angular velocity (spin) and the bob
	// velocity are integrated, without gravity or collisions.
	SetMoveType( MOVETYPE_NOCLIP );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );
	SetModelScale( m_flScale );
	CollisionProp()->UseTriggerBounds( true, KART_ITEM_BOX_BLOAT );

	m_vecBaseOrigin = GetLocalOrigin();
	m_flBobPhase = entindex() * 0.37f;
	SetLocalAngularVelocity( QAngle( 0, KART_ITEM_BOX_SPIN, 0 ) );

	m_bTaken = false;
	SetTouch( &CKartItemBox::BoxTouch );
	SetThink( &CKartItemBox::BoxThink );
	SetNextThink( gpGlobals->curtime );
}

void CKartItemBox::BoxTouch( CBaseEntity *pOther )
{
	if ( m_bTaken || !pOther->IsPlayer() )
		return;

	// Karts only: on foot, a stock deathmatch player walks through it.
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( pOther );
	if ( !pPlayer || !pPlayer->IsInKart() || !pPlayer->IsAlive() )
		return;

	if ( KartPlayerHasItem( pPlayer ) && !kart_item_box_always.GetBool() )
		return;

	Take( pPlayer );
}

void CKartItemBox::Take( CHL2MP_Player *pPlayer )
{
	KartGiveRandomItem( pPlayer );

	EmitSound( "Kart.ItemPickup" );
	Flash( 1.0f );

	if ( kart_debug_server.GetBool() )
	{
		Msg( "[kart] %s took item box %d at %.2f\n", pPlayer->GetPlayerName(), entindex(), gpGlobals->curtime );
	}

	m_bTaken = true;
	m_flMaterializeTime = gpGlobals->curtime + m_flRespawnTime;
	AddEffects( EF_NODRAW );
	SetTouch( NULL );
}

void CKartItemBox::Materialize( void )
{
	m_bTaken = false;
	RemoveEffects( EF_NODRAW );

	// Grow back from nothing with a flash, like a CItem respawn.
	SetModelScale( 0.01f );
	SetModelScale( m_flScale, KART_ITEM_BOX_GROW_TIME );
	EmitSound( "Kart.ItemRespawn" );
	Flash( 0.5f );

	SetTouch( &CKartItemBox::BoxTouch );
}

// A glow sprite that fades out at the box.
void CKartItemBox::Flash( float flScale )
{
	CSprite *pSprite = CSprite::SpriteCreate( KART_ITEM_BOX_FLASH_SPRITE, WorldSpaceCenter(), false );
	if ( !pSprite )
		return;

	pSprite->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
	pSprite->SetScale( flScale );
	pSprite->FadeAndDie( 0.4f );
}

void CKartItemBox::BoxThink( void )
{
	SetNextThink( gpGlobals->curtime + KART_ITEM_BOX_THINK );

	if ( m_bTaken && gpGlobals->curtime >= m_flMaterializeTime )
	{
		Materialize();
	}

	// Keep the spin's yaw small; the network sends it modulo 360 anyway.
	QAngle angles = GetLocalAngles();
	if ( angles.y >= 360.0f )
	{
		angles.y = anglemod( angles.y );
		SetLocalAngles( angles );
	}

	// Bob: aim the vertical velocity at where the box should be by the next
	// think, so it follows the sine without drifting.
	float flNext = gpGlobals->curtime + KART_ITEM_BOX_THINK;
	float flTargetZ = m_vecBaseOrigin.z + KART_ITEM_BOX_BOB_HEIGHT * sinf( ( flNext / KART_ITEM_BOX_BOB_PERIOD + m_flBobPhase ) * 2.0f * M_PI_F );
	SetLocalVelocity( Vector( 0, 0, ( flTargetZ - GetLocalOrigin().z ) / KART_ITEM_BOX_THINK ) );

	if ( kart_debug_server.GetBool() && gpGlobals->curtime >= m_flNextDebugDraw )
	{
		m_flNextDebugDraw = gpGlobals->curtime + KART_ITEM_BOX_DEBUG_DRAW;
		float flDuration = KART_ITEM_BOX_DEBUG_DRAW + 0.05f;

		// Pickup area (trigger bounds) and the model's own bounds.
		Vector vecMins, vecMaxs;
		CollisionProp()->WorldSpaceTriggerBounds( &vecMins, &vecMaxs );
		if ( m_bTaken )
		{
			NDebugOverlay::Box( vec3_origin, vecMins, vecMaxs, 128, 128, 128, 16, flDuration );
			NDebugOverlay::EntityTextAtPosition( WorldSpaceCenter(), 0, CFmtStr( "item box (%.1fs)", m_flMaterializeTime - gpGlobals->curtime ), flDuration );
		}
		else
		{
			NDebugOverlay::Box( vec3_origin, vecMins, vecMaxs, 255, 0, 255, 16, flDuration );
			NDebugOverlay::EntityBounds( this, 255, 0, 255, 0, flDuration );
			NDebugOverlay::EntityTextAtPosition( WorldSpaceCenter(), 0, "item box", flDuration );
		}
	}
}
