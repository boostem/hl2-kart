// Written by assets_src/items/build_items.py; compile with tools/wine/studiomdl.sh assets_src/items/seeker/seeker.qc
$modelname "kart/items/seeker.mdl"
$cdmaterials "models/kart/items/"
$surfaceprop "metal"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the item faces +X

$body body "seeker.smd"
$sequence idle "seeker.smd" fps 1

$collisionmodel "seeker_phys.smd"
{
	$mass 15
}
