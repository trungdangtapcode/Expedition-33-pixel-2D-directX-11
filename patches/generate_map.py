#!/usr/bin/env python3
"""Generate the Tiled-compatible overworld map used by OverworldState."""

from __future__ import annotations

import json
from pathlib import Path


TILE_SIZE = 64
WIDTH = 96
HEIGHT = 72

GROUND_GRASS_A = 1
GROUND_GRASS_B = 2
GROUND_DIRT_A = 5
GROUND_DIRT_B = 6

HOUSE_FIRST_GID = 9
TABLE_GID = 73
ROCK_GID = 74

OUT_PATH = Path("assets/environments/overworld_map.json")


def tile_index(tx: int, ty: int) -> int:
    return ty * WIDTH + tx


def make_ground() -> list[int]:
    data: list[int] = []
    center_x = WIDTH // 2
    center_y = HEIGHT // 2

    for y in range(HEIGHT):
        for x in range(WIDTH):
            on_plaza = abs(x - center_x) <= 7 and abs(y - center_y) <= 4
            on_main_road = abs(y - center_y) <= 1
            on_cross_road = abs(x - center_x) <= 1 and 6 <= y <= HEIGHT - 7
            on_north_lane = abs(y - (center_y - 22)) <= 1 and 6 <= x <= WIDTH - 7
            on_south_lane = abs(y - (center_y + 22)) <= 1 and 6 <= x <= WIDTH - 7
            on_west_lane = abs(x - (center_x - 32)) <= 1 and 8 <= y <= HEIGHT - 8
            on_east_lane = abs(x - (center_x + 32)) <= 1 and 8 <= y <= HEIGHT - 8
            on_northwest_cut = abs((x - 14) - (y - 10)) <= 1 and 12 <= x <= 33 and 8 <= y <= 29
            on_southeast_cut = abs((x - 62) - (y - 42)) <= 1 and 58 <= x <= 84 and 38 <= y <= 64

            if (on_plaza or on_main_road or on_cross_road or on_north_lane
                    or on_south_lane or on_west_lane or on_east_lane
                    or on_northwest_cut or on_southeast_cut):
                data.append(GROUND_DIRT_A if (x + y) % 2 == 0 else GROUND_DIRT_B)
            else:
                data.append(GROUND_GRASS_A if (x + y) % 2 == 0 else GROUND_GRASS_B)

    return data


def place_tile(objects: list[int], tx: int, ty: int, gid: int) -> None:
    if 0 <= tx < WIDTH and 0 <= ty < HEIGHT:
        objects[tile_index(tx, ty)] = gid


def add_collider(colliders: list[dict[str, object]],
                 name: str,
                 x: int,
                 y: int,
                 width: int,
                 height: int) -> None:
    colliders.append({
        "id": len(colliders) + 1,
        "name": name,
        "x": x,
        "y": y,
        "width": width,
        "height": height,
    })


def place_house(objects: list[int],
                colliders: list[dict[str, object]],
                start_x: int,
                start_y: int,
                name: str) -> None:
    if start_x < 0 or start_y < 0 or start_x + 8 > WIDTH or start_y + 8 > HEIGHT:
        raise ValueError(f"House {name} is outside the map.")

    current_gid = HOUSE_FIRST_GID
    for row in range(8):
        for col in range(8):
            place_tile(objects, start_x + col, start_y + row, current_gid)
            current_gid += 1

    add_collider(
        colliders,
        name,
        start_x * TILE_SIZE,
        (start_y + 5) * TILE_SIZE,
        8 * TILE_SIZE,
        3 * TILE_SIZE,
    )


def add_map_bounds(colliders: list[dict[str, object]]) -> None:
    map_w = WIDTH * TILE_SIZE
    map_h = HEIGHT * TILE_SIZE
    t = TILE_SIZE

    add_collider(colliders, "BoundaryNorth", 0, 0, map_w, t)
    add_collider(colliders, "BoundarySouth", 0, map_h - t, map_w, t)
    add_collider(colliders, "BoundaryWest", 0, 0, t, map_h)
    add_collider(colliders, "BoundaryEast", map_w - t, 0, t, map_h)


def place_prop(objects: list[int],
               colliders: list[dict[str, object]],
               tx: int,
               ty: int,
               gid: int,
               name: str,
               collider_offset_y: int,
               collider_height: int,
               collider_width: int = TILE_SIZE) -> None:
    place_tile(objects, tx, ty, gid)
    add_collider(
        colliders,
        name,
        tx * TILE_SIZE,
        ty * TILE_SIZE + collider_offset_y,
        collider_width,
        collider_height,
    )


