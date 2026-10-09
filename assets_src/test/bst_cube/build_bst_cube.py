"""Smoke test for the Blender -> BST -> studiomdl chain. Not shipped.

    blender -b -P assets_src/test/bst_cube/build_bst_cube.py
    tools/wine/studiomdl.sh assets_src/test/bst_cube/bst_cube.qc
"""
import os
import sys

import bpy

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, "..", "..", "..", "tools", "blender"))
from export_smd import export_smd

bpy.ops.wm.read_factory_settings(use_empty=True)
mat = bpy.data.materials.new("bst_cube")  # becomes the SMD material name, looked up under $cdmaterials

def cube(collection_name, size):
    col = bpy.data.collections.new(collection_name)
    bpy.context.scene.collection.children.link(col)
    bpy.ops.mesh.primitive_cube_add(size=size, location=(0, 0, size / 2))
    ob = bpy.context.object
    for c in ob.users_collection:
        c.objects.unlink(ob)
    col.objects.link(ob)
    ob.name = collection_name
    ob.data.materials.append(mat)
    return ob

cube("bst_cube", 32)
cube("bst_cube_phys", 32)

export_smd(here)

with open(os.path.join(here, "bst_cube.qc"), "w") as f:
    f.write('''$modelname "test/bst_cube.mdl"
$cdmaterials "models/test/"
$surfaceprop "metal"
$body body "bst_cube.smd"
$sequence idle "bst_cube.smd" fps 1
$collisionmodel "bst_cube_phys.smd"
{
	$mass 40
}
''')
print("built", here)
