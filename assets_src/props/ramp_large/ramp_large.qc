// Written by assets_src/props/build_props.py; compile with tools/wine/studiomdl.sh assets_src/props/ramp_large/ramp_large.qc
$modelname "kart/props/ramp_large.mdl"
$cdmaterials "models/kart/props/"
$surfaceprop "metalpanel"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the prop faces +X
$staticprop  // needed by prop_static; prop_dynamic takes it too

$body body "ramp_large.smd"
$sequence idle "ramp_large.smd" fps 1

$collisionmodel "ramp_large_phys.smd"
{
	$mass 600
}
