#!/usr/bin/env python3
"""Generate the Spectra app / favicon mark: black square, cyan glow, cyan outline."""

from __future__ import annotations

import math
import os
from pathlib import Path

from PIL import Image

CYAN = (0, 240, 255)
SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" role="img" aria-label="Spectra">
  <rect width="512" height="512" fill="#000000"/>
  <defs>
    <radialGradient id="dot" cx="50%" cy="50%" r="42%">
      <stop offset="0%" stop-color="#00f0ff" stop-opacity="1"/>
      <stop offset="28%" stop-color="#00f0ff" stop-opacity="0.75"/>
      <stop offset="100%" stop-color="#00f0ff" stop-opacity="0"/>
    </radialGradient>
  </defs>
  <circle cx="256" cy="256" r="215" fill="url(#dot)"/>
  <rect x="7" y="7" width="498" height="498" fill="none" stroke="#00f0ff" stroke-width="14"/>
</svg>
"""


def make_icon(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    pixels = img.load()
    center = (size - 1) / 2.0
    stroke = max(1, round(size * 0.028))
    glow_inner = size * 0.055
    glow_outer = size * 0.22
    for y in range(size):
        for x in range(size):
            radius = math.hypot(x - center, y - center)
            if radius <= glow_inner:
                alpha = 1.0
            elif radius >= glow_outer:
                alpha = 0.0
            else:
                fade = (radius - glow_inner) / (glow_outer - glow_inner)
                alpha = (1.0 - fade) * (1.0 - fade)
            if alpha > 0:
                pixels[x, y] = (
                    int(CYAN[0] * alpha),
                    int(CYAN[1] * alpha),
                    int(CYAN[2] * alpha),
                    255,
                )
            if x < stroke or y < stroke or x >= size - stroke or y >= size - stroke:
                pixels[x, y] = (*CYAN, 255)
    return img


def write_svg(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(SVG, encoding="utf-8", newline="\n")


def main() -> None:
    repo = Path(__file__).resolve().parents[1]
    market = Path(os.environ.get("SPECTRA_MARKETPLACE", r"C:\Users\quent\ProfilerMarketplace"))
    png_dir = repo / "Source" / "Assets" / "png"
    png_dir.mkdir(parents=True, exist_ok=True)

    ico_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    big = make_icon(1024)
    big.save(png_dir / "SpectraIcon.png", "PNG")
    make_icon(256).save(png_dir / "SpectraIcon.ico", format="ICO", sizes=ico_sizes)
    write_svg(repo / "Source" / "Assets" / "Svg" / "spectra-mark.svg")

    (market / "app").mkdir(parents=True, exist_ok=True)
    (market / "public" / "brand").mkdir(parents=True, exist_ok=True)
    make_icon(512).save(market / "app" / "icon.png", "PNG")
    make_icon(180).save(market / "app" / "apple-icon.png", "PNG")
    make_icon(256).save(market / "app" / "favicon.ico", format="ICO", sizes=ico_sizes)
    write_svg(market / "app" / "icon.svg")
    write_svg(market / "public" / "brand" / "spectra-mark.svg")
    write_svg(market / "public" / "brand" / "profiler-mark.svg")
    print("generated Spectra icons")


if __name__ == "__main__":
    main()
