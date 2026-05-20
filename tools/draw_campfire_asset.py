#!/usr/bin/env python3
"""Generate the overworld campfire checkpoint sprite sheet."""

from __future__ import annotations

import os
import struct
import zlib


FRAME_W = 64
FRAME_H = 64
FRAMES = 4
OUT_PATH = os.path.join("assets", "animations", "campfire_checkpoint.png")


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


def put(px: list[list[tuple[int, int, int, int]]], x: int, y: int,
        color: tuple[int, int, int, int]) -> None:
    width = FRAME_W * FRAMES
    if 0 <= x < width and 0 <= y < FRAME_H:
        px[y][x] = blend(px[y][x], color)


def circle(px: list[list[tuple[int, int, int, int]]], cx: int, cy: int, radius: int,
           color: tuple[int, int, int, int]) -> None:
    r2 = radius * radius
    for y in range(cy - radius, cy + radius + 1):
        for x in range(cx - radius, cx + radius + 1):
            dx = x - cx
            dy = y - cy
            if dx * dx + dy * dy <= r2:
                put(px, x, y, color)


def rect(px: list[list[tuple[int, int, int, int]]], x0: int, y0: int, x1: int, y1: int,
         color: tuple[int, int, int, int]) -> None:
    for y in range(y0, y1):
        for x in range(x0, x1):
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
    width = FRAME_W * FRAMES

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
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", width, FRAME_H, 8, 6, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(chunk(b"IEND", b""))

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as file:
        file.write(png)


def draw_frame(px: list[list[tuple[int, int, int, int]]], frame: int) -> None:
    ox = frame * FRAME_W
    flicker = [0, 3, -2, 2][frame]

    circle(px, ox + 32, 49, 21, (28, 20, 19, 70))
    rect(px, ox + 18, 49, ox + 47, 54, (74, 46, 32, 255))
    line(px, ox + 17, 53, ox + 46, 44, (105, 62, 37, 255))
    line(px, ox + 20, 45, ox + 47, 53, (123, 78, 46, 255))

    circle(px, ox + 32, 35 + flicker // 2, 15, (252, 89, 35, 210))
    circle(px, ox + 31, 32 + flicker, 11, (255, 171, 48, 235))
    circle(px, ox + 34, 37, 8, (255, 230, 117, 240))
    circle(px, ox + 31, 41, 5, (255, 250, 196, 255))

    line(px, ox + 23, 43, ox + 29, 20 + flicker, (255, 153, 38, 225))
    line(px, ox + 38, 44, ox + 34, 18 - flicker, (255, 103, 35, 220))
    line(px, ox + 30, 42, ox + 40, 24, (255, 219, 91, 205))

    circle(px, ox + 32, 36, 24, (255, 170, 48, 35))
    circle(px, ox + 32, 36, 31, (255, 199, 74, 15))


def main() -> None:
    px = [[(0, 0, 0, 0) for _ in range(FRAME_W * FRAMES)] for _ in range(FRAME_H)]
    for frame in range(FRAMES):
        draw_frame(px, frame)
    write_png(OUT_PATH, px)
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    main()
