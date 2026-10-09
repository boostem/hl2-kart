// Written by build_kart_racer.py; compile with tools/wine/studiomdl.sh assets_src/kart_racer/kart_racer.qc
$modelname "kart/kart_racer.mdl"
$cdmaterials "models/kart/"
$surfaceprop "metal"
$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the kart faces +X like the player

$body body "kart_racer.smd"
$sequence idle "kart_racer.smd" fps 1

// Wheels: tyre contact patch on the floor. exhaust: pointing out of the pipe. item_hold: behind the kart.
$attachment "wheel_fl" "root" 36 27 0 rigid rotate 0 0 0
$attachment "wheel_fr" "root" 36 -27 0 rigid rotate 0 0 0
$attachment "wheel_rl" "root" -36 26.5 0 rigid rotate 0 0 0
$attachment "wheel_rr" "root" -36 -26.5 0 rigid rotate 0 0 0
$attachment "exhaust" "root" -61 -8 25.5 rigid rotate -15 180 0
$attachment "vehicle_driver_eyes" "root" -8 0 40 rigid rotate 0 0 0
$attachment "item_hold" "root" -64 0 22 rigid rotate 0 0 0

// The driver's hands on the steering wheel, which turns with the steering.
$attachment "grip_l" "steering_wheel" 3.75 6.49519 0 rigid rotate 0 0 0
$attachment "grip_r" "steering_wheel" 3.75 -6.49519 0 rigid rotate 0 0 0

$collisionmodel "kart_racer_phys.smd"
{
	$mass 180
	$concave
}
