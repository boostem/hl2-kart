#!/usr/bin/env python3
"""Textures and materials for the track dressing kit (models/kart/props/).

    tools/venv/bin/python assets_src/props/make_materials.py

Draws the tiled textures into assets_src/props/materials/ (lamp lenses off and lit, the chequer, the boost pad
glow), converts them and every prop's baked <prop>/<prop>.png to VTF with tools/img2vtf.py, and writes the VMTs.
Run it after build_props.py.
"""
import math
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(REPO / "tools"))
from img2vtf import write_vtf  # noqa: E402

MATERIALS = REPO / "game/mod_hl2mp/materials"
PATH = "models/kart/props/"

# prop -> $surfaceprop (as in its QC)
PROPS = {
    "tire_wall_straight": "rubbertire",
    "tire_wall_corner": "rubbertire",
    "ramp_small": "metalpanel",
    "ramp_medium": "metalpanel",
    "ramp_large": "metalpanel",
    "finish_gantry": "metal",
    "start_lights": "metal",
    "sign_arrow_left": "wood_panel",
    "sign_arrow_right": "wood_panel",
    "sign_hazard": "wood_panel",
    "boost_pad": "metal",
}

# lamp colours (sRGB): lit centre, lit edge, unlit glass
LAMPS = {
    "red": ((255, 200, 170), (235, 30, 20), (70, 12, 10)),
    "yellow": ((255, 250, 200), (250, 175, 20), (75, 52, 10)),
    "green": ((220, 255, 210), (30, 220, 60), (12, 55, 22)),
}


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def lens(centre, edge, lit, size=64):
    """A round lamp lens filling the square: fresnel rings, a hot centre when lit, a dull highlight when not."""
    img = Image.new("RGB", (size, size))
    px = img.load()
    for j in range(size):
        for i in range(size):
            dx, dy = (i + 0.5) / size * 2 - 1, (j + 0.5) / size * 2 - 1
            r = min(1.0, math.hypot(dx, dy))
            ring = 0.88 + 0.12 * math.cos(r * math.pi * 7)
            if lit:
                c = lerp(centre, edge, min(1.0, r * 1.3) ** 0.8)
            else:
                hl = max(0.0, 1 - math.hypot(dx + 0.35, dy + 0.35) / 0.35)   # highlight towards the top left
                c = lerp(lerp(edge, (0, 0, 0), r * 0.5), (150, 150, 150), hl * 0.5)
            px[i, j] = tuple(min(255, round(v * ring)) for v in c)
    return img


def checker(size=64):
    img = Image.new("RGB", (size, size))
    px = img.load()
    for j in range(size):
        for i in range(size):
            px[i, j] = (225, 222, 210) if (i * 2 // size + j * 2 // size) % 2 else (22, 22, 22)
    return img


def glow(size=128):
    """Two chevrons pointing +U (along the pad, the way karts drive) on a dark blue field, tiling."""
    img = Image.new("RGB", (size, size))
    px = img.load()
    for j in range(size):
        for i in range(size):
            u, v = i / size, abs((j + 0.5) / size - 0.5)
            # distance behind the chevron's leading edge, which runs back from the tip at the centre line
            d = (-u - v * 0.9) % 0.5
            core = 1.0 if d < 0.16 else max(0.0, 1 - (d - 0.16) / 0.12) ** 2 if d < 0.28 else 0.0
            edge = max(0.0, 1 - v / 0.5) ** 0.3
            c = lerp((8, 26, 46), (150, 245, 255), core * edge)
            px[i, j] = c
    return img


def vmt(name, lines):
    (MATERIALS / PATH / (name + ".vmt")).write_text("\n".join(lines) + "\n")
    print("wrote", (MATERIALS / PATH / (name + ".vmt")).relative_to(REPO))


def main():
    src = HERE / "materials"
    src.mkdir(exist_ok=True)
    tiled = {"checker": checker(), "boost_pad_glow": glow()}
    for colour, (centre, edge, off) in LAMPS.items():
        tiled["lamp_" + colour] = lens(centre, off, False)
        tiled["lamp_%s_on" % colour] = lens(centre, edge, True)
    for name, img in tiled.items():
        img.save(src / (name + ".png"))

    for name, surfaceprop in PROPS.items():
        write_vtf(str(HERE / name / (name + ".png")), PATH + name)
        vmt(name, ['"VertexLitGeneric"', "{", '\t"$basetexture" "%s%s"' % (PATH, name),
                   '\t"$surfaceprop" "%s"' % surfaceprop, "}"])
    for name in tiled:
        write_vtf(str(src / (name + ".png")), PATH + name)
    vmt("checker", ['"VertexLitGeneric"', "{", '\t"$basetexture" "%schecker"' % PATH, '\t"$surfaceprop" "metal"', "}"])
    for colour in LAMPS:  # unlit glass; lit: full bright
        vmt("lamp_" + colour, ['"VertexLitGeneric"', "{", '\t"$basetexture" "%slamp_%s"' % (PATH, colour),
                               '\t"$surfaceprop" "glass"', "}"])
        vmt("lamp_%s_on" % colour, ['"UnlitGeneric"', "{", '\t"$basetexture" "%slamp_%s_on"' % (PATH, colour),
                                    '\t"$model" "1"', '\t"$surfaceprop" "glass"', "}"])
    # The chevrons run along +X (the pad's U); scrolling at 180 degrees moves them forward.
    vmt("boost_pad_glow", ['"UnlitGeneric"', "{", '\t"$basetexture" "%sboost_pad_glow"' % PATH, '\t"$model" "1"',
                           '\t"$surfaceprop" "metal"', "", '\t"Proxies"', "\t{", '\t\t"TextureScroll"', "\t\t{",
                           '\t\t\t"texturescrollvar" "$basetexturetransform"', '\t\t\t"texturescrollrate" "1.5"',
                           '\t\t\t"texturescrollangle" "180"', "\t\t}", "\t}", "}"])


if __name__ == "__main__":
    main()
