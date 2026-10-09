#!/usr/bin/env python3
"""Draws the kart HUD item icons (CKartItemSlot) and converts them to VTF.

Original single-color glyphs, white on transparent; the HUD tints them. Each is drawn at 4x and
scaled down to 64x64 for smooth edges. Writes <name>.png next to this script, then
materials/vgui/kart/items/<name>.vtf/.vmt with tools/img2vtf.py --type vgui.

usage: tools/venv/bin/python assets_src/textures/hud_items/make_icons.py
"""
import math
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
SIZE = 64
SS = 4
S = SIZE * SS
WHITE = (255, 255, 255, 255)
CLEAR = (255, 255, 255, 0)


def ring(d, cx, cy, r, w):
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=WHITE, width=w)


def disc(d, cx, cy, r, fill=WHITE):
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=fill)


def icon_none(d):
    # Empty slot: a dashed ring.
    c, r, w = S / 2, S * 0.30, S // 24
    for i in range(12):
        a0 = i * 30 + 5
        d.arc((c - r, c - r, c + r, c + r), a0, a0 + 18, fill=WHITE, width=w)


def icon_hubcap(d):
    # Rim, five spokes, hub.
    c = S / 2
    ring(d, c, c, S * 0.42, S // 14)
    ring(d, c, c, S * 0.30, S // 32)
    for i in range(5):
        a = math.radians(i * 72 - 90)
        x0, y0 = c + math.cos(a) * S * 0.10, c + math.sin(a) * S * 0.10
        x1, y1 = c + math.cos(a) * S * 0.36, c + math.sin(a) * S * 0.36
        d.line((x0, y0, x1, y1), fill=WHITE, width=S // 14)
    disc(d, c, c, S * 0.11)
    disc(d, c, c, S * 0.04, CLEAR)


def icon_oil_slick(d):
    # A drop over a puddle.
    c = S / 2
    r = S * 0.17
    cy = S * 0.42
    d.polygon([(c, S * 0.08), (c - r * 0.95, cy - r * 0.3), (c + r * 0.95, cy - r * 0.3)], fill=WHITE)
    disc(d, c, cy, r)
    disc(d, c + r * 0.4, cy + r * 0.05, r * 0.25, CLEAR)
    d.ellipse((S * 0.12, S * 0.70, S * 0.88, S * 0.90), fill=WHITE)
    d.ellipse((S * 0.24, S * 0.64, S * 0.56, S * 0.78), fill=WHITE)


def icon_nitro_can(d):
    # A canister with a nozzle and a lightning bolt cut out.
    d.rounded_rectangle((S * 0.28, S * 0.24, S * 0.72, S * 0.92), radius=S * 0.10, fill=WHITE)
    d.rectangle((S * 0.43, S * 0.12, S * 0.57, S * 0.24), fill=WHITE)
    d.rectangle((S * 0.36, S * 0.06, S * 0.64, S * 0.13), fill=WHITE)
    bolt = [(0.56, 0.32), (0.40, 0.60), (0.50, 0.60), (0.44, 0.84), (0.62, 0.52), (0.52, 0.52), (0.58, 0.32)]
    d.polygon([(x * S, y * S) for x, y in bolt], fill=CLEAR)


def icon_seeker(d):
    # A reticle around a dot.
    c = S / 2
    ring(d, c, c, S * 0.30, S // 14)
    w, gap, end = S // 14, S * 0.18, S * 0.46
    d.line((c, c - end, c, c - gap), fill=WHITE, width=w)
    d.line((c, c + gap, c, c + end), fill=WHITE, width=w)
    d.line((c - end, c, c - gap, c), fill=WHITE, width=w)
    d.line((c + gap, c, c + end, c), fill=WHITE, width=w)
    disc(d, c, c, S * 0.08)


def icon_buffer(d):
    # A shield with an inner outline.
    def shield(inset):
        top, bot, l, r = S * 0.10 + inset, S * 0.92 - inset * 1.4, S * 0.16 + inset, S * 0.84 - inset
        pts = [(l, top + S * 0.06), (S / 2, top), (r, top + S * 0.06), (r, S * 0.50)]
        # Lower curve, right side round to the point.
        for i in range(1, 9):
            t = i / 9
            pts.append((r + (S / 2 - r) * t, S * 0.50 + (bot - S * 0.50) * math.sin(t * math.pi / 2)))
        pts.append((S / 2, bot))
        for x, y in reversed(pts[3:-1]):
            pts.append((S - x, y))
        return pts

    d.polygon(shield(0), fill=WHITE)
    d.polygon(shield(S * 0.07), fill=CLEAR)
    d.polygon(shield(S * 0.13), fill=WHITE)


ICONS = {
    "none": icon_none,
    "hubcap": icon_hubcap,
    "oil_slick": icon_oil_slick,
    "nitro_can": icon_nitro_can,
    "seeker": icon_seeker,
    "buffer": icon_buffer,
}


def main():
    for name, draw in ICONS.items():
        img = Image.new("RGBA", (S, S), CLEAR)
        draw(ImageDraw.Draw(img))
        img = img.resize((SIZE, SIZE), Image.LANCZOS)
        png = HERE / f"{name}.png"
        img.save(png)
        subprocess.run([sys.executable, str(REPO / "tools/img2vtf.py"), str(png), f"vgui/kart/items/{name}",
                        "--type", "vgui"], check=True)


if __name__ == "__main__":
    main()
