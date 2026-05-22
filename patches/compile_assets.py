#!/usr/bin/env python3
"""Generate deterministic top-down overworld atlases."""

from __future__ import annotations

import math
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


def channel_noise(x: int, y: int, seed: int) -> int:
    value = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
    return (value ^ (value >> 16)) & 0xFF


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def road_tile(mask: int, seed: int) -> Image.Image:
    tile = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    pixels = tile.load()
    base = rgba("#6a4930")
    warm = rgba("#7c5737")
    dark = rgba("#4c3323")
    grit = rgba("#9a7544")
    connected_n = (mask & 1) != 0
    connected_e = (mask & 2) != 0
    connected_s = (mask & 4) != 0
    connected_w = (mask & 8) != 0

    for y in range(TILE):
        for x in range(TILE):
            alpha = 255.0
            edge_width = 13.0

            if not connected_n:
                alpha = min(alpha, clamp((y - 2.0 + (channel_noise(x, 0, seed) - 128) * 0.035) / edge_width, 0.0, 1.0) * 255.0)
            if not connected_e:
                alpha = min(alpha, clamp((TILE - 3.0 - x + (channel_noise(63, y, seed + 1) - 128) * 0.035) / edge_width, 0.0, 1.0) * 255.0)
            if not connected_s:
                alpha = min(alpha, clamp((TILE - 3.0 - y + (channel_noise(x, 63, seed + 2) - 128) * 0.035) / edge_width, 0.0, 1.0) * 255.0)
            if not connected_w:
                alpha = min(alpha, clamp((x - 2.0 + (channel_noise(0, y, seed + 3) - 128) * 0.035) / edge_width, 0.0, 1.0) * 255.0)

            if alpha <= 0.0:
                continue

            noise = channel_noise(x, y, seed + 9)
            wave = math.sin((x * 0.21) + (y * 0.13) + seed * 0.01) * 7.0
            mix = clamp((noise + wave - 82.0) / 145.0, 0.0, 1.0)
            r = int(base[0] * (1.0 - mix) + warm[0] * mix)
            g = int(base[1] * (1.0 - mix) + warm[1] * mix)
            b = int(base[2] * (1.0 - mix) + warm[2] * mix)

            if noise < 24:
                r = int(r * 0.78 + dark[0] * 0.22)
                g = int(g * 0.78 + dark[1] * 0.22)
                b = int(b * 0.78 + dark[2] * 0.22)
            elif noise > 226:
                r = int(r * 0.76 + grit[0] * 0.24)
                g = int(g * 0.76 + grit[1] * 0.24)
                b = int(b * 0.76 + grit[2] * 0.24)

            if alpha < 235.0:
                r = int(r * 0.78 + dark[0] * 0.22)
                g = int(g * 0.78 + dark[1] * 0.22)
                b = int(b * 0.78 + dark[2] * 0.22)

            pixels[x, y] = (r, g, b, int(alpha))

    draw = ImageDraw.Draw(tile)
    rng = random.Random(seed)
    for _ in range(26):
        x = rng.randrange(4, 60)
        y = rng.randrange(4, 60)
        if tile.getpixel((x, y))[3] < 220:
            continue
        color = rng.choice([rgba("#8f6b3f"), rgba("#493222"), rgba("#b18a4e")])
        draw.rectangle([x, y, x + 1, y + 1], fill=color)
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
    grass = [rgba("#4f7247"), rgba("#507146"), rgba("#4d7046"), rgba("#517348")]
    for i, base in enumerate(grass):
        paste_tile(atlas, i, solid_tile(base, 100 + i, [rgba("#5e7d4f"), rgba("#466a40"), rgba("#668453")], 115))

    wild = [rgba("#4d6f43"), rgba("#506f44"), rgba("#4b6e42"), rgba("#527247")]
    for local_id, base in enumerate(wild, start=4):
        tile = solid_tile(base, 200 + local_id, [rgba("#607f4e"), rgba("#425f3b"), rgba("#6d7647")], 130)
        draw_noise(ImageDraw.Draw(tile), (0, 0, TILE, TILE), 500 + local_id, [rgba("#8c7540")], 18, 2)
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
