# Environment & Asset Pipeline Documentation

This document outlines the core architecture and scripting pipeline used to build the AAA-scale Multi-Layer TileMap environment, covering the procedural asset generation, renderer refactoring, and independent-axis physics collision.

## 1. Multi-Layer TileMap Architecture

The game world has been vastly expanded from a single flat terrain map into a fully composited multi-layer system that heavily leverages standard 16-bit JRPG grid rules but scales beautifully for AAA modern games.

### Map Rendering Design
The `TileMapRenderer` class utilizes DirectX 11 `SpriteBatch` configured with `DirectX::SpriteSortMode_Deferred` to loop through multiple hierarchical layers loaded natively from the `.json` Tiled Map export.

- **Layer 1: Ground (`overworld_tiles.png`)**
  This maps the base terrain (perfectly square grass patterns, dirt roads, etc). It renders absolutely first, ensuring no blank skyboxes bleed underneath properties.
- **Layer 2: Objects (`overworld_objects.png`)**
  This handles massive, fully-transparent overlay objects. Elements like massive houses, tables, and rocks are mapped directly as GIDs overlaid precisely on top of the ground textures. Because DirectX 11 gracefully resolves Alpha transparencies, players walking "behind" objects will render correctly based on Z-ordering matrices.

## 2. Procedural AAA Asset Pipeline

Instead of manually dragging and dropping single pixel layers, the project now relies on a scalable Python and Pillow (PIL) build pipeline located in `patches/`.

### AI Asset Compiling (`compile_assets.py`)
1. **High-Definition Sourcing**: High-definition, strictly-orthogonal AI images (like the JRPG Grass and Dirt, and the Massive AAA village house) are procedurally loaded.
2. **Transparency Masking**: The script heuristically isolates flats colors (such as #FF00FF magenta) using flood gradients to isolate the structure from its background natively.
3. **Atlas Construction**: The script dynamically splits or compiles elements exactly to 64x64 modularity.
   - The huge AAA House gets dynamically scaled to `512x512` and occupies a contiguous 8x8 tile block (64 GID assignments!).
4. **Export**: The completely masked and tiled Atlases are flushed directly to `assets/environments/`, ready to be ingested instantly by `DirectX::CreateWICTextureFromFileEx` upon game launch.

### Automated Generative JSON Mapping (`generate_map.py`)
Instead of manually mapping a dense 25x18 world inside software suites, the `generate_map.py` script automatically synthesizes the level structures:
- Procedural dirt paths algorithmically drawn inside the terrain block.
- Calculates and injects multi-tile objects. For instance, the script iterates through an 8x8 matrix (spanning columns `X` and rows `Y`) precisely distributing 64 unique tile GIDs identically across the object array to construct the massive house.

## 3. Sub-Tile Collisions & Separating Axis Physics

A massive AAA Game environment cannot rely on crude "stop-on-tile" physics grids.

### AABB Physics Generation
During the generation step in `generate_map.py`, a third conceptual layer (`objectgroup`) is dynamically constructed.
Instead of the generic 64x64 solid bounds, the script evaluates the true structural requirements of the Massive House.
- **Roof Pass-through**: The hitboxes strictly encapsulate the bottom 3 foundational tiles (spanning `512` width by `192` height), leaving the remaining 260+ pixels of roof space perfectly passable. This lets players traverse "behind" the house smoothly.

### 2-Axis Separating Resolution (`ControllableCharacter.cpp`)
The core physics integration was rewritten to fully untangle `X` and `Y` axis checking.
Rather than completely zeroing out the velocity vector when the player impacts an `ICollider`, the engine evaluates X-axis and Y-axis movements linearly:
1. Steps the X-axis velocity -> performs bounding collision checks. If hitting a wall horizontally, only the X velocity is purged.
2. Steps the Y-axis velocity -> performs collision checks. If hitting the wall vertically, only the Y velocity is purged.

*Result*: The player perfectly and effortlessly "slides" across the massive foundations instead of suffering from generic tile sticking.
