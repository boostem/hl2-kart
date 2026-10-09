// Written by assets_src/items/build_items.py; compile with tools/wine/studiomdl.sh assets_src/items/hubcap/hubcap.qc
$modelname "kart/items/hubcap.mdl"
$cdmaterials "models/kart/items/"
$surfaceprop "metal"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the item faces +X

$body body "hubcap.smd"
$sequence idle "hubcap.smd" fps 1

$collisionmodel "hubcap_phys.smd"
{
	$mass 8
}
