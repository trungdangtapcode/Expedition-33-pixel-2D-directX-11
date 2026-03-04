# Turn-Based Battle — Visual Rendering Layer

This document covers the two rendering systems added on top of the existing
battle simulation (`docs/05-turn-based-battle-simple`):

| System | File | What it does |
|---|---|---|
| `BattleRenderer` | `src/Battle/BattleRenderer.h/.cpp` | Draws animated combatant sprites at fixed screen positions |
| `HealthBarRenderer` | `src/UI/HealthBarRenderer.h/.cpp` | Draws the 3-layer HP bar widget in the top-left corner |

Both systems are owned by `BattleState` and live only for the duration of one battle.

---

## Table of Contents

1. [Screen Layout](#1-screen-layout)
2. [BattleRenderer — Combatant Sprites](#2-battlerenderer)
3. [SlotInfo — Data-Driven Slot Assignment](#3-slotinfo)
4. [Camera Coordinate Conversion](#4-camera-coordinate-conversion)
5. [HealthBarRenderer — HP Bar Widget](#5-healthbarrenderer)
6. [HP Lerp / Tween Animation](#6-hp-lerp--tween-animation)
7. [Observer Wiring — verso_hp_changed](#7-observer-wiring)
8. [HealthBarConfig — JSON Layout Parser](#8-healthbarconfig)
9. [sRGB Gamma Fix — WIC_LOADER_IGNORE_SRGB](#9-srgb-gamma-fix)
10. [Render Order Per Frame](#10-render-order-per-frame)
11. [Skeleton JSON Data Bug — Root Cause](#11-skeleton-json-data-bug)
12. [How to Add a New Character Sprite](#12-how-to-add-a-new-character-sprite)
13. [Common Mistakes Reference](#13-common-mistakes-reference)

---

## 1. Screen Layout

The battle scene uses a **staggered diagonal formation** — the front-row slot is
closer to screen center; back-row slots are further out and vertically offset.

```
1280 × 720 screen

 PLAYER SIDE (face right)          ENEMY SIDE (face left — flipX=true)
 ─────────────────────────          ─────────────────────────────────────
 Slot 1  (200, 280)  back-top       Slot 1  (1080, 280)  back-top
 Slot 0  (320, 400)  front          Slot 0  ( 960, 400)  front
 Slot 2  (200, 520)  back-bot       Slot 2  (1080, 520)  back-bot
```

These numbers are **screen-pixel coordinates**.  
`BattleRenderer::Render()` converts them to world space before passing to
`WorldSpriteRenderer::Draw()` — see [Section 4](#4-camera-coordinate-conversion).

---

## 2. BattleRenderer

`BattleRenderer` owns **6 `WorldSpriteRenderer` instances** — 3 for the player
team and 3 for the enemy team — each stored as a value member in a fixed-size array.

```cpp
// src/Battle/BattleRenderer.h  (simplified)
class BattleRenderer
{
public:
    static constexpr int kMaxSlots = 3;

    struct SlotInfo { /* texture path, json path, start clip, occupied flag */ };

    bool Initialize(device, context, playerSlots[3], enemySlots[3], screenW, screenH);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context);
    void Shutdown();

private:
    WorldSpriteRenderer mPlayerRenderers[kMaxSlots];
    WorldSpriteRenderer mEnemyRenderers [kMaxSlots];
    bool mPlayerActive[kMaxSlots];
    bool mEnemyActive [kMaxSlots];
    std::unique_ptr<Camera2D> mCamera;
    int mScreenW, mScreenH;
};
```

### Lifecycle

```
BattleState::OnEnter()
  └── BattleRenderer::Initialize(device, ctx, playerSlots, enemySlots, W, H)
        ├── Build flat Camera2D (pos=0,0, zoom=1)
        ├── For each occupied player slot:
        │     ├── JsonLoader::LoadSpriteSheet(jsonPath, sheet)
        │     ├── WorldSpriteRenderer::Initialize(device, ctx, texturePath, sheet)
        │     └── WorldSpriteRenderer::PlayClip("idle")
        └── For each occupied enemy slot:
              ├── JsonLoader::LoadSpriteSheet(jsonPath, sheet)
              ├── WorldSpriteRenderer::Initialize(device, ctx, texturePath, sheet)
              └── WorldSpriteRenderer::PlayClip("idle")

BattleState::Update(dt)
  └── BattleRenderer::Update(dt)
        └── For each active slot: WorldSpriteRenderer::Update(dt)

BattleState::Render()
  └── BattleRenderer::Render(ctx)
        ├── For each active player slot: WorldSpriteRenderer::Draw(..., flipX=false)
        └── For each active enemy  slot: WorldSpriteRenderer::Draw(..., flipX=true)

BattleState::OnExit()
  └── BattleRenderer::Shutdown()
        └── For each slot: WorldSpriteRenderer::Shutdown()
```

### Current slot assignment (wired in BattleState::OnEnter)

| Team | Slot | Character | Asset |
|---|---|---|---|
| Player | 0 | Verso | `assets/animations/verso.png` + `verso.json` |
| Player | 1 | *(empty)* | — |
| Player | 2 | *(empty)* | — |
| Enemy | 0 | Skeleton A | `assets/animations/skeleton.png` + `skeleton.json` |
| Enemy | 1 | Skeleton B | same atlas — each renderer gets its own `SpriteBatch` |
| Enemy | 2 | *(empty)* | — |

---

## 3. SlotInfo

`SlotInfo` is a plain data struct that `BattleState::OnEnter()` builds before
calling `BattleRenderer::Initialize()`:

```cpp
struct BattleRenderer::SlotInfo {
    std::wstring texturePath;  // L"assets/animations/verso.png"
    std::string  jsonPath;     // "assets/animations/verso.json"
    std::string  startClip;    // "idle"
    bool         occupied = false;
};
```

`occupied = false` → the slot is skipped in every loop (`Initialize`, `Update`,
`Render`, `Shutdown`). This keeps the renderer data-driven without
`if (character == "skeleton")` conditionals anywhere.

To add a third enemy, set `enemySlots[2].occupied = true` and fill the paths — zero
other code changes needed.

---

## 4. Camera Coordinate Conversion

`Camera2D` at `pos=(0,0)`, `zoom=1` maps **world origin `(0,0)` → screen center
`(W/2, H/2)`**, NOT screen top-left. This is because `RebuildView()` appends
`T(W/2, H/2)` to center the world on screen.

To draw at screen pixel `(px, py)`, `BattleRenderer::Render()` converts:

```cpp
const float halfW = mScreenW * 0.5f;
const float halfH = mScreenH * 0.5f;

mPlayerRenderers[i].Draw(
    ctx, *mCamera,
    kPlayerScreenX[i] - halfW,   // world x
    kPlayerScreenY[i] - halfH,   // world y
    scale, flipX
);
```

| Screen pixel | `halfW=640, halfH=360` | World coord passed to Draw |
|---|---|---|
| Verso front (320, 400) | — | (320-640, 400-360) = **(-320, 40)** |
| Skeleton front (960, 400) | — | (960-640, 400-360) = **(320, 40)** |
| Skeleton back-top (1080, 280) | — | (1080-640, 280-360) = **(440, -80)** |

The camera view matrix then adds `(640, 360)` back, landing the sprite exactly at
the target screen pixel. The GPU handles all the math — the caller never computes
NDC manually.

> **Common mistake:** passing raw screen pixels `(960, 400)` directly as world
> coords. The sprite appears at screen pixel `(960+640, 400+360) = (1600, 760)` —
> completely off-screen.

---

## 5. HealthBarRenderer

`HealthBarRenderer` draws a **3-layer sprite stack** in the top-left corner:

```
Layer 1 (bottom) — UI_hp_background.png
  The full 256×256 background quad including the dark "empty bar" visual.

Layer 2 (middle) — 1×1 white texture tinted red, SCALED to fill-width × bar-height
  Clipped on the right by the current HP ratio.
  Width = HpBarWidth() * (displayedHP / maxHP)
  Position = exact pixel coordinates from HP_description.json

Layer 3 (top) — UI_verso_hp.png
  Decorative frame + character portrait overlay.
  Drawn last so the border covers any fill overdraw.
```

The fill is a **runtime-created 1×1 white `ID3D11Texture2D`** uploaded with
`D3D11_USAGE_IMMUTABLE`. It is tinted `(200/255, 50/255, 50/255)` (dark red) via
the `SpriteBatch::Draw` color argument. This approach avoids needing a separate
fill-color PNG and prevents pixel-art distortion — the bar shrinks by trimming the
`destRect` width, not by scaling the source texture.

### Why three separate SpriteBatch Begin/End blocks?

| Layer | Blend state | Reason |
|---|---|---|
| Background | `NonPremultiplied` | PNG has standard alpha channel |
| Fill (1×1 quad) | `Opaque` | Solid color, no alpha blending needed |
| Frame + portrait | `NonPremultiplied` | PNG has standard alpha channel |

Mixing opaque and alpha-blended draws in one `Begin/End` block produces incorrect
results because DirectXTK's sort modes operate per-batch, not per-draw.

---

## 6. HP Lerp / Tween Animation

When Verso takes damage the bar does not snap instantly — it smoothly drains:

```
Event fires → mTargetHP = newValue  (instant)

Each Update(dt):
  mDisplayedHP += (mTargetHP - mDisplayedHP) * kLerpSpeed * dt
  // kLerpSpeed = 5.0f  →  ~95% of the gap closed in ~0.6 seconds
```

`Render()` uses `mDisplayedHP` (not `mTargetHP`) to calculate the fill width:

```cpp
const float ratio     = mDisplayedHP / mMaxHP;            // 0.0 → 1.0
const float fillWidth = mConfig.HpBarWidth() * ratio;     // pixels
```

This is a standard **exponential decay lerp** — frame-rate independent because the
coefficient is multiplied by `dt`. At 60 fps and 240 fps the drain speed looks
identical.

---

## 7. Observer Wiring

`HealthBarRenderer` subscribes to `"verso_hp_changed"` via `EventManager` during
`Initialize()` and unsubscribes in `Shutdown()`:

```
BattleState::OnEnter()
  └── HealthBarRenderer::Initialize()
        └── EventManager::Get().Subscribe("verso_hp_changed", callback)
              → stores ListenerID mHpListenerID

BattleManager resolves damage action
  └── EventManager::Get().Broadcast("verso_hp_changed", {newHP})

HealthBarRenderer callback fires
  └── mTargetHP = newHP   (sets lerp target)

BattleState::OnExit()
  └── HealthBarRenderer::Shutdown()
        └── EventManager::Get().Unsubscribe("verso_hp_changed", mHpListenerID)
```

> **Why unsubscribe in OnExit?**
> If the state is destroyed while the listener is still registered, the callback
> captures `this` (a dangling pointer). The next `Broadcast` call writes through
> the dead pointer → undefined behavior / crash.

---

## 8. HealthBarConfig — JSON Layout Parser

`assets/UI/HP_description.json` describes where the fill area sits inside the
background PNG (in pixels):

```json
{
  "hp_bar_start": [60, 174],
  "hp_bar_end":   [186, 189]
}
```

`HealthBarConfig` parses this and exposes:

```cpp
int HpBarLeft()   const;   // 60   — left edge of fill area
int HpBarTop()    const;   // 174  — top edge of fill area
int HpBarWidth()  const;   // 126  — 186 - 60
int HpBarHeight() const;   // 15   — 189 - 174
```

These pixel coordinates are used in `HealthBarRenderer::Render()` to position the
fill quad exactly over the bar slot in the background PNG — no hardcoded pixel
values in `.cpp` files.

---

## 9. sRGB Gamma Fix — WIC_LOADER_IGNORE_SRGB

All texture loads use `CreateWICTextureFromFileEx` with `WIC_LOADER_IGNORE_SRGB`:

```cpp
CreateWICTextureFromFileEx(
    device, context, path,
    0,                              // maxSize: 0 = no resize
    D3D11_USAGE_DEFAULT,
    D3D11_BIND_SHADER_RESOURCE,
    0, 0,
    WIC_LOADER_IGNORE_SRGB,         // ← critical flag
    nullptr,
    srv.GetAddressOf()
);
```

### Why this flag?

Without it, WIC detects the sRGB ICC profile embedded in PNG files and promotes
the format to `R8G8B8A8_UNORM_SRGB`. The GPU then **linearizes** pixel values
before they reach the UNORM back buffer — all colors appear noticeably darker than
the artist intended (e.g. a vibrant green `#B5E61D` becomes a muted `#76CA03`).

`WIC_LOADER_IGNORE_SRGB` loads the raw 8-bit channel values as-is, bypassing the
format promotion. The pixels on screen exactly match the source PNG.

### Files affected

Every texture loader in the project applies this flag:

| File | Textures loaded |
|---|---|
| `WorldSpriteRenderer.cpp` | Character animation atlases |
| `UIRenderer.cpp` | Screen-space UI sprites |
| `SpriteRenderer.cpp` | World-space background sprites |
| `WorldRenderer.cpp` | World sprite layers |
| `HealthBarRenderer.cpp` | `UI_hp_background.png`, `UI_verso_hp.png` |
| `DebugTextureViewer.cpp` | Debug overlay textures |

---

## 10. Render Order Per Frame

```
GameApp::Render()
  │
  ├── D3DContext::BeginFrame(0.05, 0.05, 0.1)   ← gray-blue clear (GameApp default)
  │
  ├── StateManager::Render()
  │     └── BattleState::Render()
  │           ├── D3DContext::BeginFrame(0.05, 0.05, 0.20)  ← navy-blue clear (battle)
  │           │
  │           ├── BattleRenderer::Render(ctx)
  │           │     ├── [Player slots] WorldSpriteRenderer::Draw (flipX=false)
  │           │     └── [Enemy  slots] WorldSpriteRenderer::Draw (flipX=true)
  │           │
  │           └── HealthBarRenderer::Render(ctx)
  │                 ├── SpriteBatch Begin/Draw/End — background
  │                 ├── SpriteBatch Begin/Draw/End — red fill quad
  │                 └── SpriteBatch Begin/Draw/End — frame + portrait
  │
  └── D3DContext::EndFrame()   ← Present() — exactly once per frame
```

**Rule:** `EndFrame()` (which calls `IDXGISwapChain::Present`) is called **exactly
once per frame** by `GameApp`. `BattleState` calls `BeginFrame()` again to
overwrite the clear color — this is safe because `BeginFrame` only clears the RTV,
it never calls `Present`.

---

## 11. Skeleton JSON Data Bug — Root Cause

During development the skeleton atlas `skeleton.png` was `768×256` pixels (6 columns
× 2 rows of 128×128 frames — 12 cells total, enough for the idle clip only).

The original `skeleton.json` declared `"width": 1536, "height": 512` — dimensions
**double** the actual image. This produced two bugs:

### Bug A — Wrong `framesPerRow` → multiple atlas rows drawn

```
JsonLoader reads:  sheetWidth=1536, frameWidth=128  →  fpr = 12
Actual image:      width=768        frameWidth=128  →  fpr = 6

With fpr=6, idle frame 6..11:
  atlasRow = startRow + (frameIndex / fpr) = 0 + (6/6) = 1
  → srcRect wraps to row 1 of the atlas (y=128..256)
  → correct: that row exists in the 256px-tall image

With the wrong fpr=12, idle frame 0..11:
  atlasRow = 0 + (frameIndex / 12) = 0  for ALL frames
  → always draws row 0 → animation appears to work but...
  → the JSON also declares attack-1/walk/attack-2 on rows 1/2/3
  → with height=512, JsonLoader thinks rows 2/3 exist
  → SpriteBatch samples outside the 256px texture → wraps → shows
     row 0/1 content at unexpected screen positions
```

### Bug B — phantom clips reference non-existent atlas rows

The JSON listed 4 clips (`idle`, `attack-1`, `walk`, `attack-2`) with a total of
35 frames. The actual 768×256 PNG holds only 12 cells. Clips 1–3 referenced rows
that don't exist.

### Fix applied

`assets/animations/skeleton.json` corrected to match the actual PNG:

```json
{
    "width": 768,
    "height": 256,
    "frame_width": 128,
    "frame_height": 128,
    "animations": [
        { "name": "idle", "num_frames": 12, "frame_rate": 8, "loop": true,
          "pivot": [65, 122], "align": "bottom-center" }
    ]
}
```

**Lesson:** the JSON descriptor must always match the actual PNG dimensions. A
mismatch in `width`/`height` silently corrupts `framesPerRow` and `startRow`
calculations — the renderer has no way to detect this at runtime.

---

## 12. How to Add a New Character Sprite

### Step 1 — Add the asset files

Place the sprite sheet PNG and JSON in `assets/animations/`:
```
assets/animations/maelle.png
assets/animations/maelle.json
```

The JSON must match the actual PNG pixel dimensions:
```json
{
    "sprite_name": "maelle",
    "character": "maelle",
    "width": 1024,
    "height": 128,
    "frame_width": 128,
    "frame_height": 128,
    "animations": [
        { "name": "idle", "num_frames": 8, "frame_rate": 8, "loop": true,
          "pivot": [64, 128], "align": "bottom-center" }
    ]
}
```

### Step 2 — Fill the SlotInfo in BattleState::OnEnter

```cpp
// In BattleState::OnEnter() — add Maelle to player slot 1:
playerSlots[1].occupied    = true;
playerSlots[1].texturePath = L"assets/animations/maelle.png";
playerSlots[1].jsonPath    = "assets/animations/maelle.json";
playerSlots[1].startClip   = "idle";
```

That's all. `BattleRenderer::Initialize()` handles the GPU upload, SpriteBatch
creation, and clip wiring automatically.

### Step 3 — (Optional) Wire a per-character HP bar

`HealthBarRenderer` currently shows Verso's HP only. To show a second bar, add a
second `HealthBarRenderer` instance with a different JSON config and event name.

---

## 13. Common Mistakes Reference

| Mistake | Symptom | Fix |
|---|---|---|
| JSON `width`/`height` don't match the PNG | Extra sprites appear, wrong animation frames | Always verify with `PIL.Image.open(path).size` |
| Raw screen pixels passed as world coords to `Draw()` | Sprites appear off-screen or at wrong position | Subtract `screenW/2`, `screenH/2` to convert to world space |
| `WIC_LOADER_IGNORE_SRGB` missing | All textures appear darker than their source PNG | Add the flag to every `CreateWICTextureFromFileEx` call |
| Not unsubscribing `EventManager` listener in `OnExit` | Crash when battle ends and next `Broadcast` fires | Always store `ListenerID` and call `Unsubscribe` in `Shutdown()` |
| `EndFrame()` called inside a state's `Render()` | Double `Present()` → DXGI error or flickering | Only `GameApp::Render()` may call `EndFrame()` |
| Slot `occupied=false` but renderer not guarded | Draw called on uninitialized `WorldSpriteRenderer` → null deref | Always check `mPlayerActive[i]` before calling `Draw()` or `Update()` |
| Scaling the HP fill instead of clipping | Fill bar pixel art looks stretched | Trim `destRect.right`, never change `scale` |
