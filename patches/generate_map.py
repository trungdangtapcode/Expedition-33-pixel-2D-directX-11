#!/usr/bin/env python3
"""Generate the Tiled-compatible overworld map used by OverworldState."""

from __future__ import annotations

import json
from pathlib import Path


TILE_SIZE = 64
WIDTH = 128
HEIGHT = 96

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


def paint_tile(data: list[int], tx: int, ty: int, gid: int) -> None:
    if 0 <= tx < WIDTH and 0 <= ty < HEIGHT:
        data[tile_index(tx, ty)] = gid


def paint_dirt(data: list[int], tx: int, ty: int) -> None:
    paint_tile(data, tx, ty, GROUND_DIRT_A if (tx + ty) % 2 == 0 else GROUND_DIRT_B)


def paint_rect(data: list[int], left: int, top: int, right: int, bottom: int) -> None:
    for ty in range(top, bottom + 1):
        for tx in range(left, right + 1):
            paint_dirt(data, tx, ty)


def paint_disc(data: list[int], cx: int, cy: int, radius: int) -> None:
    radius_sq = radius * radius
    for ty in range(cy - radius, cy + radius + 1):
        for tx in range(cx - radius, cx + radius + 1):
            dx = tx - cx
            dy = ty - cy
            if dx * dx + dy * dy <= radius_sq:
                paint_dirt(data, tx, ty)


def paint_road_line(data: list[int],
                    start_x: int,
                    start_y: int,
                    end_x: int,
                    end_y: int,
                    half_width: int = 1) -> None:
    steps = max(abs(end_x - start_x), abs(end_y - start_y), 1)
    for step in range(steps + 1):
        t = step / steps
        tx = round(start_x + (end_x - start_x) * t)
        ty = round(start_y + (end_y - start_y) * t)
        paint_rect(data, tx - half_width, ty - half_width, tx + half_width, ty + half_width)


def make_ground() -> list[int]:
    data: list[int] = []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            data.append(GROUND_GRASS_A if (x + y) % 2 == 0 else GROUND_GRASS_B)

    center_x = WIDTH // 2
    center_y = HEIGHT // 2

    # The map is built as a readable adventure route:
    # center camp -> village ruins -> branch choices -> shrine -> mirror gate.
    paint_disc(data, center_x, center_y, 8)
    paint_rect(data, 8, center_y - 1, WIDTH - 9, center_y + 1)
    paint_road_line(data, center_x, center_y, center_x, 16, 1)
    paint_road_line(data, center_x, center_y, 22, 38, 1)
    paint_road_line(data, center_x, center_y, 72, 80, 1)
    paint_road_line(data, center_x, center_y, 116, 24, 1)
    paint_road_line(data, 22, 38, 40, 22, 1)
    paint_road_line(data, 72, 80, 105, 73, 1)

    # Landmarks get broader dirt silhouettes so they read from the camera view.
    paint_disc(data, 60, 24, 7)    # Silent market ruins.
    paint_disc(data, 23, 38, 6)    # Western watch.
    paint_disc(data, 72, 80, 7)    # Glass shrine camp.
    paint_disc(data, 91, 33, 5)    # Pilgrim crossing.
    paint_disc(data, 116, 24, 7)   # Mirror gate arena.

    # Scars and side paths hint at a world that existed before the player arrived.
    paint_road_line(data, 49, 31, 35, 18, 1)
    paint_road_line(data, 84, 43, 98, 56, 1)
    paint_road_line(data, 29, 50, 20, 66, 1)
    paint_road_line(data, 112, 31, 119, 42, 1)

    return data


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


def add_map_bounds(colliders: list[dict[str, object]]) -> None:
    map_w = WIDTH * TILE_SIZE
    map_h = HEIGHT * TILE_SIZE
    t = TILE_SIZE

    add_collider(colliders, "BoundaryNorth", 0, 0, map_w, t)
    add_collider(colliders, "BoundarySouth", 0, map_h - t, map_w, t)
    add_collider(colliders, "BoundaryWest", 0, 0, t, map_h)
    add_collider(colliders, "BoundaryEast", map_w - t, 0, t, map_h)


def place_tile(objects: list[int], tx: int, ty: int, gid: int) -> None:
    if 0 <= tx < WIDTH and 0 <= ty < HEIGHT:
        objects[tile_index(tx, ty)] = gid


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


def place_rocks(objects: list[int],
                colliders: list[dict[str, object]],
                specs: list[tuple[int, int, str]]) -> None:
    for tx, ty, name in specs:
        place_prop(objects, colliders, tx, ty, ROCK_GID, name, 28, 36)


def place_tables(objects: list[int],
                 colliders: list[dict[str, object]],
                 specs: list[tuple[int, int, str]]) -> None:
    for tx, ty, name in specs:
        place_prop(objects, colliders, tx, ty, TABLE_GID, name, 24, 40)


def make_objects() -> tuple[list[int], list[dict[str, object]]]:
    objects = [0] * (WIDTH * HEIGHT)
    colliders: list[dict[str, object]] = []

    add_map_bounds(colliders)

    house_specs = [
        (54, 17, "SilentMarketHouseA"),
        (66, 18, "SilentMarketHouseB"),
        (45, 27, "SilentMarketHouseC"),
        (17, 31, "WesternWatchHouseA"),
        (28, 36, "WesternWatchHouseB"),
        (12, 58, "OldFarmHouse"),
        (66, 72, "GlassShrineCaretakerHouse"),
        (78, 78, "GlassShrinePilgrimHouse"),
        (97, 66, "FarOrchardHouseA"),
        (108, 70, "FarOrchardHouseB"),
        (88, 25, "PilgrimCrossingHouse"),
        (107, 15, "MirrorGateOutpostA"),
        (118, 18, "MirrorGateOutpostB"),
        (58, 55, "MeadowStorehouseA"),
        (70, 55, "MeadowStorehouseB"),
    ]

    for tx, ty, name in house_specs:
        place_house(objects, colliders, tx, ty, name)

    place_tables(objects, colliders, [
        (58, 44, "MeadowTableWest"),
        (68, 45, "MeadowTableEast"),
        (60, 27, "SilentMarketTableA"),
        (65, 28, "SilentMarketTableB"),
        (72, 76, "ShrineOfferingTableA"),
        (75, 82, "ShrineOfferingTableB"),
        (91, 36, "PilgrimCrossingTable"),
    ])

    place_rocks(objects, colliders, [
        (61, 41, "MeadowMemoryStoneA"),
        (67, 42, "MeadowMemoryStoneB"),
        (51, 33, "SilentMarketRubbleA"),
        (70, 31, "SilentMarketRubbleB"),
        (21, 34, "WesternWatchStoneA"),
        (25, 42, "WesternWatchStoneB"),
        (15, 68, "OldFarmStone"),
        (69, 84, "GlassShardA"),
        (76, 73, "GlassShardB"),
        (82, 82, "GlassShardC"),
        (89, 30, "PilgrimRoadStoneA"),
        (96, 34, "PilgrimRoadStoneB"),
        (111, 23, "MirrorGatePillarA"),
        (116, 18, "MirrorGatePillarB"),
        (121, 24, "MirrorGatePillarC"),
        (116, 30, "MirrorGatePillarD"),
        (103, 55, "AshRoadStone"),
        (117, 42, "BrokenTrailStone"),
        (35, 20, "OldNorthTrailStoneA"),
        (40, 24, "OldNorthTrailStoneB"),
    ])

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
