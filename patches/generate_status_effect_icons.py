#!/usr/bin/env python3
# ============================================================
# File: generate_status_effect_icons.py
# Responsibility: Validate the imagegen-sourced status-effect icon atlas.
#
# Owns:
#   - No generated pixels. The atlas is art-sourced and committed under
#     assets/UI/status_effect_icons.png.
#
# Lifetime:
#   Run manually after replacing the icon atlas or metadata.
#
# Important:
#   - This script intentionally does not draw icons. Earlier versions produced
#     primitive placeholder art, which made polished assets regress during
#     regeneration.
#   - The script keeps the historical path as a validation entry point so docs
#     and local workflows do not call a missing file.
# ============================================================

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUT_PNG = ROOT / "assets" / "UI" / "status_effect_icons.png"
OUT_JSON = ROOT / "assets" / "UI" / "status_effect_icons.json"
ICON_SIZE = 32

EXPECTED_ICONS = [
    "fallback",
    "attack",
    "rage",
    "burn",
    "weaken",
    "power_up",
    "guard_up",
    "haste",
    "vulnerable",
    "heal",
    "revive",
    "cleanse",
    "sweep",
]


def main() -> None:
    if not OUT_PNG.exists():
        raise SystemExit(f"Missing status icon atlas: {OUT_PNG}")
    if not OUT_JSON.exists():
        raise SystemExit(f"Missing status icon metadata: {OUT_JSON}")

    with Image.open(OUT_PNG) as image:
        image = image.convert("RGBA")
        expected_size = (ICON_SIZE * len(EXPECTED_ICONS), ICON_SIZE)
        if image.size != expected_size:
            raise SystemExit(
                f"Unexpected atlas size {image.size}; expected {expected_size}."
            )

        alpha_pixels = sum(1 for pixel in image.getdata() if pixel[3] > 0)
        if alpha_pixels == 0:
            raise SystemExit("Status icon atlas has no visible pixels.")

        for index, icon_id in enumerate(EXPECTED_ICONS):
            cell = image.crop(
                (
                    index * ICON_SIZE,
                    0,
                    (index + 1) * ICON_SIZE,
                    ICON_SIZE,
                )
            )
            bounds = cell.getchannel("A").getbbox()
            if bounds is None:
                raise SystemExit(f"Icon '{icon_id}' has no visible pixels.")

            left, top, right, bottom = bounds
            center_x = (left + right) / 2.0
            center_y = (top + bottom) / 2.0
            allowed_offset = 0.75
            if abs(center_x - (ICON_SIZE / 2.0)) > allowed_offset:
                raise SystemExit(
                    f"Icon '{icon_id}' is not horizontally centered: {bounds}."
                )
            if abs(center_y - (ICON_SIZE / 2.0)) > allowed_offset:
                raise SystemExit(
                    f"Icon '{icon_id}' is not vertically centered: {bounds}."
                )

    metadata = json.loads(OUT_JSON.read_text(encoding="utf-8"))
    if metadata.get("iconSize") != ICON_SIZE:
        raise SystemExit(f"iconSize must be {ICON_SIZE}.")

    icons = metadata.get("icons", [])
    actual_ids = [entry.get("id") for entry in icons]
    if actual_ids != EXPECTED_ICONS:
        raise SystemExit(
            f"Icon id order mismatch.\nExpected: {EXPECTED_ICONS}\nActual:   {actual_ids}"
        )

    for index, entry in enumerate(icons):
        expected_x = index * ICON_SIZE
        if entry.get("x") != expected_x or entry.get("y") != 0:
            raise SystemExit(f"Icon '{entry.get('id')}' has the wrong source origin.")
        if entry.get("w") != ICON_SIZE or entry.get("h") != ICON_SIZE:
            raise SystemExit(f"Icon '{entry.get('id')}' has the wrong source size.")

    print("Status effect icon atlas is valid.")


if __name__ == "__main__":
    main()
