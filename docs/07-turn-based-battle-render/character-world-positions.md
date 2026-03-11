# Character World-Space Positions in Turn-Based Battle

This document explains **how combatant sprites are positioned on screen**
starting from the data-driven formation layout, through the one-time coordinate
conversion, all the way to the final GPU draw call.

> **Refactor note:** Slot positions are no longer stored as `constexpr` arrays
> inside `BattleRenderer.h`.  They live in `data/formations.json` and are loaded
> by `BattleState::OnEnter()`, which resolves world coordinates **before** passing
> them to `BattleRenderer::Initialize()` via `SlotInfo::worldX/Y`.
> `BattleRenderer` never performs a pixel-to-world conversion.

---

## Table of Contents

1. [The Two Coordinate Spaces](#1-the-two-coordinate-spaces)
2. [Formation Layout — data/formations.json](#2-formation-layout--dataformationsjson)
3. [Where the Conversion Happens — BattleState::OnEnter()](#3-where-the-conversion-happens--battlestateonenter)
4. [World-Space Slot Table](#4-world-space-slot-table)
5. [Why BattleRenderer Has No Conversion Math](#5-why-battlerenderer-has-no-conversion-math)
6. [Render() Drawing at World Positions](#6-render-drawing-at-world-positions)
7. [How Camera2D Maps World to Screen](#7-how-camera2d-maps-world-to-screen)
8. [Full Data Flow Diagram](#8-full-data-flow-diagram)
9. [The Camera Controller and Slot Positions](#9-the-camera-controller-and-slot-positions)
10. [Adding or Moving a Slot](#10-adding-or-moving-a-slot)
11. [Common Mistakes](#11-common-mistakes)

---

## 1. The Two Coordinate Spaces

The project uses exactly two coordinate spaces and they must never be confused:

| Space | Origin | Who uses it |
|---|---|---|
| **Screen space** | Top-left corner (0, 0) | Human layout design, art tools, formation JSON |
| **World space** | Screen center (W/2, H/2) | Camera2D, WorldSpriteRenderer::Draw(), BattleCameraController |

Camera2D at default position (0,0) zoom 1.0 places world origin at the screen
center. This is baked into Camera2D::RebuildView() which appends Translate(W/2, H/2).

```
Screen space              World space  (W=1280, H=720)
(   0,   0)  top-left  -> (-640, -360)
( 640, 360)  center    -> (   0,    0)   <- world origin
(1280, 720)  bot-right -> ( 640,  360)
```

Conversion formula (applied once in BattleState::OnEnter()):

```
worldX = battleCenterX + offsetX          (battleCenter = 0,0 = screen center)
worldY = battleCenterY + offsetY
```

The JSON stores the offsets already in world-space units (screenX - W/2, screenY - H/2)
relative to the battle center, so the "conversion" is simply addition.

---

## 2. Formation Layout — data/formations.json

Slot positions are defined in `data/formations.json` as world-space offsets
relative to the battle center:

```json
{
  "player_offsets": [
    { "slot": 0, "offset_x": -320, "offset_y":  40 },
    { "slot": 1, "offset_x": -440, "offset_y": -80 },
    { "slot": 2, "offset_x": -440, "offset_y": 160 }
  ],
  "enemy_offsets": [
    { "slot": 0, "offset_x":  320, "offset_y":  40 },
    { "slot": 1, "offset_x":  440, "offset_y": -80 },
    { "slot": 2, "offset_x":  440, "offset_y": 160 }
  ]
}
```

These values correspond to the original screen-pixel design on a 1280×720 display:

```
 0                   640                  1280
 +-------------------------------------------+  0
 |                                           |
 |   P1 (200,280)              E1 (1080,280) |  280
 |                                           |
 |      P0 (320,400)      E0 (960,400)       |  400
 |                                           |
 |   P2 (200,520)              E2 (1080,520) |  520
 |                                           |
 +-------------------------------------------+  720
        PLAYER SIDE             ENEMY SIDE
        (face right)            (face left, flipX=true)
```

Derivation: offset = screenXY − (W/2, H/2) = screenXY − (640, 360).
Example: P0 screen(320, 400) → world offset (320−640, 400−360) = (−320, 40).

Rule: **to move a slot, edit `data/formations.json` only.**
No C++ recompile is required; the next run picks up the change.

---

## 3. Where the Conversion Happens — BattleState::OnEnter()

`BattleState::OnEnter()` is the **single conversion site** — the only place
in the codebase that reads formation offsets and resolves them to world positions:

```cpp
// Load formation offsets from data/formations.json.
// Non-fatal if missing — formation defaults to zero (all slots at battle center).
JsonLoader::FormationData formation{};
if (!JsonLoader::LoadFormations("data/formations.json", formation))
{
    LOG("[BattleState] WARNING: failed to load formations.json — slots at origin.");
}

const float battleCenterX = 0.0f;  // world origin = screen center
const float battleCenterY = 0.0f;

// Build SlotInfo: world position = battle center + JSON offset (ONE conversion).
std::array<BattleRenderer::SlotInfo, BattleRenderer::kMaxSlots> playerSlots{};
playerSlots[0].worldX = battleCenterX + formation.player[0].offsetX;  // 0 + (-320) = -320
playerSlots[0].worldY = battleCenterY + formation.player[0].offsetY;  // 0 + 40     =  40
// ... (same for all occupied slots)

// BattleRenderer receives pre-computed world positions — no math inside.
mBattleRenderer.Initialize(device, ctx, playerSlots, enemySlots, W, H);
```

After `Initialize()`, `mPlayerWorldX/Y[]` and `mEnemyWorldX/Y[]` inside
`BattleRenderer` are the single authoritative source of world positions for
the rest of the battle session.

---

## 4. World-Space Slot Table

Battle center = (0, 0).  These values come directly from `data/formations.json`.

### Player side (flipX=false)

| Slot | Role       | JSON offset (ox, oy) | World (wx, wy) |
|------|------------|----------------------|----------------|
|  0   | front      | (−320,  40)          | (−320,  40)    |
|  1   | back-top   | (−440, −80)          | (−440, −80)    |
|  2   | back-bot   | (−440, 160)          | (−440, 160)    |

### Enemy side (flipX=true)

| Slot | Role       | JSON offset (ox, oy) | World (wx, wy) |
|------|------------|----------------------|----------------|
|  0   | front      | ( 320,  40)          | ( 320,  40)    |
|  1   | back-top   | ( 440, −80)          | ( 440, −80)    |
|  2   | back-bot   | ( 440, 160)          | ( 440, 160)    |

World-space diagram (origin = screen center):

```
       -640   -440  -320    0    320   440    640
  -80     P1(-440,-80)           E1(440,-80)
   40        P0(-320,40)      E0(320,40)
  160     P2(-440,160)           E2(440,160)
```

The formation is symmetric about the Y axis.

---

## 5. Why BattleRenderer Has No Conversion Math

Before the refactor, `screenX − halfW` appeared in BOTH `BattleRenderer::Initialize()`
AND `BattleRenderer::SetCameraPhase()`.  A slot move required two edits to stay
in sync; one missed update caused the camera to focus on a point different from
where the sprite was drawn.

The new rule eliminates duplication entirely:

```
data/formations.json
  (edit offsets here to move slots — no recompile)
           |
           |  BattleState::OnEnter()  <- ONLY conversion site
           |  worldX = battleCenterX + offsetX
           v
  SlotInfo[].worldX/Y
  (already world-space; BattleRenderer stores them as-is)
           |
           v
  BattleRenderer::mPlayerWorldX/Y[]   mEnemyWorldX/Y[]
           |                                  |
           v                                  v
       Render()                      SetCameraPhase()
       Draw() calls                  SetActorPos()
       (no math)                     SetTargetPos()
                                     (no math)
```

`BattleRenderer` never calls `screenX − halfW`.  It receives world positions
from the caller and uses them verbatim every frame.

---

## 6. Render() Drawing at World Positions

`Render()` reads the stored arrays and passes values directly to `Draw()`.
No arithmetic:

```cpp
Camera2D& cam = mCameraCtrl.GetCamera();

// Player side -- face right
for (int i = 0; i < kMaxSlots; ++i)
{
    if (!mPlayerActive[i]) continue;
    mPlayerRenderers[i].Draw(context, cam,
        mPlayerWorldX[i],   // stored world-space value from SlotInfo
        mPlayerWorldY[i],
        2.0f, false);       // scale=2, flipX=false
}

// Enemy side -- face left (mirror sprite)
for (int i = 0; i < kMaxSlots; ++i)
{
    if (!mEnemyActive[i]) continue;
    mEnemyRenderers[i].Draw(context, cam,
        mEnemyWorldX[i],    // stored world-space value from SlotInfo
        mEnemyWorldY[i],
        2.0f, true);        // scale=2, flipX=true
}
```

`flipX=true` on enemy slots mirrors the sprite so every enemy faces the
player team regardless of how the source PNG was drawn.

---

## 7. How Camera2D Maps World to Screen

For Camera2D at position (cx, cy), zoom z, screen (W, H):

```
pixel = (world - cam) * zoom + (W/2, H/2)
```

Example — OVERVIEW (pos=0,0, zoom=1.0):

```
Slot P0 world(-320, 40):
  pixelX = (-320 - 0) * 1.0 + 640 = 320   matches original design screen px
  pixelY = (  40 - 0) * 1.0 + 360 = 400   matches original design screen py
```

Example — ACTOR_FOCUS (pos=-320,40, zoom=1.6, camera locked on P0):

```
Slot P0 world(-320, 40):
  pixelX = (-320 - (-320)) * 1.6 + 640 = 640   <- screen center
  pixelY = (  40 -    40 ) * 1.6 + 360 = 360   <- screen center
```

The actor lands at screen center — the zoom-in close-up effect.

---

## 8. Full Data Flow Diagram

```
data/formations.json
  (offset_x, offset_y per slot — world-space units relative to battle center)
  |
  | JsonLoader::LoadFormations()
  v
BattleState::OnEnter()
  |
  +-- Resolve world positions (ONE site):
  |     SlotInfo[i].worldX = battleCenterX + formation.player[i].offsetX
  |     SlotInfo[i].worldY = battleCenterY + formation.player[i].offsetY
  |
  +-> BattleRenderer::Initialize(device, ctx, playerSlots, enemySlots, W, H)
        |
        +- 1. Store SlotInfo[].worldX/Y into mPlayerWorldX/Y[], mEnemyWorldX/Y[]
        |      (no conversion — values are already world space)
        |
        +- 2. BattleCameraController::Initialize(W, H)
        |      Camera2D built, phase=OVERVIEW, pos=(0,0), zoom=1.0
        |
        +- 3. For each occupied slot:
               JsonLoader::LoadSpriteSheet()  ->  SpriteSheet
               WorldSpriteRenderer::Initialize()  ->  GPU upload
               WorldSpriteRenderer::PlayClip("idle")

Per frame:

BattleState::Update(dt)
  +-> BattleRenderer::Update(dt)
        +- BattleCameraController::Update(dt)   lerps toward desired phase
        +- WorldSpriteRenderer::Update(dt)      advances animation frame

BattleState::Render()
  +-> BattleRenderer::Render(ctx)
        +- cam = mCameraCtrl.GetCamera()
        +- [player] Draw(ctx, cam, mPlayerWorldX[i], mPlayerWorldY[i], 2, false)
        +- [enemy]  Draw(ctx, cam, mEnemyWorldX [i], mEnemyWorldY [i], 2, true)
```

---

## 9. The Camera Controller and Slot Positions

`BattleRenderer::SetCameraPhase()` feeds slot world positions directly to
`BattleCameraController` — no conversion here:

```cpp
void BattleRenderer::SetCameraPhase(BattleCameraPhase phase,
                                     int actorSlot, int targetSlot)
{
    if (actorSlot  >= 0 && mPlayerActive[actorSlot])
        mCameraCtrl.SetActorPos (mPlayerWorldX[actorSlot],  mPlayerWorldY[actorSlot]);

    if (targetSlot >= 0 && mEnemyActive[targetSlot])
        mCameraCtrl.SetTargetPos(mEnemyWorldX [targetSlot], mEnemyWorldY [targetSlot]);

    mCameraCtrl.SetPhase(phase);
}
```

The three camera phases:

| Phase        | Triggered when          | Camera pos                    | Zoom | Effect                         |
|--------------|-------------------------|-------------------------------|------|--------------------------------|
| OVERVIEW     | Simulation / default    | (0, 0)                        | 1.0  | Whole field visible            |
| ACTOR_FOCUS  | Player picks a skill    | mPlayerWorldX/Y[actorSlot]    | 1.6  | Close-up on acting character   |
| TARGET_FOCUS | Player cycles targets   | target*0.8 + actor*0.2        | 1.0  | Pan to enemy, actor in frame   |

All transitions use exponential-decay lerp (frame-rate independent):

```cpp
mCurrentX    += (desiredX    - mCurrentX)    * kSmoothSpeed * dt;
mCurrentY    += (desiredY    - mCurrentY)    * kSmoothSpeed * dt;
mCurrentZoom += (desiredZoom - mCurrentZoom) * kSmoothSpeed * dt;
// kSmoothSpeed = 5.0f  ->  ~95% gap closed in ~0.6 s at any frame rate
```

---

## 10. Adding or Moving a Slot

### Move an existing slot

Edit `data/formations.json` — no C++ recompile required:

```json
// Move player slot 0 from x=-320 to x=-280:
{ "slot": 0, "offset_x": -280, "offset_y": 40 }
```

`BattleState::OnEnter()` reloads the JSON every time the battle starts.
`Render()`, `SetCameraPhase()`, and `GetPlayerSlotPos()` all pick up the change
automatically — they only read `mPlayerWorldX[]`, never the JSON offsets.

### Override the battle center (future: triggered battles at player position)

```cpp
const float battleCenterX = player->GetWorldX();  // overworld position
const float battleCenterY = player->GetWorldY();
// Formation offsets then place combatants relative to that point.
```

No change to `data/formations.json` or `BattleRenderer` is needed.

### Increase max slots beyond 3

1. Change `kMaxSlots` in `BattleRenderer.h`.
2. Add new offset objects in `data/formations.json` for the new slots.
3. Fill the extra `SlotInfo` entries in `BattleState::OnEnter()`.

All loops are `for (int i = 0; i < kMaxSlots; ++i)` — no other code changes needed.

---

## 11. Common Mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Pass raw screen pixel (960,400) to SlotInfo::worldX/Y | Sprite appears far off-screen (~1600,760) | Use JSON offset (+battleCenter) — result is already world space |
| Add an extra screenX−halfW conversion inside BattleRenderer | After a slot move, sprite and camera disagree | Only BattleState::OnEnter() converts; BattleRenderer stores values verbatim |
| Edit mPlayerWorldX[] directly in BattleRenderer | Position change lost on next battle | Edit data/formations.json; BattleState::OnEnter() rebuilds SlotInfo each time |
| flipX=false for enemy slots | All enemies face the same direction as the player | Pass flipX=true for every enemy Draw() call |
| Pass screen pixels to SetActorPos()/SetTargetPos() | Camera zooms to wrong point | Pass mPlayerWorldX[slot] — already world space |
| Hardcode slot positions as C++ constexpr instead of JSON | Moving a slot requires a recompile | Keep all layout data in data/formations.json |
