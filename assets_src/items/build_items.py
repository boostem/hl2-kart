"""Build the kart item models: item box, hubcap, nitro can, oil slick, seeker and buffer.

    blender -b -P assets_src/items/build_items.py [-- <item> ...]   # all items, or only the ones named
    assets_src/items/build_all.sh                                    # this, then the textures and studiomdl

For each item this writes assets_src/items/<item>/: <item>.png (512 texture: procedural finishes and AO baked in
Cycles; not for the oil slick and the buffer, which only use drawn textures), <item>.smd, <item>_phys.smd (not for
the oil slick and the buffer, which never collide) and <item>.qc. make_materials.py draws the other textures.
See docs/modeling-guide.md for the models and where the game uses them.

Shapes, finishes and the bake come from the track dressing kit (assets_src/props/kit.py). 1 unit = 1 inch. Every
item faces +X. HL2 style, all original: steel and paint, worn chrome, Combine-ish blue-grey for the seeker.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "props"))
import kit  # noqa: E402
from kit import box, lathe, phys_box, phys_hull, prism, tiled, tube  # noqa: E402

kit.FINISHES.update({
    "crate": ((0.80, 0.47, 0.10), (0.36, 0.22, 0.13), 0.1, 0.35, 0.35),      # orange supply-crate paint, worn
    "crate_frame": ((0.20, 0.20, 0.19), (0.34, 0.22, 0.14), 0.12, 0.3, 0.4),  # dark steel banding
    "chrome": ((0.80, 0.80, 0.78), (0.55, 0.53, 0.49), 0.15, 0.1, 0.3),      # chrome, a little dull
    "nitro": ((0.10, 0.27, 0.62), (0.22, 0.20, 0.20), 0.15, 0.25, 0.5),      # blue bottle paint, chipped
    "nitro_band": ((0.80, 0.12, 0.08), (0.30, 0.15, 0.12), 0.2, 0.2, 0.3),   # red band
    "brass": ((0.68, 0.52, 0.22), (0.35, 0.27, 0.15), 0.2, 0.3, 0.3),        # valve
    "combine": ((0.27, 0.31, 0.35), (0.14, 0.15, 0.17), 0.12, 0.3, 0.35),    # blue-grey plating
})

# Drawn textures (make_materials.py), as SMD materials. "quad": each face built with quad_fit() gets the whole
# texture, (0, 0) at its first corner. "band": u runs round the Z axis (BAND_REPEAT times), v up the band.
FIT = {}
BAND_REPEAT = 6


def quad_fit(finish, material, corners):
    """One face with the whole texture `material` on it; corners counter-clockwise from the bottom left, seen
    from the side it faces."""
    tiled(finish, material, ("x", 1), ("y", 1))
    FIT[material] = "quad"
    kit.quad(finish, corners)


def fit_uvs(ob):
    """UVs of the FIT materials, after kit.unwrap()."""
    me = ob.data
    uv = me.uv_layers.active.data
    for poly in me.polygons:
        mode = FIT.get(me.materials[poly.material_index].name)
        if mode == "quad":
            for k, li in enumerate(poly.loop_indices):
                uv[li].uv = ((0, 0), (1, 0), (1, 1), (0, 1))[k % 4]
        elif mode == "band":
            cos = [me.vertices[me.loops[li].vertex_index].co for li in poly.loop_indices]
            angles = [math.atan2(c.y, c.x) % (2 * math.pi) for c in cos]
            if max(angles) - min(angles) > math.pi:  # across the seam
                angles = [a + 2 * math.pi if a < math.pi else a for a in angles]
            zs = [c.z for c in cos]
            for li, a, z in zip(poly.loop_indices, angles, zs):
                uv[li].uv = (a / (2 * math.pi) * BAND_REPEAT, (z - BAND_Z[0]) / (BAND_Z[1] - BAND_Z[0]))


def ring(r, z, n, start=0.0):
    return [(r * math.cos(start + 2 * math.pi * i / n), r * math.sin(start + 2 * math.pi * i / n), z)
            for i in range(n)]


ALONG_X = Matrix.Rotation(math.pi / 2, 4, "Y")   # for lathes round X: the profile's z becomes +X


def open_lathe(finish, profile, segments, matrix):
    """kit.lathe() for an open surface, which has no inside to point the faces away from: they face to the left
    of the profile's direction in (radius, z), e.g. up for a profile running outwards."""
    bm = kit.bm_for(finish)
    rings = [[bm.verts.new((0, 0, z))] if r == 0 else
             [bm.verts.new((r * math.cos(2 * math.pi * i / segments), r * math.sin(2 * math.pi * i / segments), z))
              for i in range(segments)] for r, z in profile]
    for a, b in zip(rings, rings[1:]):
        for i in range(segments):
            j = (i + 1) % segments
            if len(a) == 1:
                bm.faces.new((a[0], b[i], b[j]))
            elif len(b) == 1:
                bm.faces.new((a[i], b[0], a[j]))
            else:
                bm.faces.new((a[i], b[i], b[j], a[j]))
    bmesh.ops.transform(bm, matrix=matrix, verts=[v for r in rings for v in r])


