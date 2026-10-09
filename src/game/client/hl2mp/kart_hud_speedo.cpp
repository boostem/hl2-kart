//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart speedometer, bottom right: the speed as a friendly "km/h"
//			number (kart_hud_speed_scale) over a bar filled to kart_max_speed,
//			then the mini-turbo drift charge meter, filling through the three
//			tiers in the drift spark colors (KartTier1-3), and the boost bar,
//			draining as the current boost runs out.
//
//=============================================================================//

#include "cbase.h"
#include "kart_hud_base.h"
#include "c_hl2mp_player.h"
#include "kart_shareddefs.h"
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar kart_hud_speed_scale( "kart_hud_speed_scale", "0.2", FCVAR_ARCHIVE, "Kart HUD speedometer: displayed speed per unit per second (0.2 shows kart_max_speed 650 as 130)." );

class CKartSpeedo : public CKartHudElement
{
	DECLARE_CLASS_SIMPLE( CKartSpeedo, CKartHudElement );

public:
	CKartSpeedo( const char *pElementName ) : BaseClass( pElementName, "KartSpeedo" )
	{
		m_flBoostStartTime = 0.0f;
		m_flBoostEndTime = 0.0f;
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hNumberFont = GetKartFont( pScheme, "KartHudLarge" );
		m_hLabelFont = GetKartFont( pScheme, "KartHudSmall" );
		m_NumberColor = GetKartColor( pScheme, "KartWhite" );
		m_LabelColor = GetKartColor( pScheme, "KartAmberDim" );
		m_FillColor = GetKartColor( pScheme, "KartWhite" );
		m_OverdriveColor = GetKartColor( pScheme, "KartAmber" );
		m_BarBgColor = GetKartColor( pScheme, "KartPanelBg" );
		m_TickColor = GetKartColor( pScheme, "KartWhiteDim" );
		m_TierColors[0] = GetKartColor( pScheme, "KartWhiteDim" );
		m_TierColors[1] = GetKartColor( pScheme, "KartTier1", Color( 90, 180, 255, 255 ) );
		m_TierColors[2] = GetKartColor( pScheme, "KartTier2", Color( 255, 160, 40, 255 ) );
		m_TierColors[3] = GetKartColor( pScheme, "KartTier3", Color( 255, 60, 200, 255 ) );
	}

