// The main menu. "Quick race" starts the kart arena on a local 8-player server
// (room for bots); the rest are HL2MP's own entries.
"GameMenu"
{
	"1"
	{
		"label" "#GameUI_GameMenu_ResumeGame"
		"command" "ResumeGame"
		"OnlyInGame" "1"
	}
	"2"
	{
		"label" "#GameUI_GameMenu_Disconnect"
		"command" "Disconnect"
		"OnlyInGame" "1"
	}
	"3"
	{
		"label" "#GameUI_GameMenu_PlayerList"
		"command" "OpenPlayerListDialog"
		"OnlyInGame" "1"
	}
	"4"
	{
		"label" ""
		"command" ""
		"OnlyInGame" "1"
	}
	"5"
	{
		"label" "QUICK RACE"
		"command" "engine disconnect; maxplayers 8; map kart_arena"
	}
	"6"
	{
		"label" "#GameUI_GameMenu_FindServers"
		"command" "OpenServerBrowser"
	}
	"7"
	{
		"label" "#GameUI_GameMenu_CreateServer"
		"command" "OpenCreateMultiplayerGameDialog"
	}
	"8"
	{
		"label" ""
		"command" ""
	}
	"10"
	{
		"label" "#GameUI_GameMenu_Options"
		"command" "OpenOptionsDialog"
	}
	"11"
	{
		"label" "#GameUI_GameMenu_Quit"
		"command" "Quit"
	}
}
