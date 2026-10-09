"""Render the four reference PNGs every model PR attaches: front, side, 3q and scale.

    blender -b model.blend -P tools/blender/render_model.py -- --out renders/
    blender -b -P tools/blender/render_model.py -- --smd assets_src/<m>/<m>.smd --out renders/ [--buggy] [--texture t.png]
                [--texture-dir dir ...]

Uses the model in the open .blend, or imports --smd with Blender Source Tools. 1 unit = 1 inch.
--texture shows that image on every material of the model (its UVs), instead of the materials' flat colours.
--texture-dir (repeatable) shows <dir>/<material>.png on each material that has one, for models with several.
`scale` shows the model beside a 72-unit "citizen" box (and with --buggy a 120 x 70 x 50 buggy box).
Writes <out>/front.png, side.png, 3q.png, scale.png.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

CITIZEN = (24, 24, 72)
BUGGY = (120, 70, 50)


def parse():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    opts = {"out": None, "smd": None, "buggy": False, "texture": None, "texture_dirs": []}
    it = iter(argv)
    for a in it:
        if a == "--out":
            opts["out"] = next(it)
        elif a == "--smd":
            opts["smd"] = next(it)
        elif a == "--buggy":
            opts["buggy"] = True
        elif a == "--texture":
            opts["texture"] = next(it)
        elif a == "--texture-dir":
            opts["texture_dirs"].append(next(it))
        else:
            sys.exit("unknown option " + a)
    if not opts["out"]:
        sys.exit("usage: blender -b [model.blend] -P render_model.py -- --out <dir> [--smd f.smd] [--buggy]"
                 " [--texture t.png] [--texture-dir dir ...]")
    return opts


def bounds(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    lo = Vector((min(p[i] for p in pts) for i in range(3)))
    hi = Vector((max(p[i] for p in pts) for i in range(3)))
    return lo, hi


def ref_box(name, size, x, color, label):
    """A translucent box sitting on the floor with its centre at x, plus a text label on top."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 0, size[2] / 2))
    box = bpy.context.object
    box.name = "ref_" + name
    box.scale = size
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    box.data.materials.append(mat)
    box.display_type = "SOLID"
    bpy.ops.object.text_add(location=(x, -size[1] / 2 - 0.5, size[2] / 2), rotation=(math.pi / 2, 0, 0))
    txt = bpy.context.object
    txt.data.body = label
    txt.data.align_x = "CENTER"
    txt.data.size = max(4, size[2] / 6)
    txt.name = "ref_" + name + "_label"
    tmat = bpy.data.materials.new(name + "_label")
    tmat.diffuse_color = (0.05, 0.05, 0.05, 1)
    txt.data.materials.append(tmat)
    txt.location.y -= 1
    return [box, txt]


def apply_texture(objs, path, dirs=()):
    """Put the image on every material of objs as the active image node, which workbench's TEXTURE colour shows.
    A <dir>/<material>.png in one of dirs wins over path for that material."""
    image = bpy.data.images.load(os.path.abspath(path)) if path else None
    for o in objs:
        if not o.data.materials:
            o.data.materials.append(bpy.data.materials.new("texture"))
        for mat in o.data.materials:
            found = [os.path.join(d, mat.name + ".png") for d in dirs if os.path.isfile(os.path.join(d, mat.name + ".png"))]
            img = bpy.data.images.load(os.path.abspath(found[0])) if found else image
            if not img:
                continue
            mat.use_nodes = True
            node = mat.node_tree.nodes.new("ShaderNodeTexImage")
            node.image = img
            mat.node_tree.nodes.active = node


def camera(target, direction, dist, ortho_scale=None):
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    bpy.context.scene.collection.objects.link(cam)
    d = Vector(direction).normalized()
    cam.location = target + d * dist
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_end = dist * 10
    if ortho_scale:
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = ortho_scale
    bpy.context.scene.camera = cam
    return cam


def render(path, target, direction, extent):
    cam = camera(target, direction, extent * 3, ortho_scale=extent * 1.25)
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)


def main():
    opts = parse()
    if opts["smd"]:
        bpy.ops.preferences.addon_enable(module="io_scene_valvesource")
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.preferences.addon_enable(module="io_scene_valvesource")
        bpy.ops.import_scene.smd(files=[{"name": os.path.basename(opts["smd"])}],
                                 directory=os.path.dirname(os.path.abspath(opts["smd"])), doAnim=False)
    model = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not model:
        sys.exit("no mesh objects to render")
    if opts["texture"] or opts["texture_dirs"]:
        apply_texture(model, opts["texture"], opts["texture_dirs"])
    lo, hi = bounds(model)
    size, centre = hi - lo, (hi + lo) / 2

    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.render.resolution_x = sc.render.resolution_y = 768
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "TEXTURE" if opts["texture"] or opts["texture_dirs"] else "MATERIAL"
    sc.display.shading.show_cavity = True
    sc.world = bpy.data.worlds.new("w")
    sc.world.color = (0.55, 0.57, 0.6)
    os.makedirs(opts["out"], exist_ok=True)
    out = lambda n: os.path.join(os.path.abspath(opts["out"]), n + ".png")

    extent = max(size) * 1.1
    render(out("front"), centre, (1, 0, 0), extent)  # models face +X
    render(out("side"), centre, (0, -1, 0), extent)
    render(out("3q"), centre, (1, -1, 0.7), extent * 1.5)  # the diagonal is longer

    # scale: the citizen to the left of the model, the buggy (if asked) to the right
    refs = ref_box("citizen", CITIZEN, lo.x - CITIZEN[0], (0.9, 0.5, 0.1, 1), "citizen")
    xs = [lo.x - CITIZEN[0] * 1.5, hi.x]
    if opts["buggy"]:
        refs += ref_box("buggy", BUGGY, hi.x + BUGGY[0] / 2 + 12, (0.2, 0.5, 0.9, 1), "buggy 120x70x50")
        xs.append(hi.x + BUGGY[0] + 12)
    width = max(xs) - min(xs)
    height = max(CITIZEN[2], hi.z)
    mid = Vector(((max(xs) + min(xs)) / 2, 0, height / 2))
    render(out("scale"), mid, (0, -1, 0), max(width, height) * 1.1)
    print("rendered to", opts["out"])


main()