	virtual void Paint( void )
	{
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( !pPlayer )
			return;

		const int nWide = GetWide();

		// Speed, right-aligned, with its unit to the left: "KM/H 130".
		float flSpeed = fabs( pPlayer->GetKartSpeed() );
		wchar_t wszSpeed[16];
		V_snwprintf( wszSpeed, ARRAYSIZE( wszSpeed ), L"%d", (int)( flSpeed * kart_hud_speed_scale.GetFloat() + 0.5f ) );

		int nNumberWide, nNumberTall, nUnitWide, nUnitTall;
		surface()->GetTextSize( m_hNumberFont, wszSpeed, nNumberWide, nNumberTall );
		surface()->GetTextSize( m_hLabelFont, L"KM/H", nUnitWide, nUnitTall );

		DrawText( m_hNumberFont, m_NumberColor, nWide - nNumberWide, 0, wszSpeed );
		DrawText( m_hLabelFont, m_LabelColor, nWide - nNumberWide - m_nLabelGap - nUnitWide, nNumberTall - nUnitTall, L"KM/H" );

		int y = nNumberTall + m_nRowGap;

		// Speed bar, full at kart_max_speed; a boost's overdrive past it fills
		// the bar in amber.
		float flMaxSpeed = MAX( kart_max_speed.GetFloat(), 1.0f );
		bool bOverdrive = flSpeed > flMaxSpeed + 1.0f;
		DrawBar( 0, y, nWide, flSpeed / flMaxSpeed, bOverdrive ? m_OverdriveColor : m_FillColor );
		y += m_nBarTall + m_nRowGap;

		// Drift charge, full at tier 3, marked at tiers 1 and 2, in the color
		// of the tier reached.
		int nLabelTall = surface()->GetFontTall( m_hLabelFont );
		int nBarX = m_nLabelWide;
		int nBarWide = nWide - m_nLabelWide;
		int nBarOffset = ( nLabelTall - m_nBarTall ) / 2;

		float flTier3 = MAX( kart_turbo_tier3_time.GetFloat(), 0.01f );
		int nTier = clamp( pPlayer->GetKartDriftTier(), 0, 3 );
		DrawText( m_hLabelFont, nTier > 0 ? m_TierColors[nTier] : m_LabelColor, 0, y, L"DRIFT" );
		DrawBar( nBarX, y + nBarOffset, nBarWide, pPlayer->GetKartDriftCharge() / flTier3, m_TierColors[nTier] );
		DrawTick( nBarX, y + nBarOffset, nBarWide, kart_turbo_tier1_time.GetFloat() / flTier3 );
		DrawTick( nBarX, y + nBarOffset, nBarWide, kart_turbo_tier2_time.GetFloat() / flTier3 );
		y += nLabelTall + m_nRowGap;

		// Boost left, as a share of the whole boost.
		float flBoostFrac = UpdateBoost( pPlayer );
		DrawText( m_hLabelFont, flBoostFrac > 0.0f ? m_OverdriveColor : m_LabelColor, 0, y, L"BOOST" );
		DrawBar( nBarX, y + nBarOffset, nBarWide, flBoostFrac, m_OverdriveColor );
	}

private:
	// Fraction of the current boost left, 0 when not boosting. The boost's
	// start isn't networked, so it is the time its end was last pushed out:
	// a new or stacked boost refills the bar.
	float UpdateBoost( C_HL2MP_Player *pPlayer )
	{
		float flEndTime = pPlayer->GetKartBoostEndTime();
		if ( !pPlayer->IsKartBoosting() )
		{
			m_flBoostEndTime = 0.0f;
			return 0.0f;
		}

		if ( flEndTime > m_flBoostEndTime + 0.01f )
		{
			m_flBoostStartTime = gpGlobals->curtime;
		}
		m_flBoostEndTime = flEndTime;

		float flDuration = flEndTime - m_flBoostStartTime;
		if ( flDuration <= 0.0f )
			return 0.0f;

		return clamp( ( flEndTime - gpGlobals->curtime ) / flDuration, 0.0f, 1.0f );
	}

	void DrawText( HFont hFont, Color col, int x, int y, const wchar_t *pszText )
	{
		surface()->DrawSetTextFont( hFont );
		surface()->DrawSetTextColor( col );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( pszText, wcslen( pszText ) );
	}

	// A bar_tall bar at x, y, filled to flFrac (0-1) from the left.
	void DrawBar( int x, int y, int nWide, float flFrac, Color fillColor )
	{
		surface()->DrawSetColor( m_BarBgColor );
		surface()->DrawFilledRect( x, y, x + nWide, y + m_nBarTall );

		int nFill = (int)( nWide * clamp( flFrac, 0.0f, 1.0f ) + 0.5f );
		if ( nFill > 0 )
		{
			surface()->DrawSetColor( fillColor );
			surface()->DrawFilledRect( x, y, x + nFill, y + m_nBarTall );
		}
	}

	// A one pixel mark across the bar at x, y, flFrac of the way along it.
	void DrawTick( int x, int y, int nWide, float flFrac )
	{
		if ( flFrac <= 0.0f || flFrac >= 1.0f )
			return;

		int nTickX = x + (int)( nWide * flFrac );
		surface()->DrawSetColor( m_TickColor );
		surface()->DrawFilledRect( nTickX, y - 1, nTickX + 1, y + m_nBarTall + 1 );
	}

	CPanelAnimationVarAliasType( int, m_nBarTall, "bar_tall", "6", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nRowGap, "row_gap", "4", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nLabelWide, "label_wide", "44", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nLabelGap, "label_gap", "4", "proportional_int" );

	float	m_flBoostStartTime;
	float	m_flBoostEndTime;

	HFont	m_hNumberFont;
	HFont	m_hLabelFont;
	Color	m_NumberColor;
	Color	m_LabelColor;
	Color	m_FillColor;
	Color	m_OverdriveColor;
	Color	m_BarBgColor;
	Color	m_TickColor;
	Color	m_TierColors[4];	// by drift tier, 0 for none
};

DECLARE_HUDELEMENT( CKartSpeedo );
