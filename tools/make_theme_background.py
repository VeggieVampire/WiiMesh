#!/usr/bin/env python3
"""Convert a PNG/JPG/BMP image into WiiMesh's SD theme background format.

Output file:
  SD:/apps/wii-mesh/theme/background.rgb565

The raw file is 640x480 little-endian RGB565 pixels. WiiMesh uses this format
so custom themes do not require PNG/JPEG libraries on the Wii build.
"""

import argparse
import os
import struct
import sys


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main():
    parser = argparse.ArgumentParser(description="Create a WiiMesh theme background.")
    parser.add_argument("image", help="Input PNG/JPG/BMP image")
    parser.add_argument(
        "output",
        nargs="?",
        default="background.rgb565",
        help="Output path, default: background.rgb565",
    )
    args = parser.parse_args()

    try:
        from PIL import Image
    except ImportError:
        print("Pillow is required: py -m pip install pillow", file=sys.stderr)
        return 1

    img = Image.open(args.image).convert("RGB")
    img.thumbnail((640, 480), Image.Resampling.LANCZOS)
    canvas = Image.new("RGB", (640, 480), (2, 13, 23))
    x = (640 - img.width) // 2
    y = (480 - img.height) // 2
    canvas.paste(img, (x, y))

    os.makedirs(os.path.dirname(os.path.abspath(args.output)) or ".", exist_ok=True)
    with open(args.output, "wb") as f:
        for r, g, b in canvas.getdata():
            f.write(struct.pack("<H", rgb565(r, g, b)))

    print(f"Wrote {args.output} ({640 * 480 * 2} bytes)")
    print("Copy it to SD:/apps/wii-mesh/theme/background.rgb565")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
