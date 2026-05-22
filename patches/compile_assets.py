#!/usr/bin/env python3
"""Generate deterministic top-down overworld atlases."""

from __future__ import annotations

import random
from pathlib import Path
from typing import Iterable

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "assets" / "environments"
TILE = 64
COLS = 8


def rgba(hex_color: str, alpha: int = 255) -> tuple[int, int, int, int]:
    value = hex_color.lstrip("#")
    return (
        int(value[0:2], 16),
        int(value[2:4], 16),
        int(value[4:6], 16),
        alpha,
    )


def bounds(local_id: int) -> tuple[int, int, int, int]:
    col = local_id % COLS
    row = local_id // COLS
    x = col * TILE
    y = row * TILE
    return x, y, x + TILE, y + TILE


def paste_tile(atlas: Image.Image, local_id: int, tile: Image.Image) -> None:
    x, y, _, _ = bounds(local_id)
    atlas.paste(tile, (x, y), tile)


def draw_noise(
    draw: ImageDraw.ImageDraw,
    area: tuple[int, int, int, int],
    seed: int,
    palette: Iterable[tuple[int, int, int, int]],
    count: int,
    size: int,
) -> None:
    rng = random.Random(seed)
    colors = list(palette)
    left, top, right, bottom = area
    for _ in range(count):
        x = rng.randrange(left, right - size + 1)
        y = rng.randrange(top, bottom - size + 1)
        draw.rectangle([x, y, x + size - 1, y + size - 1], fill=rng.choice(colors))


def solid_tile(
    base: tuple[int, int, int, int],
    seed: int,
    speckles: Iterable[tuple[int, int, int, int]],
    count: int,
) -> Image.Image:
    tile = Image.new("RGBA", (TILE, TILE), base)
    draw_noise(ImageDraw.Draw(tile), (0, 0, TILE, TILE), seed, speckles, count, 2)
    return tile


def road_tile(mask: int, seed: int) -> Image.Image:
    tile = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tile)
    edge = rgba("#4a3123", 220)
    dirt = rgba("#6e4930", 248)
    light = rgba("#8a613d", 220)
    connections = sum(1 for bit in (1, 2, 4, 8) if mask & bit)

    if mask == 15:
        draw.rectangle([0, 0, 63, 63], fill=dirt)
    elif connections >= 3:
        draw.rectangle([5, 5, 58, 58], fill=edge)
        draw.rectangle([9, 9, 54, 54], fill=dirt)
    else:
        draw.ellipse([11, 11, 53, 53], fill=edge)
        draw.ellipse([16, 16, 48, 48], fill=dirt)

    if mask & 1:
        draw.rectangle([20, 0, 44, 35], fill=edge)
        draw.rectangle([24, 0, 40, 39], fill=dirt)
    if mask & 2:
        draw.rectangle([29, 20, 63, 44], fill=edge)
        draw.rectangle([25, 24, 63, 40], fill=dirt)
    if mask & 4:
        draw.rectangle([20, 29, 44, 63], fill=edge)
        draw.rectangle([24, 25, 40, 63], fill=dirt)
    if mask & 8:
        draw.rectangle([0, 20, 35, 44], fill=edge)
        draw.rectangle([0, 24, 39, 40], fill=dirt)

    if (mask & 1) and (mask & 2):
        draw.rectangle([32, 0, 63, 32], fill=dirt)
    if (mask & 2) and (mask & 4):
        draw.rectangle([32, 32, 63, 63], fill=dirt)
    if (mask & 4) and (mask & 8):
        draw.rectangle([0, 32, 32, 63], fill=dirt)
    if (mask & 8) and (mask & 1):
        draw.rectangle([0, 0, 32, 32], fill=dirt)

    draw_noise(draw, (4, 4, 60, 60), seed, [light, rgba("#503622", 210)], 38, 2)
    return tile


