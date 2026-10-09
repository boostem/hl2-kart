"""Build the racer kart: an original low-poly go-kart in the classic cartoon racer style.

    blender -b -P assets_src/kart_racer/build_kart_racer.py
    tools/venv/bin/python tools/img2vtf.py assets_src/kart_racer/kart_racer.png models/kart/kart_racer
    tools/wine/studiomdl.sh assets_src/kart_racer/kart_racer.qc

The first step builds the scene from code, bakes the 512x512 base texture (procedural paint, plastic, chrome and
rubber with ambient occlusion, baked in Cycles) to kart_racer.png, exports kart_racer.smd and kart_racer_phys.smd
with Blender Source Tools and writes kart_racer.qc. img2vtf.py overwrites the VMT, so put back the committed one
(git checkout game/mod_hl2mp/materials/models/kart/kart_racer.vmt) if it had changes.

A low rounded tub, a wide front bumper, fat tyres (bigger at the back), side pods, a high-backed seat and a rear
engine with twin pipes and a little wing. The body paint is near white so cl_kart_color tints it.

1 unit = 1 inch. About 123 long, 71 wide and 43 tall, facing +X with the origin on the floor, centred.
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

NAME = "kart_racer"
TEX_SIZE = 512

# Wheels: (x, half track, radius, width). Fat racing tyres, the rear ones bigger and wider.
FRONT = (36.0, 27.0, 11.0, 12.0)
REAR = (-36.0, 26.5, 13.5, 16.0)

# Attachments in model space: name -> (position, (pitch, yaw, roll)). Wheels sit at the tyre's contact patch on
# the floor (for skid marks and dust); the exhaust points out of the right-hand pipe, backwards and a little up.
EXHAUST_END = Vector((-61.0, -8.0, 25.5))
ATTACHMENTS = {
    "wheel_fl": ((FRONT[0], FRONT[1], 0.0), (0, 0, 0)),
    "wheel_fr": ((FRONT[0], -FRONT[1], 0.0), (0, 0, 0)),
    "wheel_rl": ((REAR[0], REAR[1], 0.0), (0, 0, 0)),
    "wheel_rr": ((REAR[0], -REAR[1], 0.0), (0, 0, 0)),
    "exhaust": (tuple(EXHAUST_END), (-15, 180, 0)),
    "vehicle_driver_eyes": ((-8.0, 0.0, 40.0), (0, 0, 0)),
    "item_hold": ((-64.0, 0.0, 22.0), (0, 0, 0)),
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


def rbox(finish, lo, hi, r, taper=None, rot=None, segments=2):
    """box() with its edges rounded off by r."""
    bm = bm_for(finish)
    verts = box(finish, lo, hi, taper, rot)
    edges = list({e for v in verts for e in v.link_edges})
    bmesh.ops.bevel(bm, geom=verts + edges, offset=r, offset_type="OFFSET", segments=segments, profile=0.5,
                    affect="EDGES", clamp_overlap=True)


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


def wheel(x, y, radius, width, segments=18):
    """A fat, round-shouldered racing tyre on a chrome dish rim. The outer face points away from the kart."""
    side = 1 if y > 0 else -1
    # local Z is the axle, pointing outwards
    m = Matrix.Translation((x, y, radius)) @ Matrix.Rotation(-side * math.pi / 2, 4, "X")
    R, w = radius, width
    lathe("rubber", [(0.6 * R, -w / 2), (0.86 * R, -w / 2), (0.97 * R, -0.4 * w), (R, -0.2 * w), (R, 0.2 * w),
                     (0.97 * R, 0.4 * w), (0.86 * R, w / 2), (0.6 * R, w / 2), (0.6 * R, 0.3 * w)], segments, m)
    lathe("rim", [(0.62 * R, 0.42 * w), (0.5 * R, 0.36 * w), (0.3 * R, 0.3 * w), (0.18 * R, 0.4 * w),
                  (0, 0.42 * w)], segments, m)
    lathe("rim", [(0.62 * R, -0.42 * w), (0, -0.3 * w)][::-1], segments, m)  # inner face of the rim
    for i in range(5):  # spokes of the dish
        a = 2 * math.pi * (i + 0.5) / 5
        tube("trim", m @ Vector((0.2 * R * math.cos(a), 0.2 * R * math.sin(a), 0.38 * w)),
             m @ Vector((0.52 * R * math.cos(a), 0.52 * R * math.sin(a), 0.34 * w)), 0.9, 4, False)


def mirror_y(points):
    return [(x, -y, z) for x, y, z in points]


# --- the kart ---

def build_kart():
    # wheels, and axles hidden under the body
    for x, y, r, w in (FRONT, REAR):
        wheel(x, y, r, w)
        wheel(x, -y, r, w)
    fx, fy, fr, fw = FRONT
    rx, ry, rr, rw = REAR
    tube("steel", (fx, -fy + fw / 2, fr), (fx, fy - fw / 2, fr), 1.4)
    tube("steel", (rx, -ry + rw / 2, rr), (rx, ry - rw / 2, rr), 1.8, 8)

    # the tub: a low rounded body from the engine bay to the nose, with a tapered nose cone
    rbox("paint", (-44, -19, 5), (30, 19, 18), 4.0)
    rbox("paint", (24, -19, 5), (55, 19, 21), 4.0, taper=(0.75, 0.6))
    # cowl over the driver's legs, rising to the steering wheel
    rbox("paint", (6, -16, 15), (34, 16, 25), 4.5, taper=(0.8, 0.6))

    # wide front bumper spanning the front wheels, on two struts
    rbox("trim", (52, -32, 4), (60, 32, 13), 3.5)
    for s in (1, -1):
        tube("steel", (50, s * 10, 8), (53, s * 12, 8), 1.2)
    # round number disc on the nose
    disc = Matrix.Translation((51.5, 0, 15.5)) @ Matrix.Rotation(math.radians(62), 4, "Y")
    lathe("chrome", [(0, 0.5), (5.5, 0.5), (6.0, 0), (5.5, -0.5), (0, -0.5)][::-1], 14, disc)

    # side pods between the wheels, rounded and lower at the back, with a dark plastic scuff strip
    for s in (1, -1):
        lo, hi = (-22, 18, 5), (22, 34, 17)
        if s < 0:
            lo, hi = (-22, -34, 5), (22, -18, 17)
        rbox("paint", lo, hi, 3.0)
        rbox("trim", (-20, s * 34 - 1.5, 4), (20, s * 34 + 1.5, 9), 1.2)

    # rear bumper
    rbox("trim", (-63, -31, 4), (-55, 31, 13), 3.5)
    for s in (1, -1):
        tube("steel", (-56, s * 10, 8), (-50, s * 12, 8), 1.2)

    # high-backed racing seat with side bolsters and a headrest
    rbox("seat", (-22, -12, 16), (-2, 12, 21), 2.0)
    rbox("seat", (-27, -13, 16), (-19, 13, 38), 2.5, rot=("Y", -14))
    for s in (1, -1):
        rbox("seat", (-24, s * 12 - 2, 17), (-4, s * 12 + 2, 25), 1.5)
    rbox("seat", (-31, -7, 34), (-25, 7, 43), 2.0, rot=("Y", -14))

    # steering column and a chunky three-spoke wheel
    tube("steel", (22, 0, 20), (10, 0, 30), 1.1)
    wheel_m = Matrix.Translation((9, 0, 31)) @ Matrix.Rotation(math.radians(-55), 4, "Y")
    rim = [(7.5 + 1.3 * math.cos(2 * math.pi * i / 8), 1.3 * math.sin(2 * math.pi * i / 8)) for i in range(9)]
    lathe("rubber", rim, 20, wheel_m)
    for a in (90, 210, 330):
        tube("trim", wheel_m @ Vector((0, 0, 0)),
             wheel_m @ Vector((7 * math.cos(math.radians(a)), 7 * math.sin(math.radians(a)), 0)), 0.7, 4, False)
    lathe("chrome", [(0, -1), (2.2, -1), (2.2, 1), (0, 1)][::-1], 10, wheel_m)

    # engine in the back: a block with chrome rocker covers, an air box and twin pipes
    rbox("engine", (-53, -14, 10), (-32, 14, 24), 2.0)
    for s in (1, -1):
        rbox("chrome", (-50, s * 9 - 3, 24), (-34, s * 9 + 3, 28), 1.2)
    filt = Matrix.Translation((-42, 0, 28)) @ Matrix.Rotation(0, 4, "X")
    lathe("chrome", [(0, 0), (4.5, 0), (5.5, 2), (5.0, 4), (0, 4.5)], 12, filt)
    for s in (1, -1):
        y = s * 8
        polytube("chrome", [(-40, y, 18), (-52, y, 21), (-59, y, 24.5)], 1.6, 8)
        tip = Matrix.Translation((-59, y, 24.5)) @ Matrix.Rotation(math.radians(-90 + 15), 4, "Y")
        lathe("chrome", [(1.6, 0), (2.4, 2.5), (2.6, 3.0), (2.0, 3.0), (1.4, 0.5)], 10, tip)

    # little rear wing on two struts above the engine
    rbox("paint", (-61, -26, 35), (-49, 26, 37.5), 1.0, rot=("Y", 8))
    for s in (1, -1):
        rbox("paint", (-61, s * 26 - 0.9, 32), (-50, s * 26 + 0.9, 39), 0.6)  # end plates
        tube("trim", (-46, s * 12, 24), (-55, s * 14, 35), 1.0, 6)


# --- materials: procedural finishes, baked with AO into one texture ---

FINISHES = {
    # name: (base sRGB, secondary sRGB, noise scale, secondary amount, scratch amount)
    "paint": ((0.92, 0.92, 0.90), (0.86, 0.86, 0.84), 0.06, 0.2, 0.0),     # glossy body paint, tinted in game
    "trim": ((0.20, 0.20, 0.22), (0.16, 0.16, 0.18), 0.2, 0.2, 0.0),       # bumpers and scuff strips
    "steel": ((0.45, 0.46, 0.48), (0.38, 0.39, 0.40), 0.25, 0.2, 0.0),     # struts and axles
    "chrome": ((0.86, 0.87, 0.90), (0.62, 0.64, 0.68), 0.4, 0.45, 0.0),    # pipes, rims of the trim, disc
    "rubber": ((0.12, 0.12, 0.12), (0.17, 0.17, 0.17), 0.5, 0.2, 0.0),     # tyres, wheel grip
    "rim": ((0.82, 0.83, 0.86), (0.66, 0.67, 0.70), 0.3, 0.3, 0.0),        # chrome dish rims
    "seat": ((0.14, 0.14, 0.16), (0.22, 0.22, 0.25), 0.15, 0.25, 0.0),     # padded black seat
    "engine": ((0.30, 0.30, 0.32), (0.20, 0.20, 0.22), 0.3, 0.35, 0.0),    # cast block
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
    """Collision: two convex pieces, the wheelbase slab and the seat back with the wing."""
    box("phys", (-62, -35, 2), (59, 35, 25))
    box("phys", (-60, -26, 25), (-20, 26, 40))
    ob = make_objects(collection)[0]
    me = ob.data
    ob.name = me.name = NAME + "_phys"
    # studiomdl finds the convex pieces by shared vertices: smooth normals and no UVs keep each box in one piece
    me.uv_layers.remove(me.uv_layers[0])
    me.shade_smooth()
    me.materials.append(bpy.data.materials[NAME])


def write_qc(path):
    lines = [
        "// Written by build_kart_racer.py; compile with tools/wine/studiomdl.sh assets_src/kart_racer/kart_racer.qc",
        '$modelname "kart/kart_racer.mdl"',
        '$cdmaterials "models/kart/"',
        '$surfaceprop "metal"',
        "$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the kart faces +X like the player",
        "",
        '$body body "kart_racer.smd"',
        '$sequence idle "kart_racer.smd" fps 1',
        "",
        "// Wheels: tyre contact patch on the floor. exhaust: pointing out of the pipe. item_hold: behind the kart.",
    ]
    for name, (pos, rot) in ATTACHMENTS.items():
        lines.append('$attachment "%s" "root" %g %g %g rigid rotate %g %g %g' % ((name,) + tuple(pos) + rot))
    lines += [
        "",
        '$collisionmodel "kart_racer_phys.smd"',
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
