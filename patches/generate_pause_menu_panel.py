from __future__ import annotations

import json
import random
from pathlib import Path
from typing import Iterable, Tuple

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_PNG = ROOT / "assets" / "UI" / "pause_menu_panel.png"
OUTPUT_JSON = ROOT / "assets" / "UI" / "pause_menu_panel.json"

SIZE = 128
SLICE = 28

Color = Tuple[int, int, int, int]


def premultiply(color: Color) -> Color:
    r, g, b, a = color
    return (
        int(round(r * a / 255.0)),
        int(round(g * a / 255.0)),
        int(round(b * a / 255.0)),
        a,
    )


def draw_polyline(draw: ImageDraw.ImageDraw, points: Iterable[Tuple[int, int]], color: Color, width: int) -> None:
    draw.line(list(points), fill=premultiply(color), width=width, joint="curve")


def draw_chamfered_panel(draw: ImageDraw.ImageDraw, inset: int, color: Color) -> None:
    max_coord = SIZE - 1 - inset
    points = [
        (inset + 11, inset),
        (max_coord - 11, inset),
        (max_coord, inset + 11),
        (max_coord, max_coord - 11),
        (max_coord - 11, max_coord),
        (inset + 11, max_coord),
        (inset, max_coord - 11),
        (inset, inset + 11),
    ]
    draw.polygon(points, fill=premultiply(color))


def add_center_texture(image: Image.Image) -> None:
    rng = random.Random(1337)
    texture = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(texture, "RGBA")

    for _ in range(420):
        x = rng.randrange(12, SIZE - 12)
        y = rng.randrange(12, SIZE - 12)
        alpha = rng.randrange(5, 18)
        shade = rng.randrange(90, 145)
        draw.point((x, y), fill=premultiply((shade, shade, shade, alpha)))

    for _ in range(20):
        x = rng.randrange(18, SIZE - 18)
        y = rng.randrange(18, SIZE - 18)
        length = rng.randrange(5, 16)
        alpha = rng.randrange(8, 18)
        draw.line(
            [(x, y), (x + length, y + rng.randrange(-2, 3))],
            fill=premultiply((170, 160, 135, alpha)),
            width=1,
        )

    image.alpha_composite(texture.filter(ImageFilter.GaussianBlur(radius=0.35)))


def draw_corner_ornaments(draw: ImageDraw.ImageDraw) -> None:
    gold = (214, 178, 98, 210)
    bright = (248, 224, 160, 170)
    shade = (78, 52, 26, 190)

    corners = [
        (0, 0, 1, 1),
        (SIZE - 1, 0, -1, 1),
        (0, SIZE - 1, 1, -1),
        (SIZE - 1, SIZE - 1, -1, -1),
    ]

    for origin_x, origin_y, sx, sy in corners:
        def p(x: int, y: int) -> Tuple[int, int]:
            return origin_x + sx * x, origin_y + sy * y

        draw_polyline(draw, [p(7, 22), p(7, 12), p(12, 7), p(23, 7)], shade, 4)
        draw_polyline(draw, [p(7, 22), p(7, 12), p(12, 7), p(23, 7)], gold, 2)
        draw_polyline(draw, [p(11, 26), p(17, 18), p(27, 12)], bright, 1)
        draw_polyline(draw, [p(15, 8), p(8, 15)], bright, 1)

        for dx, dy in [(10, 25), (24, 10)]:
            x, y = p(dx, dy)
            draw.rectangle([x - 1, y - 1, x + 1, y + 1], fill=premultiply((242, 198, 112, 180)))


def build_panel() -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image, "RGBA")

    draw_chamfered_panel(draw, 0, (12, 10, 9, 176))
    draw_chamfered_panel(draw, 5, (23, 21, 20, 214))
    add_center_texture(image)

    shadow = (10, 7, 5, 210)
    gold = (192, 154, 78, 218)
    bright = (246, 220, 150, 190)
    muted = (114, 82, 42, 185)

    outer = [(11, 1), (116, 1), (126, 11), (126, 116), (116, 126), (11, 126), (1, 116), (1, 11), (11, 1)]
    inner = [(15, 8), (112, 8), (119, 15), (119, 112), (112, 119), (15, 119), (8, 112), (8, 15), (15, 8)]
    highlight = [(19, 12), (109, 12), (115, 18), (115, 109), (109, 115), (19, 115), (12, 109), (12, 19), (19, 12)]

    draw_polyline(draw, outer, shadow, 5)
    draw_polyline(draw, outer, gold, 2)
    draw_polyline(draw, inner, muted, 2)
    draw_polyline(draw, highlight, bright, 1)

    draw_corner_ornaments(draw)

    # Keep the stretch bands readable by adding straight, subtle edge accents.
    draw.line([(SLICE, 5), (SIZE - SLICE, 5)], fill=premultiply((250, 226, 162, 96)), width=1)
    draw.line([(SLICE, SIZE - 6), (SIZE - SLICE, SIZE - 6)], fill=premultiply((250, 226, 162, 80)), width=1)
    draw.line([(5, SLICE), (5, SIZE - SLICE)], fill=premultiply((250, 226, 162, 70)), width=1)
    draw.line([(SIZE - 6, SLICE), (SIZE - 6, SIZE - SLICE)], fill=premultiply((250, 226, 162, 70)), width=1)

    return image


def write_metadata() -> None:
    data = {
        "width": SIZE,
        "height": SIZE,
        "crop-region": {"left": 0, "top": 0, "right": SIZE, "bottom": SIZE},
        "nine-slice": {"left": SLICE, "top": SLICE, "right": SIZE - SLICE, "bottom": SIZE - SLICE},
    }
    OUTPUT_JSON.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    OUTPUT_PNG.parent.mkdir(parents=True, exist_ok=True)
    build_panel().save(OUTPUT_PNG)
    write_metadata()
    print(f"Wrote {OUTPUT_PNG.relative_to(ROOT)}")
    print(f"Wrote {OUTPUT_JSON.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
