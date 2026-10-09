//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart race HUD: the held item slot, top center. A framed box with
//			the held item's icon, the icons cycling while the roulette spins
//			(kart_items.h), the uses left when there are several, and a use
//			hint under it. Icons are the kart_item_<name> entries of
//			scripts/mod_textures.txt.
//
//=============================================================================//

#include "cbase.h"
#include "kart_hud_base.h"
#include "kart_items.h"
#include "c_hl2mp_player.h"
#include "iclientmode.h"
#include "engine/IEngineSound.h"
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/AnimationController.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: The held item. Empty-handed it shows a dim empty-slot glyph. Each
//			icon the spinning roulette shows ticks (Kart.RouletteTick), and the
//			box flashes when it lands (KartItemSettle, HudAnimations.txt).
//-----------------------------------------------------------------------------
class CKartItemSlot : public CKartHudElement
{
	DECLARE_CLASS_SIMPLE( CKartItemSlot, CKartHudElement );

public:
	CKartItemSlot( const char *pElementName ) : BaseClass( pElementName, "KartItemSlot" )
	{
		for ( int i = 0; i < KART_ITEM_COUNT; i++ )
		{
			m_pIcons[i] = NULL;
		}
		Reset();
	}

	virtual void Reset( void )
	{
		m_nRouletteShown = KART_ITEM_NONE;
		m_nSettledItem = KART_ITEM_NONE;
		m_flFlash = 0.0f;
	}

	virtual void VidInit( void )
	{
		char szIcon[64];
		for ( int i = 0; i < KART_ITEM_COUNT; i++ )
		{
			V_snprintf( szIcon, sizeof( szIcon ), "kart_item_%s", KartItem_GetName( i ) );
			m_pIcons[i] = gHUD.GetIcon( szIcon );
		}
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hCountFont = GetKartFont( pScheme, "KartHudMedium" );
		m_hHintFont = GetKartFont( pScheme, "KartHudSmall" );
		m_BgColor = GetKartColor( pScheme, "KartPanelBg", Color( 0, 0, 0, 110 ) );
		m_FrameColor = GetKartColor( pScheme, "KartAmberDim" );
		m_IconColor = GetKartColor( pScheme, "KartAmber" );
		m_FlashColor = GetKartColor( pScheme, "KartWhite" );
		m_DimColor = GetKartColor( pScheme, "KartWhiteDim" );
	}

	virtual void OnThink( void )
	{
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( !pPlayer )
			return;

		if ( pPlayer->IsKartRouletteSpinning() )
		{
			// One tick per icon the server steps the roulette to.
			int nRoulette = pPlayer->GetKartRouletteItem();
			if ( nRoulette != m_nRouletteShown && KartItem_IsValid( nRoulette ) )
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Kart.RouletteTick" );
			}
			m_nRouletteShown = nRoulette;
			m_nSettledItem = KART_ITEM_NONE;
			return;
		}

		m_nRouletteShown = KART_ITEM_NONE;

