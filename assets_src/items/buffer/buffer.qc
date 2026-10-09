// Written by assets_src/items/build_items.py; compile with tools/wine/studiomdl.sh assets_src/items/buffer/buffer.qc
$modelname "kart/items/buffer.mdl"
$cdmaterials "models/kart/items/"
$surfaceprop "default"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the item faces +X

$body body "buffer.smd"
$sequence idle "buffer.smd" fps 1
