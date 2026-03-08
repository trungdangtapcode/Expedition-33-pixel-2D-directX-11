# 09 — Enhance Turn-Based Battle

This document covers three improvements made to the battle and overworld systems:

1. **Shared animation interface (`CombatantAnim`)** — a common vocabulary for all combat animation roles, shared by every combatant, decoupled from raw strings.
2. **Death animation + bug fix** — enemies now play a collapse animation when HP reaches 0; traced and fixed a bug that caused the sprite to vanish instantly.
3. **Overworld Y-sort rendering** — entities at the bottom of the screen now correctly appear in front of entities higher up on screen (painter's algorithm).

---

## Table of Contents

1. [CombatantAnim — Shared Animation Interface](#1-combatantanim--shared-animation-interface)
2. [Death Animation System](#2-death-animation-system)
3. [Bug Fix — Enemy Vanishes on Death](#3-bug-fix--enemy-vanishes-on-death)
4. [Overworld Y-Sort Rendering](#4-overworld-y-sort-rendering)
5. [Files Changed](#5-files-changed)

---

## 1. CombatantAnim — Shared Animation Interface

### Problem

Before this change, every system that needed to drive a character's animation passed raw string literals:

```cpp
mRenderer.PlayClip("die");       // BattleState death detection
mRenderer.PlayClip("attack-1");  // AttackAction
mRenderer.PlayClip("idle");      // BattleManager phase change
```

This is fragile for several reasons:
- A typo (`"att-1"`, `"Dead"`) fails silently — `WorldSpriteRenderer::PlayClip` logs a warning and no-ops; the character freezes in whatever pose it was already in.
- If a character uses non-default clip names (e.g. `"death_collapse"` instead of `"die"`), every call site that hardcodes `"die"` must be found and updated.
- There is no single place that documents all valid animation roles.

### Solution — `CombatantAnim` enum + `DefaultClipName()`

**File:** `src/Battle/CombatantAnim.h`

```cpp
enum class CombatantAnim
{
    Idle,    // Standing still — plays when no action is in progress.
    Attack,  // Attack windup + strike — played by AttackAction.
    Walk,    // Movement cycle — used in overworld.
    Die,     // Death collapse — triggered on HP = 0.
    Hurt,    // Hit-reaction — triggered when taking damage.
    kCount   // Sentinel — array dimension; never use as a request.
};

static constexpr int kCombatantAnimCount = static_cast<int>(CombatantAnim::kCount);

inline const char* DefaultClipName(CombatantAnim anim)
{
    switch (anim)
    {
        case CombatantAnim::Idle:   return "idle";
        case CombatantAnim::Attack: return "attack-1";
        case CombatantAnim::Walk:   return "walk";
        case CombatantAnim::Die:    return "die";
        case CombatantAnim::Hurt:   return "hurt";
        default:                    return "idle";
    }
}
```

All callers now use the enum. The string is resolved exactly once — inside `BattleRenderer::Initialize()` — never again at runtime.

### How clip names are resolved (per slot, per role)

`BattleRenderer::SlotInfo` has a `clipOverrides` array indexed by `CombatantAnim`:

```cpp
struct SlotInfo
{
    // ...sprite paths, world position, camera offsets...

    // Per-role override. Leave empty to use DefaultClipName().
    // Index with static_cast<int>(CombatantAnim::X).
    std::string clipOverrides[kCombatantAnimCount];
};
```

During `BattleRenderer::Initialize()`, for each slot × each role, the internal name table is seeded once:

```cpp
for (int i = 0; i < kMaxSlots; ++i)
{
    for (int a = 0; a < kCombatantAnimCount; ++a)
    {
        const CombatantAnim role = static_cast<CombatantAnim>(a);

        const std::string& eOvr = enemySlots[i].clipOverrides[a];
        mEnemyClipNames[i][a] = eOvr.empty() ? DefaultClipName(role) : eOvr;
    }
}
```

At runtime, `PlayEnemyClip(slot, anim)` just reads this pre-built table:

```cpp
void BattleRenderer::PlayEnemyClip(int slot, CombatantAnim anim)
{
    if (slot < 0 || slot >= kMaxSlots || !mEnemyActive[slot]) return;
    const std::string& clipName = mEnemyClipNames[slot][static_cast<int>(anim)];
    const bool found = mEnemyRenderers[slot].PlayClip(clipName);
    // ... see Section 3 for the found == false case
}
```

### Per-character overrides from JSON

`EnemySlotData` (in `src/Battle/EnemyEncounterData.h`) exposes override fields:

```cpp
struct EnemySlotData
{
    std::string dieClip;     // empty → "die"
    std::string attackClip;  // empty → "attack-1"
    std::string walkClip;    // empty → "walk"
    std::string hurtClip;    // empty → "hurt"
    // ... existing stat fields ...
};
```

A monster JSON can override any role:

```json
{
  "battleParty": [
    {
      "texturePath": "assets/animations/goblin.png",
      "jsonPath":    "assets/animations/goblin.json",
      "idleClip":    "idle",
      "dieClip":     "death_fall",
      "attackClip":  "slash",
      "hp": 40, "atk": 8, "def": 3, "spd": 7
    }
  ]
}
```

If `"dieClip"` is omitted, `DefaultClipName(Die)` = `"die"` is used automatically. The resolution chain is:

```
JSON dieClip field
    │ empty?
    └── YES → DefaultClipName(Die) = "die"
    └── NO  → use the value as-is
        │
        ▼
mEnemyClipNames[slot][Die] stored in Initialize()
        │
        ▼
PlayEnemyClip(slot, Die) resolves instantly from the table
```

### Design benefit

`BattleState`, `AttackAction`, and any future system never need to know clip names. They speak in roles:

```cpp
// Death detected:
mBattleRenderer.PlayEnemyClip(i, CombatantAnim::Die);

// Attack triggered:
mBattleRenderer.PlayPlayerClip(actorSlot, CombatantAnim::Attack);
```

Adding a new role (e.g. `CombatantAnim::Revive`) only requires:
1. Adding the enum value before `kCount`.
2. Adding a `case` to `DefaultClipName()`.
3. Optionally adding a `reviveClip` field to `EnemySlotData`.

Zero call sites need to change unless they explicitly want the new role.

---

## 2. Death Animation System

### Problem

When an enemy's HP dropped to 0, the battle transitioned immediately to the iris close. The death visual was instant — no collapse animation, the enemy just disappeared on the same frame it died.

### `IsClipDone()` — knowing when a clip has finished

`WorldSpriteRenderer` gained `mClipFinished`, `mFrozen`, and `IsClipDone()`:

```cpp
// Returns true when it is safe to proceed past this clip:
//   • No clip active              → true  (nothing to wait for)
//   • FreezeCurrentFrame() called → true  (held pose — see Section 3)
//   • Looping clip                → true  (never "done")
//   • Non-looping, still playing  → false (still animating)
//   • Non-looping, last frame hit → true  (mClipFinished = true)
bool IsClipDone() const
{
    if (!mActiveClip)      return true;
    if (mFrozen)           return true;
    if (mActiveClip->loop) return true;
    return mClipFinished;
}
```

`mClipFinished` is set inside `WorldSpriteRenderer::Update()` the moment the last frame of a non-looping clip is reached:

```cpp
while (mFrameTimer >= frameDuration)
{
    mFrameTimer -= frameDuration;
    mFrameIndex++;

    if (mFrameIndex >= mActiveClip->numFrames)
    {
        if (mActiveClip->loop)
        {
            mFrameIndex = 0;   // wrap for looping clips
        }
        else
        {
            // Hold the last frame; mark the clip as finished.
            mFrameIndex   = mActiveClip->numFrames - 1;
            mFrameTimer   = 0.0f;
            mClipFinished = true;
            break;
        }
    }
}
```

It is reset to `false` every time `PlayClip()` switches to a new clip so the state is clean for the next non-looping animation.

### `AreAllDeathAnimsDone()` — polling across all slots

`BattleRenderer` provides a single query that covers every active slot on both teams:

```cpp
bool BattleRenderer::AreAllDeathAnimsDone() const
{
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (mEnemyActive[i]  && !mEnemyRenderers[i].IsClipDone())  return false;
        if (mPlayerActive[i] && !mPlayerRenderers[i].IsClipDone()) return false;
    }
    return true;
}
```

Alive combatants play looping `"idle"` — looping clips always return `IsClipDone() == true` — so they never block the wait. Only the non-looping die clip of the dead combatant can block it.

### `mWaitingForDeathAnims` — the gate in BattleState

`BattleState` gained a `mWaitingForDeathAnims` flag:

```cpp
bool mWaitingForDeathAnims = false;  // set when outcome first detected
```

The frame-by-frame control flow is:

```
Frame N:
  BattleManager::Update(dt) → enemy HP drops to 0 → outcome = VICTORY

  // Death detection block (runs after mBattle.Update, before renderer.Update):
  for each enemy i:
      if (mEnemyWasAlive[i] && !enemies[i]->IsAlive())
          mBattleRenderer.PlayEnemyClip(i, CombatantAnim::Die);
          // → mClipFinished = false on the new die clip

  // Outcome gate (first time only):
  if (outcome != NONE && !mExitTransitionStarted && !mWaitingForDeathAnims)
      mWaitingForDeathAnims = true;   // start waiting, NO iris yet

  mBattleRenderer.Update(dt);          // die clip advances its first frame
  // AreAllDeathAnimsDone() → false    → iris stays closed

Frames N+1 … N+K:
  // still inside !mExitTransitionStarted block
  mBattleRenderer.Update(dt);          // die clip advances each frame
  if (mWaitingForDeathAnims && AreAllDeathAnimsDone())
      → false, false, ...              → iris stays closed

Frame N+K+1  (last frame of die clip held):
  WorldSpriteRenderer::Update sets mClipFinished = true
  AreAllDeathAnimsDone() → true

  mWaitingForDeathAnims  = false
  mExitTransitionStarted = true
  mIris.StartClose(...)               // iris begins to close over the finished animation
```

**Critical ordering:** `mBattleRenderer.Update(dt)` is inside the `if (!mExitTransitionStarted)` block. Once `mExitTransitionStarted` becomes `true`, the renderer stops updating — which is correct because the iris is now closing over the scene and no further animation advancement is needed.

---

## 3. Bug Fix — Enemy Vanishes on Death

### Symptom

After implementing the death animation system above, the enemy sprite still disappeared the moment its HP hit 0. The die animation never played. Tracing from `BattleState` → `BattleRenderer` → `WorldSpriteRenderer` was required to find the root cause.

### Tracing the disappearance

1. **`BattleState::Render()`** — no gate, always calls `mBattleRenderer.Render()`. ✓
2. **`BattleRenderer::Render()`** — only skips slots where `mEnemyActive[i] == false`. `mEnemyActive` is set in `Initialize()` and never cleared at runtime. ✓
3. **`WorldSpriteRenderer::Draw()`** — returns early if `!mActiveClip`. The active clip IS set by `PlayEnemyClip(Die)`. ✓
4. **The die clip frame UV** — `mFrameIndex = 0`, `atlasRow = clip.startRow`. The die clip is at row 4 (fifth animation). **atlas row 4 = y=512 in a 512-pixel-tall texture = out of bounds.**

### Root Cause — Out-of-Bounds UV Lookup

`skeleton.png` was **1536 × 512**: four 128-pixel rows.  
`skeleton.json` declared five animations, making the die clip's `startRow = 4`.

| Row | y start | y end | Clip |
|-----|---------|-------|------|
| 0 | 0 | 128 | `idle` |
| 1 | 128 | 256 | `attack-1` |
| 2 | 256 | 384 | `walk` |
| 3 | 384 | 512 | `attack-2` |
| **4** | **512** | **640** | **`die`** ← out-of-bounds |

`SpriteBatch::Draw()` sampled y=512–640 on a 512px-tall texture. The GPU clamped or wrapped to a transparent region — **the enemy appeared to vanish instantly** rather than sampling garbage pixels.

This also meant `mClipFinished` was set at `false` and `IsClipDone()` returned `false` — so `AreAllDeathAnimsDone()` would wait forever if not for the iris close never starting.

### Fix 1 — Correct the sprite sheet

`assets/animations/skeleton.json` `"height"` field was updated from `512` to `640` and `skeleton.png` was updated to include the die animation row, making all five clips valid.

> **Validation rule for sprite sheets:**
> `"height"` must be ≥ `numAnimations × frameHeight`.
> Row count = `"height" / frameHeight` must cover all defined clips.

### Fix 2 — `FreezeCurrentFrame()` for missing clips

Even with a correct `skeleton.png`, any character that genuinely has no die clip in their sheet needed a safe fallback. The old behaviour left the idle looping clip active — `IsClipDone()` returned `true` immediately for looping clips, so `AreAllDeathAnimsDone()` returned `true` on the same frame the enemy died, and the iris fired instantly.

**`WorldSpriteRenderer::PlayClip()` now returns `bool`:**

```cpp
// Returns true  — clip found and playback started.
// Returns false — clip absent from the sheet; active clip is UNCHANGED.
bool WorldSpriteRenderer::PlayClip(const std::string& clipName);
```

**`WorldSpriteRenderer::FreezeCurrentFrame()` and `mFrozen` were added:**

```cpp
// Halt animation at the current frame.
// IsClipDone() returns true immediately from this point.
// mFrozen is reset to false by the next PlayClip() call.
void FreezeCurrentFrame() { mFrozen = true; }
```

`Update()` checks `mFrozen` first:

```cpp
void WorldSpriteRenderer::Update(float dt)
{
    // Skip advancement when frozen, uninitialized, or single-frame.
    if (mFrozen || !mActiveClip || mActiveClip->numFrames <= 1) return;
    // ...
}
```

**`BattleRenderer::PlayEnemyClip()` calls `FreezeCurrentFrame()` when the die clip is absent:**

```cpp
void BattleRenderer::PlayEnemyClip(int slot, CombatantAnim anim)
{
    if (slot < 0 || slot >= kMaxSlots || !mEnemyActive[slot]) return;

    const std::string& clipName = mEnemyClipNames[slot][static_cast<int>(anim)];
    const bool found = mEnemyRenderers[slot].PlayClip(clipName);

    // Die clip not in sheet → freeze on last visible pose.
    // IsClipDone() returns true immediately → iris proceeds without stalling.
    if (!found && anim == CombatantAnim::Die)
    {
        mEnemyRenderers[slot].FreezeCurrentFrame();
        LOG("[BattleRenderer] Enemy slot %d: die clip '%s' not found — frame frozen.",
            slot, clipName.c_str());
    }
}
```

The same pattern applies in `PlayPlayerClip()`.

### Complete state machine for `IsClipDone()`

```
PlayClip("die") returns true
    → mClipFinished = false, mFrozen = false
    → Update() advances die clip frame by frame
    → last frame reached → mClipFinished = true
    → IsClipDone() = true  ✓

PlayClip("die") returns false   (clip not in sheet)
    → FreezeCurrentFrame() → mFrozen = true
    → Update() skips (mFrozen = true)
    → IsClipDone() = true  ✓  (immediately)

Idle looping clip (no PlayClip called)
    → IsClipDone() = true  (loop = true path)
    → AreAllDeathAnimsDone() not blocked  ✓
```

### Summary of the bug trace path

```
BattleState::Render()          ─── always calls mBattleRenderer.Render()
BattleRenderer::Render()       ─── checks mEnemyActive[i], never cleared → OK
WorldSpriteRenderer::Draw()    ─── mActiveClip set by PlayEnemyClip(Die) → OK
                                   BUT mFrameIndex=0, atlasRow=4, y=512
                                   ↓
SpriteBatch samples y=512-640  ─── texture is only 512px tall
                                   GPU returns transparent pixels
                                   ↓
Enemy sprite is invisible      ← ROOT CAUSE
```

---

## 4. Overworld Y-Sort Rendering

### Problem

In the overworld, the player character and enemy sprites were drawn in a fixed layer order: enemies on layer 48, player on layer 50. This meant the player was **always** drawn on top of enemies regardless of vertical position.

When the player walked above an enemy (lower Y on screen = further "into" the world), the player sprite still overlapped the enemy — incorrect for a 2.5D top-down scene.

### The Painter's Algorithm

In a top-down scene, the vertical position on screen represents depth:
- Higher on screen (smaller world Y) = further away from the viewer = drawn first.
- Lower on screen (larger world Y) = closer to the viewer = drawn last (on top).

```
world Y = 100  ──────── background character (drawn first)
              │
              │  player walks down
              ▼
world Y = 300  ──────── foreground character (drawn last = on top)
```

### Solution — Compound Sort in SceneGraph

`IGameObject` gained a new virtual method with a default implementation:

```cpp
// Returns the world-space Y used as a secondary sort key within the same layer.
// Default = 0.0f (objects not needing Y-sort participate in layer sort only).
virtual float GetSortY() const { return 0.0f; }
```

`SceneGraph::Render()` was updated to sort by **(layer ASC, sortY ASC)**:

```cpp
std::stable_sort(indices.begin(), indices.end(),
    [this](int a, int b)
    {
        const int layerA = mObjects[a]->GetLayer();
        const int layerB = mObjects[b]->GetLayer();
        if (layerA != layerB) return layerA < layerB;
        // Same layer: lower Y drawn first (further up = behind).
        return mObjects[a]->GetSortY() < mObjects[b]->GetSortY();
    });
```

### Entity changes

| Class | `GetLayer()` | `GetSortY()` |
|---|---|---|
| `ControllableCharacter` | 50 | `mPosY` |
| `OverworldEnemy` | ~~48~~ → **50** | `mWorldY` |

`OverworldEnemy`'s layer was changed from 48 to 50. At layer 48 the layer difference always won the sort and Y-sort between player and enemy would never apply. Both entities must share the same layer to participate in Y-sort relative to each other.

### Visual result

```
Before:
  player always drawn on top of enemies  → unnatural overlap from any direction

After:
  player Y=100, enemy Y=300 → enemy drawn on top (enemy is "in front")
  player Y=300, enemy Y=100 → player drawn on top (enemy is "behind")
```

Objects that do not need Y-sorting — UI (`GetLayer()` = 100+), background tiles — return the default `GetSortY() = 0.0f` and are sorted by layer only. No existing code needed changes.

---

## 5. Files Changed

| File | Change |
|---|---|
| `src/Battle/CombatantAnim.h` | **NEW** — `CombatantAnim` enum + `DefaultClipName()` |
| `src/Battle/EnemyEncounterData.h` | Added `dieClip`, `attackClip`, `walkClip`, `hurtClip` to `EnemySlotData` |
| `src/Battle/BattleRenderer.h` | `SlotInfo::clipOverrides[]`; `PlayEnemyClip()`; `PlayPlayerClip()`; `AreAllDeathAnimsDone()`; per-slot clip name tables |
| `src/Battle/BattleRenderer.cpp` | Seeded name tables in `Initialize()`; freeze-on-missing-die logic in `PlayEnemyClip`/`PlayPlayerClip` |
| `src/Renderer/WorldSpriteRenderer.h` | `PlayClip()` returns `bool`; added `mClipFinished`, `mFrozen`, `IsClipDone()`, `FreezeCurrentFrame()` |
| `src/Renderer/WorldSpriteRenderer.cpp` | `PlayClip()` returns bool, resets `mFrozen`; `Update()` checks `mFrozen` first; `mClipFinished` set on last non-looping frame |
| `src/States/BattleState.h` | Added `mWaitingForDeathAnims`, `mEnemyWasAlive[]`, `mPlayerWasAlive[]` |
| `src/States/BattleState.cpp` | Death detection block (per-frame alive comparison); `mWaitingForDeathAnims` gate delays iris close until `AreAllDeathAnimsDone()` |
| `assets/animations/skeleton.json` | `"height"` corrected to 640; die clip row is now valid |
| `src/Scene/IGameObject.h` | Added `GetSortY()` virtual (default `0.0f`) |
| `src/Scene/SceneGraph.h` | Updated Render() doc for compound sort |
| `src/Scene/SceneGraph.cpp` | Compound sort: layer ASC then sortY ASC |
| `src/Entities/ControllableCharacter.h` | Added `GetSortY()` returning `mPosY` |
| `src/Entities/OverworldEnemy.h` | Layer 48 → 50; added `GetSortY()` returning `mWorldY` |
| `src/Battle/CombatantAnim.h` | NEW — `CombatantAnim` enum + `DefaultClipName()` |
| `src/Battle/EnemyEncounterData.h` | Added `dieClip`, `attackClip`, `walkClip`, `hurtClip` to `EnemySlotData` |
| `src/Battle/BattleRenderer.h` | Added `clipOverrides[]`, `PlayEnemyClip()`, `PlayPlayerClip()`, `AreAllDeathAnimsDone()` |
| `src/Battle/BattleRenderer.cpp` | Implemented above; freeze-on-missing-die-clip logic |
| `src/Renderer/WorldSpriteRenderer.h` | Added `mClipFinished`, `mFrozen`, `IsClipDone()`, `FreezeCurrentFrame()`, `PlayClip()` returns `bool` |
| `src/Renderer/WorldSpriteRenderer.cpp` | `PlayClip()` returns bool, resets `mFrozen`; `Update()` checks `mFrozen` |
| `src/States/BattleState.h` | Added `mWaitingForDeathAnims`, `mEnemyWasAlive[]`, `mPlayerWasAlive[]` |
| `src/States/BattleState.cpp` | Death detection block; wait-for-death-anims pattern before iris close |
| `src/Scene/IGameObject.h` | Added `GetSortY()` virtual method (default 0.0f) |
| `src/Scene/SceneGraph.h` | Updated Render() doc for compound sort |
| `src/Scene/SceneGraph.cpp` | Compound sort: layer ASC then sortY ASC |
| `src/Entities/ControllableCharacter.h` | Added `GetSortY()` returning `mPosY` |
| `src/Entities/OverworldEnemy.h` | Layer 48 → 50; added `GetSortY()` returning `mWorldY` |
| `assets/animations/skeleton.json` | Fixed `"height"` to 640; `"die"` clip entry is valid |
