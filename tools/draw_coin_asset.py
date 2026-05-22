from pathlib import Path

from PIL import Image, ImageDraw


def draw_coin(path: Path) -> None:
    size = 32
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    shadow = (80, 46, 8, 120)
    rim_dark = (145, 86, 15, 255)
    rim = (239, 173, 37, 255)
    fill = (255, 214, 77, 255)
    shine = (255, 244, 169, 255)

    draw.ellipse((5, 7, 29, 31), fill=shadow)
    draw.ellipse((3, 3, 27, 27), fill=rim_dark)
    draw.ellipse((5, 4, 25, 24), fill=rim)
    draw.ellipse((8, 7, 22, 21), fill=fill)
    draw.arc((7, 6, 23, 22), start=205, end=335, fill=rim_dark, width=2)
    draw.line((13, 8, 18, 8), fill=shine, width=2)
    draw.line((10, 11, 20, 11), fill=shine, width=1)
    draw.polygon(((16, 13), (18, 17), (16, 21), (14, 17)), fill=rim_dark)
    draw.line((13, 17, 19, 17), fill=rim_dark, width=1)

    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


if __name__ == "__main__":
    draw_coin(Path("assets/UI/coin_icon.png"))
