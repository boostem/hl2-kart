// Kart race results panel (CKartResults, client hl2mp/kart_hud_results.cpp).
// Shown when the local player finishes and when the race ends; kart_results
// toggles it. Proportional, 640x480 units. The list's columns are sized in
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

	"ResultsList"
	{
		"ControlName"		"SectionedListPanel"
		"fieldName"			"ResultsList"
		"xpos"				"12"
		"ypos"				"42"
		"wide"				"336"
		"tall"				"250"
		"visible"			"1"
		"enabled"			"1"
		"linespacing"		"18"
	}
}
