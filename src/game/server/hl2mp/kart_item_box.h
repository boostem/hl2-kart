//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: kart_item_box, a floating, spinning box on the track that gives the
//			kart touching it an item, vanishes and comes back after a delay.
//
//			Pattern of CItem (item_world.cpp): touch, hide, respawn, materialize.
//			Karts are players, so the box is a trigger-sized bbox with SetTouch
//			and only kart players can take it.
//
//=============================================================================//

#ifndef KART_ITEM_BOX_H
#define KART_ITEM_BOX_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"

class CHL2MP_Player;

// Placeholder model until the custom one (M8): HL2's item crate. Valve
// content, referenced by path only.
#define KART_ITEM_BOX_MODEL		"models/items/item_item_crate.mdl"

//-----------------------------------------------------------------------------
// kart_item_box
//-----------------------------------------------------------------------------
class CKartItemBox : public CBaseAnimating
{
	DECLARE_CLASS( CKartItemBox, CBaseAnimating );
	DECLARE_DATADESC();

public:
	CKartItemBox();

	virtual void Spawn( void );
	virtual void Precache( void );

	void BoxTouch( CBaseEntity *pOther );
	void BoxThink( void );

	bool IsAvailable( void ) const { return !m_bTaken; }

private:
	void Take( CHL2MP_Player *pPlayer );
	void Materialize( void );
	void Flash( float flScale );

	float m_flRespawnTime;		// keyvalue respawn_time, seconds
	float m_flScale;			// keyvalue scale, model scale

	bool m_bTaken;
	float m_flMaterializeTime;	// when a taken box comes back
	Vector m_vecBaseOrigin;		// placed position, the bob is around it
	float m_flBobPhase;			// so neighbouring boxes do not bob in step
	float m_flNextDebugDraw;
};

// Item hooks, stubs until the item system (#38) lands.
bool KartPlayerHasItem( CHL2MP_Player *pPlayer );
void KartGiveRandomItem( CHL2MP_Player *pPlayer );

#endif // KART_ITEM_BOX_H
