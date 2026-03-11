# 08 — The Bridge Between Overworld and Turn-Based Battle

This document explains the full pipeline that connects an overworld enemy to the turn-based battle system. After reading this your team will know exactly which files to touch — and which ones to leave alone — when adding a new monster.

---

## Table of Contents

1. [Concept: The "Bridge" Pattern](#1-concept-the-bridge-pattern)
2. [Data Flow Diagram](#2-data-flow-diagram)
3. [Key Files at a Glance](#3-key-files-at-a-glance)
4. [The EnemyEncounterData Struct](#4-the-enemyencounterdata-struct)
5. [Enemy JSON File Schema](#5-enemy-json-file-schema)
6. [Step-by-Step: Adding a New Monster](#6-step-by-step-adding-a-new-monster)
7. [How PlayState Detects Combat Proximity](#7-how-playstate-detects-combat-proximity)
8. [How BattleState Receives the Enemy](#8-how-battlestate-receives-the-enemy)
9. [The Iris Circle Reveal Transition](#9-the-iris-circle-reveal-transition)
10. [Formation Slots (where enemies stand in battle)](#10-formation-slots)
11. [Common Mistakes](#11-common-mistakes)
12. [FAQ](#12-faq)

---

## 1. Concept: The "Bridge" Pattern

The overworld and the battle system are completely separated `IGameState` classes. They share **zero direct references** to each other. The only object that crosses the boundary is a plain data struct called `EnemyEncounterData`.

```
┌─────────────────────────────┐        ┌─────────────────────────────┐
│          PlayState          │        │         BattleState          │
│                             │        │                              │
│  OverworldEnemy             │  DATA  │  BattleRenderer              │
│    ├── mData (struct) ──────┼───────►│    └── enemySlots[0]         │
│    ├── WorldSpriteRenderer  │  ONLY  │         ├── texturePath      │
│    └── IsPlayerNearby()     │        │         ├── jsonPath         │
│                             │        │         ├── startClip        │
│  IrisTransitionRenderer     │        │  IrisTransitionRenderer      │
│  (owned by PlayState)       │        │  (owned by BattleState)      │
└─────────────────────────────┘        └─────────────────────────────┘
```

**The rule:** `PlayState` and `BattleState` do **not** include each other's headers for logic. The `EnemyEncounterData` struct is the only thing that transfers between them. Both sides can evolve independently as long as the struct stays compatible.

---

## 2. Data Flow Diagram

```
data/enemies/goblin.json
        │
        │  JsonLoader::LoadEnemyEncounterData()
        ▼
EnemyEncounterData  (loaded once in PlayState::OnEnter)
        │
        │  SceneGraph::Spawn<OverworldEnemy>(..., data, worldX, worldY, camera)
        ▼
OverworldEnemy  (owns a copy of EnemyEncounterData)
  ├── WorldSpriteRenderer — renders the goblin sprite in the overworld
  └── IsPlayerNearby(px, py) — radius check each frame
        │
        │  Player presses B + is within contactRadius
        ▼
PlayState copies  mPendingEncounter = enemy->GetEncounterData()
        │
        │  StateManager::PushState( BattleState(d3d, mPendingEncounter) )
        ▼
BattleState::OnEnter()
  └── enemySlots[0].texturePath  ← from EnemyEncounterData
      enemySlots[0].jsonPath     ← from EnemyEncounterData
      enemySlots[0].startClip   ← from EnemyEncounterData
      BattleManager loads stats  ← hp/atk/def/spd from EnemyEncounterData
        │
        │  IrisTransitionRenderer::StartOpen()
        ▼
  Circle expands outward → battle scene revealed
```

---

## 3. Key Files at a Glance

| File | What it does | Edit for new monster? |
|---|---|---|
| `data/enemies/*.json` | All stats, paths, and the full `battleParty` array | **YES — create new file** |
| `assets/animations/*.png` | Sprite sheet texture | **YES — add new texture** |
| `assets/animations/*.json` | Sprite sheet clip descriptors | **YES — add new JSON** |
| `src/Battle/EnemyEncounterData.h` | `EnemySlotData` + `EnemyEncounterData` structs | No — only if adding new fields |
| `src/Entities/OverworldEnemy.h/.cpp` | The overworld entity class | No — generic, works for all monsters |
| `src/Utils/JsonLoader.h` | Loads the enemy JSON + battleParty array | No — already handles all fields |
| `src/States/PlayState.cpp` | Spawns enemies, detects B-press | **YES — add spawn call** |
| `src/States/BattleState.cpp` | Fills battle slots from battleParty | No — already fully data-driven |
| `src/Renderer/IrisTransitionRenderer.h/.cpp` | Circle reveal transition | No |
| `data/formations.json` | Battle slot world positions | No |

---

## 4. The EnemyEncounterData Struct

**File:** `src/Battle/EnemyEncounterData.h`

There are **two** structs. Both are plain C++ with no methods.

### EnemySlotData — one combatant in the battle party

```cpp
struct EnemySlotData
{
    // ---- Sprite (independent from the overworld sprite) ----
    std::wstring texturePath;         // L"assets/animations/goblin.png"
    std::string  jsonPath;            // "assets/animations/goblin.json"
    std::string  idleClip;            // "idle"

    // ---- Battle stats ----
    int hp  = 0;
    int atk = 0;
    int def = 0;
    int spd = 0;

    // ---- Battle camera ----
    float cameraFocusOffsetY = -128.0f;  // lifts focus from feet to chest
};
```

### EnemyEncounterData — the full encounter package

```cpp
struct EnemyEncounterData
{
    // ---- Overworld identity + sprite ----
    std::string  name;           // "Goblin Scout"  — shown in enemy HP bar
    std::wstring texturePath;    // overworld sprite  L"assets/animations/goblin.png"
    std::string  jsonPath;       // overworld sprite sheet JSON
    std::string  idleClip;       // overworld idle clip
    float contactRadius = 80.0f; // world-pixel radius for "near enough to fight"

    // ---- Battle party (1–3 slots) ----
    // Each entry appears as one enemy combatant in battle.
    // The overworld sprite (above) and the battle party do NOT need to match:
    // a single overworld icon can trigger a 3-enemy group fight.
    std::vector<EnemySlotData> battleParty;
};
```

**Key design:** the overworld entity and the battle party are **decoupled**. A tiny goblin icon in the overworld can trigger a `battleParty` of `[Goblin, Skeleton, Skeleton]`. Both the overworld sprite and each party member's sprite are fully data-driven.

---

## 5. Enemy JSON File Schema

**Location:** `data/enemies/<name>.json`

```json
{
  "name":         "Goblin Scout",
  "texturePath":  "assets/animations/goblin.png",
  "jsonPath":     "assets/animations/goblin.json",
  "idleClip":     "idle",
  "contactRadius": 85.0,

  "battleParty": [
    {
      "texturePath":        "assets/animations/goblin.png",
      "jsonPath":           "assets/animations/goblin.json",
      "idleClip":           "idle",
      "hp": 40, "atk": 8, "def": 3, "spd": 7,
      "cameraFocusOffsetY": -96.0
    },
    {
      "texturePath":        "assets/animations/skeleton.png",
      "jsonPath":           "assets/animations/skeleton.json",
      "idleClip":           "idle",
      "hp": 50, "atk": 10, "def": 5, "spd": 5,
      "cameraFocusOffsetY": -128.0
    }
  ]
}
```

### Top-level fields (overworld only)

| Field | Type | Description |
|---|---|---|
| `name` | string | Display name shown in the enemy HP bar during battle |
| `texturePath` | string | Overworld sprite texture path (ASCII, auto-converted to `wstring`) |
| `jsonPath` | string | Overworld sprite sheet JSON |
| `idleClip` | string | Overworld animation clip name |
| `contactRadius` | float | Overworld collision radius in world pixels. 80–120 is typical. |

### `battleParty` array (1–3 objects)

Each object in the array becomes one enemy in the battle. Slot indices map directly to `data/formations.json` enemy offsets: `[0]` = front, `[1]` = back-top, `[2]` = back-bottom.

| Field | Type | Description |
|---|---|---|
| `texturePath` | string | Texture for this battle combatant (can differ from the overworld sprite) |
| `jsonPath` | string | Sprite sheet JSON for this combatant |
| `idleClip` | string | Starting animation clip |
| `hp` | int | Max HP / starting HP |
| `atk` | int | Attack stat |
| `def` | int | Defense stat |
| `spd` | int | Speed — determines turn order |
| `cameraFocusOffsetY` | float | Lifts camera from feet to chest. Formula: `-(frameHeight × renderScale) / 2` |

> **Never hardcode stats in C++ source.** Every numeric value must come from this JSON.

---

## 6. Step-by-Step: Adding a New Monster

### Step 1 — Prepare the sprite sheet

Place files in:
```
assets/animations/goblin.png       ← the sprite sheet texture
assets/animations/goblin.json      ← the clip descriptor
```

The sprite sheet JSON format is the same used by Verso and Skeleton. It describes:
- `frameWidth`, `frameHeight` — dimensions of one frame in pixels
- `animations` — array of clip objects with `name`, `startFrame`, `frameCount`, `fps`, `loop`, `align`

See `assets/animations/skeleton.json` as a reference template.

---

### Step 2 — Create the enemy data file

Create `data/enemies/goblin.json`. The `battleParty` array defines how many enemies appear when combat starts:

**Single enemy (1-on-1 fight):**
```json
{
  "name": "Goblin",
  "texturePath": "assets/animations/goblin.png",
  "jsonPath": "assets/animations/goblin.json",
  "idleClip": "idle",
  "contactRadius": 85.0,

  "battleParty": [
    {
      "texturePath": "assets/animations/goblin.png",
      "jsonPath": "assets/animations/goblin.json",
      "idleClip": "idle",
      "hp": 40, "atk": 8, "def": 3, "spd": 7,
      "cameraFocusOffsetY": -96.0
    }
  ]
}
```

**Group encounter (goblin + 2 skeletons):**
```json
{
  "name": "Goblin Scout",
  "texturePath": "assets/animations/goblin.png",
  "jsonPath": "assets/animations/goblin.json",
  "idleClip": "idle",
  "contactRadius": 90.0,

  "battleParty": [
    {
      "texturePath": "assets/animations/goblin.png",
      "jsonPath": "assets/animations/goblin.json",
      "idleClip": "idle",
      "hp": 40, "atk": 8, "def": 3, "spd": 7,
      "cameraFocusOffsetY": -96.0
    },
    {
      "texturePath": "assets/animations/skeleton.png",
      "jsonPath": "assets/animations/skeleton.json",
      "idleClip": "idle",
      "hp": 50, "atk": 10, "def": 5, "spd": 5,
      "cameraFocusOffsetY": -128.0
    },
    {
      "texturePath": "assets/animations/skeleton.png",
      "jsonPath": "assets/animations/skeleton.json",
      "idleClip": "idle",
      "hp": 30, "atk": 7, "def": 3, "spd": 6,
      "cameraFocusOffsetY": -128.0
    }
  ]
}
```

> The `texturePath`/`jsonPath` at the **top level** control what the overworld entity looks like.  
> The `texturePath`/`jsonPath` **inside each `battleParty` object** control what appears in battle.  
> They can be completely different files.

---

### Step 3 — Spawn the enemy in PlayState

Open `src/States/PlayState.cpp`, find the enemy spawn block inside `OnEnter()`, and add a spawn call:

```cpp
EnemyEncounterData goblinData{};
if (JsonLoader::LoadEnemyEncounterData("data/enemies/goblin.json", goblinData))
{
    OverworldEnemy* goblin = mScene.Spawn<OverworldEnemy>(
        device, context,
        goblinData,
        -100.0f,   // worldX
        200.0f,    // worldY
        mCamera.get()
    );

    if (goblin)
        mOverworldEnemies.push_back(goblin);
}
```

**That is all the code you need to write.** The rest is automatic:
- `OverworldEnemy` renders using the top-level `texturePath`/`jsonPath` from the JSON.
- When B is pressed, the full `EnemyEncounterData` (including `battleParty`) is passed to `BattleState`.
- `BattleState` loops over `battleParty` and fills slots 0–2 automatically.

### Step 4 — Build and run

```
.\build_src_static.bat 2>&1
```

No other files need to change.

---

## 7. How PlayState Detects Combat Proximity

Every frame `PlayState::Update(dt)` runs this logic:

```cpp
// 1. Only check if B was just pressed (edge detection) and iris is idle.
const bool bDown    = (GetAsyncKeyState('B') & 0x8000) != 0;
const bool bPressed = bDown && !mBWasDown;   // true for exactly one frame
mBWasDown = bDown;

if (bPressed && mIris.IsIdle() && mPlayer)
{
    const float px = mPlayer->GetX();
    const float py = mPlayer->GetY();

    // 2. Search for the first enemy within contactRadius.
    for (OverworldEnemy* enemy : mOverworldEnemies)
    {
        if (enemy && enemy->IsAlive() && enemy->IsPlayerNearby(px, py))
        {
            // 3. Copy the encounter data BEFORE any state change.
            mPendingEncounter = enemy->GetEncounterData();

            // 4. Push BattleState immediately — the iris opens inside BattleState.
            StateManager::Get().PushState(
                std::make_unique<BattleState>(D3DContext::Get(), mPendingEncounter));
            break;
        }
    }
}
```

### IsPlayerNearby — the proximity test

Inside `OverworldEnemy`:

```cpp
bool OverworldEnemy::IsPlayerNearby(float px, float py) const
{
    // Squared-distance check avoids a sqrt() call every frame.
    const float dx    = px - mWorldX;
    const float dy    = py - mWorldY;
    const float distSq = dx * dx + dy * dy;
    const float r      = mData.contactRadius;
    return distSq <= r * r;
}
```

If the player ever feels "too far away when triggering" or "triggers from too far away", adjust `contactRadius` in the JSON file — no code change needed.

---

## 8. How BattleState Receives the Enemy

`BattleState`'s constructor stores the encounter data by value:

```cpp
BattleState::BattleState(D3DContext& d3d, EnemyEncounterData encounter)
    : mD3D     (d3d)
    , mEncounter(std::move(encounter))   // full party copied — independent lifetime
{}
```

Then in `OnEnter()`, the enemy slots are filled by looping over `battleParty`:

```cpp
if (!mEncounter.battleParty.empty())
{
    for (int i = 0;
         i < static_cast<int>(mEncounter.battleParty.size())
         && i < BattleRenderer::kMaxSlots;   // max 3 slots
         ++i)
    {
        const EnemySlotData& sd      = mEncounter.battleParty[i];
        enemySlots[i].occupied       = true;
        enemySlots[i].texturePath    = sd.texturePath;
        enemySlots[i].jsonPath       = sd.jsonPath;
        enemySlots[i].startClip      = sd.idleClip;
        enemySlots[i].cameraFocusOffsetY = sd.cameraFocusOffsetY;
        // Formation offsets from data/formations.json place each slot correctly.
        enemySlots[i].worldX         = battleCenterX + formation.enemy[i].offsetX;
        enemySlots[i].worldY         = battleCenterY + formation.enemy[i].offsetY;
    }
}
else
{
    // Legacy fallback: no encounter data (debug push without enemy context).
    enemySlots[0].occupied = true;
    enemySlots[0].texturePath = L"assets/animations/skeleton.png";
    // ... etc.
}
```

**Adding more enemies to a battle is purely data-authoring:** add more objects to the `battleParty` array in the JSON file. Zero code changes required.

---

## 9. The Iris Circle Reveal Transition

The iris is a fullscreen black overlay with a circular hole. The hole grows or shrinks using a **SDF smoothstep** pixel shader — this produces a soft, feathered edge.

### Visual concept

```
radius = 0          radius = 300px        radius = maxRadius
┌────────┐          ┌────────┐            ┌────────┐
│████████│          │██╔════╗│            │        │
│████████│  ──────► │█║ game║│  ──────►   │  game  │
│████████│          │██╚════╝│            │        │
└────────┘          └────────┘            └────────┘
  CLOSED              OPENING               IDLE (open)
 (fully black)     (circle expands)      (overlay gone)
```

The soft edge (feather width = 24px) is what makes it look cinematic.

### Shader formula

```hlsl
float dist  = length(pixel.xy - center);
float alpha = smoothstep(radius - softEdge, radius + softEdge, dist);
// alpha = 0 → inside the hole (transparent, game visible)
// alpha = 1 → outside the hole (opaque black, game hidden)
```

### State machine: IrisPhase

```
Initialize()
    │  mRadius = 0, mPhase = CLOSED
    ▼
StartOpen(speed)
    │  mPhase = OPENING
    ▼
Update(dt)  per frame:  mRadius += speed * dt
    │  when mRadius >= mMaxRadius:  mPhase = IDLE
    ▼
IDLE  (no draw call — no overhead)
    │
    │  (battle ends)
    ▼
StartClose(callback, speed)
    │  mPhase = CLOSING
    ▼
Update(dt)  per frame:  mRadius -= speed * dt
    │  when mRadius <= 0:  callback() → mPendingSafeExit = true
    ▼
CLOSED
    │  top of next Update():  PopState() + broadcast outcome event
    ▼
(BattleState destroyed)
```

### Who owns the iris

Each state owns its own `IrisTransitionRenderer` as a **value member** — not a pointer. It is initialized in `OnEnter()` and shut down in `OnExit()`.

```
PlayState:    mIris.Initialize(device, W, H) → StartOpen(800)
              (overworld fades in when PlayState first enters)

BattleState:  mIris.Initialize(device, W, H) → StartOpen(800)
              (circle expands outward to reveal the battle)

              on WIN/LOSE/FLEE:
              mIris.StartClose([this]() { mPendingSafeExit = true; }, 600.0f)
              (circle shrinks to black before PopState is called)
```

### The deferred-exit pattern (important)

`PopState()` destroys `BattleState` immediately. If the iris callback called `PopState()` directly, accessing any member variable afterward would be undefined behaviour (use-after-free).

The safe pattern used in `BattleState::Update()`:

```cpp
void BattleState::Update(float dt)
{
    mIris.Update(dt);   // callback may set mPendingSafeExit here

    // Check FIRST — before any other code runs.
    if (mPendingSafeExit)
    {
        EventManager::Get().Broadcast(mExitEventName, {});
        StateManager::Get().PopState();
        return;  // 'this' is now destroyed — NEVER access members after this line
    }

    // ... rest of battle logic only runs when no exit is in progress
}
```

---

## 10. Formation Slots

Enemy positions inside the battle are controlled by `data/formations.json`, not by code.

```json
"enemy_offsets": [
    { "slot": 0, "offset_x": 320,  "offset_y": 40  },
    { "slot": 1, "offset_x": 440,  "offset_y": -80 },
    { "slot": 2, "offset_x": 440,  "offset_y": 160 }
]
```

These are **world-space offsets** from the battle center `(0, 0)`. `BattleState` maps `battleParty[i]` directly to `enemy_offsets[i]`:

| `battleParty` index | Formation slot | Screen position |
|---|---|---|
| `[0]` | Slot 0 | Front (center-right) |
| `[1]` | Slot 1 | Back top-right |
| `[2]` | Slot 2 | Back bottom-right |

If `battleParty` has only 1 entry, only slot 0 is filled — slots 1 and 2 remain empty (`occupied = false`). `BattleRenderer` skips unoccupied slots automatically.

To rearrange positions: edit `data/formations.json` only — no code change needed.

---

## 11. Common Mistakes

### Mistake 1 — Wrong `cameraFocusOffsetY`
The battle camera focuses on the enemy by adding this Y offset to the enemy's world position (feet anchor). If it is zero, the camera centers on the feet and you see only the legs.

**Formula:**  
```
cameraFocusOffsetY = -(frameHeight × renderScale) / 2
```
Example: sprite is 128px, rendered at scale 2.0  
`cameraFocusOffsetY = -(128 × 2) / 2 = -128`

---

### Mistake 2 — Using `SetClip` instead of `PlayClip`
`WorldSpriteRenderer`'s method is named `PlayClip(clipName)`, not `SetClip`. This only matters if you write custom entity code that drives the renderer directly.

---

### Mistake 3 — Accessing mOverworldEnemies after OnExit
`PlayState::OnExit()` clears `mOverworldEnemies` **before** `SceneGraph::Clear()`. After `OnExit()` returns, all `OverworldEnemy*` pointers in the vector are dangling. Never stash these pointers outside `PlayState`.

---

### Mistake 4 — Zero `contactRadius`
If `contactRadius` is `0` in the JSON (or the field is missing), `IsPlayerNearby()` returns false for every position including standing directly on the enemy. The battle can never trigger. Use at least `60.0` for small enemies.

---

### Mistake 5 — Calling `PopState()` inside a callback
The iris `StartClose(callback)` lambda fires inside `Update()`. Calling `StateManager::PopState()` there destroys `this` immediately. All code following that call (including the rest of the lambda and the `Update` stack) accesses freed memory.

Always use the deferred flag pattern:
```cpp
// Inside the callback:
mPendingSafeExit = true;   // set flag only

// Top of Update():
if (mPendingSafeExit) { PopState(); return; }   // safe — nothing runs after
```

---

### Mistake 6 — Forgetting `mRenderer.Shutdown()` in the destructor
`WorldSpriteRenderer` holds GPU resources (texture SRV, SpriteBatch). If `Shutdown()` is not called on destruction, the DirectX debug layer reports live object leaks at shutdown. `OverworldEnemy`'s destructor calls `Shutdown()` via the `mInitialized` guard — do not remove it.

---

## 12. FAQ

**Q: Can two different overworld enemies share the same sprite sheet?**  
A: Yes. Point `texturePath` and `jsonPath` to the same files in both JSON definitions. Give each enemy different stats, a different `name`, and a different `contactRadius`. They both load the same GPU texture, which the driver deduplicates (same path → same `ID3D11ShaderResourceView`).

---

**Q: Can I spawn multiple instances of the same JSON?**  
A: Yes. The current skeleton example does exactly that:
```cpp
OverworldEnemy* e0 = mScene.Spawn<OverworldEnemy>(device, context, skeletonData, 300.0f, 150.0f, mCamera.get());
OverworldEnemy* e1 = mScene.Spawn<OverworldEnemy>(device, context, skeletonData, -250.0f, -100.0f, mCamera.get());
```
`skeletonData` is copied by value for each enemy. Each enemy has an independent `WorldSpriteRenderer` (its own GPU state), so they animate independently.

---

**Q: The player triggers battle from too far away. How do I fix it?**  
A: Lower `contactRadius` in the enemy's JSON. No code change needed. Rebuild is not required if you add a hot-reload path (the loader is a plain file read).

---

**Q: I added a new enemy but it never appears. How do I debug?**  
A: Check the debug console output (Visual Studio Output window or the debug console) for these lines:
```
[JsonLoader] Loaded enemy 'Goblin' from 'data/enemies/goblin.json': hp=40 atk=8 def=3 spd=7
[OverworldEnemy] Spawned 'Goblin' at world (-100.0, 200.0).
```
If the first line is absent, the file path or JSON structure is wrong.  
If the second is absent but the first is present, `WorldSpriteRenderer::Initialize` failed — check the texture and sprite-sheet JSON paths.

---

**Q: How do I add a second enemy into the same battle?**  
A: Add a second object to the `battleParty` array in the enemy's JSON file. No code changes needed:
```json
"battleParty": [
  { "texturePath": "...", "hp": 50, ... },
  { "texturePath": "...", "hp": 30, ... }
]
```
The loop in `BattleState::OnEnter` automatically creates a slot for each entry, up to the maximum of 3.

---

**Q: What layer does `OverworldEnemy` render on?**  
A: Layer `48`. The player (`ControllableCharacter`) is layer `50`. This means the player sprite renders on top of an enemy when they overlap at the same Y coordinate. Adjust `GetLayer()` in `OverworldEnemy.h` if you need a different render order.