# --- item box: a floating crate, steel banded, a glowing hazard symbol on every side ---

BOX = 20.0   # size; origin on the bottom face


def item_box():
    s, t = BOX / 2, 1.6   # half size, banding thickness
    box("crate", (-s + 0.6, -s + 0.6, 0.6), (s - 0.6, s - 0.6, BOX - 0.6))

    def band(sign, lo, hi):   # the banding's extent across one axis, at the low or high side
        return (hi - t, hi) if sign > 0 else (lo, lo + t)

    # banding along the twelve edges, proud of the panels
    for a in (-1, 1):
        for b in (-1, 1):
            (y0, y1), (z0, z1) = band(a, -s, s), band(b, 0, BOX)
            box("crate_frame", (-s, y0, z0), (s, y1, z1))
            box("crate_frame", (y0, -s, z0), (y1, s, z1))
            (x0, x1), (y0, y1) = band(a, -s, s), band(b, -s, s)
            box("crate_frame", (x0, y0, 0), (x1, y1, BOX))
    # corner caps
    for x in (-1, 1):
        for y in (-1, 1):
            for z in (0, 1):
                c = Vector((x * (s - 0.7), y * (s - 0.7), 0.7 + z * (BOX - 1.4)))
                box("crate_frame", c - Vector((1.2, 1.2, 1.2)), c + Vector((1.2, 1.2, 1.2)))
    # the symbol plates: the four sides and the top, just proud of the panels
    p, w, lo, hi = s - 0.45, s - t - 0.4, t + 0.4, BOX - t - 0.4
    for yaw in range(4):
        m = Matrix.Rotation(yaw * math.pi / 2, 4, "Z")
        quad_fit("symbol", "item_box_symbol", [m @ Vector(c) for c in
                                               ((p, -w, lo), (p, w, lo), (p, w, hi), (p, -w, hi))])
    top = BOX - 0.45
    quad_fit("symbol", "item_box_symbol", [(-w, -w, top), (w, -w, top), (w, w, top), (-w, w, top)])
    phys_box((-s, -s, 0), (s, s, BOX))


# --- hubcap: a chrome hubcap, lying flat (its axle is Z), centred on the origin ---

def hubcap():
    r = 9.0
    # dished face from the centre boss out over the rolled rim, then the dark underside
    open_lathe("chrome", [(0, 2.4), (1.4, 2.4), (1.7, 1.9), (2.6, 1.6), (6.0, 0.9), (7.6, 1.1), (8.6, 0.7),
                          (r, 0.0), (8.8, -0.7), (8.0, -0.8)], 24, Matrix())
    open_lathe("dark", [(8.0, -0.8), (7.2, -0.3), (0, -0.3)], 24, Matrix())
    # five raised spokes between the boss and the rim, five lug nuts round the boss
    for i in range(5):
        m = Matrix.Rotation(2 * math.pi * i / 5, 4, "Z")
        spoke = [(2.4, -0.7), (6.6, -1.2), (6.6, 1.2), (2.4, 0.7)]
        prism("chrome", spoke, 0.5, m @ Matrix.Translation((0, 0, 1.75)) @ Matrix.Rotation(0.2, 4, "Y"))
        a = 2 * math.pi * (i + 0.5) / 5
        tube("dark", (3.2 * math.cos(a), 3.2 * math.sin(a), 1.2), (3.2 * math.cos(a), 3.2 * math.sin(a), 2.1),
             0.55, 6, overlap=False)
    phys_hull(ring(r, -0.8, 12) + ring(r, 0.2, 12) + ring(2.6, 2.4, 6))


