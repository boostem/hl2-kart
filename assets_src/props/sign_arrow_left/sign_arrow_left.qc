// Written by assets_src/props/build_props.py; compile with tools/wine/studiomdl.sh assets_src/props/sign_arrow_left/sign_arrow_left.qc
$modelname "kart/props/sign_arrow_left.mdl"
$cdmaterials "models/kart/props/"
$surfaceprop "wood_panel"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the prop faces +X
$staticprop  // needed by prop_static; prop_dynamic takes it too

$body body "sign_arrow_left.smd"
$sequence idle "sign_arrow_left.smd" fps 1

$collisionmodel "sign_arrow_left_phys.smd"
{
	$mass 40
}
