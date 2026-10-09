//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CHL2MPCLIENTSCOREBOARDDIALOG_H
#define CHL2MPCLIENTSCOREBOARDDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <clientscoreboarddialog.h>

//-----------------------------------------------------------------------------
// Purpose: Game ScoreBoard
//-----------------------------------------------------------------------------
class CHL2MPClientScoreBoardDialog : public CClientScoreBoardDialog
{
private:
	DECLARE_CLASS_SIMPLE(CHL2MPClientScoreBoardDialog, CClientScoreBoardDialog);
	
public:
	CHL2MPClientScoreBoardDialog(IViewPort *pViewPort);
	~CHL2MPClientScoreBoardDialog();


protected:
	// scoreboard overrides
	virtual void InitScoreboardSections();
	virtual void UpdateTeamInfo();
	virtual bool GetPlayerScoreInfo(int playerIndex, KeyValues *outPlayerInfo);
	virtual void UpdatePlayerInfo();

	// vgui overrides for rounded corner background
	virtual void PaintBackground();
	virtual void PaintBorder();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	virtual void AddHeader(); // add the start header of the scoreboard
	virtual void AddSection(int teamType, int teamNumber); // add a new section header for a team

	int GetSectionFromTeamNumber( int teamNumber );

	// Kart race layout (a race on the map): race standings, no teams.
	static bool IsKartRace( void );
	void AddKartHeader( void );
	void AddKartSection( void );
	void UpdateKartTeamInfo( void );
	void GetKartPlayerScoreInfo( int playerIndex, KeyValues *kv );
	static bool StaticKartSortFunc( vgui::SectionedListPanel *list, int itemID1, int itemID2 );
	bool m_bKartLayout;	// the sections were built for a kart race
	enum 
	{ 
		CSTRIKE_NAME_WIDTH = 320,
		CSTRIKE_CLASS_WIDTH = 56,
		CSTRIKE_SCORE_WIDTH = 40,
		CSTRIKE_DEATH_WIDTH = 46,
		CSTRIKE_PING_WIDTH = 46,
//		CSTRIKE_VOICE_WIDTH = 40, 
//		CSTRIKE_FRIENDS_WIDTH = 24,
	};

	// Kart race columns; they add up to the deathmatch ones.
	enum
	{
		KART_POS_WIDTH = 34,
		KART_NAME_WIDTH = 214,
		KART_LAP_WIDTH = 40,
		KART_TIME_WIDTH = 64,
		KART_PING_WIDTH = 36,
	};

	// rounded corners
	Color					 m_bgColor;
	Color					 m_borderColor;
};


#endif // CHL2MPCLIENTSCOREBOARDDIALOG_H
