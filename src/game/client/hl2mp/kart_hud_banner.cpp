//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart race HUD messages, centered: the wrong-way warning (from the
//			local player's networked m_bKartWrongWay) and the banner that
//			announces "LAP 2", "FINAL LAP", "FINISH" and the finishing place
//			(kart_lap and kart_race_finish events). Both are animated by
//			HudAnimations.txt: KartWrongWayFlash/Hide, KartBannerShow/Hold.
//
//=============================================================================//

#include "cbase.h"
#include "kart_hud_base.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "kart_race_shared.h"
#include "iclientmode.h"
#include "engine/IEngineSound.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/AnimationController.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// Game sounds (game_sounds_kart.txt), played by the client to the local player only.
#define KART_SOUND_WRONG_WAY		"Kart.WrongWay"
#define KART_SOUND_FINAL_LAP		"Kart.FinalLap"
#define KART_SOUND_FINISH			"Kart.Finish"

static void KartHud_PlayLocalSound( const char *pszSound )
{
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSound );
}

// Draws pszText centered on the panel x, at y.
static void KartHud_DrawCenteredText( HFont hFont, Color col, int nPanelWide, int y, const wchar_t *pszText )
{
	int wide, tall;
	surface()->GetTextSize( hFont, pszText, wide, tall );

	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( ( nPanelWide - wide ) / 2, y );
	surface()->DrawPrintText( pszText, wcslen( pszText ) );
}

//-----------------------------------------------------------------------------
// Purpose: "WRONG WAY" between two U-turn arrows, flashing red, center of
//			the screen. Shown while the server says the player drives against
//			the track (CKartRaceManager::UpdateWrongWay).
//-----------------------------------------------------------------------------
class CKartWrongWay : public CKartHudElement
{
	DECLARE_CLASS_SIMPLE( CKartWrongWay, CKartHudElement );

public:
	CKartWrongWay( const char *pElementName ) : BaseClass( pElementName, "KartWrongWay" )
	{
		m_bShown = false;
	}

	virtual void Reset( void )
	{
		SetShown( false );
	}

	virtual bool ShouldDraw( void )
	{
		bool bDraw = BaseClass::ShouldDraw();
		if ( bDraw )
		{
			C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
			bDraw = pPlayer->IsKartWrongWay() && !pPlayer->IsKartFinished();
		}

		SetShown( bDraw );
		return bDraw;
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hFont = GetKartFont( pScheme, "KartHudMedium" );
		m_Color = GetKartColor( pScheme, "KartRed" );
	}

	virtual void Paint( void )
	{
		Color col = m_Color;
		col[3] *= clamp( m_flAlpha, 0.0f, 1.0f );

		int nTextWide, nTextTall;
		surface()->GetTextSize( m_hFont, L"WRONG WAY", nTextWide, nTextTall );
		KartHud_DrawCenteredText( m_hFont, col, GetWide(), ( GetTall() - nTextTall ) / 2, L"WRONG WAY" );

		// An arrow on each side of the text, the right one mirrored.
		int nArrow = GetTall();
		int nGap = nArrow / 4;
		int xText = ( GetWide() - nTextWide ) / 2;
		PaintUTurn( col, xText - nGap - nArrow, nArrow, false );
		PaintUTurn( col, xText + nTextWide + nGap, nArrow, true );
	}

private:
	// Starts the flash on the way in and stops it on the way out.
	void SetShown( bool bShown )
	{
		if ( bShown == m_bShown )
			return;

		m_bShown = bShown;
		if ( bShown )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "KartWrongWayFlash" );
			KartHud_PlayLocalSound( KART_SOUND_WRONG_WAY );
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "KartWrongWayHide" );
		}
	}

	// A U-turn arrow in an nSize square at x: up one side, over the top and
	// down the other to a head pointing down, the way to turn around.
	void PaintUTurn( Color col, int x, int nSize, bool bMirror )
	{
		int nStroke = MAX( 2, nSize / 8 );
		int nHead = nSize / 3;
		int xIn = x + nHead / 2;					// the shaft the kart drives up
		int xOut = x + nSize - nHead / 2 - nStroke;	// the shaft it comes back down
		if ( bMirror )
		{
			V_swap( xIn, xOut );
		}

		int yTop = nSize / 6;
		int yBottom = nSize - nSize / 6;
		int yHead = yBottom - nHead / 2;

		surface()->DrawSetColor( col );
		surface()->DrawFilledRect( xIn, yTop, xIn + nStroke, yBottom );
		surface()->DrawFilledRect( MIN( xIn, xOut ), yTop, MAX( xIn, xOut ) + nStroke, yTop + nStroke );
		surface()->DrawFilledRect( xOut, yTop, xOut + nStroke, yHead );

		// The head: rows narrowing to the tip.
		int xCenter = xOut + nStroke / 2;
		int nRows = yBottom - yHead;
		for ( int i = 0; i < nRows; i++ )
		{
			int nHalf = ( nHead / 2 ) * ( nRows - i ) / nRows;
			surface()->DrawFilledRect( xCenter - nHalf, yHead + i, xCenter + nHalf + 1, yHead + i + 1 );
		}
	}

	// Animated by KartWrongWayFlash: 1 is fully drawn.
	CPanelAnimationVar( float, m_flAlpha, "Alpha", "0" );

	bool	m_bShown;

	HFont	m_hFont;
	Color	m_Color;
};

