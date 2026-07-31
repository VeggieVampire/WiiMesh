from pathlib import Path
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Pillow is required to compile emoji icons. Run: py -m pip install pillow", file=sys.stderr)
    raise


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "include" / "wiimesh" / "generated" / "EmojiIcons.h"
PREVIEW = ROOT.parent.parent / "outputs" / "emoji_atlas_preview.png"
FONT_CANDIDATES = [
    Path(r"C:\Windows\Fonts\seguiemj.ttf"),
    Path(r"C:\Windows\Fonts\SegoeUIEmoji.ttf"),
    Path(r"C:\Windows\Fonts\seguisym.ttf"),
]

EMOJIS = [
    (0x1F93A, "PersonFencing"),
    (0x1F426, "Bird"),
    (0x1F99E, "Lobster"),
    (0x1F5FC, "TokyoTower"),
    (0x1F3C3, "Runner"),
    (0x1F6B6, "Walking"),
    (0x1F3E0, "Home"),
    (0x1F4E1, "SatelliteAntenna"),
    (0x1F514, "Bell"),
    (0x1F4AC, "SpeechBalloon"),
    (0x1F4CD, "RoundPushpin"),
    (0x1F512, "Lock"),
    (0x1F511, "Key"),
    (0x1F465, "People"),
    (0x1F697, "Car"),
    (0x1F4F6, "SignalBars"),
    (0x2B50, "Star"),
    (0x2753, "Question"),
    (0x26A0, "Warning"),
]


def font_path() -> Path:
    for path in FONT_CANDIDATES:
        if path.exists():
            return path
    raise FileNotFoundError("No Windows emoji/symbol font found")


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def image_for(cp: int, font: ImageFont.FreeTypeFont):
    size = 64
    glyph = chr(cp)
    src = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(src)
    bbox = draw.textbbox((0, 0), glyph, font=font, embedded_color=True)
    gw = bbox[2] - bbox[0]
    gh = bbox[3] - bbox[1]
    x = (size - gw) // 2 - bbox[0]
    y = (size - gh) // 2 - bbox[1]
    draw.text((x, y), glyph, font=font, embedded_color=True, fill=(255, 255, 255, 255))
    return src.resize((32, 32), Image.Resampling.LANCZOS)


def pixels_for(img: Image.Image):
    colors = []
    alpha = []
    for yy in range(32):
        for xx in range(32):
            r, g, b, a = img.getpixel((xx, yy))
            colors.append(rgb565(r, g, b) if a else 0)
            alpha.append(a)
    return colors, alpha


def write_preview(images):
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    cell = 56
    preview = Image.new("RGBA", (cell * len(images), 72), (8, 18, 22, 255))
    draw = ImageDraw.Draw(preview)
    for i, (cp, name, img) in enumerate(images):
        x = i * cell
        preview.alpha_composite(img.resize((48, 48), Image.Resampling.NEAREST), (x + 4, 4))
        draw.text((x + 3, 55), f"{cp:X}"[-4:], fill=(235, 238, 238, 255))
    preview.save(PREVIEW)


def main():
    fp = font_path()
    print(f"using emoji font {fp}")
    font = ImageFont.truetype(str(fp), 52)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace wiimesh {",
        "namespace emoji {",
        "",
        "struct Icon {",
        "    uint32_t codepoint;",
        "    uint16_t pixels[32 * 32];",
        "    uint8_t alpha[32 * 32];",
        "};",
        "",
        f"constexpr int IconCount = {len(EMOJIS)};",
        "constexpr Icon Icons[IconCount] = {",
    ]
    rendered = [(cp, name, image_for(cp, font)) for cp, name in EMOJIS]
    write_preview(rendered)
    for cp, name, img in rendered:
        colors, alpha = pixels_for(img)
        lines.append(f"    {{0x{cp:08x}u, {{ // {name} pixels")
        for i in range(0, len(colors), 16):
            lines.append("        " + ", ".join(f"0x{v:04x}u" for v in colors[i:i + 16]) + ",")
        lines.append("    }, {")
        for i in range(0, len(alpha), 16):
            lines.append("        " + ", ".join(f"{v}u" for v in alpha[i:i + 16]) + ",")
        lines.append("    }},")
    lines += [
        "};",
        "",
        "}",
        "}",
        "",
    ]
    OUT.write_text("\n".join(lines), encoding="ascii")
    print(f"compiled {len(EMOJIS)} emoji icons -> {OUT}")
    print(f"preview -> {PREVIEW}")


if __name__ == "__main__":
    main()
