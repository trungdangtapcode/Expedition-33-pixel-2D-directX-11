import json

width = 25
height = 18

ground_data = []
for y in range(height):
    for x in range(width):
        if 8 <= y <= 10:
            ground_data.append(5 if (x+y)%2 == 0 else 6)
        else:
            ground_data.append(1 if (x+y)%2 == 0 else 2)

objects_data = [0] * (width * height)
colliders = []

def place_tile(tx, ty, gid):
    if 0 <= tx < width and 0 <= ty < height:
        objects_data[ty * width + tx] = gid

def place_massive_house(start_x, start_y):
    # The house is 8x8 tiles. Total 64 tiles.
    # The GIDs start at 9 and go to 72.
    current_gid = 9
    for row in range(8):
        for col in range(8):
            place_tile(start_x + col, start_y + row, current_gid)
            current_gid += 1
            
    # The massive AAA house needs a massive hitbox.
    # We want Verso to walk behind the roof but collide with the foundation.
    # The house occupies Y from start_y to start_y + 8.
    # The foundation is probably the bottom 3 tiles.
    colliders.append({
        "id": len(colliders) + 1,
        "name": "MassiveHouse",
        "x": start_x * 64,
        "y": (start_y + 5) * 64,  # Collide only on the bottom 3 tiles
        "width": 8 * 64,
        "height": 3 * 64
    })

def place_object(tx, ty, gid, collider_offset_y=0, collider_height=64, collider_width=64):
    place_tile(tx, ty, gid)
    colliders.append({
        "id": len(colliders) + 1,
        "name": f"Obj_{gid}",
        "x": tx * 64,
        "y": ty * 64 + collider_offset_y,
        "width": collider_width,
        "height": collider_height
    })

# Place the 8x8 AAA House spanning from x=10, y=0!
place_massive_house(10, 0)

# The table GID is now 73! Rock is 74!
place_object(6, 9, 73, 24, 40) # Table on dirt
place_object(20, 14, 74, 28, 36) # Rock

map_json = {
  "type": "map",
  "version": "1.10",
  "orientation": "orthogonal",
  "renderorder": "right-down",
  "width": width,
  "height": height,
  "tilewidth": 64,
  "tileheight": 64,
  "tilesets": [
    {
      "firstgid": 1,
      "name": "ground",
      "image": "overworld_tiles.png",
      "imagewidth": 512,
      "imageheight": 64,
      "tilewidth": 64,
      "tileheight": 64
    },
    {
      "firstgid": 9,
      "name": "objects",
      "image": "overworld_objects.png",
      "imagewidth": 512,
      "imageheight": 576,
      "tilewidth": 64,
      "tileheight": 64
    }
  ],
  "layers": [
    {
      "type": "tilelayer",
      "id": 1,
      "name": "Ground",
      "x": 0,
      "y": 0,
      "width": width,
      "height": height,
      "visible": True,
      "opacity": 1,
      "data": ground_data
    },
    {
      "type": "tilelayer",
      "id": 2,
      "name": "Objects",
      "x": 0,
      "y": 0,
      "width": width,
      "height": height,
      "visible": True,
      "opacity": 1,
      "data": objects_data
    },
    {
      "type": "objectgroup",
      "id": 3,
      "name": "Collisions",
      "objects": colliders
    }
  ]
}

with open("assets/environments/overworld_map.json", "w") as f:
    json.dump(map_json, f, indent=2)

print("Generated massive AAA overworld_map.json")
