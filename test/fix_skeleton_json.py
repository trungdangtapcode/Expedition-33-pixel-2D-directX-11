import os

path = os.path.join(os.path.dirname(__file__), '..', 'assets', 'animations', 'skeleton.json')

# Write manually to keep pivot on one line — JsonLoader's ParseIntArray2
# searches for "[x, y]" on a single line.
content = """{
        "sprite_name": "skeleton",
        "character": "skeleton",
        "width": 768,
        "height": 256,
        "frame_width": 128,
        "frame_height": 128,
        "animations":[
                {
                        "name": "idle",
                        "num_frames": 12,
                        "frame_rate": 8,
                        "loop": true,
                        "pivot": [65, 122],
                        "align": "bottom-center"
                }
        ]
}
"""

with open(path, 'w', newline='\n') as f:
    f.write(content)

print("Written:", path)
print(content)
