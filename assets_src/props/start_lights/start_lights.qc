// Written by assets_src/props/build_props.py; compile with tools/wine/studiomdl.sh assets_src/props/start_lights/start_lights.qc
$modelname "kart/props/start_lights.mdl"
$cdmaterials "models/kart/props/"
$surfaceprop "metal"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the prop faces +X
$staticprop  // needed by prop_static; prop_dynamic takes it too

$body body "start_lights.smd"
$sequence idle "start_lights.smd" fps 1

// Skins: 0 all lamps off, 1 red lit, 2 yellow lit, 3 green lit (set the entity's skin to switch).
$texturegroup skinfamilies
{
	{ "lamp_red"    "lamp_yellow"    "lamp_green"    }
	{ "lamp_red_on" "lamp_yellow"    "lamp_green"    }
	{ "lamp_red"    "lamp_yellow_on" "lamp_green"    }
	{ "lamp_red"    "lamp_yellow"    "lamp_green_on" }
}

$collisionmodel "start_lights_phys.smd"
{
	$mass 200
	$concave
}
