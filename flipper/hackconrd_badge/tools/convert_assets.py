#!/usr/bin/env python3
"""Generate the Flipper assets from the original badge firmware repository.

Converts the 240x240 RGB565 lock screens (img/*.h) to 64x64 1-bit raw XBM files,
produces the app icon, and pre-renders the Learning module QR code.

Usage:
    pip install pillow qrcode
    python3 tools/convert_assets.py /path/to/Badge_HackConRD_2026

To leave a lock screen out of the app, remove it from LOCKS below and from
SAVER_NAMES in src/lockscreen.c (keep both lists in the same order).
"""
import pathlib
import re
import sys

import qrcode
from PIL import Image, ImageEnhance, ImageFilter, ImageOps

# (output index order == SAVER_NAMES in src/lockscreen.c)
LOCKS = [
    "nolimit_saver",
    "id_saver",
    "nika_saver",
    "rickmorty_saver",
    "bulbasaur_saver",
    "boot_sp",
    "bender_saver",
    "hackerman_saver",
    "imin_saver",
    "zoro_saver",
    "gojo_saver",
    "inosuke_saver",
    "zenitsu_saver",
    "levi_saver",
    "nanami_saver",
    "homer_saver",
    "kali_saver",
    "arch_saver",
    "parrot_saver",
    "hacker_saver",
    "matrix_saver",
]

ART_SIZE = 64
QR_URL = "https://hackconrd.org/"


def parse_hex(path):
    text = path.read_text(errors="replace")
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{1,2})\b", text))


def rgb565_image(data, w, h):
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 2
            v = (data[i] << 8) | data[i + 1]
            px[x, y] = (
                (v >> 11) * 255 // 31,
                ((v >> 5) & 63) * 255 // 63,
                (v & 31) * 255 // 31,
            )
    return img


