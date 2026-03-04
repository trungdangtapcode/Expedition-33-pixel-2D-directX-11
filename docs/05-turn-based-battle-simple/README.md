# Turn-Based Battle System — Simple MVP

This document explains the architecture of the turn-based combat system built in
`src/Battle/` and `src/States/BattleState.*`.
It covers every class, how they connect, the flow through one full round of combat,
the skill and status effect extension points, and how the battle wires into the
game's state stack.

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Class Map — What Owns What](#2-class-map)
3. [BattlerStats — The Data Backbone](#3-battlerstats)
4. [IBattler + Combatant Hierarchy](#4-ibattler--combatant-hierarchy)
5. [Skill System — ISkill and Strategy Pattern](#5-skill-system)
6. [Status Effects — IStatusEffect](#6-status-effects)
7. [Action Queue — IAction and Command Pattern](#7-action-queue)
8. [BattleManager FSM — Turn Order and Phase Logic](#8-battlemanager-fsm)
9. [BattleState — The IGameState Wrapper](#9-battlestate)
10. [Persistent Party HP — PartyManager](#10-persistent-party-hp)
11. [How to Add a New Skill](#11-how-to-add-a-new-skill)
12. [How to Add a New Status Effect](#12-how-to-add-a-new-status-effect)
13. [How to Add a New Enemy](#13-how-to-add-a-new-enemy)
14. [Common Mistakes Reference](#14-common-mistakes-reference)

---

## 1. System Overview

```
PlayState
  └── (B key pressed)
        └── StateManager::PushState(BattleState)
              │
              BattleState::OnEnter()
                └── BattleManager::Initialize()
                      ├── Read Verso's current HP from PartyManager
                      ├── Spawn PlayerCombatant("Verso", currentStats)
                      ├── Spawn EnemyCombatant("Goblin A")
                      ├── Spawn EnemyCombatant("Goblin B")
                      └── BuildTurnOrder() — sort by SPD descending

Each frame:
  BattleState::Update(dt)
    ├── (if PLAYER_TURN)  HandleInput()  — 1/2/3 skill, Tab target, Enter confirm
    ├── BattleManager::Update(dt)        — drive FSM + drain ActionQueue
    └── (if WIN or LOSE)
          ├── Save Verso's HP back to PartyManager
          ├── EventManager::Broadcast("battle_end_victory" / "battle_end_defeat")
          └── StateManager::PopState()  — resume PlayState

BattleState::Render()
  └── D3DContext::BeginFrame(navy blue)  — overwrite background; no EndFrame
```

**Key constraint:** `BattleState` never calls `EndFrame()`. `GameApp::Render()` is
the sole owner of `Present()`. See `docs/04-how-did-directX-directXTK-render/` for
the full render ownership rule.

---

## 2. Class Map

```
Battle/
│
├── BattlerStats.h              — plain data struct (HP/MP/ATK/DEF/SPD/rage)
│
├── IBattler.h                  — pure virtual: combat-facing interface
│   └── Combatant.h/.cpp        — base: owns stats + active status effects
│         ├── PlayerCombatant   — adds skill list + pending-action slot
│         └── EnemyCombatant    — adds AI targeting
│
├── IStatusEffect.h             — pure virtual: Apply/Revert/OnTurnEnd/IsExpired
│   └── WeakenEffect.h/.cpp     — 2-turn ATK+DEF debuff with mApplied guard
│
├── ISkill.h                    — pure virtual: CanUse + Execute → vector<IAction>
│   ├── AttackSkill.h/.cpp      — always available; ATK damage
│   ├── RageSkill.h/.cpp        — requires full rage; ATK×2 damage
│   └── WeakenSkill.h/.cpp      — applies WeakenEffect(2, 15, 10) to target
│
├── IAction.h                   — pure virtual: Execute(dt) → bool (complete?)
│   ├── DamageAction.h/.cpp     — instant: TakeDamage(rawDmg, attacker)
│   ├── StatusEffectAction.h/.cpp — instant: AddEffect(effect) to target
│   └── LogAction.h/.cpp        — instant: append message to battle log vector
│
├── ActionQueue.h/.cpp          — deque<unique_ptr<IAction>>, FIFO, one per frame
└── BattleManager.h/.cpp        — FSM owner; drives everything above

States/
└── BattleState.h/.cpp          — IGameState; bridges input → BattleManager

Systems/
└── PartyManager.h/.cpp         — Singleton; holds Verso's persistent BattlerStats
```

Ownership is strict:
- `BattleManager` owns teams via `vector<unique_ptr<PlayerCombatant>>` and `vector<unique_ptr<EnemyCombatant>>`.
- `ActionQueue` is a **value member** of `BattleManager` — no heap allocation.
- `BattleState` owns `BattleManager` by value.
- `PartyManager` stores `BattlerStats` by value — no `unique_ptr` needed (pure data).

---

## 3. BattlerStats

`BattlerStats` (`src/Battle/BattlerStats.h`) is a plain data struct — no virtual
methods, no GPU resources, no behaviors. Every combatant owns one by value.

```cpp
struct BattlerStats {
    int hp, maxHp;       // current / maximum HP
    int mp, maxMp;       // reserved for future use
    int atk, def, spd;   // combat attributes
    int rage, maxRage;   // player-only; enemies have maxRage = 0

    bool IsAlive()    const { return hp > 0; }
    bool IsRageFull() const { return maxRage > 0 && rage >= maxRage; }
    void ClampHp();        // clamp to [0, maxHp]
    void AddRage(int);     // cap at maxRage; no-op if maxRage == 0
};
```

**Rage mechanic:**
- Verse gains rage on both dealing **and** receiving damage.
- When `rage == maxRage`, `IsRageFull()` returns true and `RageSkill::CanUse()` allows it.
- `RageSkill` executes and resets `rage = 0` via an inner `RageResetAction`.
- Enemies have `maxRage = 0` — the rage system silently no-ops for them.

**Damage formula (in `Combatant::TakeDamage`):**
```
effective = max(1, raw - def)
target.hp -= effective

attacker rage += effective / 4   (diminishing rage gain per hit)
defender rage += effective / 8   (smaller rage gain on being hit)
```
`max(1, ...)` ensures every hit deals at least 1 damage.

---

## 4. IBattler + Combatant Hierarchy

### IBattler — the interface

```cpp
struct IBattler {
    virtual std::string  GetName()    const = 0;
    virtual BattlerStats& GetStats()        = 0;
    virtual const BattlerStats& GetStats() const = 0;

    virtual void TakeDamage(int raw, IBattler* source) = 0;
    virtual void AddEffect(std::unique_ptr<IStatusEffect>) = 0;

    virtual void OnTurnStart() = 0;   // applies turn-start effects
    virtual void OnTurnEnd()   = 0;   // decrements effect durations, purges expired

    virtual bool IsAlive()            const = 0;
    virtual bool IsPlayerControlled() const = 0;
};
```

`BattleManager` and `BattleState` operate exclusively through `IBattler*`.
No code outside the factory (`BattleManager::Initialize`) knows whether
a pointer is a `PlayerCombatant` or an `EnemyCombatant`.

### Combatant — the base class

`Combatant` (`src/Battle/Combatant.h/.cpp`) implements `IBattler` and owns:
- `BattlerStats mStats` — the numeric state.
- `vector<unique_ptr<IStatusEffect>> mEffects` — active debuffs/buffs.

`OnTurnEnd()` calls `effect->OnTurnEnd(mStats)` on each active effect, then
`PurgeExpiredEffects()` walks backwards to safely remove expired ones.

### PlayerCombatant

Adds:
- `vector<unique_ptr<ISkill>> mSkills` — the character's skill list.
- **Pending action slot** — `SetPendingAction(skillIndex, target)` / `HasPendingAction()` / `ClearPendingAction()`.

`BattleState` sets the pending action; `BattleManager` consumes it.
This is the clean separation between UI input handling and combat logic.

### EnemyCombatant

Adds `ChooseTarget(vector<IBattler*>& players)` — returns the first alive player.
AI is intentionally simple for the MVP; it can be replaced with a policy class later.

---

## 5. Skill System

Each skill is an **interchangeable Strategy** (Strategy Pattern).
Skills never mutate stats directly — they return a list of `IAction` objects.

```cpp
struct ISkill {
    virtual const char* GetName()  const = 0;
    virtual bool CanUse(const IBattler& caster) const = 0;

    // Returns the sequence of IActions this skill produces.
    // The sequence is enqueued into ActionQueue and resolved one per frame.
    virtual std::vector<std::unique_ptr<IAction>>
        Execute(IBattler& caster, std::vector<IBattler*>& targets) = 0;
};
```

### Skill List

| Skill | `CanUse` condition | Action sequence produced |
|---|---|---|
| `AttackSkill` | Always | `LogAction` → `DamageAction(atk)` |
| `RageSkill` | `rage == maxRage` | `LogAction` → `DamageAction(atk×2)` → `RageResetAction` |
| `WeakenSkill` | Always | `LogAction` → `StatusEffectAction(WeakenEffect(2,15,10))` |

### How BattleManager wires skill actions to the log

Skills construct `LogAction` with a **null log pointer** (they don't own the log).
`BattleManager::EnqueueSkillActions()` detects each `LogAction` via `dynamic_cast`,
then recreates it with the real `&mBattleLog` pointer before enqueuing.
This is the only documented `dynamic_cast` in the battle system.

---

## 6. Status Effects

```cpp
struct IStatusEffect {
    virtual void Apply(BattlerStats& stats)     = 0;  // called once on AddEffect
    virtual void OnTurnEnd(BattlerStats& stats) = 0;  // decrements duration
    virtual void Revert(BattlerStats& stats)    = 0;  // called when expired
    virtual bool IsExpired() const              = 0;
    virtual std::string GetName() const         = 0;
};
```

### WeakenEffect

Constructor: `WeakenEffect(int duration, int atkReduction, int defReduction)`.

```
Apply:      stats.atk -= atkReduction;  stats.def -= defReduction;  clamp to 0
OnTurnEnd:  mDuration--; when 0 → IsExpired() true
Revert:     stats.atk += atkReduction;  stats.def += defReduction;  (undo Apply)
```

`mApplied` guard: `Revert()` is a no-op if `Apply()` was never called —
prevents double-revert if a combatant is already dead when the effect expires.

### Effect lifecycle in Combatant

```
AddEffect(effect)          → effect->Apply(mStats)  → push to mEffects
OnTurnEnd()                → for each effect: effect->OnTurnEnd(mStats)
PurgeExpiredEffects()      → iterate backward:
                               if IsExpired(): effect->Revert(mStats); remove
```

---

## 7. Action Queue

The `ActionQueue` (`src/Battle/ActionQueue.h/.cpp`) is a `deque<unique_ptr<IAction>>`.

```cpp
struct IAction {
    virtual bool Execute(float dt) = 0;  // return true when complete
};
```

Each frame `BattleManager` calls `mQueue.Update(dt)`:
```cpp
void ActionQueue::Update(float dt) {
    if (mQueue.empty()) return;
    if (mQueue.front()->Execute(dt))  // run the front action
        mQueue.pop_front();            // done → advance to next
}
```

**Why a queue instead of executing everything immediately?**
Multi-step sequences (log → animate → damage → log result) must resolve one
step at a time. Future `PlayAnimationAction` and `WaitAction` types will span
multiple frames. The queue makes the entire combat timeline:
- **Deterministic** — same inputs always produce same results
- **Serializable** — the queue can be saved and replayed
- **Extensible** — add multi-frame effects without changing existing code

### Current Action Types

| Action | Instant? | What it does |
|---|---|---|
| `DamageAction` | ✅ | `target->TakeDamage(rawDmg, attacker)` |
| `StatusEffectAction` | ✅ | `target->AddEffect(std::move(effect))` — transfers ownership |
| `LogAction` | ✅ | `mLog->push_back(message)` + `LOG()` to console |

---

## 8. BattleManager FSM

`BattleManager` drives a six-phase FSM:

```
INIT ──► PLAYER_TURN ──► RESOLVING ──► ENEMY_TURN ──► RESOLVING
                                                │
                                           (repeat loop)
                                                │
                                    AllPlayersDefeated? ──► LOSE
                                    AllEnemiesDefeated? ──► WIN
```

### Phase Details

| Phase | What happens |
|---|---|
| `INIT` | Run `Initialize()` on first Update; transition to `PLAYER_TURN` |
| `PLAYER_TURN` | Wait for `PlayerCombatant::HasPendingAction()` to be true (set by BattleState input) |
| `RESOLVING` | Drain `ActionQueue`; on empty → check WIN/LOSE; call `AdvanceTurn()` |
| `ENEMY_TURN` | AI picks target, enqueues attack actions, calls `OnTurnEnd()`, enters `RESOLVING` |
| `WIN` | Set `mOutcome = VICTORY`; `BattleState` detects and pops |
| `LOSE` | Set `mOutcome = DEFEAT`; `BattleState` detects and pops |

### Turn Order

Built once at `Initialize()` by sorting all combatants by `spd` descending:
```cpp
std::sort(mTurnOrder.begin(), mTurnOrder.end(),
    [](const IBattler* a, const IBattler* b) {
        return a->GetStats().spd > b->GetStats().spd;
    });
```
`AdvanceTurn()` skips dead combatants without rebuilding the vector.
`OnTurnStart()` is called on the incoming combatant (applies buffs, logs name).

---

## 9. BattleState

`BattleState` (`src/States/BattleState.h/.cpp`) is the `IGameState` that glues
the battle system to the game:

**Lifecycle:**
```
OnEnter()  → BattleManager::Initialize()  — spawn combatants, build turn order
Update(dt) → HandleInput() if PLAYER_TURN → BattleManager::Update(dt)
           → detect WIN/LOSE → save HP to PartyManager → PopState()
Render()   → D3DContext::BeginFrame(navy blue)  — no other draw calls yet
OnExit()   → LOG("[BattleState] OnExit")
```

**Input mapping (during PLAYER_TURN only):**

| Key | Action |
|---|---|
| `1` | Select skill slot 0 (Attack) |
| `2` | Select skill slot 1 (Rage — disabled if rage not full) |
| `3` | Select skill slot 2 (Weaken) |
| `Tab` | Cycle to next alive enemy target |
| `Enter` | Confirm skill + target → `SetPlayerAction()` |

State is pushed by `PlayState` when `B` is pressed:
```cpp
StateManager::Get().PushState(std::make_unique<BattleState>(D3DContext::Get()));
```

**Events broadcast on exit:**
- `"battle_end_victory"` — all enemies defeated
- `"battle_end_defeat"` — all players defeated

Any system (e.g. `CutsceneSystem`, reward screen) can subscribe to these events.

---

## 10. Persistent Party HP — PartyManager

`PartyManager` (`src/Systems/PartyManager.h/.cpp`) is a Meyers' Singleton that
stores Verso's `BattlerStats` between battle instances.

```
First battle:
  BattleState::OnEnter → BattleManager::Initialize()
    → PartyManager::Get().GetVersoStats()  (returns full HP on first call)
    → PlayerCombatant("Verso", fullStats) spawned

Battle ends (VICTORY, Verso at 60/100 HP):
  BattleState::Update detects WIN
    → PartyManager::Get().SetVersoStats(verso.GetStats())  — saves 60/100 HP
    → PopState()

Next battle:
  BattleState::OnEnter → BattleManager::Initialize()
    → PartyManager::Get().GetVersoStats()  (returns 60/100 HP)
    → PlayerCombatant("Verso", {hp=60, maxHp=100, ...}) spawned

Verso enters the next fight already wounded — persistence achieved.
```

**Design decisions:**
- `PartyManager` stores a **copy** of `BattlerStats`. It does not know the
  `PlayerCombatant` object exists — no circular dependency.
- Rage is **reset to 0** between battles (intentional — rage is a per-fight resource).
- MP is preserved along with HP (reserved for future skill costs).
- `RestoreFullHP()` is provided for rest-site or save-load scenarios.

---

## 11. How to Add a New Skill

1. Create `src/Battle/MySkill.h/.cpp` inheriting `ISkill`.
2. Implement `GetName()`, `CanUse()`, and `Execute()`.
3. `Execute()` returns a `vector<unique_ptr<IAction>>` — typically `[LogAction, DamageAction]`.
4. Add `mSkills.push_back(std::make_unique<MySkill>())` in `PlayerCombatant::PlayerCombatant()`.
5. Add `MySkill.cpp` to `build_src_static.bat`.

**No existing files change except `PlayerCombatant.cpp`.** This is Open/Closed.

Example — a heal skill:
```cpp
class HealSkill : public ISkill {
    const char* GetName() const override { return "Heal"; }
    bool CanUse(const IBattler& c) const override {
        return c.GetStats().mp >= 20;  // costs 20 MP
    }
    std::vector<std::unique_ptr<IAction>>
    Execute(IBattler& caster, std::vector<IBattler*>&) override {
        std::vector<std::unique_ptr<IAction>> out;
        out.push_back(std::make_unique<LogAction>(nullptr,
            caster.GetName() + " casts Heal!"));
        out.push_back(std::make_unique<HealAction>(&caster, 30));
        return out;
    }
};
```

---

## 12. How to Add a New Status Effect

1. Create `src/Battle/MyEffect.h/.cpp` inheriting `IStatusEffect`.
2. Implement `Apply()`, `OnTurnEnd()`, `Revert()`, `IsExpired()`, `GetName()`.
3. Use `mApplied` guard in `Revert()` to prevent double-revert.
4. In your skill's `Execute()`, produce a `StatusEffectAction(target, std::make_unique<MyEffect>(...))`.
5. Add `.cpp` to `build_src_static.bat`.

**No combat code changes** — `Combatant::AddEffect()` and `OnTurnEnd()` handle everything.

---

## 13. How to Add a New Enemy

For the MVP, enemy data is inline. For the full data-driven path:

1. Create `data/enemies/new_enemy.json` with `name`, `hp`, `atk`, `def`, `spd`.
2. Add a `JsonLoader::LoadEnemyData(path) → EnemyData` call in `BattleManager::Initialize()`.
3. Pass the loaded stats to `EnemyCombatant` constructor.

For now (MVP):
1. In `BattleManager::Initialize()`, add:
   ```cpp
   mEnemies.push_back(std::make_unique<EnemyCombatant>("New Enemy", customStats));
   ```
2. Add the stats overload to `EnemyCombatant`'s constructor if needed.

The `BuildTurnOrder()` call picks up the new enemy automatically — no other changes.

---

## 14. Common Mistakes Reference

| Mistake | Symptom | Fix |
|---|---|---|
| State calls `EndFrame()` | Black flash / flicker on battle entry | Only `GameApp::Render()` calls `EndFrame()` |
| `LOG("text" + str)` | Compiler error | `LOG("%s text", str.c_str())` — printf-style only |
| Missing `#define NOMINMAX` before `<algorithm>` | `C2589 '(' : illegal token on right side of '::'` | Add `#define NOMINMAX` before any Win32 header include in that file |
| Method named `GetMessage()` | Expands to `GetMessageW` in UNICODE builds | Rename to `GetText()`, `GetDescription()`, etc. |
| Calling `Revert()` twice | Stats go negative (ATK below zero) | Use `mApplied` guard in every `IStatusEffect` |
| Not calling `OnTurnEnd()` on combatant | Status effects never expire | `BattleManager::HandlePlayerTurn/EnemyTurn` must call `combatant->OnTurnEnd()` |
| Forgetting to add `.cpp` to `build_src_static.bat` | Linker error: unresolved external symbol | Open `build_src_static.bat` and add the new file path |
| `dynamic_cast` on `IBattler*` in game code | Tight coupling, fragile code | Only `BattleManager::EnqueueSkillActions` may downcast, for the log injection pattern |
| Not saving HP to `PartyManager` before `PopState()` | HP resets to full every battle | `BattleState::Update` must call `PartyManager::Get().SetVersoStats(...)` before popping |
