// Written by assets_src/items/build_items.py; compile with tools/wine/studiomdl.sh assets_src/items/oil_slick/oil_slick.qc
$modelname "kart/items/oil_slick.mdl"
$cdmaterials "models/kart/items/"
$surfaceprop "default"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the item faces +X

$body body "oil_slick.smd"
$sequence idle "oil_slick.smd" fps 1
