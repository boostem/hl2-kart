"""Build the track dressing kit: tyre walls, ramps, finish gantry, start lights, sign boards and the boost pad.

    blender -b -P assets_src/props/build_props.py [-- <prop> ...]   # all props, or only the ones named
    assets_src/props/build_all.sh                                    # this, then the textures and studiomdl

For each prop this writes assets_src/props/<prop>/: <prop>.png (512 texture: procedural finishes and AO baked in
Cycles), <prop>.smd, <prop>_phys.smd and <prop>.qc. See docs/dressing-kit.md for the props and how to place them.

1 unit = 1 inch. Every prop faces +X with its origin on the floor (the tyre wall corner: on its pivot).
HL2 wasteland style: rusted steel, weathered wood, worn tyres, faded paint.
"""
import math
import os
import random
import sys

from mathutils import Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit  # noqa: E402
from kit import box, hull, lathe, phys_box, phys_hull, prism, quad, text, tiled, tube  # noqa: E402

# --- tyre walls ---

TYRE_R = 12.8       # outer radius; 5 tyres make a 128-unit straight
TYRE_H = 8.0        # height of one tyre lying flat
TYRE_LAYERS = 4     # 32 units tall
CORNER_R = 128.0    # the corner's centre line radius


def tyre(x, y, z, rng, finish):
    """A tyre lying flat, its bottom at z. Slightly off-centre and squashed so a stack doesn't look machined."""
    r, h = TYRE_R - rng.uniform(0, 0.4), TYRE_H - rng.uniform(0, 0.3)
    profile = [(r * 0.55, 0.5), (r * 0.82, 0), (r, h * 0.22), (r, h * 0.78), (r * 0.82, h), (r * 0.55, h - 0.5)]
    m = (Matrix.Translation((x + rng.uniform(-0.5, 0.5), y + rng.uniform(-0.5, 0.5), z))
         @ Matrix.Rotation(rng.uniform(0, math.pi), 4, "Z"))
    lathe(finish, profile, 10, m, closed=True)


def tyre_stack(points, rng):
    """Stacks of TYRE_LAYERS tyres at points; the top layer is painted red and white in turn."""
    for i, (x, y) in enumerate(points):
        for layer in range(TYRE_LAYERS):
            finish = ("tyre_red" if i % 2 else "tyre_white") if layer == TYRE_LAYERS - 1 else "rubber"
            tyre(x, y, layer * TYRE_H, rng, finish)


def tire_wall_straight():
    rng = random.Random("tire_wall_straight")
    tyre_stack([(-51.2 + 25.6 * i, 0) for i in range(5)], rng)
    phys_box((-64, -TYRE_R, 0), (64, TYRE_R, TYRE_LAYERS * TYRE_H))


def tire_wall_corner():
    """A quarter circle of radius 128 round the origin, from (0, -128) to (128, 0)."""
    rng = random.Random("tire_wall_corner")
    n = 8
    pts = []
    for i in range(n):
        a = math.radians(-90 + 90 * (i + 0.5) / n)
        pts.append((CORNER_R * math.cos(a), CORNER_R * math.sin(a)))
    tyre_stack(pts, rng)
    pieces = 4
    for k in range(pieces):  # each piece: the ring sector between two angles, as a convex hull
        pts = []
        for a in (k * 90 / pieces - 90, (k + 1) * 90 / pieces - 90):
            for r in (CORNER_R - TYRE_R, CORNER_R + TYRE_R / math.cos(math.radians(45 / pieces))):
                for z in (0, TYRE_LAYERS * TYRE_H):
                    pts.append((r * math.cos(math.radians(a)), r * math.sin(math.radians(a)), z))
        phys_hull(pts)


# --- ramps: a kart driving +X goes up the deck and leaves over the back edge ---

