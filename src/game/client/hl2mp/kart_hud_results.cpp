//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart race results panel (resource/ui/KartResults.res): every
//			racer in finish order with their best lap and total time, the
//			karts still racing listed under them by race position. Updates as
//			others finish.
//
//			Opens when the local player finishes (kart_race_finish, DNF too)
//			and when the race enters RESULTS. Closes when the next race starts
//			(kart_race_start), when the local player changes team, respawns
//			(the grid respawn of the next race too) or the map changes.
//			kart_results toggles it, for testing.
//
//			Finish order, times and best laps come from the race events rather
//			than the networked race state, which goes stale for karts outside
//			the PVS. The networked state only orders the karts still racing.
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "c_hl2mp_player.h"
#include "c_playerresource.h"
#include "hl2mp_gamerules.h"
#include "kart_hud_base.h"
#include "kart_race_shared.h"
#include "iclientmode.h"
#include <vgui/IScheme.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/SectionedListPanel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// How often the list is rebuilt while open, seconds, to follow the karts still racing.
#define KART_RESULTS_REFRESH	0.5f

enum
{
	KART_RESULTS_SECTION_FINISHED = 0,
	KART_RESULTS_SECTION_RACING,
};

// m:ss.mmm, the race timer's format.
static void KartResults_FormatTime( float flTime, char *pszOut, int nOutChars )
{
	if ( flTime <= 0.0f )
	{
		V_strncpy( pszOut, "--", nOutChars );
		return;
	}

	int nMillis = (int)( flTime * 1000.0f + 0.5f );
	V_snprintf( pszOut, nOutChars, "%02d:%02d.%03d", nMillis / 60000, ( nMillis / 1000 ) % 60, nMillis % 1000 );
}

static const char *KartResults_Ordinal( int n )
{
	switch ( KartHud_OrdinalSuffix( n )[0] )
	{
	case L's':	return "st";
	case L'n':	return "nd";
	case L'r':	return "rd";
	default:	return "th";
	}
}

//-----------------------------------------------------------------------------
// Purpose: The results panel. A HUD element so it sees the race events and
//			the HUD's reset and level init, an EditablePanel for its layout.
//-----------------------------------------------------------------------------
class CKartResults : public CHudElement, public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CKartResults, EditablePanel );

public:
	CKartResults( const char *pElementName );

	virtual void	Init( void );
	virtual void	Reset( void );
	virtual void	LevelInit( void );
	virtual bool	ShouldDraw( void );
	virtual void	FireGameEvent( IGameEvent *event );
	virtual void	ApplySchemeSettings( IScheme *pScheme );
	virtual void	OnThink( void );

	void			SetOpen( bool bOpen );
	void			Toggle( void ) { SetOpen( !m_bOpen ); }

private:
	// One racer seen in this race's events.
	struct Racer_t
	{
		int		userid;
		char	szName[MAX_PLAYER_NAME_LENGTH];
		float	flBestLap;		// 0 before the first lap
		int		nPosition;		// finishing position, 0 while racing
		float	flTotalTime;
		bool	bDNF;
	};

	Racer_t		*FindRacer( int userid, bool bCreate );
	void		ClearRace( void );
	void		RebuildList( void );
	void		AddSections( void );

	static int	FinishedSortFunc( Racer_t * const *a, Racer_t * const *b );
	static int	RacingSortFunc( C_HL2MP_Player * const *a, C_HL2MP_Player * const *b );

	SectionedListPanel		*m_pList;
	CUtlVector< Racer_t >	m_Racers;

	bool		m_bOpen;
	int			m_nLastRaceState;
	float		m_flNextRefresh;

	HFont		m_hRowFont;
	Color		m_LocalColor;
	Color		m_RowColor;
	Color		m_DimColor;
};

DECLARE_HUDELEMENT( CKartResults );