DECLARE_HUDELEMENT( CKartWrongWay );

//-----------------------------------------------------------------------------
// Purpose: One centered line that drops in and fades out, upper center:
//			"LAP 3" and "FINAL LAP" when a lap is completed, "FINISH" over the
//			line, then the place ("2ND PLACE") which stays until the next race.
//-----------------------------------------------------------------------------
// How long "FINISH" shows before the place replaces it.
#define KART_BANNER_PLACE_DELAY		2.0f

class CKartBanner : public CKartHudElement
{
	DECLARE_CLASS_SIMPLE( CKartBanner, CKartHudElement );

public:
	CKartBanner( const char *pElementName ) : BaseClass( pElementName, "KartBanner" )
	{
		Clear();
	}

	virtual void Init( void )
	{
		ListenForGameEvent( KART_EVENT_LAP );
		ListenForGameEvent( KART_EVENT_RACE_FINISH );
	}

	virtual void Reset( void )
	{
		Clear();
	}

	virtual bool ShouldDraw( void )
	{
		return m_wszText[0] && BaseClass::ShouldDraw();
	}

	virtual void FireGameEvent( IGameEvent *event )
	{
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( !pPlayer || event->GetInt( "userid" ) != pPlayer->GetUserID() )
			return;

		if ( !V_strcmp( event->GetName(), KART_EVENT_LAP ) )
		{
			int nNext = event->GetInt( "lap" ) + 1;
			int nLaps = HL2MPRules() ? HL2MPRules()->GetKartLaps() : 0;

			// Completing the last lap is the finish, which kart_race_finish shows.
			if ( nLaps > 0 && nNext > nLaps )
				return;

			if ( nNext == nLaps )
			{
				Show( L"FINAL LAP", "KartBannerShow" );
				KartHud_PlayLocalSound( KART_SOUND_FINAL_LAP );
			}
			else
			{
				wchar_t wszText[32];
				V_snwprintf( wszText, ARRAYSIZE( wszText ), L"LAP %d", nNext );
				Show( wszText, "KartBannerShow" );
			}
			return;
		}

		// kart_race_finish
		if ( event->GetBool( "dnf" ) )
		{
			Show( L"DID NOT FINISH", "KartBannerHold" );
			return;
		}

		Show( L"FINISH", "KartBannerShow" );
		KartHud_PlayLocalSound( KART_SOUND_FINISH );

		// The HL2MP face is drawn upper case.
		int nPosition = event->GetInt( "position" );
		wchar_t wszSuffix[4];
		V_wcsncpy( wszSuffix, KartHud_OrdinalSuffix( nPosition ), sizeof( wszSuffix ) );
		for ( wchar_t *pch = wszSuffix; *pch; pch++ )
		{
			*pch = towupper( *pch );
		}
		V_snwprintf( m_wszNext, ARRAYSIZE( m_wszNext ), L"%d%ls PLACE", nPosition, wszSuffix );
		m_flNextTime = gpGlobals->curtime + KART_BANNER_PLACE_DELAY;
	}

	virtual void OnThink( void )
	{
		// A new race clears what is left of the last one.
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( pPlayer && pPlayer->GetKartLap() == 0 && !pPlayer->IsKartFinished() )
		{
			if ( m_wszText[0] || m_wszNext[0] )
			{
				Clear();
			}
			return;
		}

		if ( m_wszNext[0] && gpGlobals->curtime >= m_flNextTime )
		{
			Show( m_wszNext, "KartBannerHold" );
			m_wszNext[0] = 0;
		}
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hFont = GetKartFont( pScheme, "KartHudMedium" );
		m_Color = GetKartColor( pScheme, "KartAmber" );
	}

	virtual void Paint( void )
	{
		Color col = m_Color;
		col[3] *= clamp( m_flAlpha, 0.0f, 1.0f );

		int y = ( GetTall() - surface()->GetFontTall( m_hFont ) ) / 2 + scheme()->GetProportionalScaledValueEx( GetScheme(), (int)m_flOffset );
		KartHud_DrawCenteredText( m_hFont, col, GetWide(), y, m_wszText );
	}

private:
	// pszSequence: KartBannerShow fades the text out again, KartBannerHold keeps it.
	void Show( const wchar_t *pszText, const char *pszSequence )
	{
		V_wcsncpy( m_wszText, pszText, sizeof( m_wszText ) );
		m_wszNext[0] = 0;

		AnimationController *pAnim = g_pClientMode->GetViewportAnimationController();
		pAnim->StopAnimationSequence( g_pClientMode->GetViewport(), "KartBannerShow" );
		pAnim->StopAnimationSequence( g_pClientMode->GetViewport(), "KartBannerHold" );
		pAnim->StartAnimationSequence( pszSequence );
	}

	void Clear( void )
	{
		m_wszText[0] = 0;
		m_wszNext[0] = 0;
		m_flNextTime = 0.0f;
	}

	// Animated by KartBannerShow and KartBannerHold.
	CPanelAnimationVar( float, m_flAlpha, "Alpha", "0" );
	// Offset is in 480-line units, scaled like the layout.
	CPanelAnimationVar( float, m_flOffset, "Offset", "0" );

	wchar_t	m_wszText[32];
	wchar_t	m_wszNext[32];	// shown at m_flNextTime: the place after "FINISH"
	float	m_flNextTime;

	HFont	m_hFont;
	Color	m_Color;
};

DECLARE_HUDELEMENT( CKartBanner );
