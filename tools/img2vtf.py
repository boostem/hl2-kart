#!/usr/bin/env python3
"""Convert an image to a Source VTF (+ VMT) under game/mod_hl2mp/materials/.

usage: img2vtf.py <image> <material path> [--type model|world|vgui] [--normal <image>]
                  [--surfaceprop NAME] [--metal] [--game-dir DIR]

<material path> is relative to materials/, without extension (e.g. props/crate01).
Run with tools/venv/bin/python (see tools/setup_venv.sh).

The VTF is written here directly (version 7.2, DXT1 without alpha, DXT5 with, full mip chain,
no thumbnail); Pillow does the resize and the DXT compression.

--type vgui is for HUD icons drawn by vgui (scripts/mod_textures.txt): UnlitGeneric, clamped, tinted
by the draw color through $vertexcolor and $vertexalpha. Draw those white on transparent.
"""
import argparse
import io
import struct
import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parent.parent
MAX_SIZE = 1024

IMAGE_FORMAT_DXT1, IMAGE_FORMAT_DXT5 = 13, 15
FLAG_CLAMPS, FLAG_CLAMPT = 0x4, 0x8
FLAG_NORMAL = 0x80
FLAG_NOLOD = 0x200
FLAG_EIGHTBITALPHA = 0x2000


def pow2_floor(n):
    p = 1
    while p * 2 <= n:
        p *= 2
    return p


def pow2_round(n):
    lo = pow2_floor(n)
    return lo * 2 if n - lo > lo * 2 - n else lo


def load(path):
    img = Image.open(path)
    img = img.convert("RGBA")
    w, h = (min(MAX_SIZE, max(4, pow2_round(d))) for d in img.size)
    if (w, h) != img.size:
        img = img.resize((w, h), Image.LANCZOS)
    return img


def has_alpha(img):
    return img.getchannel("A").getextrema()[0] < 255


def dxt_blocks(img, fmt):
    """Raw DXT block data for one mip level (DDS header stripped)."""
    if img.width < 4 or img.height < 4:
        canvas = Image.new("RGBA", (max(4, img.width), max(4, img.height)))
        canvas.paste(img, (0, 0))
        img = canvas
    buf = io.BytesIO()
    img.save(buf, "DDS", pixel_format="DXT5" if fmt == IMAGE_FORMAT_DXT5 else "DXT1")
    return buf.getvalue()[128:]


def encode_vtf(img, flags=0):
    fmt = IMAGE_FORMAT_DXT5 if has_alpha(img) else IMAGE_FORMAT_DXT1
    if fmt == IMAGE_FORMAT_DXT5:
        flags |= FLAG_EIGHTBITALPHA
    mips = [img]
    while mips[-1].width > 1 or mips[-1].height > 1:
        w, h = mips[-1].size
        mips.append(img.resize((max(1, w // 2), max(1, h // 2)), Image.BOX))
    r, g, b = (c / 255.0 for c in img.convert("RGB").resize((1, 1), Image.BOX).getpixel((0, 0)))
    refl = [c ** 2.2 for c in (r, g, b)]
    header = struct.pack(
        "<4sIIIHHIHH4x3f4xfiBiBBH",
        b"VTF\0", 7, 2, 80, img.width, img.height, flags, 1, 0,
        *refl, 1.0, fmt, len(mips), -1, 0, 0, 1,
    )
    header = header.ljust(80, b"\0")
    # Source stores the mip chain smallest first.
    return header + b"".join(dxt_blocks(m, fmt) for m in reversed(mips))


def write_vtf(src, rel, flags=0):
    out = REPO / "game/mod_hl2mp/materials" / (rel + ".vtf")
    out.parent.mkdir(parents=True, exist_ok=True)
    img = load(src)
    out.write_bytes(encode_vtf(img, flags))
    print(f"wrote {out.relative_to(REPO)} ({img.width}x{img.height})")
    return has_alpha(img)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image")
    ap.add_argument("path", help="material path relative to materials/, no extension")
    ap.add_argument("--type", choices=["model", "world", "vgui"], default="model")
    ap.add_argument("--normal", help="normal map image (writes <path>_normal.vtf, sets $bumpmap)")
    ap.add_argument("--surfaceprop", default="default")
    ap.add_argument("--metal", action="store_true", help="add $envmap env_cubemap and $envmaptint")
    ap.add_argument("--game-dir", help=argparse.SUPPRESS)
    a = ap.parse_args()
    global REPO
    if a.game_dir:
        REPO = Path(a.game_dir)

    path = a.path.strip("/").removesuffix(".vtf").removesuffix(".vmt")
    if a.type == "vgui":
        write_vtf(a.image, path, FLAG_CLAMPS | FLAG_CLAMPT | FLAG_NOLOD)
        lines = ['"UnlitGeneric"', "{", f'\t"$basetexture" "{path}"', '\t"$translucent" "1"',
                 '\t"$vertexcolor" "1"', '\t"$vertexalpha" "1"', '\t"$ignorez" "1"', '\t"$no_fullbright" "1"', "}"]
        write_vmt(path, lines)
        return

    alpha = write_vtf(a.image, path)
    shader = "VertexLitGeneric" if a.type == "model" else "LightmappedGeneric"
    lines = [f'"{shader}"', "{", f'\t"$basetexture" "{path}"', f'\t"$surfaceprop" "{a.surfaceprop}"']
    if a.normal:
        write_vtf(a.normal, path + "_normal", FLAG_NORMAL)
        lines.append(f'\t"$bumpmap" "{path}_normal"')
    if a.metal:
        lines += ['\t"$envmap" "env_cubemap"', '\t"$envmaptint" "[ .5 .5 .5 ]"']
    if alpha and a.type == "model":
        lines.append('\t"$translucent" "1"')
    lines.append("}")
    write_vmt(path, lines)


def write_vmt(path, lines):
    vmt = REPO / "game/mod_hl2mp/materials" / (path + ".vmt")
    vmt.write_text("\n".join(lines) + "\n")
    print(f"wrote {vmt.relative_to(REPO)}")


if __name__ == "__main__":
    sys.exit(main())
