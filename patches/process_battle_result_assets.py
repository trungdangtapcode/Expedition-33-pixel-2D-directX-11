"""Extract battle result UI assets from a generated concept sheet.

The generated sheet is a flat RGBA composite. This script turns the useful
parts into game-ready transparent PNGs with stable crop boxes and simple alpha
cleanup so future runs produce the same files.
"""

from __future__ import annotations

from pathlib import Path
from typing import Iterable, Tuple

from PIL import Image, ImageChops, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "source_assets" / "battle_result_ui_raw.png"
OUTPUT_DIR = ROOT / "assets" / "UI" / "battle_result"


def clamp_byte(value: float) -> int:
    return max(0, min(255, int(value)))


def ensure_dirs() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)


def save_with_alpha(image: Image.Image, alpha: Image.Image, output: Path) -> None:
    rgba = image.convert("RGBA")
    rgba.putalpha(alpha)
    rgba.save(output)


def alpha_from_red_signal(image: Image.Image) -> Image.Image:
    pixels = image.convert("RGBA").load()
    alpha = Image.new("L", image.size, 0)
    out = alpha.load()

    for y in range(image.height):
        for x in range(image.width):
            r, g, b, _ = pixels[x, y]
            signal = r - (max(g, b) * 1.18)
            out[x, y] = clamp_byte((signal - 3.0) * 3.2)

    return alpha.filter(ImageFilter.GaussianBlur(0.35))


def alpha_from_gold_signal(image: Image.Image, threshold: float = 22.0, multiplier: float = 2.8) -> Image.Image:
    pixels = image.convert("RGBA").load()
    alpha = Image.new("L", image.size, 0)
    out = alpha.load()

    for y in range(image.height):
        for x in range(image.width):
            r, g, b, _ = pixels[x, y]
            warm = min(r, g * 1.35) - b * 0.55
            out[x, y] = clamp_byte((warm - threshold) * multiplier)

    return alpha.filter(ImageFilter.GaussianBlur(0.25))


def extract_sigil(sheet: Image.Image) -> None:
    crop = sheet.crop((150, 22, 590, 520))
    alpha = alpha_from_red_signal(crop)
    save_with_alpha(crop, alpha, OUTPUT_DIR / "battle_result_defeat_sigil.png")


def extract_prompt_panel(sheet: Image.Image) -> None:
    crop = sheet.crop((700, 145, 1465, 400)).convert("RGBA")
    width, height = crop.size

    fill_mask = Image.new("L", crop.size, 0)
    draw = ImageDraw.Draw(fill_mask)
    draw.rounded_rectangle((12, 12, width - 12, height - 12), radius=24, fill=210)

    gold_mask = alpha_from_gold_signal(crop).point(lambda p: clamp_byte(p * 1.35))
    alpha = ImageChops.lighter(fill_mask, gold_mask)
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.2))
    save_with_alpha(crop, alpha, OUTPUT_DIR / "battle_result_prompt_panel.png")


def extract_flourish(sheet: Image.Image) -> None:
    crop = sheet.crop((760, 635, 1470, 770))
    alpha = alpha_from_gold_signal(crop, threshold=92.0, multiplier=4.6)
    save_with_alpha(crop, alpha, OUTPUT_DIR / "battle_result_victory_flourish.png")


def extract_vignette(sheet: Image.Image) -> None:
    crop = sheet.crop((0, 505, 705, 940)).convert("RGBA")
    gray = crop.convert("L")
    alpha = gray.point(lambda p: clamp_byte((92 - p) * 3.0))
    alpha = alpha.filter(ImageFilter.GaussianBlur(2.0))

    brush = Image.new("RGBA", crop.size, (0, 0, 0, 0))
    brush.putalpha(alpha)
    brush = brush.resize((1920, 1080), Image.Resampling.LANCZOS)
    brush.save(OUTPUT_DIR / "battle_result_ink_vignette.png")


def main() -> None:
    ensure_dirs()
    sheet = Image.open(DEFAULT_INPUT).convert("RGBA")
    extract_sigil(sheet)
    extract_prompt_panel(sheet)
    extract_flourish(sheet)
    extract_vignette(sheet)


if __name__ == "__main__":
    main()
