#!/usr/bin/env python3
"""Writes kart_bg.vmf, the main menu's background map (docs/asset-pipeline.md, "Main menu background").

usage: python3 assets_src/maps/kart_bg.py [out.vmf]   (default: kart_bg.vmf next to this script)

A lit corner of a wasteland track at dusk with the racer kart parked on it, seen from one fixed spot: the
info_player_deathmatch is the camera (on a background map the player is a fixed spectator there, see
CHL2MP_Player::Spawn). Uses kart_arena.py's brush and VMF writer. Standard library only.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kart_arena import Map, NODRAW, SKY, SINK, cross, sub, write  # noqa: E402

GROUND = "nature/dirtfloor006a"
TRACK = "concrete/concretefloor033a"
TRACK_EDGE = "concrete/concretewall004a"
BERM = "nature/dirtwall001a"
SKYNAME = "sky_wasteland02"

HALF = 1536        # interior is 3072 x 3072 between the sky walls
HEIGHT = 1024
SHELL = 64
BERM_H = 96        # low dirt banks along the sky walls, so the ground doesn't just stop at the horizon
ROAD = 4           # the track is a 4-high slab on the dirt
CX, CY = -768, -768          # centre of the corner: the track turns round it, from due east (0) to due north (90)
R_IN, R_OUT = 512, 1024      # inner and outer edge of the track
SEGS = 12

KART = "models/kart/kart_racer.mdl"
TIRE = "models/kart/props/tire_wall_straight.mdl"
CONE = "models/props_junk/trafficcone001a.mdl"
BARRIER = "models/props_c17/concrete_barrier001a.mdl"
LAMP = "models/props_c17/lamppost03a_on.mdl"
SIGN = "models/kart/props/sign_arrow_left.mdl"


def polar(r, deg):
    a = math.radians(deg)
    return CX + r * math.cos(a), CY + r * math.sin(a)


def build():
    m = Map()
    sky = {"top": SKY, "bottom": SKY, "side": SKY}

    # Shell: a dirt floor (seals the bottom), four sky walls and a sky ceiling.
    m.box(-HALF, -HALF, -SHELL, HALF, HALF, 0, {"top": GROUND, "bottom": NODRAW, "side": NODRAW})
    m.box(-HALF, -HALF, HEIGHT, HALF, HALF, HEIGHT + SHELL, sky)
    out = HALF + SHELL
    m.box(-out, -out, -SHELL, -HALF, out, HEIGHT + SHELL, sky)
    m.box(HALF, -out, -SHELL, out, out, HEIGHT + SHELL, sky)
    m.box(-HALF, -out, -SHELL, HALF, -HALF, HEIGHT + SHELL, sky)
    m.box(-HALF, HALF, -SHELL, HALF, out, HEIGHT + SHELL, sky)

    # Dirt banks along the sky walls, sloping up to them.
    berm = {"top": BERM, "bottom": NODRAW, "side": BERM}
    b0, b1 = HALF - 384, HALF
    m.prism([(b0, -b0), (b1, -b1), (b1, b1), (b0, b0)], SINK, [0, BERM_H, BERM_H, 0], berm)     # east
    m.prism([(b0, b0), (b1, b1), (-b1, b1), (-b0, b0)], SINK, [0, BERM_H, BERM_H, 0], berm)     # north
    m.prism([(-b0, b0), (-b1, b1), (-b1, -b1), (-b0, -b0)], SINK, [0, BERM_H, BERM_H, 0], berm)  # west
    m.prism([(-b0, -b0), (-b1, -b1), (b1, -b1), (b0, -b0)], SINK, [0, BERM_H, BERM_H, 0], berm)  # south

    # The track: a quarter circle round (CX, CY), with straight run-offs at both ends into the berms.
    road = {"top": TRACK, "bottom": NODRAW, "side": TRACK_EDGE}

    def quad(p0, p1, p2, p3):
        pts = [tuple(int(round(c)) for c in p) for p in (p0, p1, p2, p3)]
        if cross(sub(pts[1] + (0,), pts[0] + (0,)), sub(pts[2] + (0,), pts[0] + (0,)))[2] < 0:
            pts = pts[::-1]
        m.prism(pts, SINK, [ROAD] * 4, road)

    for k in range(SEGS):
        a0, a1 = 90 * k / SEGS, 90 * (k + 1) / SEGS
        quad(polar(R_IN, a0), polar(R_OUT, a0), polar(R_OUT, a1), polar(R_IN, a1))
    quad((CX + R_IN, -b0 + 16), (CX + R_OUT, -b0 + 16), (CX + R_OUT, CY), (CX + R_IN, CY))   # south end
    quad((-b0 + 16, CY + R_IN), (CX, CY + R_IN), (CX, CY + R_OUT), (-b0 + 16, CY + R_OUT))   # west end

    def prop(model, origin, yaw=0, **kv):
        m.entity("prop_static", origin, model=model, angles="0 %g 0" % yaw, solid="6", skin="0",
                 fademindist="-1", fadescale="1", disableshadows="0", **kv)

    def still(model, origin, yaw=0, **kv):
        # Models without static collision (the cone, the kart) as non-solid dynamic props.
        m.entity("prop_dynamic", origin, model=model, angles="0 %g 0" % yaw, solid="0", skin="0",
                 fademindist="-1", fadescale="1", DefaultAnim="idle", DisableBoneFollowers="1", **kv)

    # Tyre walls along the outside of the corner, each straight piece square to the radius.
    r_tire = R_OUT + 56
    step = math.degrees(128 / r_tire)
    a = step / 2
    while a < 90:
        x, y = polar(r_tire, a)
        prop(TIRE, (round(x), round(y), 0), a + 90)
        a += step
    # Concrete barriers and cones on the inside of the corner.
    for deg in (20, 45, 70):
        x, y = polar(R_IN - 56, deg)
        prop(BARRIER, (round(x), round(y), 0), deg)
    for deg in (8, 32, 58, 82):
        x, y = polar(R_IN + 40, deg)
        still(CONE, (round(x), round(y), ROAD), deg * 3)
    x, y = polar(R_OUT + 200, 66)
    prop(SIGN, (round(x), round(y), 0), 66 + 180)

    # The kart, mid corner on the racing line, heading round it (counter-clockwise seen from above).
    kdeg = 38
    kx, ky = polar(700, kdeg)
    kyaw = kdeg + 90 + 12
    still(KART, (round(kx), round(ky), ROAD), kyaw, targetname="menu_kart")

    # Lighting: a low warm sun behind the corner and a lamppost lighting the kart.
    m.entity("light_environment", (0, 0, 512), angles="0 200 0", pitch="-18",
             _light="255 170 110 300", _lightHDR="-1 -1 -1 1", _lightscaleHDR="1",
             _ambient="90 110 150 70", _ambientHDR="-1 -1 -1 1", _AmbientScaleHDR="1", SunSpreadAngle="8")
    lx, ly = polar(R_OUT + 120, 14)
    lyaw = math.degrees(math.atan2(ky - ly, kx - lx))     # facing the kart
    prop(LAMP, (round(lx), round(ly), 0), lyaw)
    m.entity("light_spot", (round(lx), round(ly), 184), angles="0 %d 0" % lyaw, pitch="-50",
             _light="255 214 160 900", _lightHDR="-1 -1 -1 1", _lightscaleHDR="1",
             _inner_cone="30", _cone="55", _exponent="1", _distance="0", _constant_attn="0",
             _linear_attn="0", _quadratic_attn="1", spawnflags="0")
    m.entity("light", (round(kx), round(ky), 120), _light="255 190 140 60", _lightHDR="-1 -1 -1 1",
             _lightscaleHDR="1", _constant_attn="0", _linear_attn="0", _quadratic_attn="1", _distance="0")
    m.entity("env_cubemap", (round(kx), round(ky), 64), cubemapsize="0")

    # The camera: on the inside lane, ahead of the kart, so it sees the kart's front three-quarters. It looks
    # a little left of the kart, so the kart sits right of centre and the menu on the left covers track.
    cx, cy = polar(560, kdeg + 14)
    cz = 46
    look = math.degrees(math.atan2(ky - cy, kx - cx)) + 14
    pitch = math.degrees(math.atan2(cz - (ROAD + 24), math.hypot(kx - cx, ky - cy)))
    m.entity("info_player_deathmatch", (round(cx), round(cy), cz), angles="%.1f %.1f 0" % (pitch, look))
    return m


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "kart_bg.vmf")
    with open(path, "w") as f:
        write(build(), f, sky=SKYNAME)
    print("wrote", path)
