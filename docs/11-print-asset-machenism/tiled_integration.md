# Overworld Tiled Map Integration

**Date:** April 2026
**Target:** Turn-based JRPG Engine (DirectX 11, C++17)

## Overview
This document summarizes the comprehensive integration of an optimized, Tiled-compatible 2D background tilemap renderer for the `OverworldState`. The system was designed to allow seamless bidirectional workflow: mapping levels using the [Tiled Editor](https://www.mapeditor.org/), and loading them perfectly in-engine at runtime without build steps or conversion.

## 1. Engine & Rendering Architecture Updates

### `JsonLoader.h` Update
The raw, hand-rolled JSON parser was upgraded to ingest the standardized Tiled JSON export schema accurately.
* **Metadata Extraction:** Extracting `"tilewidth"`, `"tileheight"`, `"width"`, `"height"` directly from the root layer.
* **Tileset Gid Calculation:** Successfully parses `"tilesets"`, identifying `"firstgid"` and associated `"image"`. This handles Tiled's index-shift architecture (where `0` is transparent and valid tile IDs begin at `firstgid`).
* **Clean Formatting Correction:** Added robust `detail::Trim` functionality to strip leading newlines and spaces off JSON key-value pairs before unquoting them, resolving a critical bug that crashed DirectX texture load arrays (`CreateWICTextureFromFileEx`) with corrupt paths.

### `TileMapRenderer` Class (New Component)
Created an optimized `SpriteBatch` rendering loop intended solely for rendering background geometries beneath entities.
* **Double Projection Fix:** Addressed a critical bug where the camera projection was squashing vertices out of clip space by explicitly utilizing `camera.GetViewMatrix()` instead of `camera.GetViewProjectionMatrix()`. The GPU properly completes the pipeline (World → Screen Pixels → NDC).
* **Z-Order Independence:** Integrated directly into `OverworldState::Render()` before the JRPG characters (`mScene.Render()`) and debug layers to ensure visual depth is respected naturally without relying heavily on Z-buffer writes.
* **Offset Math Fix:** Set optimal center alignment variables: `startX = -((cols * width) / 2)` and `startY = -((rows * height) / 2)`. This centers the origin directly onto the player's spawn point seamlessly.

## 2. Asset Work & Pipelines

### Beautiful Python Sub-Textures (`patches/`)
Bypassed AI rendering anomalies (which attempted to draw UI margins and text tags over spritesheets) by providing a mathematical compilation script `patches/generate_tiles.py` and `combine_tiles.ps1`.
* Dynamically combines mathematically seamless tiles into `assets/environments/overworld_tiles.png`.
* Scales and fits high-resolution noise-textures (grass, dirt, stone) perfectly into single 128x128 bounding boxes.

### Native Tiled Cross-Compatibility
We updated `assets/environments/overworld_map.json` with all critical Tiled metadata:
* `version`, `orientation: "orthogonal"`, `renderorder: "right-down"`.
* Complete `layers` objects and nested `"id"`, `"name"`, `"visible"` components.

With these injected, developers can now literally open `overworld_map.json` in the Tiled editor. Edits inside the Tiled UI instantly update upon "Save", allowing real-time level building alongside DirectX11 testing.