# --- nitro can: a dented gas bottle with a valve, a red band and a gauge; origin on its bottom ---

def dent(finish, angle, z, size, depth):
    """Push the side of the round body in around (angle, z): a dent `size` units across, `depth` deep."""
    bm = kit.bm_for(finish)
    for v in bm.verts:
        r = math.hypot(v.co.x, v.co.y)
        if r < 1.0:
            continue
        da = (math.atan2(v.co.y, v.co.x) - angle + math.pi) % (2 * math.pi) - math.pi
        d = math.hypot(da * r, v.co.z - z) / size
        if d < 1.0:
            k = 1.0 - depth * (1.0 - d * d) ** 2 / r
            v.co.x *= k
            v.co.y *= k


def nitro_can():
    r, n = 4.0, 16
    open_lathe("nitro", [(0, 13.6), (1.2, 13.6), (1.5, 13.5), (2.6, 13.0), (3.6, 12.0), (r, 10.4), (r, 9.0),
                         (r, 7.5), (r, 5.7)], n, Matrix())
    open_lathe("nitro_band", [(r, 5.7), (r + 0.05, 5.6), (r + 0.05, 3.7), (r, 3.6)], n, Matrix())
    open_lathe("nitro", [(r, 3.6), (r, 2.2), (r, 1.0), (3.8, 0.3), (3.4, 0), (0, 0)], n, Matrix())
    dent("nitro", 0.6, 8.2, 3.6, 0.6)
    dent("nitro", 2.9, 2.6, 2.6, 0.4)
    dent("nitro_band", 2.9, 3.9, 2.6, 0.3)
    # valve: brass neck, a hand wheel on top, an outlet stub to the back
    tube("brass", (0, 0, 13.3), (0, 0, 15.6), 0.85, 8, overlap=False)
    lathe("brass", [(1.6, 15.9), (2.1, 16.2), (1.6, 16.5), (1.1, 16.2)], 12, Matrix(), closed=True)
    for i in range(3):
        a = math.pi * i / 3
        tube("brass", (-1.7 * math.cos(a), -1.7 * math.sin(a), 16.2), (1.7 * math.cos(a), 1.7 * math.sin(a), 16.2),
             0.2, 4, overlap=False)
    tube("brass", (0, 0, 15.6), (0, 0, 16.4), 0.5, 6, overlap=False)
    tube("brass", (0, 0, 14.6), (-2.2, 0, 14.6), 0.45, 6, overlap=False)
    tube("dark", (-2.2, 0, 14.6), (-2.7, 0, 14.6), 0.6, 6, overlap=False)
    # pressure gauge on the shoulder, facing +X
    tube("dark", (2.2, 0, 13.2), (2.2, 0, 14.0), 0.3, 4, overlap=False)
    tube("chrome", (2.0, 0, 14.6), (3.0, 0, 14.6), 1.0, 10, overlap=False)
    tiled("gauge", "nitro_gauge", ("y", 1.8, 0.5), ("z", 1.8, 0.5 - 14.6 / 1.8))
    open_lathe("gauge", [(0, 3.02), (0.85, 3.02)], 10, Matrix.Translation((0, 0, 14.6)) @ ALONG_X)
    phys_hull(ring(r, 0, 8) + ring(r, 10.4, 8) + ring(1.5, 13.6, 6))
    phys_box((-1, -1, 13.3), (1, 1, 16.5))


# --- oil slick: a flat puddle on the ground (texture and normal map from make_materials.py) ---

OIL_RADIUS = 40.0   # half the texture's side; KART_OIL_RADIUS in kart_hazards_shared.h

