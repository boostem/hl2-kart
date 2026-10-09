// Kart race results panel (CKartResults, client hl2mp/kart_hud_results.cpp).
// Shown when the local player finishes and when the race ends; kart_results
// toggles it. StatusLabel follows the race state. Proportional, 640x480 units. The list's columns are sized in
// code to fill ResultsList: POS 40, BEST LAP and TOTAL 72 each, NAME the rest.
"resource/ui/KartResults.res"
{
	"KartResults"
	{
		"ControlName"		"EditablePanel"
		"fieldName"			"KartResults"
		"xpos"				"c-180"
		"ypos"				"80"
		"wide"				"360"
		"tall"				"300"
		"visible"			"1"
		"enabled"			"1"
		"paintbackground"	"1"
		"paintbackgroundtype"	"2"
	}

	"TitleLabel"
	{
		"ControlName"		"Label"
		"fieldName"			"TitleLabel"
		"xpos"				"0"
		"ypos"				"8"
		"wide"				"360"
		"tall"				"28"
		"visible"			"1"
		"enabled"			"1"
		"labelText"			"RACE RESULTS"
		"textAlignment"		"center"
		"font"				"KartHudMedium"
		"fgcolor_override"	"KartAmber"
	}

	"StatusLabel"
	{
		"ControlName"		"Label"
		"fieldName"			"StatusLabel"
		"xpos"				"0"
		"ypos"				"36"
		"wide"				"360"
		"tall"				"14"
		"visible"			"1"
		"enabled"			"1"
		"labelText"			""
		"textAlignment"		"center"
		"font"				"KartHudSmall"
		"fgcolor_override"	"KartWhiteDim"
	}

	"ResultsList"
	{
		"ControlName"		"SectionedListPanel"
		"fieldName"			"ResultsList"
		"xpos"				"12"
		"ypos"				"54"
		"wide"				"336"
		"tall"				"238"
		"visible"			"1"
		"enabled"			"1"
		"linespacing"		"18"
	}
}
