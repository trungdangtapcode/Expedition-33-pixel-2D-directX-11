#!/usr/bin/env python3
"""Generate deterministic save/load UI assets without external packages."""

from __future__ import annotations

import os
import struct
import zlib


WIDTH = 96
HEIGHT = 96
OUT_PATH = os.path.join("assets", "UI", "save_checkpoint_badge.png")


def clamp(value: int) -> int:
    return max(0, min(255, value))


def blend(dst: tuple[int, int, int, int], src: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    sr, sg, sb, sa = src
    dr, dg, db, da = dst
    alpha = sa / 255.0
    inv = 1.0 - alpha
    return (
        clamp(int(sr * alpha + dr * inv)),
        clamp(int(sg * alpha + dg * inv)),
        clamp(int(sb * alpha + db * inv)),
        clamp(int(sa + da * inv)),
    )


def put(px: list[list[tuple[int, int, int, int]]], x: int, y: int, color: tuple[int, int, int, int]) -> None:
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        px[y][x] = blend(px[y][x], color)


def rect(px: list[list[tuple[int, int, int, int]]], x0: int, y0: int, x1: int, y1: int,
         color: tuple[int, int, int, int]) -> None:
    for y in range(y0, y1):
        for x in range(x0, x1):
            put(px, x, y, color)


def rounded_rect(px: list[list[tuple[int, int, int, int]]], x0: int, y0: int, x1: int, y1: int,
                 radius: int, color: tuple[int, int, int, int]) -> None:
    for y in range(y0, y1):
        for x in range(x0, x1):
            dx = max(x0 + radius - x, 0, x - (x1 - radius - 1))
            dy = max(y0 + radius - y, 0, y - (y1 - radius - 1))
            if dx * dx + dy * dy <= radius * radius:
                put(px, x, y, color)


def circle(px: list[list[tuple[int, int, int, int]]], cx: int, cy: int, radius: int,
           color: tuple[int, int, int, int]) -> None:
    r2 = radius * radius
    for y in range(cy - radius, cy + radius + 1):
        for x in range(cx - radius, cx + radius + 1):
            dx = x - cx
            dy = y - cy
            if dx * dx + dy * dy <= r2:
                put(px, x, y, color)


def line(px: list[list[tuple[int, int, int, int]]], x0: int, y0: int, x1: int, y1: int,
         color: tuple[int, int, int, int]) -> None:
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy

    while True:
        put(px, x0, y0, color)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def write_png(path: str, px: list[list[tuple[int, int, int, int]]]) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw = bytearray()
    for row in px:
        raw.append(0)
        for r, g, b, a in row:
            raw.extend((r, g, b, a))

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(chunk(b"IEND", b""))

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as file:
        file.write(png)


def main() -> None:
    px = [[(0, 0, 0, 0) for _ in range(WIDTH)] for _ in range(HEIGHT)]

    rounded_rect(px, 10, 10, 86, 86, 14, (13, 19, 32, 238))
    rounded_rect(px, 14, 14, 82, 82, 10, (25, 38, 58, 245))
    rounded_rect(px, 18, 18, 78, 78, 8, (39, 57, 79, 255))

    circle(px, 48, 48, 29, (238, 188, 73, 255))
    circle(px, 48, 48, 24, (43, 61, 84, 255))
    circle(px, 48, 48, 17, (74, 150, 194, 255))
    circle(px, 48, 48, 11, (173, 231, 229, 255))

    rounded_rect(px, 32, 31, 64, 66, 4, (15, 25, 38, 255))
    rect(px, 37, 35, 59, 46, (232, 239, 231, 255))
    rect(px, 41, 35, 55, 43, (91, 154, 198, 255))
    rounded_rect(px, 38, 52, 58, 62, 3, (232, 239, 231, 255))
    rect(px, 42, 55, 54, 58, (74, 90, 108, 255))

    for offset in range(3):
        line(px, 35, 72 + offset, 44, 80 + offset, (70, 227, 155, 255))
        line(px, 44, 80 + offset, 64, 62 + offset, (70, 227, 155, 255))

    line(px, 24, 25, 35, 18, (255, 246, 172, 190))
    line(px, 64, 17, 76, 25, (255, 246, 172, 160))
    line(px, 22, 70, 35, 77, (255, 246, 172, 130))

    write_png(OUT_PATH, px)
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    main()
