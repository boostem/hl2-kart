// Hand-written test model for the wine toolchain: tools/wine/studiomdl.sh assets_src/test/test_cube/test_cube.qc
$modelname "test/test_cube.mdl"
$cdmaterials "models/test/"
$surfaceprop "metal"

$body body "test_cube.smd"
$sequence idle "test_cube.smd" fps 1

// No $staticprop, so it works as a prop_physics.
$collisionmodel "test_cube_phys.smd"
{
	$mass 40
}

$keyvalues
{
	prop_data
	{
		base "Metal.Small"
	}
}
