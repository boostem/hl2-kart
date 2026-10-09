#!/usr/bin/env python3
"""Writes kart_arena.vmf, the kart test arena (layout: docs/asset-pipeline.md, "Kart test arena").

usage: python3 assets_src/maps/kart_arena.py [out.vmf]   (default: kart_arena.vmf next to this script)

The VMF is plain text and committed; this script only saves typing the banked curve's and the ramps' planes by
hand. Edit the layout here and regenerate, or open the VMF in Hammer. Standard library only.
"""
import math
import os
import sys

# Materials (Source content by path; see docs/valve-content.md).
FLOOR = "concrete/concretefloor011a"
WALL = "concrete/concretewall004a"
ISLAND_TOP = "concrete/concretefloor020a"
ISLAND_SIDE = "concrete/concretewall008a"
RAMP = "concrete/concretefloor033a"
RAMP_SIDE = "concrete/concretewall004a"
BANK = "concrete/concretefloor020a"
NODRAW = "tools/toolsnodraw"
SKY = "tools/toolsskybox"

HALF = 3072        # interior is 6144 x 6144 between the sky walls
HEIGHT = 1024      # floor (z 0) to sky ceiling
SHELL = 64         # thickness of the sealing brushes
WALL_H = 128       # perimeter walls
INNER = HALF - SHELL   # drivable area is +-INNER
ISLAND = 1792      # central island spans +-ISLAND
LANE_MID = (INNER + ISLAND) // 2   # 2400, middle of each lane
SINK = -16         # brushes that sit on the floor start this far into it, so no face is degenerate


class Map:
    def __init__(self):
        self.next_id = 1
        self.solids = []
        self.entities = []

    def id(self):
        self.next_id += 1
        return self.next_id

    def prism(self, base, z0, top, mats):
        """A convex brush: polygon `base` (x, y) counter-clockwise from above, from z0 up to a planar top.

        `top` is one height per base vertex (they must lie on a plane). `mats` maps "top", "bottom" and
        "side" to materials."""
        n = len(base)
        bot = [(x, y, z0) for x, y in base]
        up = [(x, y, z) for (x, y), z in zip(base, top)]
        faces = [("bottom", bot[::-1]), ("top", up)]
        for i in range(n):
            j = (i + 1) % n
            faces.append(("side", [bot[i], bot[j], up[j], up[i]]))
        verts = bot + up
        centre = [sum(v[k] for v in verts) / len(verts) for k in range(3)]
        self.solids.append((self.id(), [(mats[kind], loop) for kind, loop in faces], centre))

    def box(self, x0, y0, z0, x1, y1, z1, mats):
        self.prism([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], z0, [z1] * 4, mats)

    def entity(self, classname, origin, **kv):
        self.entities.append((self.id(), classname, origin, kv))


