# Character World-Space Positions in Turn-Based Battle

This document explains **how combatant sprites are positioned on screen** 
starting from the human-readable pixel layout, through the one-time coordinate
conversion, all the way to the final GPU draw call.

---

## Table of Contents

1. [The Two Coordinate Spaces](#1-the-two-coordinate-spaces)
2. [Screen-Pixel Design Layout](#2-screen-pixel-design-layout)
3. [The One-Time Conversion in Initialize()](#3-the-one-time-conversion-in-initialize)
4. [World-Space Slot Table](#4-world-space-slot-table)
5. [Why the Conversion Lives Only in Initialize()](#5-why-the-conversion-lives-only-in-initialize)
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
| **Screen space** | Top-left corner (0, 0) | Human layout design, art tools |
| **World space** | Screen center (W/2, H/2) | Camera2D, WorldSpriteRenderer::Draw(), BattleCameraController |

Camera2D at default position (0,0) zoom 1.0 places world origin at the screen
center. This is baked into Camera2D::RebuildView() which appends Translate(W/2, H/2).

```
Screen space              World space  (W=1280, H=720)
(   0,   0)  top-left  -> (-640, -360)
( 640, 360)  center    -> (   0,    0)   <- world origin
(1280, 720)  bot-right -> ( 640,  360)
```

Conversion formula:

```
worldX = screenX - screenW / 2
worldY = screenY - screenH / 2
```

---

## 2. Screen-Pixel Design Layout

The battle formation is a staggered diagonal. The layout is stored as constexpr
constants in BattleRenderer.h (used ONLY inside Initialize()):

```cpp
static constexpr float kPlayerScreenX[kMaxSlots] = { 320.0f,  200.0f,  200.0f };
static constexpr float kPlayerScreenY[kMaxSlots] = { 400.0f,  280.0f,  520.0f };
static constexpr float kEnemyScreenX [kMaxSlots] = { 960.0f, 1080.0f, 1080.0f };
static constexpr float kEnemyScreenY [kMaxSlots] = { 400.0f,  280.0f,  520.0f };
```

Visualised on a 1280x720 screen:

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

Rule: to move a slot, edit the k*ScreenX/Y constants here only.
Initialize() recomputes the world arrays automatically.

---

## 3. The One-Time Conversion in Initialize()

BattleRenderer::Initialize() converts all slots to world space exactly once:

```cpp
const float halfW = static_cast<float>(screenW) * 0.5f;   // 640.0
const float halfH = static_cast<float>(screenH) * 0.5f;   // 360.0

for (int i = 0; i < kMaxSlots; ++i)
{
    mPlayerWorldX[i] = kPlayerScreenX[i] - halfW;
    mPlayerWorldY[i] = kPlayerScreenY[i] - halfH;
    mEnemyWorldX [i] = kEnemyScreenX [i] - halfW;
    mEnemyWorldY [i] = kEnemyScreenY [i] - halfH;
}
```

After this loop, the four instance arrays are the single authoritative
source of world-space positions for the entire BattleRenderer lifetime.

---

## 4. World-Space Slot Table

W=1280, H=720 -> halfW=640, halfH=360

### Player side (flipX=false)

| Slot | Role       | Screen (px,py)  | World (wx,wy) |
|------|------------|-----------------|----------------|
|  0   | front      | (320, 400)      | (-320,  40)    |
|  1   | back-top   | (200, 280)      | (-440, -80)    |
|  2   | back-bot   | (200, 520)      | (-440, 160)    |

### Enemy side (flipX=true)

| Slot | Role       | Screen (px,py)  | World (wx,wy) |
|------|------------|-----------------|----------------|
|  0   | front      | ( 960, 400)     | ( 320,  40)    |
|  1   | back-top   | (1080, 280)     | ( 440, -80)    |
|  2   | back-bot   | (1080, 520)     | ( 440, 160)    |

World-space diagram (origin = screen center):

```
       -640   -440  -320    0    320   440    640
  -80     P1(-440,-80)           E1(440,-80)
   40        P0(-320,40)      E0(320,40)
  160     P2(-440,160)           E2(440,160)
```

The formation is symmetric about the Y axis.

---

## 5. Why the Conversion Lives Only in Initialize()

Before the refactor, screenX - halfW appeared in BOTH Render() AND
SetCameraPhase(). If the screen size changed or a slot moved, both sites had
to be updated in sync  one missed edit meant the camera pointed at a
different position than where the sprite was drawn.

The fixed rule:

  ONE conversion site, zero duplication.
  Initialize() converts once -> stores world coords.
  Every method after that reads the stored values directly.

```
  kPlayerScreenX/Y   kEnemyScreenX/Y
  (constexpr in header -- edit here to move slots)
           |
           |  Initialize()  <- ONLY conversion site
           v
  mPlayerWorldX/Y[]   mEnemyWorldX/Y[]
  (instance arrays -- world space)
       |                  |
       v                  v
   Render()         SetCameraPhase()
   Draw() calls     SetActorPos()
   (no math)        SetTargetPos()
                    (no math)
```

---

## 6. Render() Drawing at World Positions

Render() reads the precomputed arrays and passes values directly to Draw().
No arithmetic at all:

```cpp
Camera2D& cam = mCameraCtrl.GetCamera();

// Player side -- face right
for (int i = 0; i < kMaxSlots; ++i)
{
    if (!mPlayerActive[i]) continue;
    mPlayerRenderers[i].Draw(context, cam,
        mPlayerWorldX[i],   // already world space
        mPlayerWorldY[i],
        2.0f, false);       // scale=2, flipX=false
}

// Enemy side -- face left (mirror sprite)
for (int i = 0; i < kMaxSlots; ++i)
{
    if (!mEnemyActive[i]) continue;
    mEnemyRenderers[i].Draw(context, cam,
        mEnemyWorldX[i],    // already world space
        mEnemyWorldY[i],
        2.0f, true);        // scale=2, flipX=true
}
```

flipX=true on enemy slots mirrors the sprite so every enemy faces the
player team regardless of how the source PNG was drawn.

---

## 7. How Camera2D Maps World to Screen

For Camera2D at position (cx, cy), zoom z, screen (W, H):

```
pixel = (world - cam) * zoom + (W/2, H/2)
```

Example -- OVERVIEW (pos=0,0, zoom=1.0):

```
Slot P0 world(-320, 40):
  pixelX = (-320 - 0) * 1.0 + 640 = 320   matches design screen px
  pixelY = (  40 - 0) * 1.0 + 360 = 400   matches design screen py
```

Example -- ACTOR_FOCUS (pos=-320,40, zoom=1.6, camera on P0):

```
Slot P0 world(-320, 40):
  pixelX = (-320 - (-320)) * 1.6 + 640 = 640   <- screen center
  pixelY = (  40 -    40 ) * 1.6 + 360 = 360   <- screen center
```

The actor lands at screen center -- the zoom-in close-up effect.

---

## 8. Full Data Flow Diagram

```
BattleState::OnEnter()
  |
  +-> BattleRenderer::Initialize(device, ctx, playerSlots, enemySlots, W, H)
        |
        +- 1. One-time world coord conversion
        |      kPlayerScreenX/Y[i]  --(halfW/H)-->  mPlayerWorldX/Y[i]
        |      kEnemyScreenX/Y [i]  --(halfW/H)-->  mEnemyWorldX/Y [i]
        |
        +- 2. BattleCameraController::Initialize(W, H)
        |      Camera2D built, phase=OVERVIEW, pos=(0,0), zoom=1.0
        |
        +- 3. For each occupied slot
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

BattleRenderer::SetCameraPhase() feeds slot world positions directly to
BattleCameraController -- no conversion here either:

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

Edit only the constexpr constants in BattleRenderer.h:

```cpp
// Move player slot 0 x from 320 to 400:
static constexpr float kPlayerScreenX[kMaxSlots] = { 400.0f, 200.0f, 200.0f };
```

Initialize() recomputes mPlayerWorldX[] from the new value on next run.
Render(), SetCameraPhase(), GetPlayerSlotPos() all pick up the change
automatically -- they read mPlayerWorldX[], not the constants.

### Increase max slots beyond 3

1. Change kMaxSlots to the new count in BattleRenderer.h
2. Extend the four k*ScreenX/Y[] arrays with new pixel positions
3. Fill the extra SlotInfo entries in BattleState::OnEnter()

All loops are for (int i = 0; i < kMaxSlots; ++i) -- no other changes needed.

---

## 11. Common Mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Pass raw screen pixel (960,400) to Draw() as world pos | Sprite appears at (1600,760) off-screen | Read mEnemyWorldX/Y[i] -- already world space |
| Add a second screenX-halfW conversion in Render() | After slot move, sprite and camera point at different places | One conversion site only: Initialize(). Never add another. |
| flipX=false for enemy slots | All enemies face the same direction as the player | Pass flipX=true for every enemy Draw() call |
| Edit mPlayerWorldX[] directly instead of kPlayerScreenX[] | Position change lost on next Initialize() call | Edit constexpr constants; Initialize() rebuilds the arrays |
| Pass screen pixels to SetActorPos()/SetTargetPos() | Camera zooms to wrong point | Pass mPlayerWorldX[slot] -- already correct |
