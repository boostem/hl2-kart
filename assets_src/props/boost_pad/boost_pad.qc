// Written by assets_src/props/build_props.py; compile with tools/wine/studiomdl.sh assets_src/props/boost_pad/boost_pad.qc
$modelname "kart/props/boost_pad.mdl"
$cdmaterials "models/kart/props/"
$surfaceprop "metal"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the prop faces +X
$staticprop  // needed by prop_static; prop_dynamic takes it too

$body body "boost_pad.smd"
$sequence idle "boost_pad.smd" fps 1

$collisionmodel "boost_pad_phys.smd"
{
	$mass 200
}
