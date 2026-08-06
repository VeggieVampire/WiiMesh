from pathlib import Path
import html
import re
import subprocess
import sys
import tempfile
import urllib.request

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Pillow is required to compile emoji icons. Run: py -m pip install pillow", file=sys.stderr)
    raise


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "include" / "wiimesh" / "generated" / "EmojiIcons.h"
OUTPUTS = ROOT.parent.parent / "outputs"
ICON_ROOT = OUTPUTS / "icons"
EMOJI_DIR = ICON_ROOT / "emojis"
LEGACY_BADGE_DIR = ICON_ROOT / "badges"
PREVIEW = OUTPUTS / "emoji_atlas_preview.png"
EMOJI_PREVIEW = EMOJI_DIR / "emoji_contact_sheet.png"
LIVE_CODEPOINTS = EMOJI_DIR / "live_emoji_codepoints.txt"
UNICODE_EMOJI_TEST = EMOJI_DIR / "unicode_emoji_test_latest.txt"
UNICODE_EMOJI_URL = "https://www.unicode.org/Public/emoji/latest/emoji-test.txt"
MAX_CODEPOINTS_PER_EMOJI = 16
FONT_CANDIDATES = [
    Path(r"C:\Windows\Fonts\seguiemj.ttf"),
    Path(r"C:\Windows\Fonts\SegoeUIEmoji.ttf"),
    Path(r"C:\Windows\Fonts\seguisym.ttf"),
]
CHROME_CANDIDATES = [
    Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe"),
    Path(r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"),
    Path(r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"),
    Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
]
BROWSER_CELL = 72
BROWSER_COLS = 16
BROWSER_BATCH = 512

EMOJIS = [
    (0x2302, "HouseSymbol"),
    (0x2699, "Gear"),
    (0x26A0, "Warning"),
    (0x26A1, "HighVoltage"),
    (0x26F0, "Mountain"),
    (0x2615, "HotBeverage"),
    (0x2705, "CheckButton"),
    (0x2713, "CheckMark"),
    (0x2714, "HeavyCheckMark"),
    (0x2753, "Question"),
    (0x2754, "WhiteQuestion"),
    (0x2B50, "Star"),
    (0x2B1B, "BlackLargeSquare"),
    (0x1F93A, "PersonFencing"),
    (0x1F426, "Bird"),
    (0x1F99E, "Lobster"),
    (0x1F5FC, "TokyoTower"),
    (0x1F3C3, "Runner"),
    (0x1F37A, "BeerMug"),
    (0x1F3D4, "SnowMountain"),
    (0x1F3D5, "Camping"),
    (0x1F3DE, "NationalPark"),
    (0x1F3E0, "Home"),
    (0x1F3E1, "HouseGarden"),
    (0x1F3E2, "OfficeBuilding"),
    (0x1F465, "People"),
    (0x1F4AC, "SpeechBalloon"),
    (0x1F4CD, "RoundPushpin"),
    (0x1F4E1, "SatelliteAntenna"),
    (0x1F4F6, "SignalBars"),
    (0x1F4FB, "Radio"),
    (0x1F50B, "Battery"),
    (0x1F511, "Key"),
    (0x1F512, "Lock"),
    (0x1F513, "Unlock"),
    (0x1F514, "Bell"),
    (0x1F5E8, "LeftSpeechBubble"),
    (0x1F5FA, "WorldMap"),
    (0x1F697, "Car"),
    (0x1F699, "SportUtility"),
    (0x1F69A, "Truck"),
    (0x1F6B6, "Walking"),
    (0x1F6B2, "Bicycle"),
    (0x1F6DC, "Wireless"),
    (0x1F920, "CowboyHatFace"),
    (0x1F9C0, "CheeseWedge"),
]


def font_path() -> Path:
    for path in FONT_CANDIDATES:
        if path.exists():
            return path
    raise FileNotFoundError("No Windows emoji/symbol font found")


def chrome_path() -> Path | None:
    for path in CHROME_CANDIDATES:
        if path.exists():
            return path
    return None


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def sequence_key(cps) -> str:
    return "_".join(f"{cp:x}" for cp in cps)


def emoji_text(cps) -> str:
    return "".join(chr(cp) for cp in cps)


def centered_32(img: Image.Image) -> Image.Image:
    src = img.convert("RGBA")
    bbox = src.getbbox()
    if not bbox:
        return Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    cropped = src.crop(bbox)
    scale = min(56 / max(1, cropped.width), 56 / max(1, cropped.height))
    scaled = cropped.resize(
        (max(1, int(cropped.width * scale)), max(1, int(cropped.height * scale))),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    canvas.alpha_composite(scaled, ((64 - scaled.width) // 2, (64 - scaled.height) // 2))
    return canvas.resize((32, 32), Image.Resampling.LANCZOS)


def browser_render_icons(items):
    chrome = chrome_path()
    if chrome is None:
        print("warning: Chrome/Edge not found; using Pillow emoji fallback", file=sys.stderr)
        return {}

    targets = [(tuple(cps), name) for cps, name in items if len(cps) > 1]
    if not targets:
        return {}

    rendered = {}
    EMOJI_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="wiimesh_emoji_") as td:
        temp = Path(td)
        for batch_start in range(0, len(targets), BROWSER_BATCH):
            batch = targets[batch_start:batch_start + BROWSER_BATCH]
            rows = (len(batch) + BROWSER_COLS - 1) // BROWSER_COLS
            width = BROWSER_COLS * BROWSER_CELL
            height = rows * BROWSER_CELL
            cells = "\n".join(
                f'<div class="cell">{html.escape(emoji_text(cps))}</div>'
                for cps, _ in batch
            )
            page = temp / f"emoji_{batch_start}.html"
            shot = temp / f"emoji_{batch_start}.png"
            page.write_text(
                "<!doctype html><meta charset='utf-8'>"
                "<style>"
                "html,body{margin:0;padding:0;background:transparent;overflow:hidden;}"
                ".grid{display:grid;grid-template-columns:repeat(" + str(BROWSER_COLS) + "," + str(BROWSER_CELL) + "px);}"
                ".cell{width:" + str(BROWSER_CELL) + "px;height:" + str(BROWSER_CELL) + "px;"
                "display:flex;align-items:center;justify-content:center;"
                "font-family:'Segoe UI Emoji','Segoe UI Symbol',sans-serif;font-size:48px;line-height:1;}"
                "</style><div class='grid'>" + cells + "</div>",
                encoding="utf-8",
            )
            cmd = [
                str(chrome),
                "--headless=new",
                "--disable-gpu",
                "--hide-scrollbars",
                "--force-device-scale-factor=1",
                "--default-background-color=00000000",
                f"--window-size={width},{height}",
                f"--screenshot={shot}",
                page.as_uri(),
            ]
            try:
                subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
            except Exception as exc:
                print(f"warning: browser emoji batch {batch_start} failed: {exc}", file=sys.stderr)
                continue
            sheet = Image.open(shot).convert("RGBA")
            for i, (cps, name) in enumerate(batch):
                x = (i % BROWSER_COLS) * BROWSER_CELL
                y = (i // BROWSER_COLS) * BROWSER_CELL
                icon = centered_32(sheet.crop((x, y, x + BROWSER_CELL, y + BROWSER_CELL)))
                path = emoji_path(cps, name)
                icon.save(path)
                rendered[cps] = icon
    print(f"browser-rendered {len(rendered)} emoji sequences")
    return rendered


def image_for(cps, font: ImageFont.FreeTypeFont, browser_images=None):
    browser_images = browser_images or {}
    sequence = tuple(cps)
    if sequence in browser_images:
        return browser_images[sequence]
    key = sequence_key(cps)
    if EMOJI_DIR.exists():
        for path in EMOJI_DIR.glob(f"emoji_u{key}_*.png"):
            return Image.open(path).convert("RGBA").resize((32, 32), Image.Resampling.LANCZOS)
    if LEGACY_BADGE_DIR.exists() and len(cps) == 1:
        for path in LEGACY_BADGE_DIR.glob(f"badge_u{cps[0]:04x}_*.png"):
            return Image.open(path).convert("RGBA").resize((32, 32), Image.Resampling.LANCZOS)
    size = 64
    glyph = emoji_text(cps)
    src = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(src)
    bbox = draw.textbbox((0, 0), glyph, font=font, embedded_color=True)
    gw = bbox[2] - bbox[0]
    gh = bbox[3] - bbox[1]
    x = (size - gw) // 2 - bbox[0]
    y = (size - gh) // 2 - bbox[1]
    draw.text((x, y), glyph, font=font, embedded_color=True, fill=(255, 255, 255, 255))
    return centered_32(src)


def slug_name(name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
    return slug or "icon"


def emoji_path(cps, name: str) -> Path:
    return EMOJI_DIR / f"emoji_u{sequence_key(cps)}_{slug_name(name)}.png"


def ensure_emoji_png(cps, name: str, img: Image.Image):
    EMOJI_DIR.mkdir(parents=True, exist_ok=True)
    path = emoji_path(cps, name)
    if not path.exists():
        img.save(path)
    return path


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
    sample = images[:64]
    cell = 56
    preview = Image.new("RGBA", (cell * len(sample), 72), (8, 18, 22, 255))
    draw = ImageDraw.Draw(preview)
    for i, (cps, name, img) in enumerate(sample):
        x = i * cell
        preview.alpha_composite(img.resize((48, 48), Image.Resampling.NEAREST), (x + 4, 4))
        draw.text((x + 3, 55), sequence_key(cps)[-4:], fill=(235, 238, 238, 255))
    preview.save(PREVIEW)

    cols = 8
    rows = (len(sample) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * 92, rows * 62), (8, 18, 22, 255))
    draw = ImageDraw.Draw(sheet)
    for i, (cps, name, img) in enumerate(sample):
        x = (i % cols) * 92
        y = (i // cols) * 62
        sheet.alpha_composite(img.resize((40, 40), Image.Resampling.NEAREST), (x + 4, y + 4))
        draw.text((x + 48, y + 6), "U+" + sequence_key(cps).upper()[:12], fill=(235, 238, 238, 255))
        draw.text((x + 48, y + 23), name[:12], fill=(180, 205, 212, 255))
    EMOJI_PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(EMOJI_PREVIEW)


def should_render_live_codepoint(cp: int) -> bool:
    if cp in (0x200D, 0xFE0F):
        return False
    if 0x1F3FB <= cp <= 0x1F3FF:
        return False
    return (0x2600 <= cp <= 0x27BF) or (0x2B00 <= cp <= 0x2BFF) or (0x1F000 <= cp <= 0x1FAFF)


def live_emoji_codepoints():
    found = set()
    paths = []
    for name in ("debug.log", "udp-debug.log"):
        p = OUTPUTS / name
        if p.exists():
            paths.append(p)
    logs = OUTPUTS / "logs_before_upload"
    if logs.exists():
        paths.extend(logs.glob("*debug.log"))
    for path in paths:
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for match in re.finditer(r"U\+([0-9A-Fa-f]{4,6})", text):
            cp = int(match.group(1), 16)
            if should_render_live_codepoint(cp):
                found.add(cp)
    return sorted(found)


def download_unicode_emoji_test():
    if UNICODE_EMOJI_TEST.exists() and UNICODE_EMOJI_TEST.stat().st_size > 100000:
        return
    EMOJI_DIR.mkdir(parents=True, exist_ok=True)
    try:
        with urllib.request.urlopen(UNICODE_EMOJI_URL, timeout=15) as response:
            data = response.read()
        UNICODE_EMOJI_TEST.write_bytes(data)
        print(f"cached Unicode emoji list -> {UNICODE_EMOJI_TEST}")
    except Exception as exc:
        print(f"warning: could not fetch Unicode emoji list: {exc}", file=sys.stderr)


def unicode_emoji_items():
    download_unicode_emoji_test()
    if not UNICODE_EMOJI_TEST.exists():
        return []
    items = []
    seen = set()
    for line in UNICODE_EMOJI_TEST.read_text(encoding="utf-8", errors="ignore").splitlines():
        if "; fully-qualified" not in line or "#" not in line:
            continue
        left, comment = line.split("#", 1)
        cps_text = left.split(";", 1)[0].strip()
        cps = [int(part, 16) for part in cps_text.split()]
        if len(cps) > MAX_CODEPOINTS_PER_EMOJI:
            continue
        key = tuple(cps)
        if key in seen:
            continue
        name = comment.strip()
        match = re.search(r"\sE[0-9.]+\s+(.+)$", name)
        readable = match.group(1) if match else name
        readable = re.sub(r"[^A-Za-z0-9]+", " ", readable).strip().title().replace(" ", "")
        items.append((cps, readable or f"UnicodeU{sequence_key(cps).upper()}"))
        seen.add(key)
    return items


def emoji_list_with_live():
    base = [([cp], name) for cp, name in EMOJIS]
    seen = {tuple(cps) for cps, _ in base}
    for cps, name in unicode_emoji_items():
        key = tuple(cps)
        if key not in seen:
            base.append((cps, name))
            seen.add(key)
    live = []
    for cp in live_emoji_codepoints():
        key = (cp,)
        if key not in seen:
            base.append(([cp], f"LiveU{cp:X}"))
            live.append(cp)
            seen.add(key)
    if live:
        EMOJI_DIR.mkdir(parents=True, exist_ok=True)
        LIVE_CODEPOINTS.write_text(
            "\n".join(f"U+{cp:X}" for cp in live) + "\n",
            encoding="ascii",
        )
    return base


def main():
    fp = font_path()
    print(f"using emoji font {fp}")
    font = ImageFont.truetype(str(fp), 52)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    emoji_items = emoji_list_with_live()
    browser_images = browser_render_icons(emoji_items)
    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace wiimesh {",
        "namespace emoji {",
        "",
        f"constexpr int MaxCodepoints = {MAX_CODEPOINTS_PER_EMOJI};",
        "",
        "struct Icon {",
        "    uint8_t length;",
        "    uint32_t codepoints[MaxCodepoints];",
        "    uint16_t pixels[32 * 32];",
        "    uint8_t alpha[32 * 32];",
        "};",
        "",
        f"constexpr int IconCount = {len(emoji_items)};",
        "constexpr Icon Icons[IconCount] = {",
    ]
    rendered = [(cps, name, image_for(cps, font, browser_images)) for cps, name in emoji_items]
    for cps, name, img in rendered:
        ensure_emoji_png(cps, name, img)
    write_preview(rendered)
    for cps, name, img in rendered:
        colors, alpha = pixels_for(img)
        padded = list(cps)[:MAX_CODEPOINTS_PER_EMOJI] + [0] * max(0, MAX_CODEPOINTS_PER_EMOJI - len(cps))
        cp_values = ", ".join(f"0x{cp:08x}u" for cp in padded)
        lines.append(f"    {{{len(cps)}u, {{{cp_values}}}, {{ // {name} pixels")
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
    print(f"compiled {len(emoji_items)} emoji icons -> {OUT}")
    print(f"preview -> {PREVIEW}")
    print(f"editable emoji icons -> {EMOJI_DIR}")


if __name__ == "__main__":
    main()
