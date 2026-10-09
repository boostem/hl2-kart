"""Helpers for build_props.py: geometry, procedural finishes baked to one 512 texture per prop, SMD export, QC.

Geometry goes into one bmesh per finish (rust, paint, rubber, ...). Baked finishes are joined into the reference
mesh and baked with ambient occlusion into <prop>.png, which becomes the SMD material <prop>. Tiled finishes (the
lamps, the boost pad glow, the checker band) keep their own SMD material and get planar UVs instead of a bake.
1 unit = 1 inch.
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

TEX_SIZE = 512

# Baked finishes. name: (base sRGB, secondary sRGB, noise scale, secondary amount, scratch amount[, pattern])
# pattern: ("stripes", colour, width) paints diagonal stripes of colour over the base,
#          ("grain", scale) stretches the noise along Y into wood grain.
FINISHES = {
    "rust": ((0.45, 0.28, 0.17), (0.27, 0.18, 0.12), 0.12, 0.5, 0.1),        # rusted angle iron, pipes
    "steel": ((0.42, 0.41, 0.38), (0.40, 0.26, 0.17), 0.1, 0.35, 0.25),      # galvanised tube with rust bloom
    "plate": ((0.36, 0.36, 0.34), (0.38, 0.25, 0.16), 0.06, 0.3, 0.6),       # worn steel deck plate
    "dark": ((0.12, 0.12, 0.12), (0.22, 0.20, 0.18), 0.15, 0.25, 0.15),      # black painted steel
    "rubber": ((0.13, 0.13, 0.12), (0.22, 0.21, 0.19), 0.3, 0.3, 0.0),       # worn tyres
    "tyre_white": ((0.78, 0.77, 0.72), (0.36, 0.33, 0.28), 0.25, 0.2, 0.0),  # painted tyres, dirty
    "tyre_red": ((0.62, 0.13, 0.09), (0.30, 0.17, 0.12), 0.25, 0.35, 0.0),
    "wood": ((0.44, 0.33, 0.22), (0.30, 0.22, 0.15), 0.1, 0.4, 0.0, ("grain", 0.6)),  # weathered planks
    "sign": ((0.10, 0.10, 0.09), (0.30, 0.25, 0.20), 0.15, 0.2, 0.05),       # black sign paint
    "sign_paint": ((0.90, 0.72, 0.12), (0.45, 0.30, 0.15), 0.2, 0.25, 0.3),  # yellow sign paint
    "hazard": ((0.10, 0.10, 0.09), (0.40, 0.27, 0.16), 0.15, 0.2, 0.08, ("stripes", (0.88, 0.66, 0.08), 6.0)),
    "board": ((0.84, 0.42, 0.08), (0.42, 0.26, 0.15), 0.08, 0.3, 0.4),      # orange logo board
    "letters": ((0.92, 0.90, 0.82), (0.45, 0.40, 0.33), 0.2, 0.2, 0.2),      # off-white raised letters
    "concrete": ((0.48, 0.47, 0.44), (0.35, 0.34, 0.31), 0.08, 0.35, 0.0),   # concrete feet
}

# Tiled finishes: name -> (SMD material, UV mapping). The mapping is ((axis, scale), (axis, scale)) for U and V:
# u = coordinate[axis] / scale (+ offset), so one texture repeat covers `scale` units. Offsets centre a lamp lens.
TILED = {}

parts = {}
phys_parts = []


def reset():
    """An empty scene. Only the first prop resets to factory settings: after that Blender Source Tools is enabled,
    and its handlers fail on a factory reset, so the data blocks are removed instead."""
    parts.clear()
    phys_parts.clear()
    TILED.clear()
    if not bpy.context.preferences.addons.get("io_scene_valvesource"):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        return
    for data in (bpy.data.objects, bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.images,
                 bpy.data.collections):
        for block in list(data):
            data.remove(block)


def tiled(finish, material, u, v):
    """Declare a tiled finish: faces built with it keep SMD material `material`, with planar UVs (see TILED)."""
    TILED[finish] = (material, u, v)


def bm_for(finish):
    if finish not in parts:
        parts[finish] = bmesh.new()
    return parts[finish]


def _xform(bm, verts, matrix):
    bmesh.ops.transform(bm, matrix=matrix, verts=verts)


def _outward(bm, verts):
    """Point the faces of a closed primitive outwards. (Only per primitive: single quads keep their winding.)"""
    bmesh.ops.recalc_face_normals(bm, faces=list({f for v in verts for f in v.link_faces}))


def box(finish, lo, hi, matrix=None):
    """Box from corner lo to hi, then moved by matrix."""
    bm = bm_for(finish)
    lo, hi = Vector(lo), Vector(hi)
    verts = bmesh.ops.create_cube(bm, size=1.0)["verts"]
    m = Matrix.Translation((lo + hi) / 2) @ Matrix.Diagonal((hi - lo).to_4d())
    _xform(bm, verts, (matrix or Matrix()) @ m)
    _outward(bm, verts)
    return verts


def tube(finish, p0, p1, r, sides=6, overlap=True, matrix=None):
    """A capped prism tube from p0 to p1 (extended by r at both ends when overlap, so joints close)."""
    bm = bm_for(finish)
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    length = d.length + (2 * r if overlap else 0)
    verts = bmesh.ops.create_cone(bm, cap_ends=True, segments=sides, radius1=r, radius2=r, depth=length)["verts"]
    if sides == 4:  # square tubing: put the flats on the axes
        _xform(bm, verts, Matrix.Rotation(math.pi / 4, 4, "Z"))
    rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_matrix().to_4x4()
    _xform(bm, verts, (matrix or Matrix()) @ Matrix.Translation((p0 + p1) / 2) @ rot)
    _outward(bm, verts)


def polytube(finish, points, r, sides=6, matrix=None):
    for a, b in zip(points, points[1:]):
        tube(finish, a, b, r, sides, matrix=matrix)


def lathe(finish, profile, segments, matrix, closed=False):
    """Spin a profile of (radius, z) points round the local Z axis. A point with radius 0 closes the surface;
    closed=True joins the last point back to the first (a torus-like shape such as a tyre)."""
    bm = bm_for(finish)
    rings = []
    for r, z in profile:
        if r == 0:
            rings.append([bm.verts.new((0, 0, z))])
        else:
            rings.append([bm.verts.new((r * math.cos(2 * math.pi * i / segments),
                                        r * math.sin(2 * math.pi * i / segments), z)) for i in range(segments)])
    pairs = list(zip(rings, rings[1:])) + ([(rings[-1], rings[0])] if closed else [])
    for a, b in pairs:
        for i in range(segments):
            j = (i + 1) % segments
            if len(a) == 1:
                bm.faces.new((a[0], b[i], b[j]))
            elif len(b) == 1:
                bm.faces.new((a[j], a[i], b[0]))
            else:
                bm.faces.new((a[i], b[i], b[j], a[j]))
    verts = [v for ring in rings for v in ring]
    _xform(bm, verts, matrix)
    _outward(bm, verts)


def prism(finish, outline, depth, matrix):
    """Extrude a convex 2D outline [(x, y), ...] along local Z from 0 to depth."""
    bm = bm_for(finish)
    lo = [bm.verts.new((x, y, 0)) for x, y in outline]
    hi = [bm.verts.new((x, y, depth)) for x, y in outline]
    bm.faces.new(list(reversed(lo)))
    bm.faces.new(hi)
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    _xform(bm, lo + hi, matrix)
    _outward(bm, lo + hi)


def hull(finish, points):
    """The convex hull of points, as a closed solid."""
    bm = bm_for(finish)
    verts = [bm.verts.new(p) for p in points]
    res = bmesh.ops.convex_hull(bm, input=verts)
    bmesh.ops.delete(bm, geom=res["geom_interior"] + res["geom_unused"], context="VERTS")
    _outward(bm, [v for v in res["geom"] if isinstance(v, bmesh.types.BMVert)])


def quad(finish, corners):
    """One face through four points, counter-clockwise seen from the side it faces."""
    bm = bm_for(finish)
    bm.faces.new([bm.verts.new(c) for c in corners])


def text(finish, body, height, depth, matrix, width=None):
    """Raised letters: `body` in Blender's built-in font, `height` units tall (capital height), extruded `depth`
    along local Z, centred on the local origin in X and Y. width, if given, squeezes the line to fit."""
    curve = bpy.data.curves.new("text", "FONT")
    curve.body = body
    curve.align_x = "CENTER"
    curve.align_y = "CENTER"
    curve.extrude = depth / 2
    curve.resolution_u = 2
    ob = bpy.data.objects.new("text", curve)
    bpy.context.scene.collection.objects.link(ob)
    deps = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(deps))
    lo = Vector([min(v.co[i] for v in me.vertices) for i in range(3)])
    hi = Vector([max(v.co[i] for v in me.vertices) for i in range(3)])
    s = height / (hi.y - lo.y)
    sx = min(s, width / (hi.x - lo.x)) if width else s
    m = matrix @ Matrix.Diagonal((sx, s, 1, 1)) @ Matrix.Translation((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2,
                                                                     -lo.z))
    bm = bm_for(finish)
    tmp = bmesh.new()
    tmp.from_mesh(me)
    bmesh.ops.transform(tmp, matrix=m, verts=tmp.verts)
    bmesh.ops.remove_doubles(tmp, verts=tmp.verts, dist=0.01)
    bmesh.ops.triangulate(tmp, faces=tmp.faces)
    me2 = bpy.data.meshes.new("t")
    tmp.to_mesh(me2)
    tmp.free()
    bm.from_mesh(me2)
    bpy.data.objects.remove(ob)
    bpy.data.curves.remove(curve)


# --- collision: convex pieces, each a hull of its own points ---

def phys_hull(points, matrix=None):
    m = matrix or Matrix()
    phys_parts.append([m @ Vector(p) for p in points])


def phys_box(lo, hi, matrix=None):
    phys_hull([(x, y, z) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])], matrix)


# --- materials ---

def srgb_to_linear(c):
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c) + (1.0,)


def bake_material(finish, image):
    spec = FINISHES[finish]
    base, second, scale, amount, scratch = spec[:5]
    pattern = spec[5] if len(spec) > 5 else None
    mat = bpy.data.materials.new("bake_" + finish)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    n = nt.nodes.new
    geo = n("ShaderNodeNewGeometry")
    pos = geo.outputs["Position"]
    if pattern and pattern[0] == "grain":  # stretch the noise along Y (the long axis of planks) into grain
        stretch = n("ShaderNodeVectorMath")
        stretch.operation = "MULTIPLY"
        stretch.inputs[1].default_value = (1.0, pattern[1] * 0.1, 1.0)
        nt.links.new(pos, stretch.inputs[0])
        pos = stretch.outputs["Vector"]

    patches = n("ShaderNodeTexNoise")
    patches.inputs["Scale"].default_value = scale
    patches.inputs["Detail"].default_value = 6
    nt.links.new(pos, patches.inputs["Vector"])
    ramp = n("ShaderNodeValToRGB")  # hard-edged patches of the secondary colour
    ramp.color_ramp.elements[0].position = 0.62 - amount * 0.3
    ramp.color_ramp.elements[1].position = 0.66 - amount * 0.3
    nt.links.new(patches.outputs["Fac"], ramp.inputs["Fac"])

    base_col = srgb_to_linear(base)
    if pattern and pattern[0] == "stripes":  # diagonal stripes: fract((x + y + z) / (2 width)) > 0.5
        sep = n("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Position"], sep.inputs["Vector"])
        add1 = n("ShaderNodeMath")
        add1.operation = "ADD"
        nt.links.new(sep.outputs["X"], add1.inputs[0])
        nt.links.new(sep.outputs["Y"], add1.inputs[1])
        add2 = n("ShaderNodeMath")
        add2.operation = "ADD"
        nt.links.new(add1.outputs[0], add2.inputs[0])
        nt.links.new(sep.outputs["Z"], add2.inputs[1])
        div = n("ShaderNodeMath")
        div.operation = "DIVIDE"
        div.inputs[1].default_value = pattern[2] * 2
        nt.links.new(add2.outputs[0], div.inputs[0])
        frac = n("ShaderNodeMath")
        frac.operation = "FRACT"
        nt.links.new(div.outputs[0], frac.inputs[0])
        gt = n("ShaderNodeMath")
        gt.operation = "GREATER_THAN"
        gt.inputs[1].default_value = 0.5
        nt.links.new(frac.outputs[0], gt.inputs[0])
        stripe = n("ShaderNodeMix")
        stripe.data_type = "RGBA"
        stripe.inputs["A"].default_value = base_col
        stripe.inputs["B"].default_value = srgb_to_linear(pattern[1])
        nt.links.new(gt.outputs[0], stripe.inputs["Factor"])
        base_out = stripe.outputs["Result"]
    else:
        rgb = n("ShaderNodeRGB")
        rgb.outputs[0].default_value = base_col
        base_out = rgb.outputs[0]

    mix = n("ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(base_out, mix.inputs["A"])
    mix.inputs["B"].default_value = srgb_to_linear(second)
    nt.links.new(ramp.outputs["Color"], mix.inputs["Factor"])

    grain = n("ShaderNodeTexNoise")  # fine mottling so flat panels aren't flat
    grain.inputs["Scale"].default_value = 1.2
    grain.inputs["Detail"].default_value = 4
    nt.links.new(pos, grain.inputs["Vector"])
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
        wave.inputs["Scale"].default_value = 0.2
        wave.inputs["Distortion"].default_value = 18
        wave.inputs["Detail"].default_value = 4
        nt.links.new(geo.outputs["Position"], wave.inputs["Vector"])
        sramp = n("ShaderNodeValToRGB")
        sramp.color_ramp.elements[0].position = 0.97 - scratch * 0.05
        sramp.color_ramp.elements[1].position = 1.0
        nt.links.new(wave.outputs["Fac"], sramp.inputs["Fac"])
        smix = n("ShaderNodeMix")
        smix.data_type = "RGBA"
        smix.inputs["B"].default_value = srgb_to_linear((0.62, 0.62, 0.59))
        nt.links.new(sramp.outputs["Color"], smix.inputs["Factor"])
        nt.links.new(last, smix.inputs["A"])
        last = smix.outputs["Result"]

    ao = n("ShaderNodeAmbientOcclusion")  # baked AO, softened so props still read in dark maps
    ao.inputs["Distance"].default_value = 16
    ao.samples = 32
    aor = n("ShaderNodeMapRange")
    aor.inputs["To Min"].default_value = 0.45
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


def tiled_material(finish, scratch_image):
    """A tiled finish's own material, named as its SMD material. The bake needs an active image in every
    material, so it bakes into a throwaway image."""
    mat = bpy.data.materials.new(TILED[finish][0])
    mat.use_nodes = True
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = scratch_image
    mat.node_tree.nodes.active = tex
    return mat


# --- building the model ---

def make_objects(collection):
    obs = []
    for finish, bm in parts.items():
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.001)
        me = bpy.data.meshes.new(finish)
        bm.to_mesh(me)
        bm.free()
        me.uv_layers.new(name="UVMap")
        ob = bpy.data.objects.new(finish, me)
        collection.objects.link(ob)
        obs.append(ob)
    parts.clear()
    return obs


def join(name, obs, collection, image):
    scratch = bpy.data.images.new("scratch", 8, 8)
    for ob in obs:
        if ob.name in TILED:
            ob.data.materials.append(tiled_material(ob.name, scratch))
        else:
            ob.data.materials.append(bake_material(ob.name, image))
    bpy.ops.object.select_all(action="DESELECT")
    for ob in obs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    bpy.ops.object.join()
    ob = bpy.context.object
    ob.name = ob.data.name = name
    for c in list(ob.users_collection):
        c.objects.unlink(ob)
    collection.objects.link(ob)
    return ob


def unwrap(ob):
    """One UV atlas for the baked faces; planar, tiling UVs for the tiled ones."""
    tiled_slots = {i: m.name for i, m in enumerate(ob.data.materials) if not m.name.startswith("bake_")}
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(ob.data)
    for f in bm.faces:
        f.select_set(f.material_index not in tiled_slots)
    bmesh.update_edit_mesh(ob.data)
    bpy.ops.uv.smart_project(angle_limit=math.radians(50), island_margin=0.008, area_weight=0.0,
                             correct_aspect=True, scale_to_bounds=True)
    bpy.ops.object.mode_set(mode="OBJECT")

    by_material = {spec[0]: spec for spec in TILED.values()}
    uv = ob.data.uv_layers.active.data
    for poly in ob.data.polygons:
        if poly.material_index in tiled_slots:
            _, (ua, us, *uo), (va, vs, *vo) = by_material[tiled_slots[poly.material_index]]
            for li in poly.loop_indices:
                co = ob.data.vertices[ob.data.loops[li].vertex_index].co
                uv[li].uv = (co["xyz".index(ua.lstrip("-"))] * (-1 if ua[0] == "-" else 1) / us + (uo[0] if uo else 0),
                             co["xyz".index(va.lstrip("-"))] * (-1 if va[0] == "-" else 1) / vs + (vo[0] if vo else 0))


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


def finish_ref(name, ob):
    """Merge the bake materials into the one SMD material `name`, keep the tiled ones, and smooth by angle."""
    me = ob.data
    old = [m.name for m in me.materials]
    keep = [name] + sorted({m for m in old if not m.startswith("bake_")})
    remap = {i: keep.index(m) if m in keep else 0 for i, m in enumerate(old)}
    idx = [remap[p.material_index] for p in me.polygons]
    me.materials.clear()
    for m in keep:
        me.materials.append(bpy.data.materials.get(m) if m != name else bpy.data.materials.new(name))
    for p, i in zip(me.polygons, idx):
        p.material_index = i
    me.shade_smooth()
    me.set_sharp_from_angle(angle=math.radians(35))


def build_phys(name, collection):
    """Each convex piece is its own hull; they share no vertices, so studiomdl keeps them apart."""
    bm = bmesh.new()
    for pts in phys_parts:
        piece = bmesh.new()
        for p in pts:
            piece.verts.new(p)
        bmesh.ops.convex_hull(piece, input=piece.verts)
        bmesh.ops.remove_doubles(piece, verts=piece.verts, dist=0.001)
        bmesh.ops.dissolve_limit(piece, angle_limit=0.01, verts=piece.verts, edges=piece.edges)
        bmesh.ops.triangulate(piece, faces=piece.faces)
        bmesh.ops.recalc_face_normals(piece, faces=piece.faces)
        me = bpy.data.meshes.new("piece")
        piece.to_mesh(me)
        piece.free()
        bm.from_mesh(me)
    me = bpy.data.meshes.new(name + "_phys")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name + "_phys", me)
    collection.objects.link(ob)
    # smooth normals and no UVs keep each piece in one piece for studiomdl
    me.shade_smooth()
    me.materials.append(bpy.data.materials[name])
    return len(phys_parts)


def write_qc(name, out_dir, surfaceprop, mass, extra=(), note=""):
    rel = "assets_src/props/%s/%s.qc" % (name, name)
    lines = [
        "// Written by assets_src/props/build_props.py; compile with tools/wine/studiomdl.sh " + rel,
    ]
    if note:
        lines.append("// " + note)
    lines += [
        '$modelname "kart/props/%s.mdl"' % name,
        '$cdmaterials "models/kart/props/"',
        '$surfaceprop "%s"' % surfaceprop,
        "$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the prop faces +X",
        "$staticprop  // needed by prop_static; prop_dynamic takes it too",
        "",
        '$body body "%s.smd"' % name,
        '$sequence idle "%s.smd" fps 1' % name,
    ]
    lines += list(extra)
    lines += [
        "",
        '$collisionmodel "%s_phys.smd"' % name,
        "{",
        "\t$mass %g" % mass,
    ]
    if len(phys_parts) > 1:
        lines.append("\t$concave")
    lines += ["}", ""]
    with open(os.path.join(out_dir, name + ".qc"), "w") as f:
        f.write("\n".join(lines))


def build(name, geometry, surfaceprop, mass, qc_extra=(), note=""):
    """Run geometry() in a fresh scene, bake, export and write the QC into assets_src/props/<name>/."""
    reset()
    out_dir = os.path.join(HERE, name)
    os.makedirs(out_dir, exist_ok=True)
    scene = bpy.context.scene
    ref = bpy.data.collections.new(name)
    phys = bpy.data.collections.new(name + "_phys")
    for c in (ref, phys):
        scene.collection.children.link(c)

    geometry()
    image = bpy.data.images.new(name, TEX_SIZE, TEX_SIZE, alpha=False)
    ob = join(name, make_objects(ref), ref, image)
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    unwrap(ob)
    bake(ob, image, os.path.join(out_dir, name + ".png"))
    finish_ref(name, ob)
    pieces = build_phys(name, phys)
    lo = [min((ob.matrix_world @ Vector(c))[i] for c in ob.bound_box) for i in range(3)]
    hi = [max((ob.matrix_world @ Vector(c))[i] for c in ob.bound_box) for i in range(3)]

    export_smd(out_dir, [ref, phys])
    write_qc(name, out_dir, surfaceprop, mass, qc_extra, note)
    if "--blend" in sys.argv:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, name + ".blend"))
    print("built %s: %d triangles, %d collision pieces, bounds %s .. %s" % (
        name, tris, pieces, tuple(round(x, 1) for x in lo), tuple(round(x, 1) for x in hi)))
