// Written by assets_src/items/build_items.py; compile with tools/wine/studiomdl.sh assets_src/items/item_box/item_box.qc
$modelname "kart/items/item_box.mdl"
$cdmaterials "models/kart/items/"
$surfaceprop "metal_box"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the item faces +X

$body body "item_box.smd"
$sequence idle "item_box.smd" fps 1

$collisionmodel "item_box_phys.smd"
{
	$mass 30
}
