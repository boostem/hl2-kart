//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for the kart race HUD. Every kart HUD element derives
//			from CKartHudElement, which draws only while the local player is
//			a live kart and kart_hud is on, and reads its fonts and colors
//			from the mod's resource/ClientScheme.res (KartHud* fonts, Kart*
//			colors). Positions come from scripts/HudLayout.res.
//
//			The stock HL2 HUD is hidden in kart mode by KART_HIDEHUD_BITS
//			(kart_shareddefs.h), set on the player by the server; the few
//			stock elements those bits don't cover check KartHud_LocalPlayerInKart().
//
//=============================================================================//

#ifndef KART_HUD_BASE_H
#define KART_HUD_BASE_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

// 0 hides the whole kart HUD, for screenshots.
extern ConVar kart_hud;

// True while the local player is in kart mode, dead or alive. Stock HL2
// elements that KART_HIDEHUD_BITS doesn't cover (target id, pickup history)
// stay hidden while it is.
bool KartHud_LocalPlayerInKart( void );

class CKartHudElement : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CKartHudElement, vgui::Panel );

public:
	// pPanelName is the element's HudLayout.res entry, e.g. "KartLapCounter".
	CKartHudElement( const char *pElementName, const char *pPanelName );

	virtual bool	ShouldDraw( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

protected:
	// A font or color from the client scheme. Only valid from
	// ApplySchemeSettings() on, so look them up there.
	vgui::HFont		GetKartFont( vgui::IScheme *pScheme, const char *pszName );
	Color			GetKartColor( vgui::IScheme *pScheme, const char *pszName, Color fallback = Color( 255, 255, 255, 255 ) );
};

#endif // KART_HUD_BASE_H
