#!/usr/bin/env python3
"""Create the zombie armour enemy atlas in the game's sprite-sheet format."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw


WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = WORKSPACE_ROOT / "source_assets" / "zombie_armour_raw.png"
DEFAULT_ATLAS = WORKSPACE_ROOT / "assets" / "animations" / "zombie_armour.png"
DEFAULT_JSON = WORKSPACE_ROOT / "assets" / "animations" / "zombie_armour.json"
DEFAULT_TURN_VIEW = WORKSPACE_ROOT / "assets" / "UI" / "turn-view-zombie-armour.png"
DEFAULT_REFERENCE_ATLAS = WORKSPACE_ROOT / "assets" / "animations" / "skeleton.png"
DEFAULT_REFERENCE_JSON = WORKSPACE_ROOT / "assets" / "animations" / "skeleton.json"

FRAME_SIZE = 128
PIVOT_X = 65
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


def load_reference_plans(path: Path) -> list[AnimationPlan]:
    with path.open("r", encoding="utf-8") as file:
        data = json.load(file)

    plans: list[AnimationPlan] = []
    for row_index, entry in enumerate(data.get("animations", [])):
        plans.append(AnimationPlan(
            name=str(entry["name"]),
            row_index=row_index,
            frame_count=int(entry["num_frames"]),
            frame_rate=int(entry.get("frame_rate", 8)),
            loop=bool(entry.get("loop", True)),
        ))

    if not plans:
        raise RuntimeError(f"Reference JSON '{path}' did not define any animations.")

    return plans


def tint_reference_pixels(atlas: Image.Image) -> Image.Image:
    out = atlas.convert("RGBA")
    pixels = out.load()
    width, height = out.size

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue

            light = int((r * 30 + g * 59 + b * 11) / 100)
            if light > 180:
                base = (218, 207, 166)
            elif light > 120:
                base = (166, 142, 108)
            elif light > 70:
                base = (101, 80, 64)
            else:
                base = (39, 34, 32)

            pixels[x, y] = (
                min(255, int(base[0] * 0.84 + light * 0.18)),
                min(255, int(base[1] * 0.86 + light * 0.12)),
                min(255, int(base[2] * 0.88 + light * 0.08)),
                a,
            )

    return out


def draw_armour_on_frame(frame: Image.Image) -> None:
    alpha = frame.getchannel("A")
    bounds = alpha.getbbox()
    if bounds is None:
        return

    left, top, right, bottom = bounds
    width = max(1, right - left)
    height = max(1, bottom - top)
    center_x = (left + right) // 2

    draw = ImageDraw.Draw(frame, "RGBA")

    dark = (34, 30, 27, 235)
    steel = (94, 93, 87, 230)
    steel_light = (143, 133, 111, 220)
    brass = (146, 105, 61, 230)
    rust = (108, 58, 36, 220)
    ember = (221, 75, 32, 245)

    head_top = top + max(0, height // 18)
    head_w = max(15, min(30, width // 2 + 6))
    helmet_left = center_x - head_w // 2
    helmet_right = center_x + head_w // 2
    helmet_bottom = head_top + max(9, height // 7)

    draw.rectangle((helmet_left - 1, head_top + 4, helmet_right + 1, helmet_bottom), fill=dark)
    draw.rectangle((helmet_left + 1, head_top + 3, helmet_right - 1, helmet_bottom - 2), fill=steel)
    draw.rectangle((helmet_left + 3, head_top + 2, helmet_right - 3, head_top + 5), fill=steel_light)
    draw.rectangle((helmet_left + 4, helmet_bottom - 2, helmet_right - 4, helmet_bottom), fill=brass)

    eye_y = helmet_bottom + max(1, height // 18)
    draw.rectangle((center_x - 5, eye_y, center_x - 3, eye_y + 2), fill=ember)
    draw.rectangle((center_x + 3, eye_y, center_x + 5, eye_y + 2), fill=ember)

    torso_top = top + max(22, height * 34 // 100)
    torso_bottom = top + max(36, height * 66 // 100)
    chest_half = max(8, min(16, width // 4))
    draw.polygon(
        [
            (center_x - chest_half, torso_top + 3),
            (center_x + chest_half, torso_top + 3),
            (center_x + chest_half - 3, torso_bottom),
            (center_x, torso_bottom + 4),
            (center_x - chest_half + 3, torso_bottom),
        ],
        fill=dark,
    )
    draw.polygon(
        [
            (center_x - chest_half + 2, torso_top + 4),
            (center_x + chest_half - 2, torso_top + 4),
            (center_x + chest_half - 5, torso_bottom - 2),
            (center_x, torso_bottom + 1),
            (center_x - chest_half + 5, torso_bottom - 2),
        ],
        fill=steel,
    )
    draw.line((center_x, torso_top + 5, center_x, torso_bottom), fill=steel_light, width=1)
    draw.rectangle((center_x - chest_half + 4, torso_top + 8, center_x - 2, torso_top + 10), fill=brass)
    draw.rectangle((center_x + 2, torso_top + 8, center_x + chest_half - 4, torso_top + 10), fill=rust)

    shoulder_y = torso_top + 2
    shoulder_span = max(12, min(24, width // 3))
    draw.polygon(
        [
            (center_x - chest_half - 3, shoulder_y),
            (center_x - chest_half - shoulder_span, shoulder_y + 5),
            (center_x - chest_half - 5, shoulder_y + 12),
            (center_x - chest_half + 2, shoulder_y + 8),
        ],
        fill=dark,
    )
    draw.polygon(
        [
            (center_x + chest_half + 3, shoulder_y),
            (center_x + chest_half + shoulder_span, shoulder_y + 5),
            (center_x + chest_half + 5, shoulder_y + 12),
            (center_x + chest_half - 2, shoulder_y + 8),
        ],
        fill=dark,
    )
    draw.polygon(
        [
            (center_x - chest_half - 2, shoulder_y + 1),
            (center_x - chest_half - shoulder_span + 4, shoulder_y + 6),
            (center_x - chest_half - 5, shoulder_y + 10),
            (center_x - chest_half + 1, shoulder_y + 7),
        ],
        fill=brass,
    )
    draw.polygon(
        [
            (center_x + chest_half + 2, shoulder_y + 1),
            (center_x + chest_half + shoulder_span - 4, shoulder_y + 6),
            (center_x + chest_half + 5, shoulder_y + 10),
            (center_x + chest_half - 1, shoulder_y + 7),
        ],
        fill=brass,
    )

    hip_y = torso_bottom + max(2, height // 20)
    draw.rectangle((center_x - 11, hip_y, center_x + 11, hip_y + 4), fill=dark)
    draw.rectangle((center_x - 9, hip_y + 1, center_x + 9, hip_y + 2), fill=brass)


def build_reference_atlas(reference_atlas_path: Path) -> Image.Image:
    source = Image.open(reference_atlas_path).convert("RGBA")
    atlas = tint_reference_pixels(source)

    columns = max(1, atlas.width // FRAME_SIZE)
    rows = max(1, atlas.height // FRAME_SIZE)
    for row in range(rows):
        for column in range(columns):
            box = (
                column * FRAME_SIZE,
                row * FRAME_SIZE,
                (column + 1) * FRAME_SIZE,
                (row + 1) * FRAME_SIZE,
            )
            frame = atlas.crop(box)
            draw_armour_on_frame(frame)
            atlas.alpha_composite(frame, (box[0], box[1]))

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
    parser.add_argument(
        "--source-mode",
        choices=("auto", "raw", "skeleton-reference"),
        default="auto",
        help="Choose raw sheet processing, skeleton-based generation, or automatic fallback.",
    )
    parser.add_argument("--input", default=str(DEFAULT_SOURCE), help="Raw black-background sprite sheet.")
    parser.add_argument("--atlas", default=str(DEFAULT_ATLAS), help="Output animation atlas PNG.")
    parser.add_argument("--json", default=str(DEFAULT_JSON), help="Output sprite-sheet JSON.")
    parser.add_argument("--turn-view", default=str(DEFAULT_TURN_VIEW), help="Output turn queue portrait PNG.")
    parser.add_argument("--reference-atlas", default=str(DEFAULT_REFERENCE_ATLAS), help="Fallback atlas used for skeleton-reference mode.")
    parser.add_argument("--reference-json", default=str(DEFAULT_REFERENCE_JSON), help="Fallback sprite JSON used for skeleton-reference mode.")
    parser.add_argument("--recipe", default=None, help="Optional JSON recipe that maps source rows to clip names.")
    parser.add_argument("--threshold", type=int, default=8, help="Pixels brighter than this are treated as sprite pixels.")
    parser.add_argument("--row-gap", type=int, default=6, help="Blank scanlines tolerated inside one detected row.")
    parser.add_argument("--column-gap", type=int, default=4, help="Blank columns tolerated inside one detected frame.")
    parser.add_argument("--min-frame-pixels", type=int, default=24, help="Discard detected boxes below this pixel count.")
    parser.add_argument("--scale", type=int, default=3, help="Nearest-neighbor scale applied before packing frames.")
    args = parser.parse_args()

    source_path = Path(args.input)
    should_process_raw = args.source_mode == "raw" or (args.source_mode == "auto" and source_path.exists())
    if should_process_raw and not source_path.exists():
        raise FileNotFoundError(
            f"Source sheet not found: {source_path}. Save the provided image there or pass --input."
        )

    if should_process_raw:
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
        mode_note = f"processed {len(rows)} detected source row(s)"
    else:
        reference_atlas_path = Path(args.reference_atlas)
        reference_json_path = Path(args.reference_json)
        if not reference_atlas_path.exists():
            raise FileNotFoundError(f"Reference atlas not found: {reference_atlas_path}.")
        if not reference_json_path.exists():
            raise FileNotFoundError(f"Reference JSON not found: {reference_json_path}.")

        plans = load_reference_plans(reference_json_path)
        atlas = build_reference_atlas(reference_atlas_path)
        mode_note = "generated from the skeleton reference atlas"

    atlas_path = Path(args.atlas)
    json_path = Path(args.json)
    turn_view_path = Path(args.turn_view)

    atlas_path.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(atlas_path)
    write_sprite_json(json_path, atlas, plans)
    write_turn_view(turn_view_path, atlas)

    print(f"Zombie armour asset {mode_note}.")
    print(f"Wrote {atlas_path}")
    print(f"Wrote {json_path}")
    print(f"Wrote {turn_view_path}")


if __name__ == "__main__":
    main()
