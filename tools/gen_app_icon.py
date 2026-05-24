#!/usr/bin/env python3
"""Generate resources/icons/app.ico from the kImgEdit app.svg design."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources" / "icons" / "app.ico"


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def gradient_color(t: float) -> tuple[int, int, int]:
    return (
        int(lerp(0x3B, 0x1E, t)),
        int(lerp(0x82, 0x40, t)),
        int(lerp(0xF6, 0xAF, t)),
    )


def draw_icon(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    s = size / 64.0

    def R(v: float) -> float:
        return v * s

    # Gradient background with rounded corners
    bg = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    bg_draw = ImageDraw.Draw(bg)
    for y in range(size):
        t = y / max(size - 1, 1)
        bg_draw.line([(0, y), (size, y)], fill=gradient_color(t) + (255,))
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (R(4), R(4), R(60), R(60)), radius=R(12), fill=255
    )
    rounded = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    rounded.paste(bg, mask=mask)
    img = Image.alpha_composite(img, rounded)
    d = ImageDraw.Draw(img)

    d.rounded_rectangle(
        (R(12), R(14), R(52), R(44)),
        radius=R(4),
        fill=(0xE2, 0xE8, 0xF0, 255),
    )
    d.ellipse((R(18), R(20), R(26), R(28)), fill=(0xFB, 0xBF, 0x24, 255))
    d.polygon(
        [
            (R(14), R(40)),
            (R(26), R(28)),
            (R(34), R(36)),
            (R(44), R(24)),
            (R(50), R(30)),
            (R(50), R(42)),
        ],
        fill=(0x22, 0xC5, 0x5E, 255),
    )
    w = max(int(R(4)), 1)
    d.line([(R(40), R(46)), (R(48), R(54))], fill=(0xFB, 0xBF, 0x24, 255), width=w)
    d.line([(R(46), R(46)), (R(48), R(48))], fill=(0xFB, 0xBF, 0x24, 255), width=w)
    return img


def main() -> None:
    sizes = [256, 128, 64, 48, 32, 24, 16]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    # Save from the largest raster; Pillow downscales into the ICO container.
    draw_icon(256).save(OUT, format="ICO", sizes=[(sz, sz) for sz in sizes])
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes, {len(sizes)} sizes)")


if __name__ == "__main__":
    main()
