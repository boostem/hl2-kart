// Written by assets_src/props/build_props.py; compile with tools/wine/studiomdl.sh assets_src/props/finish_gantry/finish_gantry.qc
$modelname "kart/props/finish_gantry.mdl"
$cdmaterials "models/kart/props/"
$surfaceprop "metal"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the prop faces +X
$staticprop  // needed by prop_static; prop_dynamic takes it too

$body body "finish_gantry.smd"
$sequence idle "finish_gantry.smd" fps 1

$collisionmodel "finish_gantry_phys.smd"
{
	$mass 2000
	$concave
}
