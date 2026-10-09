"""Build the scrap kart: an original low-poly kart welded together from car parts, rebar and sheet metal.

    blender -b -P assets_src/kart_scrap/build_kart_scrap.py
    tools/venv/bin/python tools/img2vtf.py assets_src/kart_scrap/kart_scrap.png models/kart/kart_scrap
    tools/wine/studiomdl.sh assets_src/kart_scrap/kart_scrap.qc

The first step builds the scene from code, bakes the 512x512 base texture (procedural paint, rust, rubber and
ambient occlusion, baked in Cycles) to kart_scrap.png, exports kart_scrap.smd and kart_scrap_phys.smd with
Blender Source Tools and writes kart_scrap.qc. img2vtf.py overwrites the VMT, so put back the committed one
(git checkout game/mod_hl2mp/materials/models/kart/kart_scrap.vmt) if it had changes.

1 unit = 1 inch. About 110 long, 70 wide and 50 tall, facing +X with the origin on the floor, centred.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "blender"))
from export_smd import export_smd  # noqa: E402

NAME = "kart_scrap"
TEX_SIZE = 512

# Wheels: (x, half track, radius, width). Car-part wheels: steel rims on worn road tyres, the rear ones bigger.
FRONT = (37.0, 30.5, 11.5, 9.0)
REAR = (-37.0, 29.0, 12.5, 12.0)

# Attachments in model space: name -> (position, (pitch, yaw, roll)). Wheels sit at the tyre's contact patch on
# the floor (for skid marks and dust); the exhaust points out of the pipe, backwards and up.
EXHAUST_END = Vector((-55.0, -14.0, 33.0))
ATTACHMENTS = {
    "wheel_fl": ((FRONT[0], FRONT[1], 0.0), (0, 0, 0)),
    "wheel_fr": ((FRONT[0], -FRONT[1], 0.0), (0, 0, 0)),
    "wheel_rl": ((REAR[0], REAR[1], 0.0), (0, 0, 0)),
    "wheel_rr": ((REAR[0], -REAR[1], 0.0), (0, 0, 0)),
    "exhaust": (tuple(EXHAUST_END), (-25, 180, 0)),
    "vehicle_driver_eyes": ((-4.0, 0.0, 40.0), (0, 0, 0)),
    "item_hold": ((-62.0, 0.0, 22.0), (0, 0, 0)),
}

# --- geometry helpers: each finish (paint, steel, ...) is one bmesh, later one object with its bake material ---

parts = {}


def bm_for(finish):
    if finish not in parts:
        parts[finish] = bmesh.new()
    return parts[finish]


def _xform(bm, verts, matrix):
    bmesh.ops.transform(bm, matrix=matrix, verts=verts)


def box(finish, lo, hi, taper=None, rot=None):
    """Axis-aligned box from corner lo to hi. taper=(sy, sz) scales the +X end's Y and Z half-extents about the
    centre line (sz < 1 lowers the top only). rot=(axis, degrees) rotates about the box centre."""
    bm = bm_for(finish)
    lo, hi = Vector(lo), Vector(hi)
    centre, size = (lo + hi) / 2, hi - lo
    verts = bmesh.ops.create_cube(bm, size=1.0)["verts"]
    if taper:
        for v in verts:
            if v.co.x > 0:
                v.co.y *= taper[0]
                if v.co.z > 0:
                    v.co.z = -0.5 + (v.co.z + 0.5) * taper[1]
    m = Matrix.Translation(centre)
    if rot:
        m = m @ Matrix.Rotation(math.radians(rot[1]), 4, rot[0])
    _xform(bm, verts, m @ Matrix.Diagonal(size.to_4d()))
    return verts


def tube(finish, p0, p1, r, sides=6, overlap=True):
    """A capped tube (a prism with `sides` sides) from p0 to p1, extended by r at both ends so joints overlap."""
    bm = bm_for(finish)
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    length = d.length + (2 * r if overlap else 0)
    verts = bmesh.ops.create_cone(bm, cap_ends=True, segments=sides, radius1=r, radius2=r, depth=length)["verts"]
    rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_matrix().to_4x4()
    _xform(bm, verts, Matrix.Translation((p0 + p1) / 2) @ rot)


def polytube(finish, points, r, sides=6):
    for a, b in zip(points, points[1:]):
        tube(finish, a, b, r, sides)


def lathe(finish, profile, segments, matrix):
    """Spin a profile of (radius, z) points round the local Z axis. A point with radius 0 closes the surface."""
    bm = bm_for(finish)
    rings = []
    for r, z in profile:
        if r == 0:
            rings.append([bm.verts.new((0, 0, z))])
        else:
            rings.append([bm.verts.new((r * math.cos(2 * math.pi * i / segments),
                                        r * math.sin(2 * math.pi * i / segments), z)) for i in range(segments)])
    for a, b in zip(rings, rings[1:]):
        for i in range(segments):
            j = (i + 1) % segments
            if len(a) == 1:
                bm.faces.new((a[0], b[i], b[j]))
            elif len(b) == 1:
                bm.faces.new((a[j], a[i], b[0]))
            else:
                bm.faces.new((a[i], b[i], b[j], a[j]))
    _xform(bm, [v for ring in rings for v in ring], matrix)


def wheel(x, y, radius, width, segments=16):
    """A road tyre on a pressed-steel rim. The outer face points away from the kart (+Y on the left)."""
    side = 1 if y > 0 else -1
    # local Z is the axle, pointing outwards
    m = Matrix.Translation((x, y, radius)) @ Matrix.Rotation(-side * math.pi / 2, 4, "X")
    R, w = radius, width
    lathe("rubber", [(0.64 * R, -w / 2), (0.88 * R, -w / 2), (R, -0.3 * w), (R, 0.3 * w),
                     (0.88 * R, w / 2), (0.64 * R, w / 2), (0.64 * R, 0.3 * w)], segments, m)
    lathe("rim", [(0.66 * R, -0.42 * w), (0.66 * R, 0.42 * w), (0.56 * R, 0.3 * w), (0.36 * R, 0.16 * w),
                  (0.2 * R, 0.24 * w), (0, 0.26 * w)], segments, m)
    lathe("rim", [(0.66 * R, -0.42 * w), (0, -0.3 * w)][::-1], segments, m)  # inner face of the rim
    for i in range(5):  # lug nuts
        a = 2 * math.pi * i / 5
        c = m @ Vector((0.27 * R * math.cos(a), 0.27 * R * math.sin(a), 0.2 * w))
        tube("steel", c, m @ Vector((0.27 * R * math.cos(a), 0.27 * R * math.sin(a), 0.2 * w + 1.2)), 0.7, 6, False)


def mirror_y(points):
    return [(x, -y, z) for x, y, z in points]


# --- the kart ---

def build_kart():
    # wheels and axles
    for x, y, r, w in (FRONT, REAR):
        wheel(x, y, r, w)
        wheel(x, -y, r, w)
    fx, fy, fr, fw = FRONT
    rx, ry, rr, rw = REAR
    tube("steel", (fx, -fy + fw / 2, fr), (fx, fy - fw / 2, fr), 1.4)   # front beam axle
    tube("steel", (rx, -ry + rw / 2, rr), (rx, ry - rw / 2, rr), 2.0, 8)  # live rear axle
    tube("engine", (rx, -12, rr), (rx, -5, rr), 5.0, 10, False)          # differential / sprocket drum
    for s in (1, -1):  # kingpins and the front axle's leaf springs up to the frame
        tube("steel", (fx, s * (fy - fw / 2 - 1), fr - 4), (fx, s * (fy - fw / 2 - 1), fr + 4), 1.0)
        tube("rust", (fx - 6, s * 17, 8.5), (fx + 6, s * 17, 9.5), 1.2, 4)
        tube("steel", (rx, s * 17, rr), (rx + 6, s * 17, 8), 1.2)        # rear trailing links

    # ladder frame of welded tube: side rails kicking up at the nose, cross members
    rail = [(-52, 17, 8), (-30, 17, 7), (28, 17, 7), (46, 13, 11), (52, 10, 12)]
    for pts in (rail, mirror_y(rail)):
        polytube("steel", pts, 1.5)
    for x, y, z in ((-50, 17, 8), (-30, 17, 7), (-4, 17, 7), (22, 17, 7), (46, 13, 11)):
        tube("steel", (x, -y, z), (x, y, z), 1.3)

    # bumpers: bent rebar at the front, a scaffold pole at the back
    polytube("rust", [(50, -18, 9), (54, -12, 13), (55, 0, 14), (54, 12, 13), (50, 18, 9)], 1.1, 5)
    for s in (1, -1):
        tube("rust", (52, s * 10, 12), (54, s * 12, 13), 1.0, 5)
    tube("steel", (-54, -22, 14), (-54, 22, 14), 1.6, 8)
    for s in (1, -1):
        tube("steel", (-52, s * 17, 8), (-54, s * 17, 14), 1.3)

    # side nerf bars between the wheels
    nerf = [(24, 18, 9), (20, 30, 11), (-20, 30, 11), (-24, 18, 9)]
    for pts in (nerf, mirror_y(nerf)):
        polytube("steel", pts, 1.2)

    # roll hoop of bent rebar behind the seat, braced back to the frame
    hoop = [(-16, -16, 7), (-17, -15.5, 38), (-18, -10, 48.8), (-18, 10, 48.8), (-17, 15.5, 38), (-16, 16, 7)]
    polytube("rust", hoop, 1.2, 6)
    for s in (1, -1):
        tube("rust", (-17.5, s * 14, 40), (-46, s * 15, 9), 1.0, 5)

    # floor pan: a sheet of scrap plate
    box("paint", (-34, -16, 4.5), (36, 16, 6))

    # nose: the front of a car's bonnet, cut down and riveted on, with a plate and one salvaged headlight
    box("paint", (28, -16, 7), (53, 16, 21), taper=(0.7, 0.55))
    box("paint", (26, -16.5, 18), (33, 16.5, 21.5))                       # dash hoop / cowl
    box("rust", (53, -7, 9), (54.2, 7, 15))                               # number plate
    hl = Matrix.Translation((43, 9, 19.5)) @ Matrix.Rotation(math.radians(80), 4, "Y")
    lathe("steel", [(0, -2.5), (3.2, -1.0), (3.6, 1.5), (3.0, 2.2), (0, 2.6)], 10, hl)

    # side pods: dented panels bolted to the nerf bars
    for s in (1, -1):
        lo, hi = (-14, 18, 6), (20, 26, 16)
        if s < 0:
            lo, hi = (-14, -26, 6), (20, -18, 16)
        box("paint", lo, hi, taper=(1.0, 0.7))

    # bucket seat from an old car, and the steering
    box("seat", (-14, -11, 6), (4, 11, 11))
    box("seat", (-17, -12, 8), (-12, 12, 36), rot=("Y", -12))
    for s in (1, -1):
        box("seat", (-13, s * 11 - 1.5, 9), (2, s * 11 + 1.5, 15))           # bolsters
    tube("steel", (24, 0, 9), (11, 0, 25), 1.0)                             # column
    wheel_m = Matrix.Translation((10.5, 0, 25.5)) @ Matrix.Rotation(math.radians(-55), 4, "Y")
    ring = [wheel_m @ Vector((7 * math.cos(2 * math.pi * i / 8), 7 * math.sin(2 * math.pi * i / 8), 0))
            for i in range(9)]
    polytube("rubber", ring, 0.9, 5)
    tube("steel", wheel_m @ Vector((0, -7, 0)), wheel_m @ Vector((0, 7, 0)), 0.6, 4)

    # engine behind the seat: a lawnmower-style single with a pull-start drum, air filter and fuel can
    box("engine", (-46, -9, 9), (-24, 7, 22))
    box("engine", (-42, -8, 22), (-28, 2, 29), rot=("X", 10))               # cylinder head
    for i in range(4):                                                      # cooling fins
        box("engine", (-41 + 3.5 * i, -9, 22), (-40 + 3.5 * i, 3, 30))
    pull = Matrix.Translation((-35, 7, 15.5)) @ Matrix.Rotation(-math.pi / 2, 4, "X")
    lathe("engine", [(7, 0), (7, 3), (5.5, 4.2), (0, 4.4)], 10, pull)
    filt = Matrix.Translation((-28, -12, 24)) @ Matrix.Rotation(math.pi / 2, 4, "X")
    lathe("steel", [(0, -2), (4.5, -2), (5, 0), (4.5, 2.5), (0, 2.5)], 10, filt)
    box("paint", (-50, 8, 8), (-40, 15, 22))                                # fuel can strapped at the back
    tube("steel", (-45, 11.5, 22), (-45, 11.5, 24.5), 1.0, 6)

    # exhaust: rusted pipe out of the head, round the back, up and out
    polytube("rust", [(-30, -6, 25), (-36, -12, 26), (-46, -14, 28), (-52, -14, 30.5), tuple(EXHAUST_END)], 1.3, 6)


# --- materials: procedural finishes, baked with AO into one texture ---

FINISHES = {
    # name: (base sRGB, secondary sRGB, noise scale, secondary amount, scratch amount)
    "paint": ((0.62, 0.62, 0.55), (0.42, 0.27, 0.17), 0.08, 0.3, 0.5),      # faded primer, rust patches
    "steel": ((0.42, 0.41, 0.38), (0.40, 0.26, 0.17), 0.25, 0.35, 0.2),     # welded tube, rust bloom
    "rust": ((0.45, 0.28, 0.17), (0.27, 0.18, 0.12), 0.3, 0.5, 0.1),        # rebar, exhaust
    "rubber": ((0.16, 0.16, 0.15), (0.24, 0.23, 0.21), 0.5, 0.3, 0.0),      # worn tyres, wheel grip
    "rim": ((0.50, 0.51, 0.47), (0.38, 0.25, 0.16), 0.2, 0.3, 0.3),         # pressed-steel rims
    "seat": ((0.33, 0.24, 0.17), (0.45, 0.43, 0.37), 0.15, 0.3, 0.2),       # cracked vinyl, tape patches
    "engine": ((0.25, 0.25, 0.23), (0.14, 0.13, 0.12), 0.3, 0.45, 0.35),    # oily cast iron
}


def srgb_to_linear(c):
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c) + (1.0,)


def bake_material(finish, image):
    base, second, scale, amount, scratch = FINISHES[finish]
    mat = bpy.data.materials.new("bake_" + finish)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    n = nt.nodes.new
    geo = n("ShaderNodeNewGeometry")

    patches = n("ShaderNodeTexNoise")
    patches.inputs["Scale"].default_value = scale
    patches.inputs["Detail"].default_value = 6
    nt.links.new(geo.outputs["Position"], patches.inputs["Vector"])
    ramp = n("ShaderNodeValToRGB")  # hard-edged patches of the secondary colour
    ramp.color_ramp.elements[0].position = 0.62 - amount * 0.3
    ramp.color_ramp.elements[1].position = 0.66 - amount * 0.3
    nt.links.new(patches.outputs["Fac"], ramp.inputs["Fac"])
    mix = n("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value = srgb_to_linear(base)
    mix.inputs["B"].default_value = srgb_to_linear(second)
    nt.links.new(ramp.outputs["Color"], mix.inputs["Factor"])

    grain = n("ShaderNodeTexNoise")  # fine mottling so flat panels aren't flat
    grain.inputs["Scale"].default_value = 1.6
    grain.inputs["Detail"].default_value = 4
    nt.links.new(geo.outputs["Position"], grain.inputs["Vector"])
    mottle = n("ShaderNodeMapRange")
    mottle.inputs["To Min"].default_value = 0.8
    mottle.inputs["To Max"].default_value = 1.15
    nt.links.new(grain.outputs["Fac"], mottle.inputs["Value"])
    col = n("ShaderNodeMix")
    col.data_type = "RGBA"
    col.blend_type = "MULTIPLY"
    col.inputs["Factor"].default_value = 1.0
    nt.links.new(mix.outputs["Result"], col.inputs["A"])
    nt.links.new(mottle.outputs["Result"], col.inputs["B"])
    last = col.outputs["Result"]

    if scratch:  # thin bright scratches of bare metal
        wave = n("ShaderNodeTexWave")
        wave.wave_type = "BANDS"
        wave.inputs["Scale"].default_value = 0.35
        wave.inputs["Distortion"].default_value = 18
        wave.inputs["Detail"].default_value = 4
        nt.links.new(geo.outputs["Position"], wave.inputs["Vector"])
        sramp = n("ShaderNodeValToRGB")
        sramp.color_ramp.elements[0].position = 0.97 - scratch * 0.05
        sramp.color_ramp.elements[1].position = 1.0
        nt.links.new(wave.outputs["Fac"], sramp.inputs["Fac"])
        smix = n("ShaderNodeMix")
        smix.data_type = "RGBA"
        smix.inputs["B"].default_value = srgb_to_linear((0.66, 0.66, 0.63))
        nt.links.new(sramp.outputs["Color"], smix.inputs["Factor"])
        nt.links.new(last, smix.inputs["A"])
        last = smix.outputs["Result"]

    ao = n("ShaderNodeAmbientOcclusion")  # baked AO, softened so the kart still reads in dark maps
    ao.inputs["Distance"].default_value = 10
    ao.samples = 32
    aor = n("ShaderNodeMapRange")
    aor.inputs["To Min"].default_value = 0.4
    aor.inputs["To Max"].default_value = 1.0
    nt.links.new(ao.outputs["AO"], aor.inputs["Value"])
    shade = n("ShaderNodeMix")
    shade.data_type = "RGBA"
    shade.blend_type = "MULTIPLY"
    shade.inputs["Factor"].default_value = 1.0
    nt.links.new(last, shade.inputs["A"])
    nt.links.new(aor.outputs["Result"], shade.inputs["B"])

    emit = n("ShaderNodeEmission")
    nt.links.new(shade.outputs["Result"], emit.inputs["Color"])
    out = n("ShaderNodeOutputMaterial")
    nt.links.new(emit.outputs["Emission"], out.inputs["Surface"])
    tex = n("ShaderNodeTexImage")  # the bake target
    tex.image = image
    nt.nodes.active = tex
    return mat


def make_objects(collection):
    obs = []
    for finish, bm in parts.items():
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.001)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        me = bpy.data.meshes.new(finish)
        bm.to_mesh(me)
        bm.free()
        me.uv_layers.new(name="UVMap")
        ob = bpy.data.objects.new(finish, me)
        collection.objects.link(ob)
        obs.append(ob)
    parts.clear()
    return obs


def join(obs, collection, image):
    """Join the parts into the reference mesh, each finish keeping its own bake material slot."""
    for ob in obs:
        ob.data.materials.append(bake_material(ob.name, image))
    bpy.ops.object.select_all(action="DESELECT")
    for ob in obs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    bpy.ops.object.join()
    ob = bpy.context.object
    ob.name = ob.data.name = NAME
    for c in list(ob.users_collection):
        c.objects.unlink(ob)
    collection.objects.link(ob)
    return ob


def unwrap(ob):
    """One UV atlas for the whole kart (unwrapping the parts separately would stack them all in 0-1)."""
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(50), island_margin=0.006, area_weight=0.0,
                             correct_aspect=True, scale_to_bounds=True)
    bpy.ops.object.mode_set(mode="OBJECT")


def bake(ob, image, path):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 32
    sc.render.bake.margin = 6
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.bake(type="EMIT", use_clear=True)
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()


def finish_ref(ob):
    """Swap the bake materials for the one SMD material and smooth by angle."""
    ob.data.materials.clear()
    ob.data.materials.append(bpy.data.materials.new(NAME))  # the SMD material name, looked up under $cdmaterials
    for poly in ob.data.polygons:
        poly.material_index = 0
    ob.data.shade_smooth()
    ob.data.set_sharp_from_angle(angle=math.radians(40))


def build_phys(collection):
    """Collision: two convex pieces, the wheelbase slab and the seat with the roll hoop."""
    box("phys", (-55, -35, 2), (55, 35, 25))
    box("phys", (-24, -17, 25), (-12, 17, 50))
    ob = make_objects(collection)[0]
    me = ob.data
    ob.name = me.name = NAME + "_phys"
    # studiomdl finds the convex pieces by shared vertices: smooth normals and no UVs keep each box in one piece
    me.uv_layers.remove(me.uv_layers[0])
    me.shade_smooth()
    me.materials.append(bpy.data.materials[NAME])


def write_qc(path):
    lines = [
        "// Written by build_kart_scrap.py; compile with tools/wine/studiomdl.sh " + "assets_src/kart_scrap/kart_scrap.qc",
        '$modelname "kart/kart_scrap.mdl"',
        '$cdmaterials "models/kart/"',
        '$surfaceprop "metal"',
        "$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the kart faces +X like the player",
        "",
        '$body body "kart_scrap.smd"',
        '$sequence idle "kart_scrap.smd" fps 1',
        "",
        "// Wheels: tyre contact patch on the floor. exhaust: pointing out of the pipe. item_hold: behind the kart.",
    ]
    for name, (pos, rot) in ATTACHMENTS.items():
        lines.append('$attachment "%s" "root" %g %g %g rigid rotate %g %g %g' % ((name,) + tuple(pos) + rot))
    lines += [
        "",
        '$collisionmodel "kart_scrap_phys.smd"',
        "{",
        "\t$mass 180",
        "\t$concave",
        "}",
        "",
    ]
    with open(path, "w") as f:
        f.write("\n".join(lines))


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    ref = bpy.data.collections.new(NAME)
    phys = bpy.data.collections.new(NAME + "_phys")
    for c in (ref, phys):
        scene.collection.children.link(c)

    build_kart()
    image = bpy.data.images.new(NAME, TEX_SIZE, TEX_SIZE, alpha=False)
    ob = join(make_objects(ref), ref, image)
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    unwrap(ob)
    bake(ob, image, os.path.join(HERE, NAME + ".png"))
    finish_ref(ob)
    build_phys(phys)

    export_smd(HERE, [ref, phys])
    write_qc(os.path.join(HERE, NAME + ".qc"))
    if "--blend" in sys.argv:
        bpy.ops.wm.save_as_mainfile(filepath=sys.argv[sys.argv.index("--blend") + 1])
    print("built %s: %d triangles" % (NAME, tris))


main()