CKartResults::CKartResults( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "KartResults" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetProportional( true );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	m_pList = new SectionedListPanel( this, "ResultsList" );
	m_pList->SetMouseInputEnabled( false );
	m_pList->SetVerticalScrollbar( false );

	m_bOpen = false;
	m_nLastRaceState = KART_RACE_STATE_NONE;
	m_flNextRefresh = 0.0f;
	m_hRowFont = INVALID_FONT;
}

void CKartResults::Init( void )
{
	ListenForGameEvent( KART_EVENT_LAP );
	ListenForGameEvent( KART_EVENT_RACE_FINISH );
	ListenForGameEvent( KART_EVENT_RACE_START );
	ListenForGameEvent( "player_team" );
}

//-----------------------------------------------------------------------------
// Purpose: ResetHUD, sent when the local player spawns: close.
//-----------------------------------------------------------------------------
void CKartResults::Reset( void )
{
	SetOpen( false );
}

void CKartResults::LevelInit( void )
{
	SetOpen( false );
	ClearRace();
	m_nLastRaceState = KART_RACE_STATE_NONE;
}

void CKartResults::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/ui/KartResults.res" );

	m_hRowFont = pScheme->GetFont( "KartHudSmall", IsProportional() );
	m_LocalColor = pScheme->GetColor( "KartAmber", Color( 255, 176, 0, 255 ) );
	m_RowColor = pScheme->GetColor( "KartWhite", Color( 255, 255, 255, 255 ) );
	m_DimColor = pScheme->GetColor( "KartWhiteDim", Color( 255, 255, 255, 140 ) );

	SetBgColor( pScheme->GetColor( "KartPanelBg", Color( 0, 0, 0, 110 ) ) );
	m_pList->SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pList->SetBorder( NULL );

	// The columns scale with the font, so they are made here.
	m_pList->DeleteAllItems();
	m_pList->RemoveAllSections();
	AddSections();
	RebuildList();
}

