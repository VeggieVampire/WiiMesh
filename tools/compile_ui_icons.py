from pathlib import Path
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("Pillow is required to compile UI icons. Run: py -m pip install pillow", file=sys.stderr)
    raise


ROOT = Path(__file__).resolve().parents[1]
PROJECT_OUTPUT_ICONS = ROOT.parent.parent / "outputs" / "icons"
LEGACY_OUTPUT_ICONS = ROOT.parent.parent / "outputs" / "ui_icons"
ICON_DIR_ENV = os.environ.get("WIIMESH_ICON_DIR")
ICON_DIR = Path(ICON_DIR_ENV) if ICON_DIR_ENV else None
if ICON_DIR is None:
    if PROJECT_OUTPUT_ICONS.exists():
        ICON_DIR = PROJECT_OUTPUT_ICONS
    elif LEGACY_OUTPUT_ICONS.exists():
        ICON_DIR = LEGACY_OUTPUT_ICONS
    else:
        ICON_DIR = ROOT / "assets" / "ui_icons"
OUT = ROOT / "include" / "wiimesh" / "generated" / "MenuIcons.h"
ICONS = [
    ("home", "Home"),
    ("node", "Node"),
    ("channels", "Channels"),
    ("chat", "Chat"),
    ("map", "Map"),
    ("settings", "Settings"),
    ("bell", "Bell"),
]


def rows_for(path: Path):
    img = Image.open(path).convert("RGBA").resize((32, 32), Image.Resampling.NEAREST)
    rows = []
    for y in range(32):
        bits = 0
        for x in range(32):
            _, _, _, a = img.getpixel((x, y))
            if a >= 64:
                bits |= 1 << (31 - x)
        rows.append(bits)
    return rows


def main():
    print(f"using UI icons from {ICON_DIR}")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace wiimesh {",
        "namespace icons {",
        "",
        "constexpr int IconSize = 32;",
        "",
    ]
    for filename, symbol in ICONS:
        path = ICON_DIR / f"{filename}.png"
        if not path.exists():
            raise FileNotFoundError(path)
        lines.append(f"constexpr uint32_t {symbol}[IconSize] = {{")
        for row in rows_for(path):
            lines.append(f"    0x{row:08x}u,")
        lines.append("};")
        lines.append("")
    lines.append("}")
    lines.append("}")
    lines.append("")
    OUT.write_text("\n".join(lines), encoding="ascii")
    print(f"compiled {len(ICONS)} UI icons -> {OUT}")


if __name__ == "__main__":
    main()