# Wobbles of the puddle's outline (lobes, amplitude, phase), as a fraction of OIL_RADIUS. make_materials.py draws
# the texture's soft edge along the same outline.
OIL_OUTLINE = [(3, 0.07, 0.4), (5, 0.05, 1.9), (7, 0.03, 3.1), (11, 0.015, 0.7)]


def oil_outline(a):
    return 0.80 + sum(amp * math.sin(k * a + ph) for k, amp, ph in OIL_OUTLINE)


def oil_slick():
    n, z = 40, 0.4
    tiled("oil", "oil_puddle", ("x", 2 * OIL_RADIUS, 0.5), ("y", 2 * OIL_RADIUS, 0.5))
    bm = kit.bm_for("oil")
    centre = bm.verts.new((0, 0, z))
    rings = []
    for f in (0.5, 1.04):   # a little past the outline, so the texture's soft edge fits on the mesh
        rings.append([bm.verts.new((OIL_RADIUS * f * oil_outline(a) * math.cos(a),
                                    OIL_RADIUS * f * oil_outline(a) * math.sin(a), z))
                      for a in (2 * math.pi * i / n for i in range(n))])
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((centre, rings[0][i], rings[0][j]))
        bm.faces.new((rings[0][i], rings[1][i], rings[1][j], rings[0][j]))


# --- seeker: a small drone with a glowing eye, swept fins and a thruster; centred on the origin ---

