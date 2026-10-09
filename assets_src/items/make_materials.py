#!/usr/bin/env python3
"""Textures and materials for the kart item models (models/kart/items/).

    tools/venv/bin/python assets_src/items/make_materials.py

Draws the textures that aren't baked into assets_src/items/materials/ (the item box symbol, the nitro gauge, the oil
slick and its normal map, the seeker's eye and thruster glow, the buffer shimmer), converts them and every baked
<item>/<item>.png to VTF with tools/img2vtf.py, and writes the VMTs. All original, drawn from a few sines and shapes
(no randomness, so they redraw the same). Run it after build_items.py.
"""
import colorsys
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(REPO / "tools"))
from img2vtf import FLAG_NORMAL, write_vtf  # noqa: E402

MATERIALS = REPO / "game/mod_hl2mp/materials"
PATH = "models/kart/items/"

# Same as OIL_OUTLINE in build_items.py: the puddle's outline, as a fraction of the texture's half side.
OIL_OUTLINE = [(3, 0.07, 0.4), (5, 0.05, 1.9), (7, 0.03, 3.1), (11, 0.015, 0.7)]
OIL_EDGE = 0.06   # soft edge width, of the radius


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def uv(i, j, size):
    """Pixel centre to -1..1, y up."""
    return (i + 0.5) / size * 2 - 1, 1 - (j + 0.5) / size * 2


def symbol(size=256):
    """The item box's side: a dark steel panel with a glowing amber diamond round a lightning bolt. The alpha is the
    $selfillum mask: the symbol and a soft halo glow, the panel is lit."""
    big = size * 4
    mask = Image.new("L", (big, big))
    d = ImageDraw.Draw(mask)
    c = big / 2
    def pt(x, y):
        return (c + x * c, c - y * c)
    outer, inner = 0.84, 0.70
    d.polygon([pt(0, outer), pt(outer, 0), pt(0, -outer), pt(-outer, 0)], fill=255)
    d.polygon([pt(0, inner), pt(inner, 0), pt(0, -inner), pt(-inner, 0)], fill=0)
    d.polygon([pt(0.10, 0.50), pt(-0.22, 0.02), pt(-0.02, 0.02), pt(-0.12, -0.50), pt(0.22, -0.02),
               pt(0.02, -0.02)], fill=255)
    mask = mask.resize((size, size), Image.LANCZOS)
    halo = mask.filter(ImageFilter.GaussianBlur(size / 40))
    img = Image.new("RGBA", (size, size))
    px, m, h = img.load(), mask.load(), halo.load()
    for j in range(size):
        for i in range(size):
            x, y = uv(i, j, size)
            e = max(abs(x), abs(y))
            panel = (52, 56, 60) if e < 0.93 else (34, 36, 38)              # inset panel, darker border
            grain = 0.92 + 0.08 * math.sin(40 * y + 3 * math.sin(7 * x))     # brushed streaks
            base = tuple(round(v * grain) for v in panel)
            k = m[i, j] / 255
            glow = max(k, h[i, j] / 255 * 0.8)
            col = lerp(lerp(base, (255, 140, 20), min(1.0, glow * 1.4)), (255, 236, 170), k)
            px[i, j] = col + (round(255 * glow),)
    return img


