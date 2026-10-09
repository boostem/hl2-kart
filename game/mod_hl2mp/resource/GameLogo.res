// The main menu logo, drawn above the menu because gameinfo.txt has "gamelogo 1".
// The image is materials/console/logo (source: assets_src/textures/logo.png), 4:1.
"Resource/GameLogo.res"
{
	"GameLogo"
	{
		"ControlName"	"EditablePanel"
		"fieldName"		"GameLogo"
		"xpos"			"0"
		"ypos"			"0"
		"zpos"			"50"
		"wide"			"480"
		"tall"			"120"
		"autoResize"	"1"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"offsetX"		"-24"
		"offsetY"		"-16"
	}
	"Logo"
	{
		"ControlName"	"ImagePanel"
		"fieldName"		"Logo"
		"xpos"			"0"
		"ypos"			"0"
		"zpos"			"50"
		"wide"			"480"
		"tall"			"120"
		"visible"		"1"
		"enabled"		"1"
		"image"			"../console/logo"
		"scaleImage"	"1"
	}
}
