"""Export meshes to SMD with Blender Source Tools (install it with tools/blender/install_bst.sh).

Run on a .blend:
    blender -b model.blend -P tools/blender/export_smd.py -- --out assets_src/<m>/
Or import it from a build script (put tools/blender on sys.path) and call export_smd(out_dir).

Convention: every top-level collection is one SMD named after the collection. The reference mesh is the
collection `<model>`, the collision mesh is `<model>_phys`. Output is <out>/<model>.smd and <out>/<model>_phys.smd.
"""
import os
import sys

import bpy


def export_smd(out_dir, collections=None):
    """Export `collections` (default: all top-level collections that hold objects) as SMDs into out_dir."""
    out_dir = os.path.abspath(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    bpy.ops.preferences.addon_enable(module="io_scene_valvesource")
    scene = bpy.context.scene
    scene.vs.export_format = "SMD"
    scene.vs.export_path = out_dir + os.sep
    scene.vs.up_axis = "Z"
    cols = collections or [c for c in scene.collection.children if c.all_objects]
    names = [c.name for c in cols]
    for c in scene.collection.children:
        c.vs.mute = c.name not in names
        c.vs.subdir = ""
    for ob in scene.objects:
        ob.vs.export = any(ob.name in c.all_objects for c in cols)  # BST skips collections with nothing enabled
    from io_scene_valvesource.utils import State
    State.update_scene()  # fills scene.vs.export_list, which the operator needs
    bpy.ops.export_scene.smd(export_scene=True)
    missing = [n for n in names if not os.path.isfile(os.path.join(out_dir, n + ".smd"))]
    if missing:
        raise RuntimeError("BST did not write: " + ", ".join(n + ".smd" for n in missing))
    return [os.path.join(out_dir, n + ".smd") for n in names]


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(argv) != 2 or argv[0] != "--out":
        sys.exit("usage: blender -b model.blend -P export_smd.py -- --out <dir>")
    for path in export_smd(argv[1]):
        print("exported", path)
