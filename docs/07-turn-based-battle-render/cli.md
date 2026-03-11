# Battle Debug CLI — HUD Reference

The battle debug HUD is printed to the **LOG() console** (timestamped, visible in
the terminal) on every input phase change and every simulation phase change.
It is also mirrored to the VS Output / DebugView window via `OutputDebugStringA`.

---

## 1. HUD Structure

Every dump follows the same layout:

```
======================== BATTLE STATE ========================
  [ ENGINE: <SimPhase>   ]   [ <ActionLabel> ]
  <KeyHints>

  <Section: Actions / Skills / Info>
  ----------------------- Combatants ------------------------
  <CombatantRows>
  ------------------------ Battle Log ------------------------
  <Last 5 battle log lines>
==============================================================
```

- **ENGINE** line — the first thing to read. Tells you both what the engine is
  doing AND what you must do right now.
- **Key hints** — printed immediately below the ENGINE line, only when there is
  something to press.
- **Combatant rows** — `>>>` prefix marks the combatant whose turn it is.
- **Battle Log** — last 5 events from the action timeline.

---

## 2. Phase Reference

### Phase 1 — COMMAND_SELECT  (top-level menu)

Appears when it is your turn and you have not yet chosen an action.

```
======================== BATTLE STATE ========================
  [ ENGINE: PLAYER_TURN   ]   [ YOUR TURN: SELECT A COMMAND ]
  [Up/Down] navigate   [Enter] confirm

  Actions :
    > Fight
      Flee
  ------------------------- Combatants -----------------------
  >>> [PLAYER] Verso         HP: 100/100  Rage:  0/100  ATK:25  DEF:10  SPD:10  ALIVE
      [ENEMY ] Skeleton A    HP:  50/ 50  ATK:15  DEF:10  SPD: 8  ALIVE
      [ENEMY ] Skeleton B    HP:  50/ 50  ATK:15  DEF: 5  SPD: 8  ALIVE
  ------------------------- Battle Log -----------------------
  --- Verso's turn ---
==============================================================
```

**Camera**: OVERVIEW — wide shot, all combatants visible, zoom=1.0.

---

### Phase 2 — SKILL_SELECT  (choose a skill)

Appears after you press **Enter** on "Fight".

```
======================== BATTLE STATE ========================
  [ ENGINE: PLAYER_TURN   ]   [ YOUR TURN: SELECT A SKILL ]
  [1] [2] [3] select skill   [Esc] back to commands

  Skills  :
    > [1] Attack          Strike the enemy.
      [2] Rage Burst       Unleash full rage for massive damage.  (unavailable)
      [3] Weaken           Reduce target ATK and DEF for 2 turns.
  ------------------------- Combatants -----------------------
  >>> [PLAYER] Verso         HP:  78/100  Rage: 11/100  ATK:25  DEF:10  SPD:10  ALIVE
      [ENEMY ] Skeleton A    HP:   5/ 50  ATK:15  DEF:10  SPD: 8  ALIVE
      [ENEMY ] Skeleton B    HP:  50/ 50  ATK:15  DEF: 5  SPD: 8  ALIVE
  ------------------------- Battle Log -----------------------
  --- Skeleton A's turn ---
  Skeleton A attacks Verso!
  --- Skeleton B's turn ---
  Skeleton B attacks Verso!
  --- Verso's turn ---
==============================================================
```

**Camera**: ACTOR_FOCUS — zooms in on Verso (the acting character), zoom=1.6,
position lerps to Verso's world position over ~0.6 s.

**Unavailable skills** are shown greyed-out with `(unavailable)` — not hidden.
This lets the player understand why they cannot use them.

---

### Phase 3 — TARGET_SELECT  (choose an enemy)

Appears after you press **1**, **2**, or **3** to select a skill.

```
======================== BATTLE STATE ========================
  [ ENGINE: PLAYER_TURN   ]   [ YOUR TURN: SELECT A TARGET ]
  [Tab] next target   [Enter] confirm   [Esc] back to skills

  Skills  :
      [1] Attack          Strike the enemy.
    > [1] Attack          (selected)
  Skill        : Attack
  Target       : Skeleton A
  Hint         : Tab = next target  Enter = confirm  Esc = back
  ------------------------- Combatants -----------------------
  >>> [PLAYER] Verso         HP:  78/100  Rage: 11/100  ATK:25  DEF:10  SPD:10  ALIVE
      [ENEMY ] Skeleton A    HP:   5/ 50  ATK:15  DEF:10  SPD: 8  ALIVE
      [ENEMY ] Skeleton B    HP:  50/ 50  ATK:15  DEF: 5  SPD: 8  ALIVE
  ------------------------- Battle Log -----------------------
  Skeleton A attacks Verso!
  --- Skeleton B's turn ---
  Skeleton B attacks Verso!
  --- Verso's turn ---
==============================================================
```

