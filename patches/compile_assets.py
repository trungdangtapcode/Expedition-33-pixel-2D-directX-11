import os
from PIL import Image, ImageDraw

brain_dir = r"C:\Users\MY LAPTOP\.gemini\antigravity\brain\a6375de8-0562-4d0f-b660-12540f92c22c"

grass_path = os.path.join(brain_dir, "jrpg_grass_square_1776806484465.png")
dirt_path = os.path.join(brain_dir, "jrpg_dirt_square_1776806499241.png")
house_path = os.path.join(brain_dir, "aaa_realistic_house_1776807592056.png")

# --- Compile overworld_tiles.png ---
tiles_img = Image.new('RGBA', (512, 64), (0, 0, 0, 0))
try:
    grass_img = Image.open(grass_path).convert('RGBA').resize((64, 64), Image.LANCZOS)
    dirt_img = Image.open(dirt_path).convert('RGBA').resize((64, 64), Image.LANCZOS)
    tiles_img.paste(grass_img, (0, 0))
    tiles_img.paste(grass_img, (64, 0))
    tiles_img.paste(dirt_img, (256, 0))
    tiles_img.paste(dirt_img, (320, 0))
except: pass
tiles_img.save("assets/environments/overworld_tiles.png")

# --- Compile MASSIVE overworld_objects.png (512 width, 640 height) ---
# 512 width = 8 columns.
# Rows 0-7 (512 height) = 8x8 House (GIDs 9 through 72).
# Row 8 (64 height) = props (Table, Rock).
objs_img = Image.new('RGBA', (512, 576), (0, 0, 0, 0))

def remove_background(img, fuzz=30):
    img = img.convert("RGBA")
    data = img.getdata()
    bg_color = data[0]
    new_data = []
    for item in data:
        diff = sum((a - b) ** 2 for a, b in zip(item[:3], bg_color[:3]))
        if diff < fuzz ** 2:
            new_data.append((255, 255, 255, 0))
        else:
            new_data.append(item)
    img.putdata(new_data)
    return img

try:
    house_img = Image.open(house_path).convert('RGBA')
    house_img = remove_background(house_img, 60)
    # Resize to exactly 512x512 (8x8 tiles!)
    house_img = house_img.resize((512, 512), Image.LANCZOS)
    objs_img.paste(house_img, (0, 0), house_img)
except Exception as e:
    print(f"Error loading large house: {e}")

d = ImageDraw.Draw(objs_img)
# Table GID 73 (Row 8, Col 0 -> X 0, Y 512)
d.rectangle([10, 512 + 24, 54, 512 + 36], fill=(139, 69, 19), outline=(0,0,0))
d.rectangle([16, 512 + 36, 22, 512 + 56], fill=(139, 69, 19), outline=(0,0,0))
d.rectangle([42, 512 + 36, 48, 512 + 56], fill=(139, 69, 19), outline=(0,0,0))

# Rock GID 74 (Row 8, Col 1 -> X 64, Y 512)
d.ellipse([64 + 12, 512 + 28, 64 + 52, 512 + 56], fill=(169, 169, 169), outline=(0,0,0))
d.ellipse([64 + 8, 512 + 36, 64 + 32, 512 + 56], fill=(169, 169, 169), outline=(0,0,0))

objs_img.save("assets/environments/overworld_objects.png")
print("Saved AAA Enormous 8x8 overworld_objects.png")
