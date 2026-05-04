from PIL import Image
import sys

def remove_black_bg(input_path, output_path):
    img = Image.open(input_path).convert("RGBA")
    data = img.getdata()
    
    new_data = []
    for item in data:
        r, g, b, a = item
        # Calculate alpha based on the maximum color component (luminance approximation for glow)
        alpha = max(r, g, b)
        
        if alpha > 0:
            # Un-premultiply the RGB values so the glow retains its pure color instead of turning gray/black
            new_r = min(int((r * 255) / alpha), 255)
            new_g = min(int((g * 255) / alpha), 255)
            new_b = min(int((b * 255) / alpha), 255)
            new_data.append((new_r, new_g, new_b, alpha))
        else:
            new_data.append((0, 0, 0, 0))
            
    img.putdata(new_data)
    img.save(output_path, "PNG")
    print(f"Successfully processed {input_path}")

input_img = r"d:\lab\vscworkplace\directX\assets\UI\round_bullet.png"
output_img = r"d:\lab\vscworkplace\directX\assets\UI\round_bullet.png"
remove_black_bg(input_img, output_img)
