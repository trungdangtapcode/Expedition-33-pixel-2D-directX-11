#!/usr/bin/env python3
"""Generate the Tiled map and data-driven static props for the overworld."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAP_OUT = ROOT / "assets" / "environments" / "overworld_map.json"
PROPS_OUT = ROOT / "data" / "overworld_props.json"
TILE = 64
WIDTH = 128
HEIGHT = 96
GROUND_TILE_COUNT = 32
OBJECT_FIRST_GID = GROUND_TILE_COUNT + 1

GRASS = (1, 2, 3, 4)
WILD = (5, 6, 7, 8)
ROAD_BASE = 9
STONE_A = 25
STONE_B = 26
SHRINE_STONE = 27
MIRROR_STONE = 28
DETAIL_CRACKS = 29
DETAIL_LEAVES = 30
DETAIL_PEBBLES = 31
DETAIL_FLOWERS = 32

WALL_H = OBJECT_FIRST_GID + 0
WALL_V = OBJECT_FIRST_GID + 1
WALL_BLOCK = OBJECT_FIRST_GID + 2
SIGN = OBJECT_FIRST_GID + 3
TABLE = OBJECT_FIRST_GID + 4
BUSH = OBJECT_FIRST_GID + 7
ROCK = OBJECT_FIRST_GID + 8
SHARD = OBJECT_FIRST_GID + 9
CANOPY = OBJECT_FIRST_GID + 48


def idx(tx: int, ty: int) -> int:
    return ty * WIDTH + tx


def inside(tx: int, ty: int) -> bool:
    return 0 <= tx < WIDTH and 0 <= ty < HEIGHT


def h(tx: int, ty: int, salt: int = 0) -> int:
    value = (tx * 374761393 + ty * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
    return value ^ (value >> 16)


def set_tile(layer: list[int], tx: int, ty: int, gid: int) -> None:
    if inside(tx, ty):
        layer[idx(tx, ty)] = gid


def add_disc(cells: set[tuple[int, int]], cx: int, cy: int, radius: int) -> None:
    r2 = radius * radius
    for ty in range(cy - radius, cy + radius + 1):
        for tx in range(cx - radius, cx + radius + 1):
            dx = tx - cx
            dy = ty - cy
            if dx * dx + dy * dy <= r2 and inside(tx, ty):
                cells.add((tx, ty))


def add_line(cells: set[tuple[int, int]], start: tuple[int, int], end: tuple[int, int], half_width: int) -> None:
    sx, sy = start
    ex, ey = end
    steps = max(abs(ex - sx), abs(ey - sy), 1)
    for step in range(steps + 1):
        t = step / steps
        tx = round(sx + (ex - sx) * t)
        ty = round(sy + (ey - sy) * t)
        add_disc(cells, tx, ty, half_width)


def world_from_tile(tx: int, ty: int, px: float = 32.0, py: float = 56.0) -> tuple[float, float]:
    start_x = -((WIDTH * TILE) / 2.0)
    start_y = -((HEIGHT * TILE) / 2.0)
    return start_x + tx * TILE + px, start_y + ty * TILE + py


def route_cells() -> set[tuple[int, int]]:
    road: set[tuple[int, int]] = set()
    meadow = (66, 49)
    market = (61, 24)
    watch = (28, 37)
    crossing = (98, 35)
    shrine = (77, 81)
    gate = (116, 24)
    for cx, cy, radius in (meadow + (8,), market + (8,), watch + (7,), crossing + (6,), shrine + (8,), gate + (8,)):
        add_disc(road, cx, cy, radius)
    add_line(road, meadow, market, 2)
    add_line(road, meadow, watch, 2)
    add_line(road, meadow, shrine, 2)
    add_line(road, meadow, crossing, 2)
    add_line(road, crossing, gate, 2)
    add_line(road, shrine, (106, 73), 1)
    add_line(road, (106, 73), gate, 1)
    add_line(road, watch, (20, 63), 1)
    add_line(road, market, (44, 20), 1)
    add_line(road, (84, 43), (98, 56), 1)
    return road


def base_ground() -> list[int]:
    layer = [0] * (WIDTH * HEIGHT)
    for ty in range(HEIGHT):
        for tx in range(WIDTH):
            variants = WILD if h(tx // 5, ty // 5, 3) % 7 == 0 else GRASS
            set_tile(layer, tx, ty, variants[h(tx, ty, 1) % len(variants)])
    return layer


def roads(road: set[tuple[int, int]]) -> list[int]:
    layer = [0] * (WIDTH * HEIGHT)
    for tx, ty in road:
        mask = 0
        if (tx, ty - 1) in road:
            mask |= 1
        if (tx + 1, ty) in road:
            mask |= 2
        if (tx, ty + 1) in road:
            mask |= 4
        if (tx - 1, ty) in road:
            mask |= 8
        set_tile(layer, tx, ty, ROAD_BASE + mask)
    return layer


def details(road: set[tuple[int, int]]) -> list[int]:
    layer = [0] * (WIDTH * HEIGHT)
    for ty in range(2, HEIGHT - 2):
        for tx in range(2, WIDTH - 2):
            value = h(tx, ty, 11) % 100
            if (tx, ty) in road:
                if value < 5:
                    set_tile(layer, tx, ty, DETAIL_PEBBLES)
                elif value < 8:
                    set_tile(layer, tx, ty, DETAIL_CRACKS)
            else:
                if value < 4:
                    set_tile(layer, tx, ty, DETAIL_LEAVES)
                elif value == 7:
                    set_tile(layer, tx, ty, DETAIL_FLOWERS)
    return layer


def stone_layer() -> list[int]:
    layer = [0] * (WIDTH * HEIGHT)
    for tx in range(54, 69):
        for ty in range(18, 31):
            if h(tx, ty, 21) % 3 != 0:
                set_tile(layer, tx, ty, STONE_A if h(tx, ty, 22) % 2 == 0 else STONE_B)
    for tx in range(70, 84):
        for ty in range(74, 88):
            if h(tx, ty, 23) % 4 != 0:
                set_tile(layer, tx, ty, SHRINE_STONE)
    for tx in range(109, 123):
        for ty in range(18, 31):
            if h(tx, ty, 24) % 4 != 0:
                set_tile(layer, tx, ty, MIRROR_STONE)
    return layer


def foreground_layer() -> list[int]:
    layer = [0] * (WIDTH * HEIGHT)
    for tx, ty in [(18, 19), (37, 17), (52, 12), (84, 57), (96, 60), (104, 51)]:
        set_tile(layer, tx, ty, CANOPY)
    return layer


def add_collider(colliders: list[dict[str, object]], name: str, tx: int, ty: int, tw: int, th: int) -> None:
    colliders.append({
        "id": len(colliders) + 1,
        "name": name,
        "x": tx * TILE,
        "y": ty * TILE,
        "width": tw * TILE,
        "height": th * TILE,
    })


def map_bounds(colliders: list[dict[str, object]]) -> None:
    add_collider(colliders, "BoundaryNorth", 0, 0, WIDTH, 1)
    add_collider(colliders, "BoundarySouth", 0, HEIGHT - 1, WIDTH, 1)
    add_collider(colliders, "BoundaryWest", 0, 0, 1, HEIGHT)
    add_collider(colliders, "BoundaryEast", WIDTH - 1, 0, 1, HEIGHT)


def place_tile(layer: list[int], tx: int, ty: int, gid: int) -> None:
    set_tile(layer, tx, ty, gid)


def low_objects(colliders: list[dict[str, object]]) -> list[int]:
    layer = [0] * (WIDTH * HEIGHT)
    map_bounds(colliders)
    for tx, ty in [(61, 45), (95, 34), (74, 78), (113, 24)]:
        place_tile(layer, tx, ty, SIGN)
    for tx, ty in [(58, 44), (64, 26), (96, 34), (73, 83)]:
        place_tile(layer, tx, ty, TABLE)
        add_collider(colliders, f"Table{tx}_{ty}", tx, ty, 1, 1)
    for tx, ty in [(53, 24), (70, 31), (25, 35), (84, 81), (112, 23)]:
        place_tile(layer, tx, ty, ROCK)
        add_collider(colliders, f"Rock{tx}_{ty}", tx, ty, 1, 1)
    for start_tx, start_ty, length, horizontal, name in [
        (52, 31, 8, True, "MarketSouthWall"),
        (72, 20, 7, False, "MarketEastWall"),
        (92, 30, 10, True, "CrossingNorthWall"),
        (91, 39, 10, True, "CrossingSouthWall"),
        (109, 18, 14, True, "MirrorNorthWall"),
        (109, 30, 14, True, "MirrorSouthWall"),
    ]:
        for offset in range(length):
            tx = start_tx + (offset if horizontal else 0)
            ty = start_ty + (0 if horizontal else offset)
            place_tile(layer, tx, ty, WALL_H if horizontal else WALL_V)
        add_collider(colliders, name, start_tx, start_ty, length if horizontal else 1, 1 if horizontal else length)
    return layer


def source(local_id: int, w: int = 64, h_: int = 64) -> tuple[int, int, int, int]:
    col = local_id % 8
    row = local_id // 8
    return col * TILE, row * TILE, w, h_


def prop(prop_id: str, local_id: int, tx: int, ty: int, w: int, h_: int, scale: float = 1.0) -> dict[str, object]:
    sx, sy, sw, sh = source(local_id, w, h_)
    wx, wy = world_from_tile(tx, ty, sw * 0.5, sh - 8)
    return {
        "id": prop_id,
        "texturePath": "assets/environments/overworld_objects_v2.png",
        "sourceX": sx,
        "sourceY": sy,
        "sourceWidth": sw,
        "sourceHeight": sh,
        "worldX": round(wx, 2),
        "worldY": round(wy, 2),
        "pivotX": round(sw * 0.5, 2),
        "pivotY": sh - 8,
        "scale": scale,
        "layer": 50,
        "sortYOffset": 0.0,
    }


def static_props(colliders: list[dict[str, object]]) -> list[dict[str, object]]:
    specs = [
        ("market_ruin_west", 16, 55, 18, 128, 128),
        ("market_ruin_east", 16, 66, 19, 128, 128),
        ("market_ruin_south", 16, 48, 27, 128, 128),
        ("western_watch_tent", 18, 30, 36, 128, 128),
        ("glass_shrine_north", 32, 70, 74, 128, 128),
        ("glass_shrine_east", 32, 81, 75, 128, 128),
        ("glass_shrine_south", 32, 71, 85, 128, 128),
        ("mirror_gate_shrine", 32, 115, 21, 128, 128),
    ]
    out: list[dict[str, object]] = []
    for prop_id, local_id, tx, ty, sw, sh in specs:
        out.append(prop(prop_id, local_id, tx, ty, sw, sh))
        add_collider(colliders, prop_id, tx, ty, sw // TILE, sh // TILE)
    return out


def build_map() -> tuple[dict[str, object], list[dict[str, object]]]:
    road = route_cells()
    colliders: list[dict[str, object]] = []
    props = static_props(colliders)
    objects = low_objects(colliders)
    layers = [
        ("BaseGround", base_ground()),
        ("StoneLandmarks", stone_layer()),
        ("Roads", roads(road)),
        ("Details", details(road)),
        ("Objects", objects),
        ("ForegroundCanopy", foreground_layer()),
    ]
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
        "tilewidth": TILE,
        "tileheight": TILE,
        "nextlayerid": len(layers) + 2,
        "nextobjectid": len(colliders) + 1,
        "tilesets": [
            {
                "firstgid": 1,
                "name": "ground_v2",
                "image": "overworld_tiles_v2.png",
                "imagewidth": 512,
                "imageheight": 256,
                "tilewidth": TILE,
                "tileheight": TILE,
                "columns": 8,
                "tilecount": 32,
                "margin": 0,
                "spacing": 0,
            },
            {
                "firstgid": OBJECT_FIRST_GID,
                "name": "objects_v2",
                "image": "overworld_objects_v2.png",
                "imagewidth": 512,
                "imageheight": 512,
                "tilewidth": TILE,
                "tileheight": TILE,
                "columns": 8,
                "tilecount": 64,
                "margin": 0,
                "spacing": 0,
            },
        ],
        "layers": [
            {
                "type": "tilelayer",
                "id": i + 1,
                "name": name,
                "x": 0,
                "y": 0,
                "width": WIDTH,
                "height": HEIGHT,
                "visible": True,
                "opacity": 1,
                "data": data,
            }
            for i, (name, data) in enumerate(layers)
        ] + [{
            "type": "objectgroup",
            "id": len(layers) + 1,
            "name": "Collisions",
            "visible": True,
            "opacity": 1,
            "draworder": "topdown",
            "objects": colliders,
        }],
    }, props


def main() -> None:
    MAP_OUT.parent.mkdir(parents=True, exist_ok=True)
    PROPS_OUT.parent.mkdir(parents=True, exist_ok=True)
    map_data, props = build_map()
    MAP_OUT.write_text(json.dumps(map_data, indent=2) + "\n", encoding="utf-8")
    PROPS_OUT.write_text(json.dumps({"props": props}, indent=2) + "\n", encoding="utf-8")
    print("Generated assets/environments/overworld_map.json")
    print("Generated data/overworld_props.json")


if __name__ == "__main__":
    main()
