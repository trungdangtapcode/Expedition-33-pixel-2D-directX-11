#!/usr/bin/env python3
"""Convert the raw zombie armour sprite sheet into the game's atlas format."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = WORKSPACE_ROOT / "source_assets" / "zombie_armour_raw.png"
DEFAULT_ATLAS = WORKSPACE_ROOT / "assets" / "animations" / "zombie_armour.png"
DEFAULT_JSON = WORKSPACE_ROOT / "assets" / "animations" / "zombie_armour.json"
DEFAULT_TURN_VIEW = WORKSPACE_ROOT / "assets" / "UI" / "turn-view-zombie-armour.png"

FRAME_SIZE = 128
PIVOT_X = 64
PIVOT_Y = 122
GROUND_Y = 120


@dataclass
class Rect:
    left: int
    top: int
    right: int
    bottom: int

    @property
    def width(self) -> int:
        return self.right - self.left + 1

    @property
    def height(self) -> int:
        return self.bottom - self.top + 1


@dataclass
class AnimationPlan:
    name: str
    row_index: int
    frame_count: int
    frame_rate: int
    loop: bool


def is_sprite_pixel(pixel: tuple[int, int, int, int], threshold: int) -> bool:
    r, g, b, a = pixel
    return a > 0 and max(r, g, b) > threshold


def detect_bands(active_counts: list[int], min_count: int, gap_tolerance: int) -> list[tuple[int, int]]:
    bands: list[tuple[int, int]] = []
    start: int | None = None
    last_active = -1

    for index, count in enumerate(active_counts):
        if count >= min_count:
            if start is None:
                start = index
            last_active = index
            continue

        if start is not None and index - last_active > gap_tolerance:
            bands.append((start, last_active))
            start = None
            last_active = -1

    if start is not None:
        bands.append((start, last_active))

    return bands


def find_frame_rects(image: Image.Image,
                     threshold: int,
                     row_gap: int,
                     column_gap: int,
                     min_frame_pixels: int) -> list[list[Rect]]:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    pixels = rgba.load()

    row_counts: list[int] = []
    for y in range(height):
        count = 0
        for x in range(width):
            if is_sprite_pixel(pixels[x, y], threshold):
                count += 1
        row_counts.append(count)

    row_bands = detect_bands(row_counts, min_count=2, gap_tolerance=row_gap)
    rows: list[list[Rect]] = []

    for top, bottom in row_bands:
        col_counts: list[int] = []
        for x in range(width):
            count = 0
            for y in range(top, bottom + 1):
                if is_sprite_pixel(pixels[x, y], threshold):
                    count += 1
            col_counts.append(count)

        column_bands = detect_bands(col_counts, min_count=1, gap_tolerance=column_gap)
        rects: list[Rect] = []
        for left, right in column_bands:
            xs: list[int] = []
            ys: list[int] = []
            for y in range(top, bottom + 1):
                for x in range(left, right + 1):
                    if is_sprite_pixel(pixels[x, y], threshold):
                        xs.append(x)
                        ys.append(y)

            if len(xs) < min_frame_pixels:
                continue

            rects.append(Rect(min(xs), min(ys), max(xs), max(ys)))

        if rects:
            rows.append(rects)

    return rows


def build_default_plan(rows: list[list[Rect]]) -> list[AnimationPlan]:
    if not rows:
        raise RuntimeError("No sprite rows were detected in the source sheet.")

    def safe_count(row_index: int, fallback: int) -> int:
        if row_index < len(rows):
            return max(1, min(len(rows[row_index]), fallback))
        return max(1, min(len(rows[0]), fallback))

    attack_row = 2 if len(rows) > 2 else 0
    die_row = len(rows) - 1

    return [
        AnimationPlan("idle", 0, safe_count(0, 8), 8, True),
        AnimationPlan("walk", 1 if len(rows) > 1 else 0, safe_count(1 if len(rows) > 1 else 0, 8), 10, True),
        AnimationPlan("fight-state", 0, safe_count(0, 8), 8, True),
        AnimationPlan("attack-1", attack_row, safe_count(attack_row, 8), 12, False),
        AnimationPlan("die", die_row, safe_count(die_row, 6), 6, False),
    ]


def load_plan(path: Path | None, rows: list[list[Rect]]) -> list[AnimationPlan]:
    if path is None:
        return build_default_plan(rows)

    with path.open("r", encoding="utf-8") as file:
        data = json.load(file)

    plans: list[AnimationPlan] = []
    for entry in data.get("animations", []):
        plans.append(AnimationPlan(
            name=str(entry["name"]),
            row_index=int(entry["sourceRow"]),
            frame_count=int(entry["frames"]),
            frame_rate=int(entry.get("frameRate", 8)),
            loop=bool(entry.get("loop", True)),
        ))

    if not plans:
        raise RuntimeError(f"Recipe '{path}' did not define any animations.")

    return plans


def extract_frame(source: Image.Image, rect: Rect, scale: int) -> Image.Image:
    frame = source.crop((rect.left, rect.top, rect.right + 1, rect.bottom + 1)).convert("RGBA")
    if scale != 1:
        frame = frame.resize((frame.width * scale, frame.height * scale), Image.Resampling.NEAREST)
    return frame


def paste_centered(cell: Image.Image, frame: Image.Image) -> None:
    x = (FRAME_SIZE - frame.width) // 2
    y = GROUND_Y - frame.height
    x = max(0, min(FRAME_SIZE - frame.width, x))
    y = max(0, min(FRAME_SIZE - frame.height, y))
    cell.alpha_composite(frame, (x, y))


def build_atlas(source: Image.Image,
                rows: list[list[Rect]],
                plans: list[AnimationPlan],
                scale: int) -> Image.Image:
    max_frames = max(plan.frame_count for plan in plans)
    atlas = Image.new("RGBA", (FRAME_SIZE * max_frames, FRAME_SIZE * len(plans)), (0, 0, 0, 0))

    for out_row, plan in enumerate(plans):
        if plan.row_index < 0 or plan.row_index >= len(rows):
            raise RuntimeError(f"Animation '{plan.name}' references missing source row {plan.row_index}.")

        source_row = rows[plan.row_index]
        if not source_row:
            raise RuntimeError(f"Animation '{plan.name}' uses an empty source row.")

        for frame_index in range(plan.frame_count):
            rect = source_row[min(frame_index, len(source_row) - 1)]
            frame = extract_frame(source, rect, scale)
            cell = Image.new("RGBA", (FRAME_SIZE, FRAME_SIZE), (0, 0, 0, 0))
            paste_centered(cell, frame)
            atlas.alpha_composite(cell, (frame_index * FRAME_SIZE, out_row * FRAME_SIZE))

    return atlas


def write_sprite_json(path: Path, atlas: Image.Image, plans: list[AnimationPlan]) -> None:
    data = {
        "sprite_name": "zombie_armour",
        "character": "zombie_armour",
        "width": atlas.width,
        "height": atlas.height,
        "frame_width": FRAME_SIZE,
        "frame_height": FRAME_SIZE,
        "animations": [
            {
                "name": plan.name,
                "num_frames": plan.frame_count,
                "frame_rate": plan.frame_rate,
                "loop": plan.loop,
                "pivot": [PIVOT_X, PIVOT_Y],
                "align": "bottom-center",
            }
            for plan in plans
        ],
    }

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as file:
        json.dump(data, file, indent=2)
        file.write("\n")


def write_turn_view(path: Path, atlas: Image.Image) -> None:
    first_frame = atlas.crop((0, 0, FRAME_SIZE, FRAME_SIZE))
    portrait = Image.new("RGBA", (256, 128), (0, 0, 0, 0))
    portrait.alpha_composite(first_frame, (64, 0))
    path.parent.mkdir(parents=True, exist_ok=True)
    portrait.save(path)


def main() -> None:
    parser = argparse.ArgumentParser(description="Process the zombie armour enemy sheet.")
    parser.add_argument("--input", default=str(DEFAULT_SOURCE), help="Raw black-background sprite sheet.")
    parser.add_argument("--atlas", default=str(DEFAULT_ATLAS), help="Output animation atlas PNG.")
    parser.add_argument("--json", default=str(DEFAULT_JSON), help="Output sprite-sheet JSON.")
    parser.add_argument("--turn-view", default=str(DEFAULT_TURN_VIEW), help="Output turn queue portrait PNG.")
    parser.add_argument("--recipe", default=None, help="Optional JSON recipe that maps source rows to clip names.")
    parser.add_argument("--threshold", type=int, default=8, help="Pixels brighter than this are treated as sprite pixels.")
    parser.add_argument("--row-gap", type=int, default=6, help="Blank scanlines tolerated inside one detected row.")
    parser.add_argument("--column-gap", type=int, default=4, help="Blank columns tolerated inside one detected frame.")
    parser.add_argument("--min-frame-pixels", type=int, default=24, help="Discard detected boxes below this pixel count.")
    parser.add_argument("--scale", type=int, default=3, help="Nearest-neighbor scale applied before packing frames.")
    args = parser.parse_args()

    source_path = Path(args.input)
    if not source_path.exists():
        raise FileNotFoundError(
            f"Source sheet not found: {source_path}. Save the provided image there or pass --input."
        )

    source = Image.open(source_path).convert("RGBA")
    rows = find_frame_rects(
        source,
        threshold=args.threshold,
        row_gap=args.row_gap,
        column_gap=args.column_gap,
        min_frame_pixels=args.min_frame_pixels,
    )
    plans = load_plan(Path(args.recipe) if args.recipe else None, rows)
    atlas = build_atlas(source, rows, plans, max(1, args.scale))

    atlas_path = Path(args.atlas)
    json_path = Path(args.json)
    turn_view_path = Path(args.turn_view)

    atlas_path.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(atlas_path)
    write_sprite_json(json_path, atlas, plans)
    write_turn_view(turn_view_path, atlas)

    print(f"Detected {len(rows)} source row(s).")
    print(f"Wrote {atlas_path}")
    print(f"Wrote {json_path}")
    print(f"Wrote {turn_view_path}")


if __name__ == "__main__":
    main()
