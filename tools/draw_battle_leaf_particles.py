"""
File: draw_battle_leaf_particles.py
Responsibility: Generate the transparent battle leaf particle texture.

The game renders many instances of this small texture through
BattleAmbientParticleRenderer. Keeping the source script in tools/ makes the
asset reproducible when the palette or silhouette needs adjustment.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "assets" / "environments" / "leaf_particle.png"


def draw_leaf() -> Image.Image:
    """Return a 32x32 transparent pixel-art leaf texture."""

    canvas_size = 128
    canvas = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)

    outline = (65, 49, 22, 210)
    dark = (116, 84, 31, 235)
    mid = (181, 139, 58, 245)
    light = (232, 199, 108, 245)

    silhouette = [
        (64, 9),
        (86, 28),
        (101, 63),
        (83, 102),
        (58, 119),
        (34, 99),
        (24, 65),
        (41, 28),
    ]

    draw.polygon(silhouette, fill=outline)
    draw.polygon(
        [
            (64, 16),
            (82, 33),
            (94, 63),
            (79, 95),
            (59, 110),
            (40, 94),
            (31, 66),
            (45, 34),
        ],
        fill=mid,
    )
    draw.polygon(
        [
            (63, 18),
            (81, 36),
            (89, 61),
            (73, 54),
            (56, 39),
        ],
        fill=light,
    )
    draw.line([(64, 17), (60, 111)], fill=dark, width=5)
    draw.line([(61, 58), (85, 41)], fill=dark, width=3)
    draw.line([(59, 72), (36, 55)], fill=dark, width=3)
    draw.line([(60, 84), (80, 78)], fill=dark, width=2)

    softened = canvas.filter(ImageFilter.GaussianBlur(radius=0.2))
    rotated = softened.rotate(-27, resample=Image.Resampling.BICUBIC, expand=False)
    return rotated.resize((32, 32), Image.Resampling.LANCZOS)


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    draw_leaf().save(OUTPUT)
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