def make_objects() -> tuple[list[int], list[dict[str, object]]]:
    objects = [0] * (WIDTH * HEIGHT)
    colliders: list[dict[str, object]] = []

    center_x = WIDTH // 2
    center_y = HEIGHT // 2

    add_map_bounds(colliders)

    house_specs = [
        (center_x + 2, center_y - 18, "NorthHouseA"),
        (center_x - 14, center_y - 19, "NorthHouseB"),
        (center_x + 19, center_y - 16, "NorthHouseC"),
        (center_x - 34, center_y - 10, "WestVillageHouseA"),
        (center_x - 42, center_y + 7, "WestVillageHouseB"),
        (center_x + 32, center_y - 9, "EastVillageHouseA"),
        (center_x + 40, center_y + 8, "EastVillageHouseB"),
        (center_x - 8, center_y + 14, "SouthHouseA"),
        (center_x + 9, center_y + 17, "SouthHouseB"),
        (center_x - 34, center_y + 23, "SouthwestFarmHouse"),
        (center_x + 33, center_y + 24, "SoutheastFarmHouse"),
        (6, 9, "NorthwestOutpost"),
        (82, 9, "NortheastOutpost"),
    ]

    for tx, ty, name in house_specs:
        place_house(objects, colliders, tx, ty, name)

    prop_specs = [
        (center_x - 4, center_y - 2, TABLE_GID, "MarketTableA", 24, 40),
        (center_x + 5, center_y + 2, TABLE_GID, "MarketTableB", 24, 40),
        (center_x - 8, center_y + 3, TABLE_GID, "MarketTableC", 24, 40),
        (center_x + 9, center_y - 3, TABLE_GID, "MarketTableD", 24, 40),
        (center_x - 11, center_y + 5, ROCK_GID, "RoadRockA", 28, 36),
        (center_x + 12, center_y + 6, ROCK_GID, "RoadRockB", 28, 36),
        (center_x - 40, center_y - 26, ROCK_GID, "FieldRockNW1", 28, 36),
        (center_x - 24, center_y - 28, ROCK_GID, "FieldRockNW2", 28, 36),
        (center_x + 28, center_y - 27, ROCK_GID, "FieldRockNE1", 28, 36),
        (center_x + 43, center_y - 22, ROCK_GID, "FieldRockNE2", 28, 36),
        (center_x - 41, center_y + 25, ROCK_GID, "FieldRockSW1", 28, 36),
        (center_x - 20, center_y + 29, ROCK_GID, "FieldRockSW2", 28, 36),
        (center_x + 24, center_y + 28, ROCK_GID, "FieldRockSE1", 28, 36),
        (center_x + 42, center_y + 24, ROCK_GID, "FieldRockSE2", 28, 36),
        (center_x - 2, center_y - 29, ROCK_GID, "NorthRoadRock", 28, 36),
        (center_x + 3, center_y + 30, ROCK_GID, "SouthRoadRock", 28, 36),
        (8, HEIGHT - 9, ROCK_GID, "FarSouthwestRock", 28, 36),
        (WIDTH - 9, HEIGHT - 10, ROCK_GID, "FarSoutheastRock", 28, 36),
    ]

    for tx, ty, gid, name, offset_y, height in prop_specs:
        place_prop(objects, colliders, tx, ty, gid, name, offset_y, height)

    return objects, colliders


def build_map() -> dict[str, object]:
    ground_data = make_ground()
    objects_data, colliders = make_objects()

    return {
        "type": "map",
        "version": "1.10",
        "tiledversion": "1.12.1",
        "orientation": "orthogonal",
        "renderorder": "right-down",
        "compressionlevel": -1,
        "infinite": False,
        "width": WIDTH,
        "height": HEIGHT,
        "tilewidth": TILE_SIZE,
        "tileheight": TILE_SIZE,
        "nextlayerid": 4,
        "nextobjectid": len(colliders) + 1,
        "tilesets": [
            {
                "firstgid": 1,
                "name": "ground",
                "image": "overworld_tiles.png",
                "imagewidth": 512,
                "imageheight": 64,
                "tilewidth": TILE_SIZE,
                "tileheight": TILE_SIZE,
                "columns": 8,
                "tilecount": 8,
                "margin": 0,
                "spacing": 0,
            },
            {
                "firstgid": HOUSE_FIRST_GID,
                "name": "objects",
                "image": "overworld_objects.png",
                "imagewidth": 512,
                "imageheight": 576,
                "tilewidth": TILE_SIZE,
                "tileheight": TILE_SIZE,
                "columns": 8,
                "tilecount": 72,
                "margin": 0,
                "spacing": 0,
            },
        ],
        "layers": [
            {
                "type": "tilelayer",
                "id": 1,
                "name": "Ground",
                "x": 0,
                "y": 0,
                "width": WIDTH,
                "height": HEIGHT,
                "visible": True,
                "opacity": 1,
                "data": ground_data,
            },
            {
                "type": "tilelayer",
                "id": 2,
                "name": "Objects",
                "x": 0,
                "y": 0,
                "width": WIDTH,
                "height": HEIGHT,
                "visible": True,
                "opacity": 1,
                "data": objects_data,
            },
            {
                "type": "objectgroup",
                "id": 3,
                "name": "Collisions",
                "visible": True,
                "opacity": 1,
                "draworder": "topdown",
                "objects": colliders,
            },
        ],
    }


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    with OUT_PATH.open("w", encoding="utf-8") as file:
        json.dump(build_map(), file, indent=2)
        file.write("\n")

    print(f"Generated {OUT_PATH} as {WIDTH}x{HEIGHT} Tiled JSON.")


if __name__ == "__main__":
    main()
