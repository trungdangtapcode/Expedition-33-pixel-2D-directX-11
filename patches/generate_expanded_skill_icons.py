from PIL import Image, ImageDraw


ATLAS_PATH = "assets/UI/status_effect_icons.png"
ICON_SIZE = 32
TARGET_WIDTH = 416


def draw_heal(draw, x):
    draw.ellipse((x + 4, 4, x + 27, 27), fill=(38, 94, 70, 255), outline=(170, 238, 178, 255), width=2)
    draw.rectangle((x + 14, 8, x + 18, 24), fill=(220, 255, 220, 255))
    draw.rectangle((x + 8, 14, x + 24, 18), fill=(220, 255, 220, 255))


def draw_revive(draw, x):
    draw.polygon([(x + 16, 3), (x + 26, 26), (x + 16, 21), (x + 6, 26)], fill=(101, 64, 150, 255), outline=(235, 220, 255, 255))
    draw.line((x + 16, 6, x + 16, 22), fill=(255, 240, 160, 255), width=2)
    draw.arc((x + 8, 9, x + 24, 25), 210, 330, fill=(255, 240, 160, 255), width=2)


def draw_cleanse(draw, x):
    draw.ellipse((x + 5, 5, x + 26, 26), fill=(46, 80, 120, 255), outline=(190, 230, 255, 255), width=2)
    draw.line((x + 10, 22, x + 22, 10), fill=(245, 255, 255, 255), width=3)
    draw.line((x + 10, 10, x + 22, 22), fill=(245, 255, 255, 255), width=3)


def draw_sweep(draw, x):
    draw.arc((x + 4, 7, x + 28, 29), 205, 350, fill=(240, 205, 110, 255), width=4)
    draw.polygon([(x + 25, 10), (x + 29, 18), (x + 20, 17)], fill=(240, 205, 110, 255))
    draw.line((x + 8, 22, x + 24, 8), fill=(235, 235, 245, 255), width=2)


def main():
    source = Image.open(ATLAS_PATH).convert("RGBA")
    if source.width >= TARGET_WIDTH:
        return

    atlas = Image.new("RGBA", (TARGET_WIDTH, ICON_SIZE), (0, 0, 0, 0))
    atlas.paste(source, (0, 0))
    draw = ImageDraw.Draw(atlas)
    draw_heal(draw, 288)
    draw_revive(draw, 320)
    draw_cleanse(draw, 352)
    draw_sweep(draw, 384)
    atlas.save(ATLAS_PATH)


if __name__ == "__main__":
    main()
