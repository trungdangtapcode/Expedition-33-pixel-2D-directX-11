import struct
import os

def write_bmp(filename, width, height, pixels):
    row_padding = (4 - (width * 3) % 4) % 4
    file_size = 54 + (width * 3 + row_padding) * height
    with open(filename, 'wb') as f:
        f.write(b'BM')
        f.write(struct.pack('<I', file_size))
        f.write(b'\x00\x00\x00\x00')
        f.write(struct.pack('<I', 54))
        f.write(struct.pack('<I', 40))
        f.write(struct.pack('<I', width))
        f.write(struct.pack('<i', -height)) # top-down image
        f.write(struct.pack('<H', 1))
        f.write(struct.pack('<H', 24))
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<I', (width * 3 + row_padding) * height))
        f.write(struct.pack('<I', 2835))
        f.write(struct.pack('<I', 2835))
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<I', 0))
        for y in range(height):
            for x in range(width):
                r, g, b = pixels[y][x]
                f.write(struct.pack('BBB', b, g, r)) # BMP is BGR
            for _ in range(row_padding):
                f.write(b'\x00')

width = 384
height = 128
pixels = [[(0,0,0) for _ in range(width)] for __ in range(height)]

for y in range(height):
    for x in range(width):
        tile_x = x // 128
        lx = x % 128
        ly = y % 128
        
        # Checkerboard pattern for natural tiles look
        pattern = ((lx // 16) + (ly // 16)) % 2
        
        if tile_x == 0: # Light green grass
            pixels[y][x] = (80, 160, 60) if pattern else (90, 180, 70)
        elif tile_x == 1: # Brown dirt
            pixels[y][x] = (130, 90, 50) if pattern else (150, 110, 60)
        else: # Grey stone
            pixels[y][x] = (100, 100, 100) if pattern else (120, 120, 120)

target = 'assets/environments/overworld_tiles.bmp'
if not os.path.exists('assets/environments'):
    os.makedirs('assets/environments')
write_bmp(target, width, height, pixels)
print("Bitmap generated successfully:", target)
