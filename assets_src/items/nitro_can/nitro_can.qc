// Written by assets_src/items/build_items.py; compile with tools/wine/studiomdl.sh assets_src/items/nitro_can/nitro_can.qc
$modelname "kart/items/nitro_can.mdl"
$cdmaterials "models/kart/items/"
$surfaceprop "metal"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the item faces +X

$body body "nitro_can.smd"
$sequence idle "nitro_can.smd" fps 1

$collisionmodel "nitro_can_phys.smd"
{
	$mass 10
	$concave
}