def detail_tile(kind: str, seed: int) -> Image.Image:
    tile = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tile)
    rng = random.Random(seed)
    if kind == "cracks":
        for _ in range(4):
            x = rng.randrange(8, 56)
            y = rng.randrange(8, 48)
            pts = [(x, y)]
            for _ in range(3):
                x += rng.randrange(-8, 9)
                y += rng.randrange(3, 10)
                pts.append((max(4, min(60, x)), max(4, min(60, y))))
            draw.line(pts, fill=rgba("#44362e", 190), width=2)
    elif kind == "leaves":
        for _ in range(12):
            x = rng.randrange(6, 58)
            y = rng.randrange(6, 58)
            color = rng.choice([rgba("#a78038", 210), rgba("#7d682d", 210), rgba("#aa5145", 180)])
            draw.ellipse([x - 3, y - 2, x + 4, y + 3], fill=color)
    elif kind == "pebbles":
        for _ in range(10):
            x = rng.randrange(8, 56)
            y = rng.randrange(8, 56)
            r = rng.randrange(2, 5)
            draw.ellipse([x - r, y - r, x + r, y + r], fill=rgba("#8c8979", 220), outline=rgba("#4b493f", 180))
    elif kind == "flowers":
        for _ in range(7):
            x = rng.randrange(8, 56)
            y = rng.randrange(8, 56)
            draw.rectangle([x, y, x + 1, y + 7], fill=rgba("#365d3a", 210))
            draw.ellipse([x - 3, y - 3, x + 4, y + 3], fill=rng.choice([rgba("#c58aa2", 210), rgba("#d8d29f", 220)]))
    return tile


def make_ground_atlas() -> Image.Image:
    atlas = Image.new("RGBA", (COLS * TILE, 4 * TILE), (0, 0, 0, 0))
    grass = [rgba("#4f7547"), rgba("#4b7043"), rgba("#53794a"), rgba("#486c42")]
    for i, base in enumerate(grass):
        paste_tile(atlas, i, solid_tile(base, 100 + i, [rgba("#628650"), rgba("#3d633c"), rgba("#6a8b55")], 145))

    wild = [rgba("#4b6d41"), rgba("#526f43"), rgba("#476a3f"), rgba("#577647")]
    for local_id, base in enumerate(wild, start=4):
        tile = solid_tile(base, 200 + local_id, [rgba("#67884d"), rgba("#384f34"), rgba("#7b7941")], 175)
        draw_noise(ImageDraw.Draw(tile), (0, 0, TILE, TILE), 500 + local_id, [rgba("#9a7840", 150)], 24, 3)
        paste_tile(atlas, local_id, tile)

    for mask in range(16):
        paste_tile(atlas, 8 + mask, road_tile(mask, 700 + mask))

    stone_defs = [
        (rgba("#6e6b5d"), [rgba("#817e70"), rgba("#4c4a42")]),
        (rgba("#5f625c"), [rgba("#767a72"), rgba("#3f423d")]),
        (rgba("#776f62"), [rgba("#918475"), rgba("#514b42")]),
        (rgba("#413d45"), [rgba("#625b69"), rgba("#2f2c35")]),
    ]
    for local_id, (base, speckles) in enumerate(stone_defs, start=24):
        tile = solid_tile(base, 900 + local_id, speckles, 105)
        draw = ImageDraw.Draw(tile)
        for x in (16, 36):
            draw.line([x, 0, x + 4, TILE], fill=rgba("#34322d", 120), width=1)
        for y in (18, 42):
            draw.line([0, y, TILE, y + 2], fill=rgba("#34322d", 120), width=1)
        paste_tile(atlas, local_id, tile)

    paste_tile(atlas, 28, detail_tile("cracks", 1001))
    paste_tile(atlas, 29, detail_tile("leaves", 1002))
    paste_tile(atlas, 30, detail_tile("pebbles", 1003))
    paste_tile(atlas, 31, detail_tile("flowers", 1004))
    return atlas