def gauge(size=64):
    """A pressure gauge face: cream dial, ticks round a 270 degree arc, a red zone, the needle near full."""
    big = size * 4
    img = Image.new("RGB", (big, big), (40, 40, 40))
    d = ImageDraw.Draw(img)
    c, r = big / 2, big / 2 * 0.94
    d.ellipse((c - r, c - r, c + r, c + r), fill=(232, 226, 205))
    def at(a, f):
        return (c + f * r * math.cos(a), c - f * r * math.sin(a))
    start, sweep = math.radians(225), math.radians(270)
    d.arc((c - r * 0.8, c - r * 0.8, c + r * 0.8, c + r * 0.8), -10, 45, fill=(200, 30, 20), width=big // 14)  # red zone
    for k in range(11):
        a = start - sweep * k / 10
        d.line([at(a, 0.62 if k % 5 == 0 else 0.7), at(a, 0.84)], fill=(30, 30, 30), width=big // 40)
    a = start - sweep * 0.88
    d.line([at(a + math.pi, 0.15), at(a, 0.78)], fill=(190, 20, 15), width=big // 28)
    d.ellipse((c - r * 0.1, c - r * 0.1, c + r * 0.1, c + r * 0.1), fill=(30, 30, 30))
    return img.resize((size, size), Image.LANCZOS)


def oil_outline(a):
    return 0.80 + sum(amp * math.sin(k * a + ph) for k, amp, ph in OIL_OUTLINE)


def oil_height(x, y):
    """Height of the oil's surface: slow swirls, and a meniscus where it thins out at the edge."""
    t = math.hypot(x, y) / oil_outline(math.atan2(y, x))
    swirl = 0.5 * math.sin(7 * x + 3 * math.sin(4 * y)) * math.sin(6 * y + 2 * math.sin(5 * x))
    rim = max(0.0, 1 - abs(t - 0.9) / 0.08)
    return swirl * min(1.0, t * 1.5) + 1.5 * rim


def oil(size=256):
    """The Oil Slick puddle: near-black oil with a faint rainbow sheen and a thin rim, soft edges on transparent;
    and its normal map."""
    img = Image.new("RGBA", (size, size))
    nrm = Image.new("RGB", (size, size))
    px, pn = img.load(), nrm.load()
    step = 2 / size
    for j in range(size):
        for i in range(size):
            x, y = uv(i, j, size)
            t = math.hypot(x, y) / oil_outline(math.atan2(y, x))
            dx = (oil_height(x + step, y) - oil_height(x - step, y)) / (2 * step) * 0.03
            dy = (oil_height(x, y + step) - oil_height(x, y - step)) / (2 * step) * 0.03
            n = (-dx, -dy, 1.0)
            ln = math.sqrt(sum(v * v for v in n))
            pn[i, j] = tuple(round((v / ln * 0.5 + 0.5) * 255) for v in n)
            if t >= 1.0:
                px[i, j] = (8, 6, 4, 0)
                continue
            sheen = 0.5 + 0.5 * math.sin(9.0 * x + 4.0 * math.sin(5.0 * y + 1.3) + 3.0 * math.sin(3.0 * x * y))
            hr, hg, hb = colorsys.hsv_to_rgb((0.6 + 1.2 * sheen + 0.4 * t) % 1.0, 0.7, 1.0)
            k = 0.04 + 0.16 * t * t * sheen
            rgb = tuple(min(255, b * (1 - k) + 255 * c * k) for b, c in zip((10, 8, 7), (hr, hg, hb)))
            rim = max(0.0, 1 - abs(t - 0.93) / 0.05) * 0.25
            px[i, j] = tuple(round(min(255, c + 90 * rim)) for c in rgb) + (round(255 * min(1.0, (1 - t) / OIL_EDGE)),)
    return img.filter(ImageFilter.SMOOTH), nrm


def eye(size=64):
    """The seeker's eye: a dark lens round a glowing red-orange iris with a hot centre."""
    img = Image.new("RGB", (size, size))
    px = img.load()
    for j in range(size):
        for i in range(size):
            x, y = uv(i, j, size)
            r = math.hypot(x, y)
            if r < 0.55:
                c = lerp((255, 230, 180), (255, 70, 20), min(1.0, r / 0.45) ** 0.7)
                c = tuple(round(v * (0.85 + 0.15 * math.cos(r * 40))) for v in c)
            else:
                c = lerp((90, 20, 10), (14, 16, 20), min(1.0, (r - 0.55) / 0.25))
            hl = max(0.0, 1 - math.hypot(x + 0.35, y - 0.4) / 0.18)   # a glint, top left
            px[i, j] = lerp(c, (255, 255, 255), hl * 0.6)
    return img


def thruster(size=64):
    """Additive thruster glow: blue-white centre, fading to black (nothing) at the edge."""
    img = Image.new("RGB", (size, size))
    px = img.load()
    for j in range(size):
        for i in range(size):
            r = min(1.0, math.hypot(*uv(i, j, size)))
            px[i, j] = lerp(lerp((230, 245, 255), (40, 120, 255), min(1.0, r / 0.6)), (0, 0, 0), max(0.0, r - 0.5) * 2)
    return img


def shimmer(w=256, h=64):
    """Additive shield band (u round the ring, v across it, tiling in u): bright cyan edges and a faint hexagon
    lattice between them."""
    img = Image.new("RGB", (w, h))
    px = img.load()
    for j in range(h):
        for i in range(w):
            u, v = i / w, (j + 0.5) / h
            edge = max(0.0, 1 - min(v, 1 - v) / 0.18) ** 2
            # hexagons: distance to the nearest cell edge on a skewed grid, 8 cells round, 2 across
            gx, gy = u * 8, v * 2.0
            gx += 0.5 * (math.floor(gy) % 2)
            fx, fy = abs(gx % 1 - 0.5), abs(gy % 1 - 0.5)
            hexd = max(fx * 1.15 + fy * 0.5, fy)
            line = max(0.0, 1 - abs(hexd - 0.5) / 0.06)
            wave = 0.5 + 0.5 * math.sin(2 * math.pi * (u * 3 + v * 0.5))
            k = min(1.0, edge + line * 0.45 + 0.12 * wave)
            px[i, j] = lerp((0, 0, 0), (110, 210, 255), k)
    return img


def vmt(name, lines):
    (MATERIALS / PATH / (name + ".vmt")).write_text("\n".join(lines) + "\n")
    print("wrote", (MATERIALS / PATH / (name + ".vmt")).relative_to(REPO))


def block(lines):
    return ["\t" + line for line in lines]


def main():
    src = HERE / "materials"
    src.mkdir(exist_ok=True)
    oil_img, oil_normal = oil()
    drawn = {"item_box_symbol": symbol(), "nitro_gauge": gauge(), "oil_puddle": oil_img,
             "oil_puddle_normal": oil_normal, "seeker_eye": eye(), "seeker_glow": thruster(), "buffer_ring": shimmer()}
    for name, img in drawn.items():
        img.save(src / (name + ".png"))
        write_vtf(str(src / (name + ".png")), PATH + name, FLAG_NORMAL if name.endswith("_normal") else 0)
    for name in ("item_box", "hubcap", "nitro_can", "seeker"):
        write_vtf(str(HERE / name / (name + ".png")), PATH + name)

    def lit(name, surfaceprop, extra=()):
        vmt(name, ['"VertexLitGeneric"', "{", '\t"$basetexture" "%s%s"' % (PATH, name),
                   '\t"$surfaceprop" "%s"' % surfaceprop] + block(extra) + ["}"])

    def unlit(name, extra=()):
        vmt(name, ['"UnlitGeneric"', "{", '\t"$basetexture" "%s%s"' % (PATH, name), '\t"$model" "1"']
            + block(extra) + ["}"])

    lit("item_box", "metal_box")
    # The symbol glows (the alpha masks it) and pulses.
    lit("item_box_symbol", "metal_box", ['"$selfillum" "1"', '"$selfillumtint" "[1 1 1]"', "", '"Proxies"', "{",
                                         '\t"Sine"', "\t{", '\t\t"sinemin" "0.6"', '\t\t"sinemax" "1.4"',
                                         '\t\t"sineperiod" "1.2"', '\t\t"resultVar" "$selfillumtint"', "\t}", "}"])
    lit("hubcap", "metal", ['"$envmap" "env_cubemap"', '"$envmaptint" "[.6 .6 .6]"'])
    lit("nitro_can", "metal", ['"$envmap" "env_cubemap"', '"$envmaptint" "[.12 .12 .12]"'])
    lit("nitro_gauge", "glass", ['"$envmap" "env_cubemap"', '"$envmaptint" "[.2 .2 .2]"'])
    vmt("oil_puddle", ['"VertexLitGeneric"', "{", '\t"$basetexture" "%soil_puddle"' % PATH,
                      '\t"$bumpmap" "%soil_puddle_normal"' % PATH, '\t"$surfaceprop" "default"', '\t"$translucent" "1"',
                      '\t"$phong" "1"', '\t"$phongexponent" "40"', '\t"$phongboost" "4"',
                      '\t"$phongfresnelranges" "[.5 .8 1]"', '\t"$envmap" "env_cubemap"',
                      '\t"$envmaptint" "[.18 .18 .18]"', "}"])
    lit("seeker", "metal", ['"$envmap" "env_cubemap"', '"$envmaptint" "[.1 .1 .1]"'])
    unlit("seeker_eye")
    unlit("seeker_glow", ['"$additive" "1"'])
    # Additive and seen from both sides; the hexagons scroll round the ring.
    unlit("buffer_ring", ['"$additive" "1"', '"$nocull" "1"', "", '"Proxies"', "{", '\t"TextureScroll"', "\t{",
                          '\t\t"texturescrollvar" "$basetexturetransform"', '\t\t"texturescrollrate" "0.15"',
                          '\t\t"texturescrollangle" "0"', "\t}", "}"])


if __name__ == "__main__":
    main()
