#!/usr/bin/env python3
"""Draws logo.png, the main menu logo: "HL2 KART" in slanted block letters over a checkered stripe.

usage: tools/venv/bin/python assets_src/textures/make_logo.py [out.png]   (default: logo.png next to this script)

The letters are drawn from rectangles and polygons below, so no font is needed. Convert the result with
tools/img2vtf.py (see docs/asset-pipeline.md, "Main menu").
"""
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

W, H = 1024, 256      # output size (img2vtf keeps powers of two as they are)
SS = 4                # supersampling
U = 17                # one glyph grid unit, in output pixels; glyphs are 5 x 7 units
GAP = 1.6             # between letters, in units
SPACE = 3.2           # extra space between the words, in units
SLANT = 0.32          # x shift per unit of height: the letters lean forward
TOP = 44              # top of the letters, in output pixels

# Each glyph: rectangles (x0, y0, x1, y1) and polygons on a 5 x 7 grid, y down. Strokes are 1.4 thick.
GLYPHS = {
    "H": ([(0, 0, 1.4, 7), (3.6, 0, 5, 7), (0, 2.8, 5, 4.2)], []),
    "L": ([(0, 0, 1.4, 7), (0, 5.6, 5, 7)], []),
    "2": ([(0, 0, 5, 1.4), (3.6, 0, 5, 4.2), (0, 2.8, 5, 4.2), (0, 2.8, 1.4, 7), (0, 5.6, 5, 7)], []),
    "K": ([(0, 0, 1.4, 7)], [[(1.2, 3.6), (3.3, 0), (5, 0), (2.5, 4.2)], [(1.2, 3.4), (2.7, 2.8), (5, 7), (3.3, 7)]]),
    "A": ([(0, 0.9, 1.4, 7), (3.6, 0.9, 5, 7), (0.9, 0, 4.1, 1.4), (0, 3.0, 5, 4.4)],
          [[(0, 0.9), (0.9, 0), (0.9, 1.4), (0, 1.4)], [(4.1, 0), (5, 0.9), (5, 1.4), (4.1, 1.4)]]),
    "R": ([(0, 0, 1.4, 7), (0, 0, 4.1, 1.4), (3.6, 0.9, 5, 3.3), (0, 2.8, 4.1, 4.2)],
          [[(4.1, 0), (5, 0.9), (5, 1.4), (4.1, 1.4)], [(4.1, 2.8), (5, 3.3), (5, 3.3), (4.1, 4.2)],
           [(2.0, 4.2), (3.6, 4.2), (5, 7), (3.4, 7)]]),
    "T": ([(0, 0, 5, 1.4), (1.8, 0, 3.2, 7)], []),
}
TEXT = "HL2 KART"


def text_mask():
    """The letters, white on black, at SS times the output size."""
    mask = Image.new("L", (W * SS, H * SS), 0)
    d = ImageDraw.Draw(mask)
    width = sum(5 + GAP for c in TEXT if c != " ") - GAP + SPACE * TEXT.count(" ")
    x = (W / U - width - SLANT * 7) / 2     # centred, slant included

    def pt(gx, gy, ox):
        # grid point -> supersampled pixel, leaning forward: the top is shifted right
        return ((ox + gx + SLANT * (7 - gy)) * U * SS, (TOP + gy * U) * SS)

    for c in TEXT:
        if c == " ":
            x += SPACE - GAP
            continue
        rects, polys = GLYPHS[c]
        for x0, y0, x1, y1 in rects:
            d.polygon([pt(x0, y0, x), pt(x1, y0, x), pt(x1, y1, x), pt(x0, y1, x)], fill=255)
        for poly in polys:
            d.polygon([pt(gx, gy, x) for gx, gy in poly], fill=255)
        x += 5 + GAP
    return mask


def gradient(top, bottom, y0, y1):
    """A vertical colour gradient from y0 to y1 (clamped outside), full size."""
    img = Image.new("RGB", (W * SS, H * SS))
    d = ImageDraw.Draw(img)
    for y in range(H * SS):
        t = min(1.0, max(0.0, (y - y0 * SS) / ((y1 - y0) * SS)))
        d.line([(0, y), (W * SS, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)))
    return img


def checker_stripe():
    """A slanted strip of two rows of checks under the letters."""
    mask = Image.new("L", (W * SS, H * SS), 0)
    d = ImageDraw.Draw(mask)
    size = 14 * SS
    y0 = (TOP + 7 * U + 18) * SS
    x_start, x_end = 224 * SS, (W - 176) * SS
    for row in range(2):
        for i, x in enumerate(range(x_start, x_end, size)):
            if (i + row) % 2:
                continue
            yy = y0 + row * size
            lean = SLANT * size
            d.polygon([(x + lean, yy), (x + size + lean, yy), (x + size, yy + size), (x, yy + size)], fill=255)
    frame = Image.new("L", mask.size, 0)
    ImageDraw.Draw(frame).rectangle([x_start, y0, x_end, y0 + 2 * size], fill=255)
    return mask, frame


def build():
    letters = text_mask()
    checks, strip = checker_stripe()

    # Black outline round the letters and the strip, and a soft drop shadow under everything.
    shape = ImageChops.lighter(letters, strip)
    outline = shape.filter(ImageFilter.MaxFilter(9 * SS // 2 * 2 + 1))
    shadow = outline.filter(ImageFilter.GaussianBlur(6 * SS))
    shadow = ImageChops.offset(shadow, 5 * SS, 7 * SS).point(lambda v: v * 0.7)

    img = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    img.paste((0, 0, 0, 255), (0, 0), shadow)
    img.paste((20, 16, 12, 255), (0, 0), outline)
    # Letters: hot orange at the top to a deep rust at the bottom, with a pale highlight band near the top.
    fill = gradient((255, 214, 96), (196, 64, 14), TOP, TOP + 7 * U)
    img.paste(fill, (0, 0), letters)
    band = Image.new("L", letters.size, 0)
    ImageDraw.Draw(band).rectangle([0, (TOP + 0.35 * U) * SS, W * SS, (TOP + 0.75 * U) * SS], fill=110)
    img.paste((255, 250, 225), (0, 0), ImageChops.multiply(letters, band))
    # The strip: white checks on near-black.
    img.paste((235, 232, 220, 255), (0, 0), checks)
    return img.resize((W, H), Image.LANCZOS)


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "logo.png")
    build().save(path)
    print("wrote", path)
