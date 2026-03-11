# Battle UI Rendering — HP Bars & Text

This document covers all UI rendering systems added to the turn-based battle scene:

| System | Source | Anchor | Description |
|---|---|---|---|
| `HealthBarRenderer` | `src/UI/HealthBarRenderer.h/.cpp` | **Bottom-right** | Player HP bar — 3-layer sprite stack |
| `EnemyHpBarRenderer` | `src/UI/EnemyHpBarRenderer.h/.cpp` | **Top-center** | Up to 3 enemy HP bars with name labels |
| `BattleTextRenderer` | `src/UI/BattleTextRenderer.h/.cpp` | Any pixel | Shared `SpriteFont` wrapper for all HUD text |

---

## Table of Contents

1. [HealthBarRenderer — Player HP Bar](#1-healthbarrenderer--player-hp-bar)
2. [Bottom-Right Anchoring](#2-bottom-right-anchoring)
3. [EnemyHpBarRenderer — Enemy HP Bars](#3-enemyhpbarrenderer--enemy-hp-bars)
4. [Enemy Bar Scale: Height-Driven + Width-Stretch](#4-enemy-bar-scale-height-driven--width-stretch)
5. [Three-Pass Rendering (BG → Fill → Frame)](#5-three-pass-rendering-bg--fill--frame)
6. [Enemy Name Labels (Pass 4)](#6-enemy-name-labels-pass-4)
7. [Slot Layout & Vertical Stride](#7-slot-layout--vertical-stride)
8. [HP Lerp Animation](#8-hp-lerp-animation)
9. [BattleTextRenderer — Shared Font Renderer](#9-battletextrenderer--shared-font-renderer)
10. [Font Generation with MakeSpriteFont](#10-font-generation-with-makespritelont)
11. [BattleState Wiring](#11-battlestate-wiring)
12. [Common Mistakes Reference](#12-common-mistakes-reference)

---

## 1. HealthBarRenderer — Player HP Bar

`HealthBarRenderer` draws a **3-layer sprite stack** for the player's HP widget.

```
Layer 1 (bottom) — UI_hp_background.png
  Full-size dark background quad (the "empty bar" visual).

Layer 2 (middle) — 1×1 white texture, tinted red, SCALED to fill-width × bar-height
  Width = HpBarWidth() * (displayedHP / maxHP)   — clipped on the right
  Position = exact pixel offsets from HP_description.json

Layer 3 (top) — UI_verso_hp.png
  Decorative frame + character portrait.
  Drawn last so the border covers any fill overdraw.
```

### Why three separate SpriteBatch Begin/End blocks?

| Layer | Blend state | Sampler | Reason |
|---|---|---|---|
| Background | `NonPremultiplied` | `LinearClamp` | PNG has standard straight alpha |
| Fill (1×1 quad) | `Opaque` | `PointClamp` | Solid color; nearest-neighbour avoids fringe |
| Frame + portrait | `NonPremultiplied` | `LinearClamp` | PNG has standard straight alpha |

Mixing opaque and alpha-blended draws in one `Begin/End` produces incorrect results
because DirectXTK sort modes operate per-batch, not per-draw.

### HP event subscription

`HealthBarRenderer` subscribes to `"verso_hp_changed"` in `Initialize()` and
**must** unsubscribe in `Shutdown()` to avoid a dangling pointer crash:

```
BattleState::OnEnter()
  └── HealthBarRenderer::Initialize()
        └── EventManager::Subscribe("verso_hp_changed", callback)
              stores ListenerID mHpListenerID

BattleManager resolves a damage action
  └── EventManager::Broadcast("verso_hp_changed", {newHP})
        └── callback: mTargetHP = newHP   (lerp target updated)

BattleState::OnExit()
  └── HealthBarRenderer::Shutdown()
        └── EventManager::Unsubscribe("verso_hp_changed", mHpListenerID)
```

---

## 2. Bottom-Right Anchoring

The player HP bar widget is anchored to the **bottom-right** corner of the screen.

`Render()` computes the widget's top-left pixel once per frame:

```cpp
// textureWidth / textureHeight come from HP_description.json
const float originX = static_cast<float>(mScreenW - mConfig.textureWidth);
const float originY = static_cast<float>(mScreenH - mConfig.textureHeight);
```

All three layers (BG, fill, frame) draw at `(originX, originY)` instead of
`(0, 0)`. Fill offsets from `mConfig.hpBarLeft` / `mConfig.hpBarTop` (JSON pixel
coordinates) are added on top of this origin:

```cpp
const XMFLOAT2 fillPos(
    originX + static_cast<float>(mConfig.hpBarLeft),
    originY + static_cast<float>(mConfig.hpBarTop)
);
```

**Why origin-relative fill coords matter:** if the fill position used absolute
screen pixels, moving the widget would also require editing the JSON. By always
adding `originX/Y`, the JSON stays resolution-independent — the fill area is always
expressed relative to the texture, not the screen.

### At 1280×720 with a 256×256 texture

```
originX = 1280 - 256 = 1024
originY =  720 - 256 =  464
Widget renders at screen rect: (1024, 464) → (1280, 720)
```

---

## 3. EnemyHpBarRenderer — Enemy HP Bars

`EnemyHpBarRenderer` draws **up to 3 HP bars** for the enemy team, stacked
vertically in the **top-center** of the screen.

Each slot contains:
- **Name label** — white text (with 1-pixel black drop shadow) centered above the bar
- **HP bar** — 3-layer sprite (background, red fill, frame/chrome)

### Data flow — polling, not events

```
BattleState::Update(dt)
  ├── mBattle.Update(dt)          ← resolve actions, update HP
  └── for each enemy slot:
        mEnemyHpBar.SetEnemy(i, stats.hp, stats.maxHp, enemy.IsAlive())
        // Names set once in OnEnter, not every frame:
        // mEnemyHpBar.SetEnemyName(i, enemies[i]->GetName())
```

Enemy HP is polled every frame (not event-driven) because `BattleState` already
owns `BattleManager` and can query it directly — no additional event plumbing needed.
Names are set once during `OnEnter()` (they never change mid-battle).

### Assets required

| File | Role |
|---|---|
| `assets/UI/enemy-hp-ui-background.png` | Background layer (Pass 1) |
| `assets/UI/enemy-hp-ui.png` | Frame/chrome layer (Pass 3) |
| `assets/UI/enemy-hp-ui.json` | Fill region pixel coords + texture size |

`enemy-hp-ui.json` format:
```json
{
  "width": 128, "height": 16,
  "health_bar_topleft":     [16, 5],
  "health_bar_bottomright": [111, 9],
  "align": "top-center"
}
```

---

## 4. Enemy Bar Scale: Height-Driven + Width-Stretch

The bar uses **two independent scale axes**:

```cpp
// scaleY: height is always exactly kTargetBarHeight screen pixels
const float scaleY  = kTargetBarHeight / static_cast<float>(mTexH);

// scaleX: width stretches to kBarWidthFactor of the screen (e.g. 60%)
const float scaledW = static_cast<float>(mScreenW) * kBarWidthFactor;
const float scaleX  = scaledW / static_cast<float>(mTexW);
```

| Constant | Value | Effect |
|---|---|---|
| `kTargetBarHeight` | `32.0f` px | Bar height is always 32 screen pixels |
| `kBarWidthFactor`  | `0.6f`    | Bar width = 60% of screen width |

At 1280×720 with a 128×16 texture:

```
scaleY  = 32 / 16     = 2.0   → bar is 32 px tall
scaleX  = 768 / 128   = 6.0   → bar is 768 px wide (60% of 1280)
barPosX = (1280 - 768) / 2    = 256   (centered horizontally)
```

**Why separate axes?**  
Using a single uniform scale (as with `scaleX = screenW * 0.6 / texW`) makes the
bar 96 px tall at 1280×720 — too large. Anchoring height to a fixed pixel value
keeps the HUD compact regardless of resolution, while the horizontal stretch ensures
the bar fills a meaningful portion of the screen.

Fill quad coordinates use the correct axis:

```cpp
fillPosX = barPosX + mHpLeft  * scaleX   // horizontal → scaleX
fillPosY = barPosY + mHpTop   * scaleY   // vertical   → scaleY
fillW    = (mHpRight - mHpLeft) * scaleX * ratio
fillH    = (mHpBottom - mHpTop) * scaleY
```

---

## 5. Three-Pass Rendering (BG → Fill → Frame)

Each frame, `EnemyHpBarRenderer::Render()` opens **three SpriteBatch Begin/End
pairs**. All active slots are drawn inside each pass — not one pass per slot.

```
Pass 1 — NonPremultiplied + LinearClamp
  for each active slot:
    Draw mBgSRV at (barPosX, barPosY)  [background]

Pass 2 — Opaque + PointClamp
  for each active slot:
    Draw mFillSRV (1×1 white, tinted red) at fill position, scaled to ratio

Pass 3 — NonPremultiplied + LinearClamp
  for each active slot:
    Draw mFrameSRV at (barPosX, barPosY)  [frame/chrome, drawn on top]

Pass 4 — BattleTextRenderer (separate SpriteBatch)
  BeginBatch(context)
  for each active slot:
    DrawStringCenteredRaw(shadow at offset +1,+1)
    DrawStringCenteredRaw(text at barCenter, nameY)
  EndBatch()
```

**Why not one pass per slot?**  
Six Begin/End pairs for 2 enemies is worse than 3 passes for all enemies. State
changes (blend + sampler + shader constant buffer) dominate GPU cost at these sizes —
minimizing Begin/End calls is more important than draw call count.

---

## 6. Enemy Name Labels (Pass 4)

Enemy names are rendered as white text centered above each HP bar, with a 1-pixel
black drop shadow for legibility against any background.

```
nameY = barPosY - kNameGapY - kNameLineHeight
```

| Constant | Value | Role |
|---|---|---|
| `kNameLineHeight` | `16.0f` px | Matches arial_16 glyph cap-height |
| `kNameGapY`       | `4.0f` px  | Gap between name bottom and bar top |

### Drop shadow technique

```cpp
// Shadow pass (black, offset +1 pixel on both axes)
DrawStringCenteredRaw(name, centerX + 1.0f, nameY + 1.0f, Colors::Black);

// Main text pass (white, at true position)
DrawStringCenteredRaw(name, centerX, nameY, Colors::White);
```

Two draw calls per active slot inside a **single** `BeginBatch` / `EndBatch` pair —
no extra state changes.

### Injection pattern

`EnemyHpBarRenderer` does not own the `BattleTextRenderer`; it receives a
non-owning pointer:

```cpp
// BattleState::OnEnter():
mEnemyHpBar.SetTextRenderer(&mTextRenderer);

// EnemyHpBarRenderer::Render() — null-safe check:
if (mTextRenderer && mTextRenderer->IsReady()) { ... }
```

This avoids coupling the enemy bar renderer to the text renderer lifecycle, and
allows other HUD elements to share the same `BattleTextRenderer` instance in future.

---

## 7. Slot Layout & Vertical Stride

The top padding and per-slot vertical stride account for both the name label and
the bar itself:

```
kTopPadding  = 36 px     ← room for slot 0's name label above it  
slotStride   = kNameLineHeight + kNameGapY + scaledH + kBarSpacing
             = 16 + 4 + 32 + 6 = 58 px  (at kTargetBarHeight=32)

Slot 0 name:  y = kTopPadding - kNameGapY - kNameLineHeight = 16 px from top
Slot 0 bar:   y = kTopPadding = 36 px from top
Slot 1 name:  y = 36 + 58 - 4 - 16 = 74 px from top
Slot 1 bar:   y = 36 + 58 = 94 px from top
```

---

## 8. HP Lerp Animation

Both `HealthBarRenderer` and `EnemyHpBarRenderer` use the same exponential approach:

```cpp
// Called each frame in Update(dt):
mDisplayedHP[i] += (mTargetHP[i] - mDisplayedHP[i]) * kLerpSpeed * dt;
```

| Renderer | `kLerpSpeed` | Effect |
|---|---|---|
| `HealthBarRenderer` | `5.0f` | ~95% closed in ~0.6 s |
| `EnemyHpBarRenderer` | `4.0f` | ~95% closed in ~0.75 s |

**Seeding on first call:** when `SetEnemy()` (or `SetHP()`) is called with
`hp > 0` for the first time on a slot, `mDisplayedHP` is immediately set to
`hp` so the bar opens at the correct fill level with no lerp-from-zero artifact.

**Frame-rate independence:** the coefficient `kLerpSpeed * dt` ensures the drain
speed looks identical at 30, 60, and 240 fps.

---

## 9. BattleTextRenderer — Shared Font Renderer

`BattleTextRenderer` wraps DirectXTK `SpriteFont` + `SpriteBatch` to draw ASCII
strings at pixel positions. It is owned by `BattleState` and shared (via raw
non-owning pointer) with any renderer that needs text.

### API

```cpp
// Convenience wrappers — each opens its own Begin/End:
void DrawString        (ctx, text, x, y, color = White);
void DrawStringCentered(ctx, text, centerX, y, color = White);

// Low-level batch API — one Begin/End for N strings:
void BeginBatch (ctx);
void DrawStringRaw        (text, x, y, color = White);
void DrawStringCenteredRaw(text, centerX, y, color = White);
void EndBatch();
```

Use `BeginBatch` / `EndBatch` when drawing multiple strings to minimize
SpriteBatch state changes. `EnemyHpBarRenderer` uses this path (2 draws per
active slot — shadow + text — in a single batch).

### Important: NonPremultiplied blend

The font was generated with `/NoPremultiply` (`MakeSpriteFont` flag), so glyph
alpha is stored as straight (not premultiplied). The renderer uses
`CommonStates::NonPremultiplied()` to match.

Using `Opaque` or `AlphaBlend` (premultiplied) with a non-premultiplied atlas
causes glyph edges to appear with a white halo or dark fringe.

### Viewport rebinding

Every `BeginBatch()` / `DrawString()` call first re-binds the viewport:

```cpp
D3D11_VIEWPORT vp = { 0.f, 0.f, (float)mScreenW, (float)mScreenH, 0.f, 1.f };
context->RSSetViewports(1, &vp);
```

`WorldSpriteRenderer` resets the RS viewport after its pass, causing subsequent
`SpriteBatch::Begin()` calls to throw `std::runtime_error` if the viewport is not
re-established first.

---

## 10. Font Generation with MakeSpriteFont

`MakeSpriteFont.exe` is a DirectXTK tool that rasterises a Windows GDI font into a
binary `.spritefont` atlas file loadable by `SpriteFont`.

### Setup

```
tools\MakeSpriteFont.exe  <-- downloaded from DirectXTK GitHub releases (oct2025)
assets\fonts\arial_16.spritefont  <-- generated output
```

### Command used

```
.\tools\MakeSpriteFont.exe "Arial" "assets\fonts\arial_16.spritefont" /FontSize:16 /NoPremultiply
```

| Flag | Purpose |
|---|---|
| `"Arial"` | GDI font name (must be installed on the build machine) |
| `/FontSize:16` | 16pt rasterisation — matches `kNameLineHeight = 16.0f` |
| `/NoPremultiply` | Store straight alpha — required to match `NonPremultiplied` blend |

**Output:** 95 glyphs, 93.7% atlas packing efficiency, CompressedMono format
(smaller file, decoded at load time by DirectXTK directly on the GPU).

### Regenerating the font

To change font size or typeface, re-run the command with new arguments and
rebuild. No code changes are needed — only the `.spritefont` file changes.

> **Note:** The build machine must have the font installed in Windows. CI or build
> agents without GDI font support cannot regenerate the atlas — commit the
> `.spritefont` binary to the repository.

---

## 11. BattleState Wiring

`BattleState` owns all three UI renderers by value:

```cpp
// BattleState.h
HealthBarRenderer   mHealthBar;      // player HP bar (bottom-right)
EnemyHpBarRenderer  mEnemyHpBar;    // enemy HP bars  (top-center, up to 3)
BattleTextRenderer  mTextRenderer;  // shared font renderer for all HUD text
```

### OnEnter() initialization order

```
1. mBattleRenderer.Initialize(...)
2. mHealthBar.Initialize(...)          — player bar + event subscription
3. mTextRenderer.Initialize(...)       — loads arial_16.spritefont
4. mEnemyHpBar.Initialize(...)         — loads both bar PNGs + JSON
5. mEnemyHpBar.SetTextRenderer(&mTextRenderer)
6. for each enemy slot:
     mEnemyHpBar.SetEnemy(i, hp, maxHp, alive)    — seed HP values
     mEnemyHpBar.SetEnemyName(i, enemy.GetName()) — store names (once)
```

### OnExit() shutdown order (reverse)

```
mBattleRenderer.Shutdown()
mHealthBar.Shutdown()      — also unsubscribes "verso_hp_changed"
mEnemyHpBar.Shutdown()
mTextRenderer.Shutdown()
```

### Update() per-frame polling

```cpp
mHealthBar.Update(dt);     // advance player HP lerp

const auto& enemies = mBattle.GetAllEnemies();
for (int i = 0; i < ...; ++i) {
    mEnemyHpBar.SetEnemy(i, stats.hp, stats.maxHp, enemy.IsAlive());
}
mEnemyHpBar.Update(dt);    // advance enemy HP lerps
```

### Render() draw order

```
BattleRenderer::Render   — world-space combatant sprites
HealthBarRenderer::Render — player HP bar (screen-space, bottom-right)
EnemyHpBarRenderer::Render — enemy HP bars + names (screen-space, top-center)
```

---

## 12. Common Mistakes Reference

| Mistake | Symptom | Fix |
|---|---|---|
| Using uniform scale for enemy bars (`screenW * 0.6 / texW`) | Bar is ~96 px tall at 1280×720 — too large and obscures sprites | Use separate `scaleX` and `scaleY`; anchor height to `kTargetBarHeight` |
| Fill X/Y coords both using `scaleX` or both using `scaleY` | Fill quad misaligns from the bar chrome when `scaleX ≠ scaleY` | Horizontal fill coords use `scaleX`; vertical fill coords use `scaleY` |
| Calling `DrawString` inside another Begin/End | `std::logic_error` in DirectXTK debug build | Use `DrawStringRaw` inside `BeginBatch`/`EndBatch` instead |
| Generating font without `/NoPremultiply` then using `NonPremultiplied` blend | Glyph edges appear with dark halo or washed-out look | Regenerate `.spritefont` with `/NoPremultiply` flag |
| Not setting a viewport before SpriteBatch Begin | `std::runtime_error` thrown by DirectXTK | Call `BindViewport()` before every `Begin()` |
| Not calling `SetTextRenderer(&mTextRenderer)` before `Render()` | Enemy name labels never appear (silently skipped due to null check) | Call `SetTextRenderer` in `OnEnter()` after both are initialized |
| Not calling `SetEnemyName(i, ...)` during `OnEnter()` | Name labels appear empty | Call `SetEnemyName` once per slot during battle setup |
| Placing `HealthBarRenderer` at `(0, 0)` | Widget anchored to top-left instead of bottom-right | Compute `originX = screenW - texW`, `originY = screenH - texH` and offset all layers |
| Not unsubscribing EventManager listener in `OnExit` | Crash when next `Broadcast` fires after state is destroyed | Store `ListenerID` and call `Unsubscribe` in `Shutdown()` |