def seeker():
    open_lathe("combine", [(2.9, 7.4), (4.3, 6.2), (5.2, 4.0), (5.5, 1.0), (5.4, -2.0), (4.8, -5.0), (3.8, -7.0),
                           (3.4, -7.4)], 12, ALONG_X)
    tiled("eye", "seeker_eye", ("y", 6.4, 0.5), ("z", 6.4, 0.5))
    open_lathe("eye", [(0, 9.0), (1.4, 8.8), (2.5, 8.2), (2.9, 7.4)], 12, ALONG_X)
    open_lathe("dark", [(3.4, -7.4), (3.0, -8.4), (2.2, -8.6), (2.2, -7.8)], 12, ALONG_X)   # thruster nozzle
    tiled("glow", "seeker_glow", ("y", 4.4, 0.5), ("z", 4.4, 0.5))
    open_lathe("glow", [(2.2, -7.8), (0, -7.6)], 12, ALONG_X)
    # armour band round the middle and a sensor spine on top
    open_lathe("dark", [(5.6, 2.2), (5.75, 1.6), (5.75, -0.6), (5.6, -1.2)], 12, ALONG_X)
    prism("dark", [(-4.0, 4.6), (3.0, 4.6), (1.0, 6.6), (-3.5, 6.6)], 0.8,
          Matrix.Translation((0, 0.4, 0)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
    # two swept fins drooping a little, and two short canards at the front
    for side in (-1, 1):
        fin = [(-6.5, 0), (-1.0, 0), (-4.0, 3.6), (-7.2, 3.6)]
        m = (Matrix.Diagonal((1, side, 1, 1)) @ Matrix.Translation((0, 4.4, -0.4)) @ Matrix.Rotation(-0.25, 4, "X")
             @ Matrix.Translation((0, 0, -0.35)))
        prism("combine", fin, 0.7, m)
        tube("dark", (4.2, side * 4.0, -0.8), (5.6, side * 6.2, -1.2), 0.45, 4)
    phys_hull([(9, 0, 0), (-8.6, 0, 0)] + [(x, 5.5 * math.cos(a), 5.5 * math.sin(a))
                                           for x in (-5, 4) for a in (i * math.pi / 4 for i in range(8))])


# --- buffer: the shield ring round a kart (see C_KartBuffer); centred on the kart hull's centre ---

BUFFER_A, BUFFER_B = 70.0, 48.0   # semi-axes along X and Y: round the 123 x 71 racer kart
BAND_Z = (-6.0, 6.0)


def buffer():
    n = 48
    tiled("band", "buffer_ring", ("x", 1), ("y", 1))
    FIT["buffer_ring"] = "band"
    bm = kit.bm_for("band")
    rows = []
    for z, grow in ((BAND_Z[0], 0.0), (0.0, 3.0), (BAND_Z[1], 0.0)):   # bulged out at the middle
        rows.append([bm.verts.new(((BUFFER_A + grow) * math.cos(a), (BUFFER_B + grow) * math.sin(a), z))
                     for a in (2 * math.pi * i / n for i in range(n))])
    for lo, hi in zip(rows, rows[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((lo[i], lo[j], hi[j], hi[i]))   # faces out; the material draws both sides


# --- building ---

BAKE_SCALE = 4.0

def write_qc(name, out_dir, surfaceprop, mass, extra=()):
    rel = "assets_src/items/%s/%s.qc" % (name, name)
    lines = [
        "// Written by assets_src/items/build_items.py; compile with tools/wine/studiomdl.sh " + rel,
        '$modelname "kart/items/%s.mdl"' % name,
        '$cdmaterials "models/kart/items/"',
        '$surfaceprop "%s"' % surfaceprop,
        "$origin 0 0 0 -90  // undo studiomdl's default 90 degree turn, so the item faces +X",
        "",
        '$body body "%s.smd"' % name,
        '$sequence idle "%s.smd" fps 1' % name,
    ]
    lines += list(extra)
    if kit.phys_parts:
        lines += ["", '$collisionmodel "%s_phys.smd"' % name, "{", "\t$mass %g" % mass]
        if len(kit.phys_parts) > 1:
            lines.append("\t$concave")
        lines.append("}")
    lines.append("")
    with open(os.path.join(out_dir, name + ".qc"), "w") as f:
        f.write("\n".join(lines))


def build(name, geometry, surfaceprop, mass, extra=()):
    """Run geometry() in a fresh scene, bake (if any face has a baked finish), export and write the QC into
    assets_src/items/<name>/."""
    kit.reset()
    FIT.clear()
    out_dir = os.path.join(HERE, name)
    os.makedirs(out_dir, exist_ok=True)
    scene = bpy.context.scene
    ref = bpy.data.collections.new(name)
    scene.collection.children.link(ref)

    geometry()
    baked = any(f not in kit.TILED for f in kit.parts)
    image = bpy.data.images.new(name, kit.TEX_SIZE, kit.TEX_SIZE, alpha=False)
    ob = kit.join(name, kit.make_objects(ref), ref, image)
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    kit.unwrap(ob)
    fit_uvs(ob)
    if baked:
        ob.scale = (BAKE_SCALE,) * 3   # the finishes are sized for props: bake them finer on these small items
        bpy.context.view_layer.update()
        kit.bake(ob, image, os.path.join(out_dir, name + ".png"))
        ob.scale = (1, 1, 1)
        bpy.context.view_layer.update()
    kit.finish_ref(name, ob)
    collections = [ref]
    if kit.phys_parts:
        phys = bpy.data.collections.new(name + "_phys")
        scene.collection.children.link(phys)
        kit.build_phys(name, phys)
        collections.append(phys)
    lo = [min((ob.matrix_world @ Vector(c))[i] for c in ob.bound_box) for i in range(3)]
    hi = [max((ob.matrix_world @ Vector(c))[i] for c in ob.bound_box) for i in range(3)]

    kit.export_smd(out_dir, collections)
    write_qc(name, out_dir, surfaceprop, mass, extra)
    if "--blend" in sys.argv:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, name + ".blend"))
    print("built %s: %d triangles, %d collision pieces, bounds %s .. %s" % (
        name, tris, len(kit.phys_parts), tuple(round(x, 1) for x in lo), tuple(round(x, 1) for x in hi)))


ITEMS = {
    "item_box": (item_box, "metal_box", 30),
    "hubcap": (hubcap, "metal", 8),
    "nitro_can": (nitro_can, "metal", 10),
    "oil_slick": (oil_slick, "default", 1),
    "seeker": (seeker, "metal", 15),
    "buffer": (buffer, "default", 1),
}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    names = [a for a in argv if not a.startswith("--")] or list(ITEMS)
    unknown = [n for n in names if n not in ITEMS]
    if unknown:
        sys.exit("unknown item(s): %s (have: %s)" % (", ".join(unknown), ", ".join(ITEMS)))
    for name in names:
        build(name, *ITEMS[name])


main()