def mono_image(data, w, h):
    img = Image.new("L", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            p = y * w + x
            px[x, y] = 255 if data[p // 8] & (0x80 >> (p % 8)) else 0
    return img


def to_xbm(img):
    """1-bit image to LSB-first rows, the layout canvas_draw_xbm() expects. Bit set = bright."""
    w, h = img.size
    row_bytes = (w + 7) // 8
    out = bytearray(row_bytes * h)
    px = img.load()
    for y in range(h):
        for x in range(w):
            if px[x, y]:
                out[y * row_bytes + x // 8] |= 1 << (x % 8)
    return bytes(out)


def dither(img, size=ART_SIZE):
    """Photos: push contrast and sharpen before dithering so shapes survive 1-bit."""
    grey = ImageOps.autocontrast(img.convert("L"), cutoff=2)
    small = grey.resize((size, size), Image.LANCZOS)
    small = small.filter(ImageFilter.UnsharpMask(radius=1.5, percent=180, threshold=2))
    small = ImageEnhance.Contrast(small).enhance(1.5)
    return small.convert("1")


def threshold(img, size=ART_SIZE, level=110):
    """Flat logo art reads better without dithering."""
    grey = ImageOps.autocontrast(img.convert("L"), cutoff=1)
    return grey.resize((size, size), Image.LANCZOS).point(lambda v: 255 if v > level else 0).convert("1")


def sponsor_band(img, top, bottom, width=128, height=48, level=170):
    """Crop one sponsor logo out of boot_sp and fit it to a full-width splash. Bright = background."""
    band = img.convert("L").crop((0, top, 240, bottom))
    dark = band.point(lambda v: 255 if v < 200 else 0)
    bbox = dark.getbbox() or (0, 0, 240, bottom - top)
    logo = band.crop(bbox)
    scale = min(width / logo.width, height / logo.height)
    logo = logo.resize((int(logo.width * scale), int(logo.height * scale)), Image.LANCZOS)
    canvas = Image.new("L", (width, height), 255)
    canvas.paste(logo, ((width - logo.width) // 2, (height - logo.height) // 2))
    return canvas.point(lambda v: 255 if v > level else 0).convert("1")


# Flat art: logos and high-contrast images are thresholded instead of dithered.
FLAT = {"id_saver", "kali_saver", "arch_saver", "parrot_saver"}

# Vertical extents of the three logos stacked inside the 240x240 boot_sp image.
SPONSOR_BANDS = [(4, 76), (84, 166), (174, 236)]


def main():
    src = pathlib.Path(sys.argv[1]) / "img"
    root = pathlib.Path(__file__).resolve().parent.parent
    locks = root / "assets" / "locks"
    locks.mkdir(parents=True, exist_ok=True)
    preview = []

    for n, name in enumerate(LOCKS):
        data = parse_hex(src / f"{name}.h")
        img = mono_image(data, 240, 240) if name == "id_saver" else rgb565_image(data, 240, 240)
        art = threshold(img) if name in FLAT else dither(img)
        (locks / f"{n:02d}.bin").write_bytes(to_xbm(art))
        preview.append((name, art))

    logo = rgb565_image(parse_hex(src / "logo.h"), 140, 140)
    logo_art = threshold(logo, level=60)
    (root / "assets" / "logo.bin").write_bytes(to_xbm(logo_art))
    preview.append(("logo", logo_art))

    sponsors = rgb565_image(parse_hex(src / "boot_sp.h"), 240, 240)
    for n, (top, bottom) in enumerate(SPONSOR_BANDS):
        band = sponsor_band(sponsors, top, bottom)
        (root / "assets" / f"sponsor{n}.bin").write_bytes(to_xbm(band))
        preview.append((f"sponsor{n}", band))

    # App icon: 10x10, 1-bit. Hand-drawn lattice echoing the HackConRD chain logo.
    icon_rows = [
        "..##..##..",
        ".#..##..#.",
        ".#..##..#.",
        "..##..##..",
        ".#..##..#.",
        ".#..##..#.",
        "..##..##..",
        ".#..##..#.",
        ".#..##..#.",
        "..##..##..",
    ]
    icon = Image.new("1", (10, 10), 1)
    for y, row in enumerate(icon_rows):
        for x, c in enumerate(row):
            if c == "#":
                icon.putpixel((x, y), 0)
    icon.save(root / "icon.png")

    # Version 2-L holds the 22-byte URL and scans at 2x scale on the 128x64 LCD.
    qr = qrcode.QRCode(
        version=2,
        error_correction=qrcode.constants.ERROR_CORRECT_L,
        box_size=1,
        border=0,
    )
    qr.add_data(QR_URL)
    qr.make(fit=False)
    matrix = qr.get_matrix()
    size = len(matrix)
    row_bytes = (size + 7) // 8
    bits = bytearray(row_bytes * size)
    for y, row in enumerate(matrix):
        for x, on in enumerate(row):
            if on:
                bits[y * row_bytes + x // 8] |= 1 << (x % 8)
    lines = [
        "// Generated by tools/convert_assets.py - do not edit.",
        "#pragma once",
        "#include <stdint.h>",
        f"#define QR_SIZE {size}",
        f"#define QR_ROW_BYTES {row_bytes}",
        f'// Encodes: {QR_URL}',
        "static const uint8_t qr_bits[] = {",
    ]
    for i in range(0, len(bits), 12):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in bits[i : i + 12]) + ",")
    lines.append("};")
    (root / "src" / "qr_data.h").parent.mkdir(parents=True, exist_ok=True)
    (root / "src" / "qr_data.h").write_text("\n".join(lines) + "\n", newline="\n")

    # Contact sheet for review, written next to the tool output (not packaged).
    if len(sys.argv) > 2:
        cols = 8
        scale = 2
        rows = (len(preview) + cols - 1) // cols
        cell = ART_SIZE * scale + 6
        sheet = Image.new("L", (cols * cell, rows * cell), 128)
        for n, (_, art) in enumerate(preview):
            big = art.convert("L")
            big = big.resize((ART_SIZE * scale, big.height * ART_SIZE * scale // big.width), Image.NEAREST)
            sheet.paste(big, ((n % cols) * cell + 3, (n // cols) * cell + 3))
        sheet.save(sys.argv[2])


if __name__ == "__main__":
    main()
