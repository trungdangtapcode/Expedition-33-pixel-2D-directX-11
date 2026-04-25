from PIL import Image
import os
import shutil

BASE_DIR = r"d:\lab\vscworkplace\directX"
ANIM_DIR = os.path.join(BASE_DIR, "assets", "animations")
UI_DIR = os.path.join(BASE_DIR, "assets", "UI")
DATA_DIR = os.path.join(BASE_DIR, "data", "characters")

def tint_image(src_path, dst_path, tint_color=(255, 100, 100)):
    if not os.path.exists(src_path):
        print(f"Skipping {src_path}, file not found.")
        return
    img = Image.open(src_path).convert("RGBA")
    data = img.getdata()
    new_data = []
    
    tr, tg, tb = tint_color
    
    for r, g, b, a in data:
        if a > 0:
            # apply red tint
            nr = int((r + tr) / 2)
            ng = int((g + tg) / 2)
            nb = int((b + tb) / 2)
            new_data.append((nr, ng, nb, a))
        else:
            new_data.append((r, g, b, a))
            
    img.putdata(new_data)
    img.save(dst_path)
    print(f"Saved {dst_path}")

# Tint Verso animations -> Maelle
tint_image(os.path.join(ANIM_DIR, "verso.png"), os.path.join(ANIM_DIR, "maelle.png"), (255, 50, 50))

# Tint UI Frame -> Maelle Frame
tint_image(os.path.join(UI_DIR, "UI_verso_hp.png"), os.path.join(UI_DIR, "UI_maelle_hp.png"), (255, 50, 50))

# Tint Turn Order Graphic -> Maelle Turn
tint_image(os.path.join(UI_DIR, "turn-view-verso.png"), os.path.join(UI_DIR, "turn-view-maelle.png"), (255, 50, 50))

# Copy the JSON mapping metadata accurately
verso_json = os.path.join(ANIM_DIR, "verso.json")
if os.path.exists(verso_json):
    shutil.copy(verso_json, os.path.join(ANIM_DIR, "maelle.json"))
else:
    print(f"Skipping verso.json, file not found.")
    
# Copy character stats
verso_char = os.path.join(DATA_DIR, "verso.json")
if os.path.exists(verso_char):
    with open(verso_char, "r") as f:
        data = f.read()
        data = data.replace('"Verso"', '"Maelle"')
        data = data.replace('"hp": 100', '"hp": 80')
        data = data.replace('"maxHp": 100', '"maxHp": 80')
        data = data.replace('"spd": 10', '"spd": 15')
    with open(os.path.join(DATA_DIR, "maelle.json"), "w") as f:
        f.write(data)
    print("Created maelle.json")
else:
    print("verso.json character config not found.")