def draw_prop_tile(draw: ImageDraw.ImageDraw, local_id: int, shape: str) -> None:
    left, top, right, bottom = bounds(local_id)
    if shape == "wall_h":
        draw.rectangle([left + 4, top + 22, right - 4, top + 42], fill=rgba("#6b6356"), outline=rgba("#292723"))
    elif shape == "wall_v":
        draw.rectangle([left + 22, top + 4, left + 42, bottom - 4], fill=rgba("#6b6356"), outline=rgba("#292723"))
    elif shape == "block":
        draw.rectangle([left + 8, top + 8, right - 8, bottom - 8], fill=rgba("#625b51"), outline=rgba("#292723"))
    elif shape == "sign":
        draw.rectangle([left + 29, top + 15, left + 35, top + 58], fill=rgba("#5a3a22"), outline=rgba("#25170f"))
        draw.rectangle([left + 15, top + 17, left + 49, top + 35], fill=rgba("#8d6337"), outline=rgba("#25170f"))
    elif shape == "table":
        draw.rectangle([left + 7, top + 22, right - 7, top + 40], fill=rgba("#8d5f34"), outline=rgba("#2c1e16"))
        draw.rectangle([left + 13, top + 40, left + 19, top + 57], fill=rgba("#6d4528"), outline=rgba("#2c1e16"))
        draw.rectangle([right - 19, top + 40, right - 13, top + 57], fill=rgba("#6d4528"), outline=rgba("#2c1e16"))
    elif shape == "crate":
        draw.rectangle([left + 14, top + 18, right - 14, top + 50], fill=rgba("#805332"), outline=rgba("#2c1e16"))
    elif shape == "barrel":
        draw.rectangle([left + 19, top + 12, right - 19, top + 54], fill=rgba("#734522"), outline=rgba("#2c1e16"))
        draw.line([left + 20, top + 24, right - 20, top + 24], fill=rgba("#b78a4a"), width=2)
    elif shape == "bush":
        draw.ellipse([left + 8, top + 26, right - 8, top + 58], fill=rgba("#426b38"), outline=rgba("#263620"))
        draw.ellipse([left + 17, top + 18, right - 18, top + 48], fill=rgba("#4f7742"), outline=rgba("#263620"))
    elif shape == "rock":
        draw.ellipse([left + 8, top + 28, right - 8, top + 58], fill=rgba("#817d70"), outline=rgba("#36332d"))
        draw.ellipse([left + 16, top + 20, right - 18, top + 48], fill=rgba("#8d897b"), outline=rgba("#36332d"))
    elif shape == "shard":
        draw.polygon([(left + 32, top + 5), (left + 52, top + 52), (left + 12, top + 52)], fill=rgba("#9cb3bd", 215), outline=rgba("#252b36"))
        draw.line([left + 32, top + 10, left + 32, top + 50], fill=rgba("#e0f1f5", 170), width=2)


def draw_multi_prop(draw: ImageDraw.ImageDraw, local_id: int, kind: str) -> None:
    left, top, right, bottom = bounds(local_id)
    if kind == "ruin":
        draw.rectangle([left + 6, top + 10, left + 122, top + 112], fill=rgba("#49382f"), outline=rgba("#241c18"))
        draw.rectangle([left + 16, top + 22, left + 58, top + 56], fill=rgba("#273339"), outline=rgba("#1b2227"))
        draw.line([left + 12, top + 72, left + 118, top + 72], fill=rgba("#2e241f"), width=4)
        for y in range(top + 20, top + 105, 12):
            draw.line([left + 10, y, left + 118, y], fill=rgba("#5f493c"), width=2)
    elif kind == "tent":
        draw.polygon([(left + 12, top + 100), (left + 64, top + 18), (left + 116, top + 100)], fill=rgba("#b19a6e"), outline=rgba("#423321"))
        draw.line([left + 64, top + 22, left + 64, top + 104], fill=rgba("#5b4a32"), width=4)
        draw.rectangle([left + 46, top + 78, left + 82, top + 112], fill=rgba("#5a4b3b"), outline=rgba("#2a221b"))
    elif kind == "shrine":
        draw.rectangle([left + 8, top + 12, left + 120, top + 116], fill=rgba("#3d4447"), outline=rgba("#181c1e"))
        draw.line([left + 18, top + 25, left + 110, top + 104], fill=rgba("#8fa6ad", 180), width=3)
        draw.rectangle([left + 34, top + 34, left + 94, top + 92], outline=rgba("#b7d0d8", 150), width=2)
    elif kind == "canopy":
        rng = random.Random(local_id)
        for _ in range(14):
            x = rng.randrange(left + 16, left + 112)
            y = rng.randrange(top + 16, top + 106)
            r = rng.randrange(12, 24)
            draw.ellipse([x - r, y - r, x + r, y + r], fill=rgba("#426a38", 230), outline=rgba("#243820", 210))


def make_object_atlas() -> Image.Image:
    atlas = Image.new("RGBA", (COLS * TILE, 8 * TILE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(atlas)
    shapes = ["wall_h", "wall_v", "block", "sign", "table", "crate", "barrel", "bush", "rock", "shard"]
    for local_id, shape in enumerate(shapes):
        draw_prop_tile(draw, local_id, shape)

    draw_multi_prop(draw, 16, "ruin")
    draw_multi_prop(draw, 18, "tent")
    draw_multi_prop(draw, 32, "shrine")
    draw_multi_prop(draw, 48, "canopy")
    return atlas


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    ground = make_ground_atlas()
    objects = make_object_atlas()
    ground.save(OUT_DIR / "overworld_tiles_v2.png")
    objects.save(OUT_DIR / "overworld_objects_v2.png")
    print("Generated assets/environments/overworld_tiles_v2.png")
    print("Generated assets/environments/overworld_objects_v2.png")


if __name__ == "__main__":
    main()
