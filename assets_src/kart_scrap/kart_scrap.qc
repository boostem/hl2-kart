// Written by build_kart_scrap.py; compile with tools/wine/studiomdl.sh assets_src/kart_scrap/kart_scrap.qc
$modelname "kart/kart_scrap.mdl"
$cdmaterials "models/kart/"
$surfaceprop "metal"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the kart faces +X like the player

$body body "kart_scrap.smd"
$sequence idle "kart_scrap.smd" fps 1

// Wheels: tyre contact patch on the floor. exhaust: pointing out of the pipe. item_hold: behind the kart.
$attachment "wheel_fl" "root" 37 30.5 0 rigid rotate 0 0 0
$attachment "wheel_fr" "root" 37 -30.5 0 rigid rotate 0 0 0
$attachment "wheel_rl" "root" -37 29 0 rigid rotate 0 0 0
$attachment "wheel_rr" "root" -37 -29 0 rigid rotate 0 0 0
$attachment "exhaust" "root" -55 -14 33 rigid rotate -25 180 0
$attachment "vehicle_driver_eyes" "root" -4 0 40 rigid rotate 0 0 0
$attachment "item_hold" "root" -62 0 22 rigid rotate 0 0 0

$collisionmodel "kart_scrap_phys.smd"
{
	$mass 180
	$concave
}
