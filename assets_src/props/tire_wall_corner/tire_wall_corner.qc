// Written by assets_src/props/build_props.py; compile with tools/wine/studiomdl.sh assets_src/props/tire_wall_corner/tire_wall_corner.qc
$modelname "kart/props/tire_wall_corner.mdl"
$cdmaterials "models/kart/props/"
$surfaceprop "rubbertire"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the prop faces +X
$staticprop  // needed by prop_static; prop_dynamic takes it too

$body body "tire_wall_corner.smd"
$sequence idle "tire_wall_corner.smd" fps 1

$collisionmodel "tire_wall_corner_phys.smd"
{
	$mass 600
	$concave
}