void CKartResults::AddSections( void )
{
	int nPos = scheme()->GetProportionalScaledValueEx( GetScheme(), 40 );
	int nTime = scheme()->GetProportionalScaledValueEx( GetScheme(), 72 );
	int nName = MAX( nTime, m_pList->GetWide() - nPos - 2 * nTime - scheme()->GetProportionalScaledValueEx( GetScheme(), 8 ) );

	static const int s_nSections[] = { KART_RESULTS_SECTION_FINISHED, KART_RESULTS_SECTION_RACING };
	for ( int i = 0; i < ARRAYSIZE( s_nSections ); i++ )
	{
		int nSection = s_nSections[i];
		bool bFinished = ( nSection == KART_RESULTS_SECTION_FINISHED );

		// Items keep the order they are added in: RebuildList sorts them.
		m_pList->AddSection( nSection, "" );
		m_pList->SetSectionAlwaysVisible( nSection, bFinished );
		m_pList->SetSectionFgColor( nSection, m_LocalColor );
		m_pList->AddColumnToSection( nSection, "pos", bFinished ? "POS" : "", 0, nPos, m_hRowFont );
		m_pList->AddColumnToSection( nSection, "name", bFinished ? "FINISHED" : "STILL RACING", 0, nName, m_hRowFont );
		m_pList->AddColumnToSection( nSection, "best", bFinished ? "BEST LAP" : "", SectionedListPanel::COLUMN_RIGHT, nTime, m_hRowFont );
		m_pList->AddColumnToSection( nSection, "total", bFinished ? "TOTAL" : "", SectionedListPanel::COLUMN_RIGHT, nTime, m_hRowFont );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Open while asked to, in kart mode with kart_hud on. Also opens it
//			when the race enters RESULTS: HUD elements only think while
//			visible, and this is checked every frame.
//-----------------------------------------------------------------------------
bool CKartResults::ShouldDraw( void )
{
	int nState = HL2MPRules() ? HL2MPRules()->GetKartRaceState() : KART_RACE_STATE_NONE;
	if ( nState != m_nLastRaceState )
	{
		if ( nState == KART_RACE_STATE_RESULTS )
		{
			SetOpen( true );
		}
		m_nLastRaceState = nState;
	}

	if ( !m_bOpen || !kart_hud.GetBool() || !KartHud_LocalPlayerInKart() )
		return false;

	return CHudElement::ShouldDraw();
}

void CKartResults::SetOpen( bool bOpen )
{
	if ( bOpen && !m_bOpen )
	{
		RebuildList();
	}
	m_bOpen = bOpen;
}

void CKartResults::OnThink( void )
{
	BaseClass::OnThink();

	if ( gpGlobals->curtime >= m_flNextRefresh )
	{
		RebuildList();
	}
}

void CKartResults::FireGameEvent( IGameEvent *event )
{
	const char *pszName = event->GetName();
	C_HL2MP_Player *pLocal = C_HL2MP_Player::GetLocalHL2MPPlayer();
	int nLocalUserID = pLocal ? pLocal->GetUserID() : -1;

	if ( FStrEq( pszName, KART_EVENT_RACE_START ) )
	{
		ClearRace();
		SetOpen( false );
		return;
	}

	if ( FStrEq( pszName, "player_team" ) )
	{
		if ( event->GetInt( "userid" ) == nLocalUserID )
		{
			SetOpen( false );
		}
		return;
	}

	Racer_t *pRacer = FindRacer( event->GetInt( "userid" ), true );
	if ( !pRacer )
		return;

	if ( FStrEq( pszName, KART_EVENT_LAP ) )
	{
		float flLapTime = event->GetFloat( "laptime" );
		if ( flLapTime > 0.0f && ( pRacer->flBestLap <= 0.0f || flLapTime < pRacer->flBestLap ) )
		{
			pRacer->flBestLap = flLapTime;
		}
	}
	else if ( FStrEq( pszName, KART_EVENT_RACE_FINISH ) )
	{
		pRacer->nPosition = event->GetInt( "position" );
		pRacer->flTotalTime = event->GetFloat( "totaltime" );
		pRacer->bDNF = event->GetBool( "dnf" );

		if ( pRacer->userid == nLocalUserID )
		{
			SetOpen( true );
		}
	}

	if ( m_bOpen )
	{
		RebuildList();
	}
}

//-----------------------------------------------------------------------------
// Purpose: The racer with this userid, created (with the player's current
//			name, kept should they leave) when bCreate is set.
//-----------------------------------------------------------------------------
CKartResults::Racer_t *CKartResults::FindRacer( int userid, bool bCreate )
{
	FOR_EACH_VEC( m_Racers, i )
	{
		if ( m_Racers[i].userid == userid )
			return &m_Racers[i];
	}

	if ( !bCreate )
		return NULL;

	int iPlayer = engine->GetPlayerForUserID( userid );
	if ( iPlayer <= 0 )
		return NULL;

	Racer_t &racer = m_Racers[ m_Racers.AddToTail() ];
	racer.userid = userid;
	V_strncpy( racer.szName, g_PR ? g_PR->GetPlayerName( iPlayer ) : "?", sizeof( racer.szName ) );
	racer.flBestLap = 0.0f;
	racer.nPosition = 0;
	racer.flTotalTime = 0.0f;
	racer.bDNF = false;
	return &racer;
}

void CKartResults::ClearRace( void )
{
	m_Racers.RemoveAll();
	RebuildList();
}

int CKartResults::FinishedSortFunc( Racer_t * const *a, Racer_t * const *b )
{
	return (*a)->nPosition - (*b)->nPosition;
}

// By networked race position, the ones without one (just joined) last.
int CKartResults::RacingSortFunc( C_HL2MP_Player * const *a, C_HL2MP_Player * const *b )
{
	int nA = (*a)->GetKartRacePosition();
	int nB = (*b)->GetKartRacePosition();
	if ( nA <= 0 || nB <= 0 )
		return ( nA <= 0 ) - ( nB <= 0 );
	return nA - nB;
}

//-----------------------------------------------------------------------------
// Purpose: Refill the list: the finishers in finish order, then every other
//			kart in the game by race position.
//-----------------------------------------------------------------------------
void CKartResults::RebuildList( void )
{
	m_flNextRefresh = gpGlobals->curtime + KART_RESULTS_REFRESH;

	if ( !m_pList || m_hRowFont == INVALID_FONT )
		return;

	m_pList->DeleteAllItems();

	C_HL2MP_Player *pLocal = C_HL2MP_Player::GetLocalHL2MPPlayer();
	int nLocalUserID = pLocal ? pLocal->GetUserID() : -1;

	CUtlVectorFixedGrowable< Racer_t *, MAX_PLAYERS > finishers;
	FOR_EACH_VEC( m_Racers, i )
	{
		if ( m_Racers[i].nPosition > 0 )
		{
			finishers.AddToTail( &m_Racers[i] );
		}
	}
	finishers.Sort( FinishedSortFunc );

	char szPos[16], szBest[32], szTotal[32];
	KeyValues *pData = new KeyValues( "data" );

	FOR_EACH_VEC( finishers, i )
	{
		Racer_t *pRacer = finishers[i];
		V_snprintf( szPos, sizeof( szPos ), "%d%s", pRacer->nPosition, KartResults_Ordinal( pRacer->nPosition ) );
		KartResults_FormatTime( pRacer->flBestLap, szBest, sizeof( szBest ) );
		if ( pRacer->bDNF )
		{
			V_strncpy( szTotal, "DNF", sizeof( szTotal ) );
		}
		else
		{
			KartResults_FormatTime( pRacer->flTotalTime, szTotal, sizeof( szTotal ) );
		}

		pData->SetString( "pos", szPos );
		pData->SetString( "name", pRacer->szName );
		pData->SetString( "best", szBest );
		pData->SetString( "total", szTotal );

		int nItem = m_pList->AddItem( KART_RESULTS_SECTION_FINISHED, pData );
		m_pList->SetItemFont( nItem, m_hRowFont );
		m_pList->SetItemFgColor( nItem, pRacer->userid == nLocalUserID ? m_LocalColor : ( pRacer->bDNF ? m_DimColor : m_RowColor ) );
	}

	// Karts still racing: everyone in a kart this race hasn't seen finish.
	CUtlVectorFixedGrowable< C_HL2MP_Player *, MAX_PLAYERS > racing;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( !g_PR || !g_PR->IsConnected( i ) || g_PR->GetTeam( i ) == TEAM_SPECTATOR )
			continue;

		C_HL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsInKart() )
			continue;

		Racer_t *pRacer = FindRacer( pPlayer->GetUserID(), false );
		if ( pRacer && pRacer->nPosition > 0 )
			continue;

		racing.AddToTail( pPlayer );
	}
	racing.Sort( RacingSortFunc );

	FOR_EACH_VEC( racing, i )
	{
		C_HL2MP_Player *pPlayer = racing[i];
		Racer_t *pRacer = FindRacer( pPlayer->GetUserID(), false );

		int nPosition = pPlayer->GetKartRacePosition();
		if ( nPosition > 0 )
		{
			V_snprintf( szPos, sizeof( szPos ), "%d%s", nPosition, KartResults_Ordinal( nPosition ) );
		}
		else
		{
			V_strncpy( szPos, "--", sizeof( szPos ) );
		}
		KartResults_FormatTime( pRacer ? pRacer->flBestLap : 0.0f, szBest, sizeof( szBest ) );

		pData->SetString( "pos", szPos );
		pData->SetString( "name", g_PR->GetPlayerName( pPlayer->entindex() ) );
		pData->SetString( "best", szBest );
		pData->SetString( "total", "--" );

		int nItem = m_pList->AddItem( KART_RESULTS_SECTION_RACING, pData );
		m_pList->SetItemFont( nItem, m_hRowFont );
		m_pList->SetItemFgColor( nItem, pPlayer == pLocal ? m_LocalColor : m_DimColor );
	}

	pData->deleteThis();
}

CON_COMMAND( kart_results, "Toggle the kart race results panel." )
{
	CKartResults *pPanel = GET_HUDELEMENT( CKartResults );
	if ( pPanel )
	{
		pPanel->Toggle();
	}
}
