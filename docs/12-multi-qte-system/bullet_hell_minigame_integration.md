# Undertale Bullet-Hell Integrations

This document covers the comprehensive architectural additions integrating an action-based Bullet-Hell Action phase natively inside the turn-based JRPG system mapping seamlessly alongside the core event loop.

## Overview
To provide deeply engaging combat dynamics similar to *Undertale*, enemies can dynamically trigger real-time dodging phases. This bypasses the typical static "take damage" animation and seamlessly transforms the bottom-third of the UI space into a bounding-box physics arena where the player controls a Hitbox Heart to dodge randomly generated enemy projectiles.

## 1. Dynamic JSON Configuration Layer
The Bullet-Hell system is entirely Data-Driven. By modifying individual `SkillData` files (e.g., `data/skills/skeleton_attack.json`), designers can radically alter the minigame properties dynamically without writing any C++ logic loops!

The parsed `SkillData` properties mapped natively:
- **`bulletHellSupported`**: (bool) The core toggle configuring whether the sub-game phase natively initializes.
- **`bulletTexturePath`**: (string) Dynamically changes bullet appearances (i.e. `assets/UI/crystal_bullet.png`). 
- **`bulletRadius`**: (float) Controls collision bounds AND sprite rendering scales uniformly.
- **`bulletSpeed`**: (float) Base directional physics velocity natively (randomized up to 20%).
- **`bulletSpawnRate`**: (float) Timer mapped bounds spawning `X` bullets per chronological second.
- **`bulletInvincibilityDuration`**: (float) Native i-frames. Configures exactly how many seconds the player natively ignores damage after eating a bullet.
- **`bulletDamageScaling`**: (float) Maps Minigame Damage calculations to fractions of a physical strike. (e.g., `0.5` causes 50% physical hit tracking).

## 2. Physics & Mathematics Pipeline
The `BulletHellAction` encapsulates the physics logic decoupled cleanly from Turn-based systems. It hooks natively into standard `dt` update polling without freezing state inputs natively!

### Random Trigonometric Angle Vectors
Previously locking onto grid axes mappings natively heavily restricted patterns. 
The updated `SpawnBullet()` algorithm utilizes pure Math mapping:
1. Identifying the location of the Heart coordinate natively (`mHeartX`, `mHeartY`).
2. Adding a randomized ± jitter constant forcing intersecting curves over pure tracking.
3. Calculating directional velocity natively using: 
   ```cpp
   float dx = targetX - b.x;
   float dy = targetY - b.y;
   b.angle = std::atan2(b.vy, b.vx);
   ```
   This mathematically stores exact direction trajectories securely down to the structural payloads dynamically natively.

## 3. UI Renderer & Visual Feedback (AAA Polish)
The rendering executes parallel mapped exclusively inside `BattleBulletHellRenderer` decoupled from mathematical physics loops!

### Asset Integration
The UI natively loads generated Python Asset files (such as `bullet_hell_frame.png` structured cleanly around a natively bounded Nine-Slice dialog). 
The Heart object integrates naturally scaled mathematically downwards (rendering pivot `0.5f`) allowing a much cleaner visual representation while significantly restricting the invisible computational Hitbox securely to `6.0f` creating fair, extremely precise dodge mappings natively!

### Angle Rotation & Transparency Mathematics
When traversing the arrays in `mSpriteBatch->Draw`:
- **Directional Velocity**: `b.angle` mathematically locks the bullet's rotational Graphic to identically align with its physical velocity vector structurally dynamically!
- **Invincibility Blinking**: If the player is hit and structurally enters their configured JSON i-frame window (`invincibilityTimer > 0.0f`), the Renderer maps a Sine Wave algorithm naturally generating AAA-industry-standard transparency blinking natively natively smoothly transitioning Alpha arrays cleanly:
  ```cpp
  alpha = 0.35f + 0.65f * std::abs(std::sin(mLastPayload.invincibilityTimer * 35.0f));
  ```

## 4. `EventManager` System Syncing 
To maintain world-consistency natively, the Action phase pushes event messages safely avoiding blocking processes.
1. When a player consumes damage in the minigame, a Native Hook pushes a `DamageTakenPayload` natively syncing traditional particle hits effortlessly mappings properly.
2. Directly after, it broadcasts `PlayAnimPayload(mAttacker, CombatantAnim::Attack)`. This gracefully commands the enemy logically standing in the physical background space to immediately physically perform an active attack swing perfectly identically overlapping the bullet striking the minigame box!

## 5. Architectural File Modification Breakdown
Below tracks the precise files built or explicitly modified integrating these features securely:

### Core New Entities
- **`src/Battle/BulletHellAction.cpp` & `.h`**: The new `IAction` subclass driving physics, math, input-loop bypass, and event payload packaging.
- **`src/UI/BattleBulletHellRenderer.cpp` & `.h`**: The new rendering container cleanly loading Sprites over primitives handling independent scaling factors mathematically.
- **`docs/12-multi-qte-system/bullet_hell_minigame_integration.md`**: This current comprehensive systems blueprint documentation natively.

### Existing Architecture Modified
- **`data/skills/skeleton_attack.json`**: Appended scaling constants natively testing dynamic arrays.
- **`src/Utils/JsonLoader.h`**: Struct fields mapped handling the data injection natively.
- **`src/Battle/AttackSkill.cpp`**: Trigger logic updated pushing parameter states correctly to initialize `BulletHellAction`.
- **`src/Battle/BattleEvents.h`**: Struct maps natively including rotation variables and timer counts!
- **`src/States/BattleState.cpp` & `.h`**: Render execution hooked natively bridging UI.
- **`build_src_static.bat`**: Compiling maps updating tracking new C++ execution routines explicitly!
