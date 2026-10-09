#!/usr/bin/env python3
"""Draws the Oil Slick puddle texture (kart_hazard_oil) and converts it to VTF.

Original, agent-made: a dark irregular puddle with a faint rainbow sheen, soft edges on transparent,
drawn from a few sines (no randomness, so it redraws the same). Writes oil_slick.png next to this
script, then materials/kart/oil_slick.vtf with tools/img2vtf.py. The VMT is hand-written
(game/mod_hl2mp/materials/kart/oil_slick.vmt): img2vtf's would be a model material.

usage: tools/venv/bin/python assets_src/textures/make_oil_slick.py
"""
import colorsys
import math
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageFilter

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
SIZE = 256
EDGE = 0.06  # soft edge width, of the radius

# Wobbles of the outline: (lobes, amplitude, phase).
OUTLINE = [(3, 0.07, 0.4), (5, 0.05, 1.9), (7, 0.03, 3.1), (11, 0.015, 0.7)]


def outline_radius(a):
    return 0.80 + sum(amp * math.sin(n * a + ph) for n, amp, ph in OUTLINE)


def sheen(x, y):
    # Swirling interference bands, strongest away from the middle.
    return 0.5 + 0.5 * math.sin(9.0 * x + 4.0 * math.sin(5.0 * y + 1.3) + 3.0 * math.sin(3.0 * x * y))


def main():
    img = Image.new("RGBA", (SIZE, SIZE))
    px = img.load()
    for j in range(SIZE):
        for i in range(SIZE):
            x = (i + 0.5) / SIZE * 2.0 - 1.0
            y = (j + 0.5) / SIZE * 2.0 - 1.0
            r = math.hypot(x, y)
            edge = outline_radius(math.atan2(y, x))
            t = r / edge
            if t >= 1.0:
                px[i, j] = (8, 6, 4, 0)
                continue
            alpha = min(1.0, (1.0 - t) / EDGE)
            # Thin film: a hue band over near-black oil, faint in the middle.
            s = sheen(x, y)
            hue = (0.6 + 1.2 * s + 0.4 * t) % 1.0
            hr, hg, hb = colorsys.hsv_to_rgb(hue, 0.7, 1.0)
            k = 0.03 + 0.14 * t * t * s
            base = (10, 8, 7)
            rgb = tuple(int(min(255, b * (1.0 - k) + 255 * c * k)) for b, c in zip(base, (hr, hg, hb)))
            # A rim where the oil thins out at the edge.
            rim = max(0.0, 1.0 - abs(t - 0.93) / 0.05) * 0.25
            rgb = tuple(int(min(255, c + 90 * rim)) for c in rgb)
            px[i, j] = (*rgb, int(255 * alpha))
    img = img.filter(ImageFilter.SMOOTH)
    out = HERE / "oil_slick.png"
    img.save(out)
    print(f"wrote {out.relative_to(REPO)}")

    tmp_vmt = REPO / "game/mod_hl2mp/materials/kart/oil_slick.vmt"
    keep = tmp_vmt.read_text() if tmp_vmt.exists() else None
    subprocess.run([sys.executable, str(REPO / "tools/img2vtf.py"), str(out), "kart/oil_slick"], check=True)
    if keep is not None:
        tmp_vmt.write_text(keep)  # img2vtf writes a model VMT; keep the hand-written one


if __name__ == "__main__":
    main()
