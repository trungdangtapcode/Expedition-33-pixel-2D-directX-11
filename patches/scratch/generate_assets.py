from PIL import Image, ImageDraw
import json

# 1. Generate new 9-slice Bullet Hell Box
frame_w, frame_h = 64, 64
frame_img = Image.new("RGBA", (frame_w, frame_h), (0, 0, 0, 180)) # Dark translucent interior
draw = ImageDraw.Draw(frame_img)
border_thickness = 4
draw.rectangle([0, 0, frame_w-1, frame_h-1], outline=(255, 255, 255, 255), width=border_thickness)

frame_path = r"d:\lab\vscworkplace\directX\assets\UI\bullet_hell_frame.png"
frame_img.save(frame_path)

frame_json = {
    "width": frame_w,
    "height": frame_h,
    "crop-region": { "left": 0, "top": 0, "right": frame_w, "bottom": frame_h },
    "nine-slice": { 
        "left": border_thickness + 1, 
        "top": border_thickness + 1, 
        "right": frame_w - border_thickness - 1, 
        "bottom": frame_h - border_thickness - 1 
    }
}
with open(r"d:\lab\vscworkplace\directX\assets\UI\bullet_hell_frame.json", "w") as f:
    json.dump(frame_json, f, indent=4)


# 2. Generate Pixel-Art Red Heart for hitbox
heart_pixels = [
    "  XXX   XXX  ",
    " XXXXX XXXXX ",
    "XXXXXXXXXXXXX",
    "XXXXXXXXXXXXX",
    " XXXXXXXXXXX ",
    "  XXXXXXXXX  ",
    "   XXXXXXX   ",
    "    XXXXX    ",
    "     XXX     ",
    "      X      "
]
heart_w, heart_h = 13, 10
scale = 4
heart_img = Image.new("RGBA", (heart_w * scale, heart_h * scale), (0,0,0,0))
heart_draw = ImageDraw.Draw(heart_img)
for y, row in enumerate(heart_pixels):
    for x, char in enumerate(row):
        if char == 'X':
            heart_draw.rectangle([x*scale, y*scale, (x+1)*scale-1, (y+1)*scale-1], fill=(255, 0, 0, 255))
            
heart_path = r"d:\lab\vscworkplace\directX\assets\UI\bullet_hell_heart.png"
heart_img.save(heart_path)

print("Assets (bullet_hell_frame and bullet_hell_heart) generated successfully.")
