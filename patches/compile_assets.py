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


def paris_cobble_tile(local_id: int, seed: int) -> Image.Image:
    tile = Image.new("RGBA", (TILE, TILE), rgba("#58524b"))
    draw = ImageDraw.Draw(tile)
    rng = random.Random(seed)
    palette = [rgba("#6b645a"), rgba("#504a44"), rgba("#746b5e"), rgba("#3d3936")]
    y = 0
    row = 0
    while y < TILE:
        block_h = rng.randrange(10, 15)
        offset = 0 if row % 2 == 0 else -16
        x = offset
        while x < TILE:
            block_w = rng.randrange(18, 28)
            color = palette[(h := channel_noise(x + local_id, y + local_id, seed)) % len(palette)]
            left = max(0, x + 1)
            top = max(0, y + 1)
            right = min(TILE - 1, x + block_w - 1)
            bottom = min(TILE - 1, y + block_h - 1)
            if right >= left and bottom >= top:
                draw.rectangle([left, top, right, bottom], fill=color, outline=rgba("#252321", 150))
                if h % 5 == 0:
                    draw.line([left + 2, bottom - 2, right - 2, top + 2], fill=rgba("#8c8170", 70), width=1)
            x += block_w
        y += block_h
        row += 1

    if local_id in (34, 38):
        draw_noise(draw, (0, 0, TILE, TILE), seed + 33, [rgba("#9a6c3d", 190), rgba("#2f2b28", 150)], 18, 2)
    if local_id in (35, 39):
        for _ in range(4):
            x = rng.randrange(8, 56)
            y = rng.randrange(8, 56)
            draw.line([(x, y), (x + rng.randrange(-10, 11), y + rng.randrange(8, 18))],
                      fill=rgba("#221f1d", 180),
                      width=2)
    return tile


def make_ground_atlas() -> Image.Image:
    atlas = Image.new("RGBA", (COLS * TILE, 5 * TILE), (0, 0, 0, 0))
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
    for local_id in range(32, 40):
        paste_tile(atlas, local_id, paris_cobble_tile(local_id, 1600 + local_id))
    return atlas


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    ground = make_ground_atlas()
    ground.save(OUT_DIR / "overworld_tiles_v2.png")
    print("Generated assets/environments/overworld_tiles_v2.png")
    print("Skipped overworld_objects_v2.png; it is an imagegen-sourced atlas.")


if __name__ == "__main__":
    main()
