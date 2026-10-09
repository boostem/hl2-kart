//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for the kart race HUD. See kart_hud_base.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_hud_base.h"
#include "c_hl2mp_player.h"
#include "iclientmode.h"
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar kart_hud( "kart_hud", "1", FCVAR_ARCHIVE, "Draw the kart race HUD. 0 hides it, for screenshots." );

bool KartHud_LocalPlayerInKart( void )
{
	C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
	return pPlayer && pPlayer->IsInKart();
}

const wchar_t *KartHud_OrdinalSuffix( int n )
{
	n = abs( n );

	// 11th, 12th, 13th, 111th...
	int nTens = n % 100;
	if ( nTens >= 11 && nTens <= 13 )
		return L"th";

	switch ( n % 10 )
	{
	case 1:	return L"st";
	case 2:	return L"nd";
	case 3:	return L"rd";
	default: return L"th";
	}
}

CKartHudElement::CKartHudElement( const char *pElementName, const char *pPanelName ) :
	CHudElement( pElementName ), BaseClass( NULL, pPanelName )
{
	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: Only for a live local kart, and only with kart_hud on.
//-----------------------------------------------------------------------------
bool CKartHudElement::ShouldDraw( void )
{
	if ( !kart_hud.GetBool() )
		return false;

	C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
	if ( !pPlayer || !pPlayer->IsInKart() || !pPlayer->IsAlive() )
		return false;

	return CHudElement::ShouldDraw();
}

void CKartHudElement::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( false );
}

vgui::HFont CKartHudElement::GetKartFont( vgui::IScheme *pScheme, const char *pszName )
{
	return pScheme->GetFont( pszName, IsProportional() );
}

//-----------------------------------------------------------------------------
// Purpose: A Kart* scheme color, or the fallback if the scheme lacks it.
//-----------------------------------------------------------------------------
Color CKartHudElement::GetKartColor( vgui::IScheme *pScheme, const char *pszName, Color fallback )
{
	return pScheme->GetColor( pszName, fallback );
}

//-----------------------------------------------------------------------------
// Purpose: TEMPORARY. One line of text in each kart font, to check that the
//			scheme loads them. Goes away when the real elements land.
//-----------------------------------------------------------------------------
static ConVar kart_hud_test( "kart_hud_test", "1", 0, "Draw the temporary kart HUD font test text." );

class CKartHudTest : public CKartHudElement
{
	DECLARE_CLASS_SIMPLE( CKartHudTest, CKartHudElement );

public:
	CKartHudTest( const char *pElementName ) : BaseClass( pElementName, "KartHudTest" ) {}

	virtual bool ShouldDraw( void )
	{
		return kart_hud_test.GetBool() && BaseClass::ShouldDraw();
	}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hFonts[0] = GetKartFont( pScheme, "KartHudLarge" );
		m_hFonts[1] = GetKartFont( pScheme, "KartHudMedium" );
		m_hFonts[2] = GetKartFont( pScheme, "KartHudSmall" );
		m_Colors[0] = GetKartColor( pScheme, "KartAmber" );
		m_Colors[1] = GetKartColor( pScheme, "KartWhite" );
		m_Colors[2] = GetKartColor( pScheme, "KartAmberDim" );
	}

	virtual void Paint( void )
	{
		static const wchar_t *s_pszText[] = { L"1234567890", L"FINAL LAP", L"Kart HUD font test" };

		int y = 0;
		for ( int i = 0; i < ARRAYSIZE( s_pszText ); i++ )
		{
			vgui::surface()->DrawSetTextFont( m_hFonts[i] );
			vgui::surface()->DrawSetTextColor( m_Colors[i] );
			vgui::surface()->DrawSetTextPos( 0, y );
			vgui::surface()->DrawPrintText( s_pszText[i], wcslen( s_pszText[i] ) );
			y += vgui::surface()->GetFontTall( m_hFonts[i] );
		}
	}

private:
	vgui::HFont	m_hFonts[3];
	Color		m_Colors[3];
};

DECLARE_HUDELEMENT( CKartHudTest );
