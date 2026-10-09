// Written by assets_src/props/build_props.py; compile with tools/wine/studiomdl.sh assets_src/props/ramp_small/ramp_small.qc
$modelname "kart/props/ramp_small.mdl"
$cdmaterials "models/kart/props/"
$surfaceprop "metalpanel"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the prop faces +X
$staticprop  // needed by prop_static; prop_dynamic takes it too

$body body "ramp_small.smd"
$sequence idle "ramp_small.smd" fps 1

$collisionmodel "ramp_small_phys.smd"
{
	$mass 150
}