def sub(a, b):
    return tuple(p - q for p, q in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return sum(p * q for p, q in zip(a, b))


def fmt(v):
    return "(%s)" % " ".join("%g" % c for c in v)


def plane(loop, centre):
    """Three points of a face in VMF order: (p3 - p1) x (p2 - p1) is the outward normal."""
    p1, p2, p3 = loop[0], loop[1], loop[2]
    normal = cross(sub(p3, p1), sub(p2, p1))
    face_c = [sum(v[k] for v in loop) / len(loop) for k in range(3)]
    if dot(normal, sub(face_c, centre)) < 0:
        p2, p3 = p3, p2
        normal = tuple(-c for c in normal)
    return (p1, p2, p3), normal


def tex_axes(normal):
    """Hammer's world-aligned texture axes for a face normal."""
    ax, ay, az = (abs(c) for c in normal)
    if az >= ax and az >= ay:
        return "[1 0 0 0] 0.25", "[0 -1 0 0] 0.25"
    if ax >= ay:
        return "[0 1 0 0] 0.25", "[0 0 -1 0] 0.25"
    return "[1 0 0 0] 0.25", "[0 0 -1 0] 0.25"


def build():
    m = Map()
    sky = {"top": SKY, "bottom": SKY, "side": SKY}

    # Shell: floor (seals the bottom), four sky walls and a sky ceiling.
    m.box(-HALF, -HALF, -SHELL, HALF, HALF, 0, {"top": FLOOR, "bottom": NODRAW, "side": NODRAW})
    m.box(-HALF, -HALF, HEIGHT, HALF, HALF, HEIGHT + SHELL, sky)
    out = HALF + SHELL
    m.box(-out, -out, -SHELL, -HALF, out, HEIGHT + SHELL, sky)
    m.box(HALF, -out, -SHELL, out, out, HEIGHT + SHELL, sky)
    m.box(-HALF, -out, -SHELL, HALF, -HALF, HEIGHT + SHELL, sky)
    m.box(-HALF, HALF, -SHELL, HALF, out, HEIGHT + SHELL, sky)

    # Perimeter walls, 128 high, inside the sky walls.
    wall = {"top": WALL, "bottom": NODRAW, "side": WALL}
    m.box(-HALF, -HALF, 0, -INNER, HALF, WALL_H, wall)
    m.box(INNER, -HALF, 0, HALF, HALF, WALL_H, wall)
    m.box(-INNER, -HALF, 0, INNER, -INNER, WALL_H, wall)
    m.box(-INNER, INNER, 0, INNER, HALF, WALL_H, wall)

    # Central island: the track is the 1216-wide lane around it, driven counter-clockwise seen from above.
    m.box(-ISLAND, -ISLAND, 0, ISLAND, ISLAND, WALL_H, {"top": ISLAND_TOP, "bottom": NODRAW, "side": ISLAND_SIDE})

    # Banked curve in the south-east corner, from the south straight into the east lane, around the island's
    # corner. The outer 416 units of the turn are banked, rising to 128 at the outer wall (~17 degrees);
    # the inner 800 stay flat. Each piece is two triangular prisms, so every top face is planar.
    bank = {"top": BANK, "bottom": NODRAW, "side": RAMP_SIDE}
    cx, cy, r_in, r_out, rise = ISLAND, -ISLAND, 800, INNER - ISLAND, 128

    def bank_piece(i0, o0, h0, i1, o1, h1):
        # i*/o*: inner/outer edge points (x, y); h*: outer edge height; inner edge is on the floor (z 0).
        for tri, hts in (((i0, o0, o1), (0, h0, h1)), ((i0, o1, i1), (0, h1, 0))):
            pts = [tuple(int(round(c)) for c in p) for p in tri]
            # counter-clockwise from above
            if cross(sub(pts[1] + (0,), pts[0] + (0,)), sub(pts[2] + (0,), pts[0] + (0,)))[2] < 0:
                pts, hts = pts[::-1], hts[::-1]
            m.prism(pts, SINK, list(hts), bank)

    # Lead-in along the south straight: the bank grows from flat to full over 640 units.
    lead = 640
    ys = (cy - r_in, cy - r_out)
    xs = [cx - lead, cx - lead // 2, cx]
    hs = [0, rise // 2, rise]
    for k in range(2):
        bank_piece((xs[k], ys[0]), (xs[k], ys[1]), hs[k], (xs[k + 1], ys[0]), (xs[k + 1], ys[1]), hs[k + 1])
    # The curve: 12 segments from due south (-90 degrees) to due east (0).
    segs = 12
    for k in range(segs):
        a0 = math.radians(-90 + 90 * k / segs)
        a1 = math.radians(-90 + 90 * (k + 1) / segs)
        bank_piece((cx + r_in * math.cos(a0), cy + r_in * math.sin(a0)), (cx + r_out * math.cos(a0), cy + r_out * math.sin(a0)), rise,
                   (cx + r_in * math.cos(a1), cy + r_in * math.sin(a1)), (cx + r_out * math.cos(a1), cy + r_out * math.sin(a1)), rise)
    # Lead-out up the east lane, back down to flat.
    xs = (cx + r_in, cx + r_out)
    ys = [cy, cy + lead // 2, cy + lead]
    hs = [rise, rise // 2, 0]
    for k in range(2):
        bank_piece((xs[0], ys[k]), (xs[1], ys[k]), hs[k], (xs[0], ys[k + 1]), (xs[1], ys[k + 1]), hs[k + 1])

    # Gentle ramp in the east lane, driven north: up 96 over 544 (~10 degrees), a 384 plateau, down again.
    ramp = {"top": RAMP, "bottom": NODRAW, "side": RAMP_SIDE}
    rx0, rx1 = LANE_MID - 384, LANE_MID + 384
    y0, run, flat, h = -768, 544, 384, 96
    m.prism([(rx0, y0), (rx1, y0), (rx1, y0 + run), (rx0, y0 + run)], SINK, [0, 0, h, h], ramp)
    m.box(rx0, y0 + run, SINK, rx1, y0 + run + flat, h, ramp)
    y2 = y0 + run + flat
    m.prism([(rx0, y2), (rx1, y2), (rx1, y2 + run), (rx0, y2 + run)], SINK, [h, h, 0, 0], ramp)

    # Jump in the north lane, driven west: a kicker rising 112 over 240 (~25 degrees) that ends in a drop, a
    # 592 gap, then a landing ramp from 64 down to the floor over 768 (~4.8 degrees).
    jy0, jy1 = LANE_MID - 320, LANE_MID + 320
    kx0, kx1 = 656, 896
    m.prism([(kx0, jy0), (kx1, jy0), (kx1, jy1), (kx0, jy1)], SINK, [112, 0, 0, 112], ramp)
    lx0, lx1 = -704, 64
    m.prism([(lx0, jy0), (lx1, jy0), (lx1, jy1), (lx0, jy1)], SINK, [0, 64, 64, 0], ramp)

    # Spawns: 8 in a row across the west end of the south straight, facing east down it.
    sx = -ISLAND - 640
    for i in range(8):
        m.entity("info_player_deathmatch", (sx, -INNER + 160 + i * 128, 8), angles="0 0 0")

    def prop(model, origin, yaw=0):
        m.entity("prop_static", origin, model=model, angles="0 %d 0" % yaw, solid="6", skin="0",
                 fademindist="-1", fadescale="1", disableshadows="0")

    def physics_prop(model, origin, yaw=0):
        # Models with physics-only collision (the cone) can't be prop_static; karts can knock these over.
        m.entity("prop_physics_multiplayer", origin, model=model, angles="0 %d 0" % yaw, skin="0",
                 physicsmode="1", fademindist="-1", fadescale="1")

    cone = "models/props_junk/trafficcone001a.mdl"
    barrier = "models/props_c17/concrete_barrier001a.mdl"
    lamp = "models/props_c17/lamppost03a_off.mdl"
    # Start line: a cone at each end of the spawn row.
    physics_prop(cone, (sx + 96, -INNER + 48, 0))
    physics_prop(cone, (sx + 96, -ISLAND - 48, 0))
    # Cones down the middle of the straight.
    for x in range(sx + 640, ISLAND - 640, 512):
        physics_prop(cone, (x, -LANE_MID, 0))
    # Barriers at the island's three square corners, at 45 degrees.
    for (x, y), yaw in (((-ISLAND - 96, -ISLAND - 96), 45), ((-ISLAND - 96, ISLAND + 96), -45), ((ISLAND + 96, ISLAND + 96), 45)):
        prop(barrier, (x, y, 0), yaw)
    # Barriers beside the jump's landing zone.
    for x in (lx0 + 128, lx0 + 384, lx0 + 640):
        prop(barrier, (x, jy0 - 64, 0), 0)
        prop(barrier, (x, jy1 + 64, 0), 0)
    # Lampposts in the arena's corners and the middle of each island side.
    c = INNER - 96
    for x, y in ((-c, -c), (c, -c), (c, c), (-c, c)):
        prop(lamp, (x, y, 0), int(math.degrees(math.atan2(-y, -x))))
    for x, y, yaw in ((0, -ISLAND - 32, -90), (ISLAND + 32, 0, 0), (0, ISLAND + 32, 90), (-ISLAND - 32, 0, 180)):
        prop(lamp, (x, y, 0), yaw)

    m.entity("light_environment", (0, 0, 768), angles="0 225 0", pitch="-50",
             _light="255 238 214 350", _lightHDR="-1 -1 -1 1", _lightscaleHDR="1",
             _ambient="150 170 200 90", _ambientHDR="-1 -1 -1 1", _AmbientScaleHDR="1", SunSpreadAngle="5")
    m.entity("env_cubemap", (-ISLAND - 256, -LANE_MID, 64), cubemapsize="0")
    m.entity("env_cubemap", (0, LANE_MID, 64), cubemapsize="0")
    return m


def write(m, f):
    w = f.write
    w('versioninfo\n{\n\t"editorversion" "400"\n\t"editorbuild" "8864"\n\t"mapversion" "1"\n'
      '\t"formatversion" "100"\n\t"prefab" "0"\n}\n')
    w('visgroups\n{\n}\n')
    w('viewsettings\n{\n\t"bSnapToGrid" "1"\n\t"bShowGrid" "1"\n\t"bShowLogicalGrid" "0"\n'
      '\t"nGridSpacing" "64"\n\t"bShow3DGrid" "0"\n}\n')
    w('world\n{\n\t"id" "1"\n\t"mapversion" "1"\n\t"classname" "worldspawn"\n\t"skyname" "sky_day01_01"\n'
      '\t"maxpropscreenwidth" "-1"\n\t"detailvbsp" "detail.vbsp"\n\t"detailmaterial" "detail/detailsprites"\n')
    for sid, faces, centre in m.solids:
        w('\tsolid\n\t{\n\t\t"id" "%d"\n' % sid)
        for mat, loop in faces:
            pts, normal = plane(loop, centre)
            u, v = tex_axes(normal)
            w('\t\tside\n\t\t{\n\t\t\t"id" "%d"\n\t\t\t"plane" "%s"\n\t\t\t"material" "%s"\n'
              '\t\t\t"uaxis" "%s"\n\t\t\t"vaxis" "%s"\n\t\t\t"rotation" "0"\n\t\t\t"lightmapscale" "16"\n'
              '\t\t\t"smoothing_groups" "0"\n\t\t}\n' % (m.id(), " ".join(fmt(p) for p in pts), mat.upper(), u, v))
        w('\t\teditor\n\t\t{\n\t\t\t"color" "0 180 220"\n\t\t\t"visgroupshown" "1"\n\t\t\t"visgroupautoshown" "1"\n\t\t}\n\t}\n')
    w('}\n')
    for eid, classname, origin, kv in m.entities:
        w('entity\n{\n\t"id" "%d"\n\t"classname" "%s"\n' % (eid, classname))
        for k, v in kv.items():
            w('\t"%s" "%s"\n' % (k, v))
        w('\t"origin" "%s"\n' % " ".join("%g" % c for c in origin))
        w('\teditor\n\t{\n\t\t"color" "220 30 220"\n\t\t"visgroupshown" "1"\n\t\t"visgroupautoshown" "1"\n'
          '\t\t"logicalpos" "[0 0]"\n\t}\n}\n')
    w('cameras\n{\n\t"activecamera" "-1"\n}\n')
    w('cordon\n{\n\t"mins" "(-1024 -1024 -1024)"\n\t"maxs" "(1024 1024 1024)"\n\t"active" "0"\n}\n')


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "kart_arena.vmf")
    with open(path, "w") as f:
        write(build(), f)
    print("wrote", path)
