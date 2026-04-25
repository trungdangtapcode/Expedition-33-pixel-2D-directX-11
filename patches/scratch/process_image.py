import sys
from PIL import Image, ImageOps

input_path = sys.argv[1]
output_path = sys.argv[2]

img = Image.open(input_path).convert("RGBA")

# Extract the alpha channel using luminescence
# Convert image to grayscale to use as an alpha map
gray = img.convert("L")

def contrast_alpha(x):
    if x < 15: return 0
    return min(255, int((x - 15) * (255.0 / 240.0) * 1.8))

alpha = gray.point(contrast_alpha)
img.putalpha(alpha)

bbox = alpha.getbbox()
if bbox:
    img = img.crop(bbox)

w, h = img.size
size = max(w, h)
new_img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
new_img.paste(img, ((size - w) // 2, (size - h) // 2))

final_img = new_img.resize((128, 128), Image.Resampling.LANCZOS)
final_img.save(output_path)
print(f"Saved {output_path} successfully.")
