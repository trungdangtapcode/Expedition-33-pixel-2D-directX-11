#!/usr/bin/env python3
"""Repair weak 64x64 cells in the active overworld object atlas.

The active atlas is imagegen-sourced. This script keeps that art direction by
copying stronger painted cells from the previous imagegen atlas instead of
drawing procedural primitive props.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ACTIVE_ATLAS = ROOT / "assets" / "environments" / "overworld_objects_v3.png"
SOURCE_ATLAS = ROOT / "assets" / "environments" / "overworld_objects_v2.png"
CELL_SIZE = 64
COLUMNS = 8

# Active map contract:
#   3 = signpost
#   4 = market table
#   6 = barrel
REPAIRED_CELLS = (3, 4, 6)


def cell_box(index: int) -> tuple[int, int, int, int]:
    col = index % COLUMNS
    row = index // COLUMNS
    left = col * CELL_SIZE
    top = row * CELL_SIZE
    return left, top, left + CELL_SIZE, top + CELL_SIZE


def main() -> None:
    active = Image.open(ACTIVE_ATLAS).convert("RGBA")
    source = Image.open(SOURCE_ATLAS).convert("RGBA")

    for index in REPAIRED_CELLS:
        active.paste(source.crop(cell_box(index)), cell_box(index))

    active.save(ACTIVE_ATLAS)
    print(f"Repaired {ACTIVE_ATLAS.relative_to(ROOT)} cells: {REPAIRED_CELLS}")


if __name__ == "__main__":
    main()
