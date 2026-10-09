//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart race HUD: the lap counter ("LAP 2/3") and the race position
//			("3rd / 8"). Both read the local player's networked race state
//			(kart_race_shared.h); the lap and racer totals come from the game
//			rules. A position change runs KartPositionPulse (HudAnimations.txt).
//
//=============================================================================//

#include "cbase.h"
#include "kart_hud_base.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "iclientmode.h"
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// Draws pszText at x, y and returns the x after it.
static int KartHud_DrawText( HFont hFont, Color col, int x, int y, const wchar_t *pszText )
{
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( pszText, wcslen( pszText ) );

	int wide, tall;
	surface()->GetTextSize( hFont, pszText, wide, tall );
	return x + wide;
}

// The y that puts hFont's text on the panel's bottom edge.
static int KartHud_BottomY( HFont hFont, int nPanelTall )
{
	return nPanelTall - surface()->GetFontTall( hFont );
}

//-----------------------------------------------------------------------------
// Purpose: "LAP 2/3", top left. Hidden before the first crossing of the line
//			(lap 0) and once the player has finished.
//-----------------------------------------------------------------------------
class CKartLapCounter : public CKartHudElement
{
	DECLARE_CLASS_SIMPLE( CKartLapCounter, CKartHudElement );

public:
	CKartLapCounter( const char *pElementName ) : BaseClass( pElementName, "KartLapCounter" ) {}

	virtual bool ShouldDraw( void )
	{
		if ( !BaseClass::ShouldDraw() )
			return false;

		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		return pPlayer->GetKartLap() > 0 && !pPlayer->IsKartFinished();
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hNumberFont = GetKartFont( pScheme, "KartHudLarge" );
		m_hLabelFont = GetKartFont( pScheme, "KartHudMedium" );
		m_LabelColor = GetKartColor( pScheme, "KartAmber" );
		m_NumberColor = GetKartColor( pScheme, "KartWhite" );
	}

	virtual void Paint( void )
	{
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( !pPlayer )
			return;

		int nLap = pPlayer->GetKartLap();
		int nLaps = HL2MPRules() ? HL2MPRules()->GetKartLaps() : 0;

		// The last lap is in amber.
		Color numberColor = ( nLaps > 0 && nLap >= nLaps ) ? m_LabelColor : m_NumberColor;

		int nNumberY = KartHud_BottomY( m_hNumberFont, GetTall() );
		int nLabelY = KartHud_BottomY( m_hLabelFont, GetTall() );

		int x = KartHud_DrawText( m_hLabelFont, m_LabelColor, 0, nLabelY, L"LAP " );

		wchar_t wszNumber[8];
		V_snwprintf( wszNumber, ARRAYSIZE( wszNumber ), L"%d", nLap );
		x = KartHud_DrawText( m_hNumberFont, numberColor, x, nNumberY, wszNumber );

		// Without a kart_race_manager the lap count is unknown: just "LAP 2".
		if ( nLaps > 0 )
		{
			wchar_t wszTotal[8];
			V_snwprintf( wszTotal, ARRAYSIZE( wszTotal ), L"/%d", nLaps );
			KartHud_DrawText( m_hLabelFont, m_NumberColor, x, nLabelY, wszTotal );
		}
	}

private:
	HFont	m_hNumberFont;
	HFont	m_hLabelFont;
	Color	m_LabelColor;
	Color	m_NumberColor;
};

DECLARE_HUDELEMENT( CKartLapCounter );

//-----------------------------------------------------------------------------
// Purpose: The race position, big, with its ordinal suffix and the number of
//			racers: "3RD / 8", bottom left. Hidden while not racing (position
//			0) and once the player has finished. Pulses when it changes.
//-----------------------------------------------------------------------------
class CKartPosition : public CKartHudElement
{
	DECLARE_CLASS_SIMPLE( CKartPosition, CKartHudElement );

public:
	CKartPosition( const char *pElementName ) : BaseClass( pElementName, "KartPosition" )
	{
		m_nShownPosition = 0;
	}

	virtual void Reset( void )
	{
		m_nShownPosition = 0;
		m_flBlur = 0.0f;
	}

	virtual bool ShouldDraw( void )
	{
		bool bDraw = BaseClass::ShouldDraw();
		if ( bDraw )
		{
			C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
			bDraw = pPlayer->GetKartRacePosition() > 0 && !pPlayer->IsKartFinished();
		}

		// Showing up again isn't a position change: no pulse for it.
		if ( !bDraw )
		{
			m_nShownPosition = 0;
		}

		return bDraw;
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hNumberFont = GetKartFont( pScheme, "KartHudLarge" );
		m_hSuffixFont = GetKartFont( pScheme, "KartHudMedium" );
		m_hRacersFont = GetKartFont( pScheme, "KartHudSmall" );
		m_RacersColor = GetKartColor( pScheme, "KartWhiteDim" );
	}

	virtual void OnThink( void )
	{
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( !pPlayer )
			return;

		int nPosition = pPlayer->GetKartRacePosition();
		if ( nPosition != m_nShownPosition )
		{
			if ( m_nShownPosition != 0 && nPosition != 0 )
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "KartPositionPulse" );
			}
			m_nShownPosition = nPosition;
		}
	}

	virtual void Paint( void )
	{
		if ( m_nShownPosition <= 0 )
			return;

		wchar_t wszNumber[8];
		V_snwprintf( wszNumber, ARRAYSIZE( wszNumber ), L"%d", m_nShownPosition );

		// The HL2MP face is drawn upper case.
		wchar_t wszSuffix[4];
		V_wcsncpy( wszSuffix, KartHud_OrdinalSuffix( m_nShownPosition ), sizeof( wszSuffix ) );
		for ( wchar_t *pch = wszSuffix; *pch; pch++ )
		{
			*pch = towupper( *pch );
		}

		int nNumberY = KartHud_BottomY( m_hNumberFont, GetTall() );

		// The pulse: extra additive passes of the number, the last one partial.
		for ( float fl = m_flBlur; fl > 0.0f; fl -= 1.0f )
		{
			Color col = m_TextColor;
			if ( fl < 1.0f )
			{
				col[3] *= fl;
			}
			KartHud_DrawText( m_hNumberFont, col, 0, nNumberY, wszNumber );
		}

		int x = KartHud_DrawText( m_hNumberFont, m_TextColor, 0, nNumberY, wszNumber );

		// Suffix raised to the top of the number, racer count along the bottom.
		int xSuffix = KartHud_DrawText( m_hSuffixFont, m_TextColor, x, nNumberY, wszSuffix );

		int nRacers = HL2MPRules() ? HL2MPRules()->GetKartRacers() : 0;
		if ( nRacers > 0 )
		{
			wchar_t wszRacers[8];
			V_snwprintf( wszRacers, ARRAYSIZE( wszRacers ), L" / %d", nRacers );
			KartHud_DrawText( m_hRacersFont, m_RacersColor, xSuffix, KartHud_BottomY( m_hRacersFont, GetTall() ), wszRacers );
		}
	}

private:
	// Animated by KartPositionPulse.
	CPanelAnimationVar( float, m_flBlur, "Blur", "0" );
	CPanelAnimationVar( Color, m_TextColor, "TextColor", "KartWhite" );

	int		m_nShownPosition;

	HFont	m_hNumberFont;
	HFont	m_hSuffixFont;
	HFont	m_hRacersFont;
	Color	m_RacersColor;
};

DECLARE_HUDELEMENT( CKartPosition );