		// Landed, or given without a roulette.
		int nItem = pPlayer->GetKartItem();
		if ( nItem != m_nSettledItem )
		{
			if ( KartItem_IsValid( nItem ) )
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "KartItemSettle" );
			}
			m_nSettledItem = nItem;
		}
	}

	virtual void Paint( void )
	{
		C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
		if ( !pPlayer )
			return;

		bool bSpinning = pPlayer->IsKartRouletteSpinning();
		int nItem = bSpinning ? pPlayer->GetKartRouletteItem() : pPlayer->GetKartItem();
		if ( nItem < KART_ITEM_NONE || nItem >= KART_ITEM_COUNT )
		{
			nItem = KART_ITEM_NONE;
		}

		int nBox = MIN( m_nBoxSize, MIN( GetWide(), GetTall() ) );
		int xBox = ( GetWide() - nBox ) / 2;

		// Box and a two pixel frame, white at the start of the landing flash.
		float flFlash = clamp( m_flFlash, 0.0f, 1.0f );
		surface()->DrawSetColor( m_BgColor );
		surface()->DrawFilledRect( xBox, 0, xBox + nBox, nBox );
		surface()->DrawSetColor( LerpColor( flFlash, m_FrameColor, m_FlashColor ) );
		surface()->DrawOutlinedRect( xBox, 0, xBox + nBox, nBox );
		surface()->DrawOutlinedRect( xBox + 1, 1, xBox + nBox - 1, nBox - 1 );

		CHudTexture *pIcon = m_pIcons[nItem];
		if ( pIcon )
		{
			Color iconColor = m_DimColor;
			if ( nItem != KART_ITEM_NONE )
			{
				iconColor = bSpinning ? m_FlashColor : LerpColor( flFlash, m_IconColor, m_FlashColor );
			}
			int nIcon = nBox - 2 * m_nIconInset;
			pIcon->DrawSelf( xBox + m_nIconInset, m_nIconInset, nIcon, nIcon, iconColor );
		}

		if ( bSpinning || nItem == KART_ITEM_NONE )
			return;

		// Uses left, bottom right inside the box, when there are several.
		int nCount = pPlayer->GetKartItemCount();
		if ( nCount > 1 )
		{
			wchar_t wszCount[8];
			V_snwprintf( wszCount, ARRAYSIZE( wszCount ), L"x%d", nCount );

			int wide, tall;
			surface()->GetTextSize( m_hCountFont, wszCount, wide, tall );
			surface()->DrawSetTextFont( m_hCountFont );
			surface()->DrawSetTextColor( m_FlashColor );
			surface()->DrawSetTextPos( xBox + nBox - wide - 4, nBox - tall - 2 );
			surface()->DrawPrintText( wszCount, wcslen( wszCount ) );
		}

		// Use hint under the box: the key bound to attack.
		const char *pszKey = engine->Key_LookupBinding( "+attack" );
		wchar_t wszHint[64];
		if ( pszKey && *pszKey )
		{
			wchar_t wszKey[32];
			g_pVGuiLocalize->ConvertANSIToUnicode( pszKey, wszKey, sizeof( wszKey ) );
			V_snwprintf( wszHint, ARRAYSIZE( wszHint ), L"%ls  USE", wszKey );
		}
		else
		{
			V_wcsncpy( wszHint, L"ATTACK  USE", sizeof( wszHint ) );
		}
		for ( wchar_t *pch = wszHint; *pch; pch++ )
		{
			*pch = towupper( *pch );
		}

		int wide, tall;
		surface()->GetTextSize( m_hHintFont, wszHint, wide, tall );
		surface()->DrawSetTextFont( m_hHintFont );
		surface()->DrawSetTextColor( m_DimColor );
		surface()->DrawSetTextPos( ( GetWide() - wide ) / 2, nBox + 2 );
		surface()->DrawPrintText( wszHint, wcslen( wszHint ) );
	}

private:
	static Color LerpColor( float t, const Color &a, const Color &b )
	{
		return Color( (int)( a.r() + ( b.r() - a.r() ) * t ), (int)( a.g() + ( b.g() - a.g() ) * t ),
			(int)( a.b() + ( b.b() - a.b() ) * t ), (int)( a.a() + ( b.a() - a.a() ) * t ) );
	}

	// HudLayout.res keys.
	CPanelAnimationVarAliasType( int, m_nBoxSize, "box_size", "72", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nIconInset, "icon_inset", "8", "proportional_int" );

	// Animated by KartItemSettle: 1 at the landing, fading to 0.
	CPanelAnimationVar( float, m_flFlash, "Flash", "0" );

	CHudTexture	*m_pIcons[KART_ITEM_COUNT];

	int		m_nRouletteShown;	// roulette item last ticked for
	int		m_nSettledItem;		// held item last flashed for

	HFont	m_hCountFont;
	HFont	m_hHintFont;
	Color	m_BgColor;
	Color	m_FrameColor;
	Color	m_IconColor;
	Color	m_FlashColor;
	Color	m_DimColor;
};

DECLARE_HUDELEMENT( CKartItemSlot );