def ramp(length, width, height):
    def geometry():
        L, W, H = length, width, height
        x0, x1 = -L / 2, L / 2
        t = 1.5                              # deck thickness
        lip = min(12.0, L * 0.12)            # hazard-painted strip along the top edge
        slope = H / L

        def deck_z(x):
            return (x - x0) * slope

        to_xz = Matrix.Translation((0, W / 2, 0)) @ Matrix.Rotation(math.pi / 2, 4, "X")  # outline (x, z), depth -Y
        xl = x1 - lip
        prism("plate", [(x0, 0), (xl, deck_z(xl)), (xl, deck_z(xl) - t), (x0 + t / slope, 0)], W, to_xz)
        prism("hazard", [(xl, deck_z(xl)), (x1, H), (x1, H - t), (xl, deck_z(xl) - t)], W, to_xz)

        # side frames: rusty triangles under the deck edges, angle-iron runners and posts at the back
        for y in (W / 2 - 1, -W / 2 + 3):
            side = Matrix.Translation((0, y, 0)) @ Matrix.Rotation(math.pi / 2, 4, "X")
            prism("rust", [(x0 + t / slope + 2, 0), (x1 - 1, 0), (x1 - 1, H - t - 0.5)], 2, side)
        posts = max(2, int(W // 48) + 1)
        for i in range(posts):
            y = -W / 2 + 3 + (W - 6) * i / (posts - 1)
            box("steel", (x1 - 3, y - 1.5, 0), (x1, y + 1.5, H - t))                 # back post
            tube("steel", (x0 + L * 0.35, y, 2), (x1 - 2, y, H - t - 1), 0.9, 4)   # brace under the deck
            box("steel", (x0 + t / slope, y - 1, 0), (x1, y + 1, 1.2))               # floor runner
        box("rust", (x1 - 2.5, -W / 2 + 1, H * 0.45), (x1 - 0.5, W / 2 - 1, H * 0.45 + 3))  # back cross bar
        if H >= 32:
            for i in range(posts - 1):  # X braces between the back posts
                ya = -W / 2 + 3 + (W - 6) * i / (posts - 1)
                yb = -W / 2 + 3 + (W - 6) * (i + 1) / (posts - 1)
                tube("rust", (x1 - 1.5, ya, 2), (x1 - 1.5, yb, H * 0.45), 0.8, 4)
        # bolt heads along the deck's side edges
        for i in range(int(L // 24)):
            x = x0 + 12 + 24 * i + t / slope
            if x > x1 - lip:
                break
            for y in (W / 2 - 2, -W / 2 + 2):
                tube("steel", (x, y, deck_z(x) - 0.2), (x, y, deck_z(x) + 0.5), 0.8, 6, overlap=False)

        phys_hull([(x, y, z) for x, z in ((x0, 0), (x1, 0), (x1, H)) for y in (-W / 2, W / 2)])
    return geometry


# --- finish gantry: a truss beam on two lattice towers, an orange logo board and a chequered band ---

GANTRY_W = 512      # outside width; 464 between the towers
GANTRY_CLEAR = 160  # clearance under the beam
TOWER = 24
BEAM_H = 32


def lattice(p0, p1, side_a, side_b, bays, r):
    """A square lattice tower/beam from p0 to p1 with chords offset by side_a and side_b (vectors) and zigzag
    braces on each face."""
    from mathutils import Vector
    p0, p1 = Vector(p0), Vector(p1)
    corners = [side_a + side_b, side_a - side_b, -side_a - side_b, -side_a + side_b]
    for c in corners:
        tube("steel", p0 + c, p1 + c, r, 4)
    for f in range(4):
        a, b = corners[f], corners[(f + 1) % 4]
        for i in range(bays):
            q0 = p0 + (p1 - p0) * (i / bays)
            q1 = p0 + (p1 - p0) * ((i + 1) / bays)
            tube("rust", q0 + (a if i % 2 else b), q1 + (b if i % 2 else a), r * 0.6, 4)


def x_face(finish, x, s, y0, y1, z0, z1):
    """A rectangle in the plane x facing +X (s = 1) or -X (s = -1)."""
    if s > 0:
        quad(finish, [(x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1)])
    else:
        quad(finish, [(x, y1, z0), (x, y0, z0), (x, y0, z1), (x, y1, z1)])


def finish_gantry():
    from mathutils import Vector
    half = GANTRY_W / 2 - TOWER / 2           # tower centre lines
    inner = half - TOWER / 2                  # inside faces of the towers
    top = GANTRY_CLEAR + BEAM_H
    tiled("checker", "checker", ("y", 32), ("z", 32))
    for s in (1, -1):
        y = s * half
        lattice((0, y, 2), (0, y, top), Vector((TOWER / 2 - 1.5, 0, 0)), Vector((0, TOWER / 2 - 1.5, 0)), 6, 1.6)
        box("concrete", (-TOWER, y - TOWER, 0), (TOWER, y + TOWER, 4))            # footing
        box("steel", (-TOWER / 2 - 1, y - TOWER / 2 - 1, 4), (TOWER / 2 + 1, y + TOWER / 2 + 1, 5))  # base plate
        for bx in (-1, 1):
            for by in (-1, 1):
                tube("dark", (bx * 9, y + by * 9, 4), (bx * 9, y + by * 9, 6.5), 0.9, 6, overlap=False)
        box("steel", (-TOWER / 2 - 1, y - TOWER / 2 - 1, top - 1), (TOWER / 2 + 1, y + TOWER / 2 + 1, top + 1))
    zc = GANTRY_CLEAR + BEAM_H / 2
    lattice((0, -half, zc), (0, half, zc), Vector((TOWER / 2 - 1.5, 0, 0)), Vector((0, 0, BEAM_H / 2 - 1.5)), 16, 1.6)

    # chequered band hanging under the beam, across the track
    band0, band1, bd = GANTRY_CLEAR - 16, GANTRY_CLEAR + 0.5, 13
    box("dark", (-bd, -inner, band0), (bd, inner, band1))
    # the logo board, on the beam's middle: one solid block with a face each way
    bw, bh, z0, d = 300, 60, GANTRY_CLEAR - 2, 15
    box("board", (-d, -bw / 2, z0), (d, bw / 2, z0 + bh))
    for s in (1, -1):
        x_face("checker", s * (bd + 0.05), s, -inner, inner, band0, GANTRY_CLEAR - 2)
        face = s * d
        # edging round the board's face
        for lo, hi in (((-bw / 2 - 1.5, z0 - 1.5), (bw / 2 + 1.5, z0)), ((-bw / 2 - 1.5, z0 + bh), (bw / 2 + 1.5, z0 + bh + 1.5)),
                       ((-bw / 2 - 1.5, z0), (-bw / 2, z0 + bh)), ((bw / 2, z0), (bw / 2 + 1.5, z0 + bh))):
            box("dark", (min(face, face + s * 0.8), lo[0], lo[1]), (max(face, face + s * 0.8), hi[0], hi[1]))
        # local X along the reading direction, Z out of the board: seen from -X it reads towards -Y, from +X +Y
        m = Matrix.Translation((face, 0, z0)) @ Matrix(((0, 0, s, 0), (s, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        text("letters", "SCRAPYARD GP", 30, 1.2, m @ Matrix.Translation((0, bh * 0.58, 0)), width=bw - 90)
        text("dark", "RUST BELT CIRCUIT", 8, 0.8, m @ Matrix.Translation((0, 10, 0)), width=bw - 140)
        for e in (-1, 1):  # chequered flags at the board's ends
            yc = e * (bw / 2 - 22)
            x_face("checker", face + s * 0.05, s, yc - 14, yc + 14, z0 + 18, z0 + 46)
    phys_box((-TOWER / 2, half - TOWER / 2, 0), (TOWER / 2, half + TOWER / 2, top + 1))
    phys_box((-TOWER / 2, -half - TOWER / 2, 0), (TOWER / 2, -half + TOWER / 2, top + 1))
    phys_box((-d, -inner + 0.5, band0), (d, inner - 0.5, top))


# --- start lights: a pole with a three-lamp head; the skin picks the lit lamp ---

LAMPS = (("lamp_red", 140), ("lamp_yellow", 122), ("lamp_green", 104))   # top to bottom, lamp centre heights
LAMP_R = 7.0
HEAD = (-6, 6, -11, 11, 92, 152)   # x0, x1, y0, y1, z0, z1


def start_lights():
    x0, x1, y0, y1, z0, z1 = HEAD
    box("concrete", (-14, -14, 0), (14, 14, 3))
    box("dark", (-9, -9, 3), (9, 9, 4))
    for bx in (-1, 1):
        for by in (-1, 1):
            tube("steel", (bx * 6.5, by * 6.5, 4), (bx * 6.5, by * 6.5, 6), 0.8, 6, overlap=False)
    box("steel", (-3, -3, 4), (3, 3, z0))                                  # square pole
    for a in (0, 90, 180, 270):                                             # gussets at the foot
        r = Matrix.Rotation(math.radians(a), 4, "Z")
        prism("steel", [(3, 0), (8, 0), (3, 10)], 0.8, r @ Matrix.Translation((0, 0.4, 4)) @
              Matrix.Rotation(math.pi / 2, 4, "X"))
    box("dark", (x0, y0, z0), (x1, y1, z1))                                 # lamp head
    box("steel", (x0 - 1, y0 - 1, z1), (x1 + 1, y1 + 1, z1 + 1.5))         # cap
    box("rust", (x0 - 1, y0 - 1, z0 - 1.5), (x1 + 1, y1 + 1, z0))          # bottom collar
    box("hazard", (x1, y0, z0 + 1), (x1 + 0.4, y1, z0 + 7))                 # striped band under the lamps
    tube("steel", (x0 - 0.5, 0, z1 + 1.5), (x0 - 0.5, 0, z1 + 9), 0.6, 6, overlap=False)   # antenna
    for mat, zc in LAMPS:
        tiled(mat, mat, ("y", 2 * LAMP_R, 0.5), ("z", 2 * LAMP_R, 0.5 - zc / (2 * LAMP_R)))
        m = Matrix.Translation((x1, 0, zc)) @ Matrix.Rotation(math.pi / 2, 4, "Y")   # local Z points +X
        lathe(mat, [(0, 1.6), (LAMP_R * 0.55, 1.35), (LAMP_R * 0.85, 0.85), (LAMP_R, 0.2), (LAMP_R, 0), (0, 0)], 12, m)
        lathe("steel", [(LAMP_R, 0.2), (LAMP_R + 1.2, 0.6), (LAMP_R + 1.2, 0), (LAMP_R, 0)], 12, m, closed=True)
        # visor: a curved hood over the top half of the lamp, shorter at the sides
        for i in range(6):
            pts = []
            for a in (math.pi * i / 6, math.pi * (i + 1) / 6):
                d = 6 * (1 - 0.3 * abs(math.cos(a)))
                for r in (LAMP_R + 1.2, LAMP_R + 1.8):
                    for x in (x1, x1 + d):
                        pts.append((x, r * math.cos(a), zc + r * math.sin(a)))
            hull("dark", pts)
    phys_box((-14, -14, 0), (14, 14, 4))
    phys_box((-3, -3, 4.5), (3, 3, z0 - 2))
    phys_box((x0 - 1, y0 - 1, z0 - 1.5), (x1 + 7, y1 + 1, z1 + 1.5))


# --- sign boards: a painted board on two wooden posts ---

SIGN_W, SIGN_H, SIGN_Z = 72, 32, 14   # board size and the height of its bottom edge


def sign_frame(board_finish):
    t = 2.0
    box(board_finish, (-t, -SIGN_W / 2, SIGN_Z), (0, SIGN_W / 2, SIGN_Z + SIGN_H))
    box("rust", (-t - 0.3, -SIGN_W / 2 - 1, SIGN_Z - 1), (0.3, SIGN_W / 2 + 1, SIGN_Z))           # steel edging
    box("rust", (-t - 0.3, -SIGN_W / 2 - 1, SIGN_Z + SIGN_H), (0.3, SIGN_W / 2 + 1, SIGN_Z + SIGN_H + 1))
    for y in (-SIGN_W / 2 + 10, SIGN_W / 2 - 10):
        box("wood", (-t - 4, y - 2, 0), (-t, y + 2, SIGN_Z + SIGN_H + 4))                        # post
        box("wood", (-t - 22, y - 1, 0), (-t - 4, y + 1, 3))                                     # foot
        tube("wood", (-t - 17, y, 2), (-t - 4, y, 18), 1.0, 4)                              # strut
        for z in (SIGN_Z + 6, SIGN_Z + SIGN_H - 6):
            tube("steel", (0, y, z), (0.6, y, z), 0.9, 6, overlap=False)                       # bolts
    phys_box((-t - 4, -SIGN_W / 2 - 1, 0), (0.6, SIGN_W / 2 + 1, SIGN_Z + SIGN_H + 4))


def chevrons(direction):
    """Three raised chevrons on the board's +X face pointing to the viewer's left (direction -1) or right (+1).
    Seen from +X the viewer's right is +Y."""
    w, h, th = 9.0, 10.0, 5.0
    zc = SIGN_Z + SIGN_H / 2
    for k in (-1, 0, 1):
        tip = k * 18 + direction * w / 2
        back = tip - direction * w
        # two convex arms (u along Y, v along Z), extruded 0.5 out of the board
        to_face = Matrix.Translation((0, 0, zc)) @ Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        for sv in (1, -1):
            arm = [(tip, 0), (tip - direction * th, 0), (back - direction * th, sv * h), (back, sv * h)]
            prism("sign_paint", arm, 0.5, to_face)


def sign_arrow_left():
    sign_frame("sign")
    chevrons(-1)


def sign_arrow_right():
    sign_frame("sign")
    chevrons(1)


def sign_hazard():
    sign_frame("hazard")


# --- boost pad: a low steel plate with a glowing, scrolling chevron panel ---

PAD_L, PAD_W, PAD_H, PAD_BEVEL = 128, 96, 1.5, 4.0


def boost_pad():
    L, W, H, b = PAD_L / 2, PAD_W / 2, PAD_H, PAD_BEVEL
    lo = [(-L, -W, 0), (L, -W, 0), (L, W, 0), (-L, W, 0)]
    hi = [(-L + b, -W + b, H), (L - b, -W + b, H), (L - b, W - b, H), (-L + b, W - b, H)]
    for i in range(4):  # sloped edges: the ones karts drive over (front and back) striped
        j = (i + 1) % 4
        quad("hazard" if i in (1, 3) else "plate", [lo[i], lo[j], hi[j], hi[i]])
    # top: a steel frame round the glow panel
    gx, gy = L - b - 6, W - b - 10
    inner = [(-gx, -gy, H), (gx, -gy, H), (gx, gy, H), (-gx, gy, H)]
    for i in range(4):
        j = (i + 1) % 4
        quad("plate", [hi[i], hi[j], inner[j], inner[i]])
    tiled("glow", "boost_pad_glow", ("x", 64), ("y", 64, 0.5))
    quad("glow", [(-gx, -gy, H + 0.05), (gx, -gy, H + 0.05), (gx, gy, H + 0.05), (-gx, gy, H + 0.05)])
    for x in (-L + b + 3, L - b - 3):
        for y in (-W + b + 3, W - b - 3):
            tube("steel", (x, y, H - 0.2), (x, y, H + 0.4), 0.9, 6, overlap=False)
    quad("plate", [lo[3], lo[2], lo[1], lo[0]])  # bottom
    phys_hull(lo + hi)


PROPS = {
    "tire_wall_straight": (tire_wall_straight, "rubbertire", 400),
    "tire_wall_corner": (tire_wall_corner, "rubbertire", 600),
    "ramp_small": (ramp(64, 96, 16), "metalpanel", 150),
    "ramp_medium": (ramp(128, 128, 32), "metalpanel", 300),
    "ramp_large": (ramp(192, 160, 64), "metalpanel", 600),
    "finish_gantry": (finish_gantry, "metal", 2000),
    "start_lights": (start_lights, "metal", 200),
    "sign_arrow_left": (sign_arrow_left, "wood_panel", 40),
    "sign_arrow_right": (sign_arrow_right, "wood_panel", 40),
    "sign_hazard": (sign_hazard, "wood_panel", 40),
    "boost_pad": (boost_pad, "metal", 200),
}

# Start lights skins: 0 all off, 1 red, 2 yellow, 3 green. Each lamp's material swaps for its lit version.
SKINS = [
    "",
    "// Skins: 0 all lamps off, 1 red lit, 2 yellow lit, 3 green lit (set the entity's skin to switch).",
    "$texturegroup skinfamilies",
    "{",
    '\t{ "lamp_red"    "lamp_yellow"    "lamp_green"    }',
    '\t{ "lamp_red_on" "lamp_yellow"    "lamp_green"    }',
    '\t{ "lamp_red"    "lamp_yellow_on" "lamp_green"    }',
    '\t{ "lamp_red"    "lamp_yellow"    "lamp_green_on" }',
    "}",
]


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    names = [a for a in argv if not a.startswith("--")] or list(PROPS)
    unknown = [n for n in names if n not in PROPS]
    if unknown:
        sys.exit("unknown prop(s): %s (have: %s)" % (", ".join(unknown), ", ".join(PROPS)))
    for name in names:
        geometry, surfaceprop, mass = PROPS[name]
        kit.build(name, geometry, surfaceprop, mass, SKINS if name == "start_lights" else ())


main()
