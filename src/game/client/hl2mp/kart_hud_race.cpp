//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart race HUD: the lap counter ("LAP 2/3"), the race position
//			("3rd / 8") and the race timer. They read the local player's
//			networked race state (kart_race_shared.h); the lap and racer totals
//			come from the game rules. A position change runs KartPositionPulse
//			and a new best lap KartTimerBestLap (HudAnimations.txt).
//
//=============================================================================//

#include "cbase.h"
#include "kart_hud_base.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "kart_race_shared.h"
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

//-----------------------------------------------------------------------------
// Purpose: Race times, top right, three right-aligned rows in mm:ss.mmm:
//			TOTAL (running race time), LAP (current lap) and BEST (fastest lap
//			so far). Hidden before the first crossing of the line. Once the
//			player has finished the clock stops: TOTAL shows the final race
//			time in amber and LAP goes away. A lap faster than the best so far
//			(kart_lap) runs KartTimerBestLap, a flash of the BEST row.
//-----------------------------------------------------------------------------
class CKartTimer : public CKartHudElement
{
	DECLARE_CLASS_SIMPLE( CKartTimer, CKartHudElement );

public:
	CKartTimer( const char *pElementName ) : BaseClass( pElementName, "KartTimer" )
	{
		m_flBestSeen = 0.0f;
	}

	virtual void Init( void )
	{
		ListenForGameEvent( KART_EVENT_LAP );
	}

	virtual void Reset( void )
	{
		m_flBestSeen = 0.0f;
		m_flBestFlash = 0.0f;
	}

	virtual bool ShouldDraw( void )
	{
		if ( !BaseClass::ShouldDraw() )
			return false;

		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		return pPlayer->GetKartLap() > 0 || pPlayer->IsKartFinished();
	}

	virtual void FireGameEvent( IGameEvent *event )
	{
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( !pPlayer || event->GetInt( "userid" ) != pPlayer->GetUserID() )
			return;

		// The networked best lap may arrive before or after the event, so
		// compare with the best this element has seen itself.
		float flLapTime = event->GetFloat( "laptime" );
		if ( m_flBestSeen <= 0.0f || flLapTime < m_flBestSeen )
		{
			m_flBestSeen = flLapTime;
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "KartTimerBestLap" );
		}
	}

	virtual void OnThink( void )
	{
		// A new race: its first lap is a new best again.
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( pPlayer && pPlayer->GetKartLap() == 0 )
		{
			m_flBestSeen = 0.0f;
		}
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hFont = GetKartFont( pScheme, "KartHudSmall" );
		m_LabelColor = GetKartColor( pScheme, "KartAmberDim" );
		m_TimeColor = GetKartColor( pScheme, "KartWhite" );
		m_FinalColor = GetKartColor( pScheme, "KartAmber" );
	}

	virtual void Paint( void )
	{
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( !pPlayer )
			return;

		bool bFinished = pPlayer->IsKartFinished();
		float flLap = bFinished ? 0.0f : MAX( 0.0f, gpGlobals->curtime - pPlayer->GetKartLapStartTime() );
		float flTotal = pPlayer->GetKartTotalTime() + flLap;
		float flBest = pPlayer->GetKartBestLap();

		int nRowTall = surface()->GetFontTall( m_hFont );
		int y = 0;

		PaintRow( y, L"TOTAL", flTotal, bFinished ? m_FinalColor : m_TimeColor );
		y += nRowTall;

		if ( !bFinished )
		{
			PaintRow( y, L"LAP", flLap, m_TimeColor );
			y += nRowTall;
		}

		// The flash: the row lerps to amber and back with BestFlash.
		Color bestColor = m_TimeColor;
		float flFlash = clamp( m_flBestFlash, 0.0f, 1.0f );
		for ( int i = 0; i < 4; i++ )
		{
			bestColor[i] = Lerp( flFlash, m_TimeColor[i], m_FinalColor[i] );
		}
		PaintRow( y, L"BEST", flBest, bestColor );
	}

private:
	// "LABEL  mm:ss.mmm", right-aligned on the panel; times of 0 or less are
	// drawn as "--:--.---".
	void PaintRow( int y, const wchar_t *pszLabel, float flTime, Color timeColor )
	{
		wchar_t wszTime[32];
		FormatTime( flTime, wszTime, ARRAYSIZE( wszTime ) );

		int nTimeWide, nTimeTall, nLabelWide, nLabelTall;
		surface()->GetTextSize( m_hFont, wszTime, nTimeWide, nTimeTall );
		surface()->GetTextSize( m_hFont, pszLabel, nLabelWide, nLabelTall );

		// The labels end where a typical time starts, so they line up.
		int nColumnWide, nColumnTall;
		surface()->GetTextSize( m_hFont, L"00:00.000", nColumnWide, nColumnTall );

		int nTimeX = GetWide() - nTimeWide;
		int nLabelX = GetWide() - nColumnWide - m_nLabelGap - nLabelWide;

		KartHud_DrawText( m_hFont, m_LabelColor, nLabelX, y, pszLabel );
		KartHud_DrawText( m_hFont, timeColor, nTimeX, y, wszTime );
	}

	static void FormatTime( float flTime, wchar_t *pwszOut, int nOutChars )
	{
		if ( flTime <= 0.0f )
		{
			V_wcsncpy( pwszOut, L"--:--.---", nOutChars * sizeof( wchar_t ) );
			return;
		}

		int nMillis = (int)( flTime * 1000.0f + 0.5f );
		V_snwprintf( pwszOut, nOutChars, L"%02d:%02d.%03d", nMillis / 60000, ( nMillis / 1000 ) % 60, nMillis % 1000 );
	}

	// Animated by KartTimerBestLap: 1 is the BEST row in amber.
	CPanelAnimationVar( float, m_flBestFlash, "BestFlash", "0" );

	// Gap between the labels and the times.
	CPanelAnimationVarAliasType( int, m_nLabelGap, "label_gap", "4", "proportional_int" );

	// Fastest lap seen in kart_lap events this race, 0 before the first.
	float	m_flBestSeen;

	HFont	m_hFont;
	Color	m_LabelColor;
	Color	m_TimeColor;
	Color	m_FinalColor;
};

DECLARE_HUDELEMENT( CKartTimer );