**Camera**: TARGET_FOCUS — position lerps to `targetPos * 0.8 + actorPos * 0.2`.
The target (enemy) dominates the frame while Verso stays partially visible at the
edge. Zoom returns to 1.0 so the full enemy row is readable. Pressing **Tab** to
cycle targets instantly re-triggers the camera lerp toward the new selection.

---

### Phase 4 — RESOLVING / ENEMY_TURN / WIN / LOSE

Appears automatically when the simulation phase changes.

```
======================== BATTLE STATE ========================
  [ ENGINE: RESOLVING     ]   [ WAIT — action in progress... ]

  ------------------------- Combatants -----------------------
      [PLAYER] Verso         HP:  78/100  Rage: 11/100  ATK:25  DEF:10  SPD:10  ALIVE
      [ENEMY ] Skeleton A    HP:   0/ 50  ATK:15  DEF:10  SPD: 8  DEAD
      [ENEMY ] Skeleton B    HP:  50/ 50  ATK:15  DEF: 5  SPD: 8  ALIVE
  ------------------------- Battle Log -----------------------
  --- Verso's turn ---
  Verso uses Attack on Skeleton A!
  Skeleton A takes 15 damage!
  Skeleton A is defeated!
  --- Skeleton B's turn ---
==============================================================
```

**Camera**: OVERVIEW — snaps back to wide shot immediately on any non-player-turn
phase. No key hints are shown — the player is watching, not acting.

---

## 3. Camera Phase Summary

| Input Phase     | Camera Phase   | Position                          | Zoom |
|-----------------|---------------|-----------------------------------|------|
| COMMAND_SELECT  | OVERVIEW       | (0, 0) — screen center            | 1.0  |
| SKILL_SELECT    | ACTOR_FOCUS    | Actor world pos (player slot 0)   | 1.6  |
| TARGET_SELECT   | TARGET_FOCUS   | 80% target + 20% actor blend      | 1.0  |
| RESOLVING       | OVERVIEW       | (0, 0)                            | 1.0  |
| ENEMY_TURN      | OVERVIEW       | (0, 0)                            | 1.0  |
| WIN / LOSE      | OVERVIEW       | (0, 0)                            | 1.0  |

All transitions are **smoothly lerped** (exponential decay, speed=5.0, ~0.6 s).  
Cycling the target with **Tab** immediately re-lerps the camera to the new enemy.

---

## 4. Architecture — How It Fits Together

```
BattleState::SetInputPhase(SKILL_SELECT)
  ├── mBattleRenderer.SetCameraPhase(ACTOR_FOCUS, actorSlot=0, targetSlot=-1)
  │     └── BattleCameraController::SetActorPos(worldX, worldY)
  │     └── BattleCameraController::SetPhase(ACTOR_FOCUS)
  └── DumpStateToDebugOutput() → LOG() console

BattleState::CycleTarget()
  └── mBattleRenderer.SetCameraPhase(TARGET_FOCUS, actorSlot=0, targetSlot=mTargetIndex)
        └── BattleCameraController::SetTargetPos(worldX, worldY)
        └── BattleCameraController::SetPhase(TARGET_FOCUS)

BattleState::Update(dt)
  ├── Simulation phase change detected
  │     └── mBattleRenderer.SetCameraPhase(OVERVIEW, -1, -1)
  └── mBattleRenderer.Update(dt)
        └── BattleCameraController::Update(dt)   ← lerp tick
              └── Camera2D::SetPosition / SetZoom / Update()
```

**Separation of concerns:**
- `BattleCameraController` — all lerp math, desired-state computation, Camera2D ownership.
- `BattleRenderer` — converts slot indices to world coords, delegates to controller.
- `BattleState` — only decides *which* phase to request; never computes world coords.

---

## 5. Tuning Constants (BattleCameraController.cpp)

| Constant        | Default | Effect |
|-----------------|---------|--------|
| `kSmoothSpeed`  | 5.0f    | Higher = snappier transitions. Lower = more cinematic. |
| `kZoomOverview` | 1.0f    | Default wide-shot zoom. |
| `kZoomFocus`    | 1.6f    | Close-up zoom during SKILL_SELECT. |
| `kZoomTarget`   | 1.0f    | Zoom level during TARGET_SELECT. |
| `kTargetBlend`  | 0.8f    | How much camera leans toward the target (0=actor, 1=target). |

All constants are `static constexpr` at the top of the `.cpp` file — change them
there and rebuild; no other file needs editing.
