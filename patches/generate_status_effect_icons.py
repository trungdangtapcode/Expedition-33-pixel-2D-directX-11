#!/usr/bin/env python3
# ============================================================
# File: generate_status_effect_icons.py
# Responsibility: Generate the status-effect icon atlas used by battle UI.
#
# Owns:
#   - assets/UI/status_effect_icons.png
#   - assets/UI/status_effect_icons.json
#
# Lifetime:
#   Run manually after changing status icon IDs or visual direction.
#
# Important:
#   - Output is deterministic so commits contain stable binary assets.
#   - Icons are intentionally simple pixel symbols; production art can
#     replace this atlas without changing C++ code.
# ============================================================

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUT_PNG = ROOT / "assets" / "UI" / "status_effect_icons.png"
OUT_JSON = ROOT / "assets" / "UI" / "status_effect_icons.json"
ICON_SIZE = 32


ICONS = [
    "fallback",
    "attack",
    "rage",
    "burn",
    "weaken",
    "power_up",
    "guard_up",
    "haste",
    "vulnerable",
]


def draw_frame(draw: ImageDraw.ImageDraw, x: int, color: tuple[int, int, int, int]) -> None:
    """Draw a thin frame so small icons remain readable over busy portraits."""
    draw.rectangle((x + 1, 1, x + 30, 30), outline=(15, 13, 12, 230))
    draw.rectangle((x + 2, 2, x + 29, 29), outline=color)


def draw_fallback(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.polygon([(x + 16, 6), (x + 25, 16), (x + 16, 26), (x + 7, 16)], fill=(155, 155, 168, 255))
    draw.line((x + 16, 10, x + 16, 22), fill=(30, 30, 36, 255), width=2)
    draw.point((x + 16, 24), fill=(30, 30, 36, 255))


def draw_attack(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.line((x + 22, 6, x + 9, 23), fill=(216, 218, 222, 255), width=4)
    draw.line((x + 22, 6, x + 9, 23), fill=(73, 80, 92, 255), width=1)
    draw.line((x + 12, 19, x + 19, 25), fill=(183, 122, 48, 255), width=3)
    draw.line((x + 8, 17, x + 14, 23), fill=(231, 180, 74, 255), width=2)


def draw_rage(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.polygon([(x + 17, 5), (x + 25, 18), (x + 18, 27), (x + 8, 22), (x + 11, 12)], fill=(151, 24, 30, 255))
    draw.polygon([(x + 17, 9), (x + 21, 18), (x + 16, 25), (x + 11, 20)], fill=(255, 96, 54, 255))
    draw.polygon([(x + 17, 14), (x + 19, 20), (x + 15, 24), (x + 13, 19)], fill=(255, 217, 118, 255))


def draw_burn(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.ellipse((x + 6, 21, x + 26, 28), fill=(67, 30, 18, 180))
    draw.polygon([(x + 16, 5), (x + 24, 18), (x + 17, 28), (x + 8, 22), (x + 12, 12)], fill=(198, 48, 22, 255))
    draw.polygon([(x + 17, 10), (x + 21, 19), (x + 15, 27), (x + 11, 20)], fill=(255, 154, 49, 255))
    draw.polygon([(x + 16, 15), (x + 18, 21), (x + 14, 25), (x + 13, 20)], fill=(255, 235, 139, 255))


def draw_weaken(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.line((x + 8, 8, x + 23, 23), fill=(162, 56, 65, 255), width=4)
    draw.line((x + 8, 23, x + 23, 8), fill=(162, 56, 65, 255), width=4)
    draw.line((x + 14, 7, x + 14, 25), fill=(218, 210, 173, 255), width=2)
    draw.line((x + 14, 25, x + 10, 20), fill=(218, 210, 173, 255), width=2)
    draw.line((x + 14, 25, x + 18, 20), fill=(218, 210, 173, 255), width=2)


def draw_power_up(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.line((x + 16, 25, x + 16, 8), fill=(231, 188, 82, 255), width=4)
    draw.polygon([(x + 16, 5), (x + 24, 14), (x + 19, 14), (x + 19, 25), (x + 13, 25), (x + 13, 14), (x + 8, 14)], fill=(247, 209, 92, 255))
    draw.line((x + 9, 24, x + 23, 24), fill=(105, 68, 24, 255), width=2)


def draw_guard_up(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.polygon([(x + 16, 5), (x + 25, 9), (x + 24, 20), (x + 16, 28), (x + 8, 20), (x + 7, 9)], fill=(88, 125, 174, 255))
    draw.polygon([(x + 16, 8), (x + 22, 11), (x + 21, 19), (x + 16, 24), (x + 11, 19), (x + 10, 11)], fill=(179, 207, 232, 255))
    draw.line((x + 16, 8, x + 16, 24), fill=(45, 71, 112, 255), width=2)


def draw_haste(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.line((x + 6, 20, x + 21, 20), fill=(110, 210, 207, 255), width=3)
    draw.line((x + 10, 14, x + 26, 14), fill=(110, 210, 207, 255), width=3)
    draw.line((x + 9, 25, x + 19, 25), fill=(110, 210, 207, 255), width=2)
    draw.polygon([(x + 20, 9), (x + 27, 14), (x + 20, 19)], fill=(223, 249, 243, 255))


def draw_vulnerable(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.polygon([(x + 16, 5), (x + 26, 15), (x + 16, 27), (x + 6, 15)], fill=(121, 69, 156, 255))
    draw.line((x + 16, 5, x + 14, 13, x + 18, 17, x + 15, 27), fill=(244, 193, 207, 255), width=2)
    draw.line((x + 8, 15, x + 14, 13), fill=(244, 193, 207, 255), width=1)


DRAWERS = {
    "fallback": draw_fallback,
    "attack": draw_attack,
    "rage": draw_rage,
    "burn": draw_burn,
    "weaken": draw_weaken,
    "power_up": draw_power_up,
    "guard_up": draw_guard_up,
    "haste": draw_haste,
    "vulnerable": draw_vulnerable,
}


def main() -> None:
    OUT_PNG.parent.mkdir(parents=True, exist_ok=True)
    image = Image.new("RGBA", (ICON_SIZE * len(ICONS), ICON_SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    metadata = {"iconSize": ICON_SIZE, "icons": []}
    for index, icon_id in enumerate(ICONS):
        x = index * ICON_SIZE
        draw_frame(draw, x, (202, 171, 97, 255))
        DRAWERS[icon_id](draw, x)
        metadata["icons"].append({"id": icon_id, "x": x, "y": 0, "w": ICON_SIZE, "h": ICON_SIZE})

    image.save(OUT_PNG)
    OUT_JSON.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {OUT_PNG}")
    print(f"Wrote {OUT_JSON}")


if __name__ == "__main__":
    main()
