"""Extract battle result UI assets from a generated concept sheet.

The generated sheet is a flat RGBA composite. This script turns the useful
parts into game-ready transparent PNGs with stable crop boxes and simple alpha
cleanup so future runs produce the same files.
"""

from __future__ import annotations

from collections import deque
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "source_assets" / "battle_result_ui_raw.png"
OUTPUT_DIR = ROOT / "assets" / "UI" / "battle_result"


def clamp_byte(value: float) -> int:
    return max(0, min(255, int(value)))


def smoothstep(edge0: float, edge1: float, value: float) -> float:
    if edge0 == edge1:
        return 1.0 if value >= edge1 else 0.0
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def ensure_dirs() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)


def save_with_alpha(image: Image.Image, alpha: Image.Image, output: Path) -> None:
    rgba = image.convert("RGBA")
    rgba.putalpha(alpha)
    rgba.save(output)


def solid_layer(size: tuple[int, int], color: tuple[int, int, int], alpha: Image.Image) -> Image.Image:
    layer = Image.new("RGBA", size, (color[0], color[1], color[2], 0))
    layer.putalpha(alpha)
    return layer


def alpha_from_red_signal(image: Image.Image,
                          threshold: float = 3.0,
                          multiplier: float = 3.2,
                          blur_radius: float = 0.35) -> Image.Image:
    pixels = image.convert("RGBA").load()
    alpha = Image.new("L", image.size, 0)
    out = alpha.load()

    for y in range(image.height):
        for x in range(image.width):
            r, g, b, _ = pixels[x, y]
            signal = r - (max(g, b) * 1.18)
            out[x, y] = clamp_byte((signal - threshold) * multiplier)

    return alpha.filter(ImageFilter.GaussianBlur(blur_radius))


def apply_elliptical_falloff(alpha: Image.Image,
                             inner_radius: float,
                             outer_radius: float,
                             x_scale: float,
                             y_scale: float,
                             center_x: float = 0.5,
                             center_y: float = 0.5) -> Image.Image:
    source = alpha.convert("L").load()
    width, height = alpha.size
    result = Image.new("L", alpha.size, 0)
    out = result.load()
    cx = width * center_x
    cy = height * center_y
    rx = max(1.0, width * 0.5 * x_scale)
    ry = max(1.0, height * 0.5 * y_scale)

    for y in range(height):
        for x in range(width):
            dx = (x - cx) / rx
            dy = (y - cy) / ry
            distance = (dx * dx + dy * dy) ** 0.5
            fade = 1.0 - smoothstep(inner_radius, outer_radius, distance)
            out[x, y] = clamp_byte(source[x, y] * fade)

    return result


def remove_small_alpha_components(alpha: Image.Image, min_alpha: int, min_area: int) -> Image.Image:
    width, height = alpha.size
    source = alpha.convert("L")
    src = source.load()
    visited = bytearray(width * height)
    keep = Image.new("L", alpha.size, 0)
    out = keep.load()

    for start_y in range(height):
        for start_x in range(width):
            start_index = start_y * width + start_x
            if visited[start_index] or src[start_x, start_y] < min_alpha:
                continue

            pixels: list[tuple[int, int]] = []
            queue: deque[tuple[int, int]] = deque([(start_x, start_y)])
            visited[start_index] = 1

            while queue:
                x, y = queue.popleft()
                pixels.append((x, y))

                for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    index = ny * width + nx
                    if visited[index] or src[nx, ny] < min_alpha:
                        continue
                    visited[index] = 1
                    queue.append((nx, ny))

            if len(pixels) < min_area:
                continue
            for x, y in pixels:
                out[x, y] = src[x, y]

    return keep


def colorize_red_strokes(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()

    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                pixels[x, y] = (0, 0, 0, 0)
                continue
            pixels[x, y] = (
                clamp_byte(max(r, 142) * 1.12),
                clamp_byte(g * 0.62 + 12),
                clamp_byte(b * 0.58 + 10),
                a,
            )

    return rgba


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
    crop = sheet.crop((80, 0, 590, 560))
    stroke_alpha = alpha_from_red_signal(crop, threshold=28.0, multiplier=6.0, blur_radius=0.18)
    stroke_alpha = remove_small_alpha_components(stroke_alpha, min_alpha=18, min_area=34)
    stroke_alpha = apply_elliptical_falloff(stroke_alpha,
                                            inner_radius=0.56,
                                            outer_radius=0.96,
                                            x_scale=0.86,
                                            y_scale=0.92,
                                            center_x=0.51,
                                            center_y=0.47)

    core_alpha = stroke_alpha.filter(ImageFilter.GaussianBlur(9.0))
    core_alpha = core_alpha.point(lambda p: clamp_byte(p * 1.35))
    core_alpha = apply_elliptical_falloff(core_alpha,
                                          inner_radius=0.46,
                                          outer_radius=0.88,
                                          x_scale=0.82,
                                          y_scale=0.88,
                                          center_x=0.51,
                                          center_y=0.47)

    glow_alpha = stroke_alpha.filter(ImageFilter.GaussianBlur(34.0))
    glow_alpha = glow_alpha.point(lambda p: clamp_byte(p * 1.65))
    glow_alpha = apply_elliptical_falloff(glow_alpha,
                                          inner_radius=0.34,
                                          outer_radius=0.90,
                                          x_scale=0.78,
                                          y_scale=0.84,
                                          center_x=0.51,
                                          center_y=0.47)

    result = Image.new("RGBA", crop.size, (0, 0, 0, 0))
    result = Image.alpha_composite(result, solid_layer(crop.size, (168, 24, 20), glow_alpha))
    result = Image.alpha_composite(result, solid_layer(crop.size, (8, 0, 6), core_alpha))

    stroke_layer = colorize_red_strokes(crop)
    stroke_layer.putalpha(stroke_alpha)
    result = Image.alpha_composite(result, stroke_layer)
    result.save(OUTPUT_DIR / "battle_result_defeat_sigil.png")


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
