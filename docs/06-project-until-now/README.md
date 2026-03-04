# Project Overview — Turn-Based JRPG in DirectX 11

**Purpose of this document:**
This document is the entry point for anyone joining this project.
It explains the vision, the complete architecture, every design pattern in use,
the current state of the codebase, and the proposed roadmap for future features.
Read this first before touching any source file.

---

## Table of Contents

1. [Project Vision](#1-project-vision)
2. [Technology Stack](#2-technology-stack)
3. [Folder Structure](#3-folder-structure)
4. [Architecture Layers](#4-architecture-layers)
5. [Design Patterns in Use](#5-design-patterns-in-use)
6. [SOLID Principles Applied](#6-solid-principles-applied)
7. [Key Invariants — Rules You Must Never Break](#7-key-invariants)
8. [What Is Built Today](#8-what-is-built-today)
9. [Proposed Roadmap](#9-proposed-roadmap)
10. [Onboarding Checklist](#10-onboarding-checklist)

---

## 1. Project Vision

This is a **turn-based JRPG** built from scratch in **pure C++ and DirectX 11**
— no game engines, no Unity, no Unreal, no Godot.

The goal is to create a production-quality game with:

| Feature | Status |
|---|---|
| Turn-based combat with a strategic party system | ✅ MVP done |
| Quick Time Events (QTE) that overlay battle | 🔲 Architecture designed, not implemented |
| Voice-acted cutscenes with lip-sync | 🔲 Architecture designed, not implemented |
| In-combat story interruption events | 🔲 Architecture designed, not implemented |
| Diverse, data-driven skill system | ✅ 3 skills working; fully data-driven |
| Animated world-space characters (sprite sheets) | ✅ Done |
| Screen-space HUD and menus | ✅ Done |

**Why no engine?**
Understanding the full stack — Win32 windowing, DirectX device creation, GPU resource
management, frame timing, and COM object lifetimes — produces engineers who can
diagnose any problem at any layer. Engines hide these layers. This project does not.

---

## 2. Technology Stack

| Layer | Technology | Why |
|---|---|---|
| Language | C++17 (`/std:c++17`) | Structured bindings, `if constexpr`, `std::optional` — modern idioms without ABI concerns |
| Ownership | `std::unique_ptr` / `std::shared_ptr` | No raw `new`/`delete`. RAII for all game objects |
| COM objects | `Microsoft::WRL::ComPtr<T>` | Automatic `AddRef`/`Release`; never call `->Release()` manually |
| Graphics | DirectX 11 (D3D11 + DXGI) | Mature, well-documented, wide GPU support on Windows |
| 2D rendering | DirectXTK — `SpriteBatch`, `SpriteFont`, `WICTextureLoader` | GPU-batched sprite drawing; one texture upload via WIC |
| Build system | Manual MSVC batch script (`build_src_static.bat`) | Zero CMake complexity; explicit control of every compiler and linker flag |
| Data format | JSON | Human-readable; all character stats, skills, enemies, and animations are data files |

**Compiler flags:**
```
/std:c++17  /EHsc  /W3  /MTd  /Zi  /DUNICODE  /D_UNICODE
```

- `/MTd` — static CRT; matches the vcpkg DirectXTK build config (x64-windows-static).
- `/DUNICODE /D_UNICODE` — passed via the build script only. **Never** `#define UNICODE` in source files — that causes warning C4005.

---

## 3. Folder Structure

```
d:\lab\vscworkplace\directX\
│
├── src/
│   ├── main.cpp                    — WinMain entry point; creates and runs GameApp
│   ├── Core/
│   │   ├── GameApp.h/.cpp          — Win32 window + game loop coordinator
│   │   └── GameTimer.h/.cpp        — High-resolution timer (QueryPerformanceCounter)
│   ├── Renderer/
│   │   ├── D3DContext.h/.cpp       — Facade: ALL DirectX 11 init/teardown
│   │   ├── Camera.h/.cpp           — Camera2D: zoom, pan, world↔screen transform
│   │   ├── WorldSpriteRenderer.h/.cpp — World-space animated sprite (Camera-aware)
│   │   ├── UIRenderer.h/.cpp       — Screen-space UI sprite renderer
│   │   └── SpriteSheet.h           — Data struct for atlas descriptor (from JSON)
│   ├── Scene/
│   │   ├── IGameObject.h           — Pure virtual: Update(dt), Render(ctx), layer, alive
│   │   └── SceneGraph.h/.cpp       — Owns all IGameObjects; drives Update + Render loops
│   ├── Entities/
│   │   ├── Character.h/.cpp        — OOP character: stats, animation, position
│   │   └── VFXSprite.h/.cpp        — One-shot animated effect (dies on last frame)
│   ├── Components/
│   │   ├── AnimationComponent.h/.cpp — Sprite-sheet clip state machine
│   │   ├── TransformComponent.h    — Position, rotation, scale in world space
│   │   └── StatsComponent.h/.cpp   — HP, MP, ATK, DEF, SPD — loaded from JSON
│   ├── States/
│   │   ├── IGameState.h            — Pure virtual interface for all states
│   │   ├── StateManager.h/.cpp     — Singleton + stack-based state machine
│   │   ├── MenuState.h/.cpp        — Main menu
│   │   ├── PlayState.h/.cpp        — Gameplay (owns SceneGraph, Camera)
│   │   ├── BattleState.h/.cpp      — Turn-based combat (owns BattleManager)
│   │   └── QTEState.h/.cpp         — Quick Time Event overlay state
│   ├── Events/
│   │   └── EventManager.h/.cpp     — Observer pattern — global Pub/Sub event bus
│   ├── Systems/
│   │   ├── ActionQueue.h/.cpp      — Serialized IAction pipeline (MOVED: now in Battle/)
│   │   └── CutsceneSystem.h/.cpp   — Cutscene playback + lip-sync controller
│   ├── Battle/
│   │   ├── BattlerStats.h          — Plain data struct: HP/MP/ATK/DEF/SPD/rage
│   │   ├── IBattler.h              — Pure virtual: the contract every combatant fulfills
│   │   ├── Combatant.h/.cpp        — Base class: owns stats + active status effects
│   │   ├── PlayerCombatant.h/.cpp  — Player side: 3 skills, pending action slot
│   │   ├── EnemyCombatant.h/.cpp   — Enemy side: auto-targets first alive player
│   │   ├── IStatusEffect.h         — Pure virtual: Apply/Revert/OnTurnEnd/IsExpired
│   │   ├── WeakenEffect.h/.cpp     — 2-turn ATK+DEF debuff (mApplied guard)
│   │   ├── ISkill.h                — Pure virtual: CanUse + Execute → vector<IAction>
│   │   ├── AttackSkill.h/.cpp      — Basic attack skill
│   │   ├── RageSkill.h/.cpp        — Rage burst (requires full rage meter)
│   │   ├── WeakenSkill.h/.cpp      — Applies WeakenEffect to target
│   │   ├── IAction.h               — Pure virtual: Execute(dt) → bool (done?)
│   │   ├── DamageAction.h/.cpp     — Instant: applies damage to a target
│   │   ├── StatusEffectAction.h/.cpp — Instant: transfers a status effect to target
│   │   ├── LogAction.h/.cpp        — Instant: appends a message to the battle log
│   │   ├── ActionQueue.h/.cpp      — deque-based FIFO: runs one action per frame
│   │   └── BattleManager.h/.cpp    — FSM (INIT→PLAYER_TURN→RESOLVING→ENEMY_TURN→WIN/LOSE)
│   └── Utils/
│       ├── Log.h                   — Timestamped console logger (printf-style macro)
│       └── HrCheck.h               — HR_CHECK / CHECK_HR macros for HRESULT validation
│
├── data/
│   ├── characters/*.json           — CharacterData (stats, sprite path, animation clips)
│   ├── skills/*.json               — SkillData definitions
│   ├── enemies/*.json              — EnemyData + encounter compositions
│   ├── cutscenes/*.json            — CutsceneScript timing + dialogue
│   └── items/*.json                — ItemData / equipment properties
│
├── assets/
│   ├── animations/*.png            — Sprite sheet atlases (one per character)
│   ├── animations/*.json           — SpriteSheet frame/pivot descriptors
│   └── ui/*.png                    — UI atlases
│
├── docs/
│   ├── 01-gameloop/                — Game loop + GameTimer explanation
│   ├── 02-loading-assets/          — Asset loading pipeline
│   ├── 03-first-game-objects/      — IGameObject + SceneGraph
│   ├── 04-how-did-directX-directXTK-render/ — Full render pipeline (this session)
│   ├── 05-turn-based-battle-simple/— Battle MVP walkthrough
│   └── 06-project-until-now/       — This document
│
├── build_src_static.bat            — THE build script — add new .cpp files here
└── bin/
    └── game.exe                    — Output binary
```

---

## 4. Architecture Layers

The project is organized into strict layers. **Lower layers know nothing about higher layers.**

```
┌─────────────────────────────────────────────────────────────┐
│  DATA LAYER                                                 │
│  JSON files in data/ and assets/                           │
│  Nothing is hardcoded — all tunable values live here       │
└──────────────────────────┬──────────────────────────────────┘
                           │ loaded by JsonLoader
┌──────────────────────────▼──────────────────────────────────┐
│  PLATFORM LAYER                                             │
│  Win32 (HWND, WndProc, PeekMessage)                        │
│  DirectX 11 (device, swap chain, RTV, DSV, viewport)       │
│  GameTimer (QueryPerformanceCounter)                       │
└──────────────────────────┬──────────────────────────────────┘
                           │ initialized by GameApp
┌──────────────────────────▼──────────────────────────────────┐
│  ENGINE CORE LAYER                                          │
│  GameApp       — window + loop coordinator                  │
│  D3DContext    — facade over all DirectX resources         │
│  StateManager  — stack-based state machine                  │
│  EventManager  — global publish/subscribe event bus        │
└──────────────────────────┬──────────────────────────────────┘
                           │ states own
┌──────────────────────────▼──────────────────────────────────┐
│  GAME STATE LAYER                                           │
│  MenuState     — main menu                                  │
│  PlayState     — overworld / exploration                    │
│  BattleState   — turn-based combat, owns BattleManager     │
│  QTEState      — quick time event overlay                  │
└──────────────────────────┬──────────────────────────────────┘
                           │ states own and drive
┌──────────────────────────▼──────────────────────────────────┐
│  GAME SYSTEMS LAYER                                         │
│  SceneGraph    — owns all IGameObjects                      │
│  BattleManager — FSM + turn order + ActionQueue            │
│  ActionQueue   — serialized IAction pipeline               │
│  CutsceneSystem — cutscene playback + lip-sync             │
└──────────────────────────┬──────────────────────────────────┘
                           │ systems operate on
┌──────────────────────────▼──────────────────────────────────┐
│  ENTITY / COMPONENT LAYER                                   │
│  IGameObject / Character / Enemy / VFXSprite               │
│  TransformComponent / AnimationComponent / StatsComponent  │
│  IBattler / Combatant / PlayerCombatant / EnemyCombatant   │
└─────────────────────────────────────────────────────────────┘
```

**The dependency arrow always points downward. No upward dependencies.**
`BattleManager` does not know `BattleState` exists. `Character` does not know
`SceneGraph` exists. `D3DContext` does not know any state or entity exists.

---

## 5. Design Patterns in Use

### 5.1 Facade — `D3DContext`

`D3DContext` wraps the entire DirectX 11 API surface behind three methods:
`Initialize()`, `BeginFrame()`, `EndFrame()`.
All states and renderers call `D3DContext::Get()` — none of them touch
`ID3D11Device*` or `IDXGISwapChain*` directly.

```
States → D3DContext::Get().GetDevice()      (read-only access for resource creation)
States → D3DContext::Get().BeginFrame()     (for overlay states only)
GameApp → D3DContext::Get().BeginFrame() + EndFrame()   (sole Present owner)
```

### 5.2 Singleton (Meyers' Singleton) — `D3DContext`, `StateManager`, `EventManager`

All three use the Meyers' Singleton:
```cpp
static T& Get() {
    static T instance;  // constructed once, destroyed at program exit
    return instance;
}
```
This is thread-safe in C++11+ (magic statics) and requires no manual `new`/`delete`.

**Access pattern:**
```cpp
EventManager::Get().Broadcast("battle_start", {});
StateManager::Get().PushState(std::make_unique<BattleState>(...));
```

### 5.3 State Pattern (Stack FSM) — `StateManager`

`StateManager` maintains a `std::stack<std::unique_ptr<IGameState>>`.
Only the **top** state receives `Update(dt)` and `Render()` each frame.

| Operation | Use case |
|---|---|
| `PushState(state)` | Overlay without losing context — BattleState over PlayState, QTEState over BattleState |
| `PopState()` | Return to the state below — battle ends, PlayState resumes |
| `ChangeState(state)` | Full scene transition — no return |

**Stack visualization for a QTE mid-battle:**
```
[bottom]  PlayState       ← paused (not updated, not rendered)
          BattleState     ← paused
[top]     QTEState        ← active (receives Update + Render)
```

When `QTEState` pops, `BattleState` becomes the top and resumes instantly.

### 5.4 Observer Pattern — `EventManager`

`EventManager` is the single global event bus. No state directly calls another state.

```cpp
// Subscribe — returns a ListenerID for later unsubscription
ListenerID id = EventManager::Get().Subscribe("boss_half_health", [this](EventData) {
    // fire cutscene
});

// Broadcast — fires all listeners registered for this event
EventManager::Get().Broadcast("boss_half_health", {});

// Unsubscribe in OnExit() — prevents dangling lambda captures after state destruction
EventManager::Get().Unsubscribe("boss_half_health", id);
```

**Cross-system flow example:**
```
BattleState: boss.hp < 50%
  → EventManager::Broadcast("boss_half_health")
    → CutsceneSystem listener fires
    → StateManager::PushState(CutsceneState("boss_intro"))
      → BattleState paused underneath on the stack
        → Cutscene ends → PopState() → BattleState resumes
```

### 5.5 Template Method — `Combatant`

`Combatant` defines the skeleton algorithm for `OnTurnStart()` / `OnTurnEnd()` / `TakeDamage()`.
`PlayerCombatant` and `EnemyCombatant` override only the parts that differ
(skill selection, AI targeting) without reimplementing damage calculation or
status effect processing.

### 5.6 Strategy Pattern — `ISkill`

Each skill is an interchangeable strategy:
```cpp
struct ISkill {
    virtual bool CanUse(const IBattler& caster) const = 0;
    virtual std::vector<std::unique_ptr<IAction>>
        Execute(IBattler& caster, std::vector<IBattler*>& targets) = 0;
};
```
`AttackSkill`, `RageSkill`, `WeakenSkill` are concrete strategies.
New skills are added by creating a new class — no `if/else` chains, no switches.
This is the **Open/Closed Principle** in practice.

### 5.7 Command Pattern — `IAction`

Every discrete step in the combat timeline is an `IAction` object:
```cpp
struct IAction {
    virtual bool Execute(float dt) = 0;  // returns true when complete
};
```
Concrete commands: `DamageAction`, `StatusEffectAction`, `LogAction`, `WaitAction`,
`PlayAnimationAction`, `SpawnVFXAction`.

Skill execution never directly mutates state — it returns a `vector<unique_ptr<IAction>>`.
The `ActionQueue` runs them one at a time, sequentially. This makes the combat
timeline **deterministic**, **replayable**, and **serializable** (for replays or netcode).

### 5.8 Chain of Responsibility — `ActionQueue`

`ActionQueue` is a `deque<unique_ptr<IAction>>`. Each frame, only the front action runs:
```cpp
void ActionQueue::Update(float dt) {
    if (mQueue.empty()) return;
    if (mQueue.front()->Execute(dt))   // returns true when done
        mQueue.pop_front();             // advance to next action
}
```
Multi-step skill sequences (log → animate → damage → log result) are simply
multiple `IAction` objects pushed in order. The queue handles sequencing automatically.

### 5.9 Component Pattern — Composition over Inheritance

Behaviors that must appear on unrelated entity types (animation, physics, stats)
are encapsulated as components. `GameObject` owns them by value and delegates:

```cpp
class Character : public IGameObject {
    TransformComponent  mTransform;   // position, rotation, scale
    AnimationComponent  mAnimation;   // sprite sheet clip state machine
    StatsComponent      mStats;       // HP/MP/ATK/DEF/SPD — from JSON
};

void Character::Update(float dt) {
    mAnimation.Update(dt);  // Character does not re-implement animation logic
}
```

### 5.10 Factory Method — `SceneGraph::Spawn<T>`

```cpp
template<typename T, typename... Args>
T* SceneGraph::Spawn(Args&&... args) {
    auto obj = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = obj.get();
    mObjects.push_back(std::move(obj));
    return raw;  // observer pointer; SceneGraph holds sole ownership
}
```
States never call `new` directly. All entity creation goes through `Spawn<T>()`.

---

## 6. SOLID Principles Applied

### S — Single Responsibility

| Class | Sole responsibility |
|---|---|
| `D3DContext` | DirectX resource management — nothing else |
| `GameTimer` | High-resolution time tracking |
| `SceneGraph` | Owns and drives IGameObjects |
| `ActionQueue` | Sequences IAction objects one frame at a time |
| `AnimationComponent` | Sprite sheet frame timing |
| `TransformComponent` | Position / rotation / scale storage |

### O — Open/Closed

Adding a new skill: create a new `ISkill` subclass. Zero existing files change.
Adding a new status effect: create a new `IStatusEffect` subclass. Zero existing files change.
Adding a new state: create a new `IGameState` subclass. `StateManager` does not change.

### L — Liskov Substitution

`BattleState` uses `IBattler*` everywhere — it never needs to know whether
a pointer is a `PlayerCombatant` or `EnemyCombatant`. The exception is one
documented `dynamic_cast<LogAction*>` in `BattleManager::EnqueueSkillActions()`
to inject the battle log pointer, which is explicitly justified in a comment.

### I — Interface Segregation

`IBattler` — combat-facing interface (damage, effects, turn callbacks).
`ISkill` — skill-facing interface (CanUse, Execute).
`IAction` — queue-facing interface (Execute one frame at a time).
`ICutsceneSystem` — state-facing interface (Play, IsPlaying, Stop).

Cross-cutting systems expose minimal interfaces. States depend on the interface,
not the implementation (Dependency Inversion).

### D — Dependency Inversion

States receive `D3DContext&` by reference (injected via constructor).
`CutsceneSystem` is injected as `ICutsceneSystem*` — states never `#include` the
concrete implementation header.
`BattleManager` does not know `BattleState` exists; it exposes `GetOutcome()`
and `BattleState` polls it.

---

## 7. Key Invariants — Rules You Must Never Break

These rules are load-bearing. Violating any of them causes hard-to-diagnose bugs.

### Rule 1 — Only `GameApp::Render()` calls `EndFrame()` / `Present()`

```cpp
// ✅ CORRECT — GameApp is the sole Present owner
void GameApp::Render() {
    D3DContext::Get().BeginFrame(...);
    StateManager::Get().Render();
    D3DContext::Get().EndFrame();   // ← only here
}

// ❌ WRONG — state calling Present causes double-present → flicker / black screen
void SomeState::Render() {
    mD3D.EndFrame();   // NEVER do this
}
```

An overlay state may call `BeginFrame()` again to re-clear with a different color.
It must never call `EndFrame()`.

### Rule 2 — `LOG()` is printf-style

```cpp
// ❌ WRONG — C++ string concatenation passed to printf-style macro
LOG("Damage: " + std::to_string(dmg));

// ✅ CORRECT
LOG("Damage: %d", dmg);
LOG("%s took %d damage.", target->GetName().c_str(), dmg);
```

### Rule 3 — `#define NOMINMAX` before `<algorithm>` (or any Win32 header)

Win32 headers define `min` and `max` as macros, which break `std::max`/`std::min`.

```cpp
// ✅ CORRECT — at the top of any .cpp that includes Win32 headers AND uses std::max
#define NOMINMAX
#include <algorithm>
```

### Rule 4 — `GetMessage()` collides with Win32 `GetMessageW` macro

Do NOT name any class method `GetMessage()` in a file that includes `<windows.h>`.
The macro expansion causes a compiler error. Use `GetText()`, `GetDescription()`, etc.

### Rule 5 — Never call `->Release()` on a `ComPtr`

`ComPtr<T>` automatically calls `Release()`. Use `.Reset()` for explicit early release.

### Rule 6 — `Subscribe()` in `OnEnter()` must have matching `Unsubscribe()` in `OnExit()`

Dangling lambda captures crash when a broadcast fires after the state is destroyed.

### Rule 7 — Every new `.cpp` file must be added to `build_src_static.bat`

The build script has a hard-coded source list. Forgetting this produces "unresolved
external symbol" linker errors that look like missing function definitions.

### Rule 8 — Zero hardcoded game values in `.cpp` files

All character stats, skill parameters, animation frame rates, and enemy compositions
come from JSON data files. A numeric literal representing a game tuning value in a
`.cpp` file is a defect. The only permitted hardcoded constants are:
- Window title string (in `main.cpp`)
- Default screen resolution (in `GameApp.cpp`)
- Layer Z-order enum ranges (in `IGameObject.h`)

### Rule 9 — `SpriteBatch::Begin()` gets `camera.GetViewMatrix()` — not `GetViewProjectionMatrix()`

SpriteBatch internally multiplies your matrix by `pixel→NDC`. If you supply a matrix
that already includes `→NDC`, the sprite is double-projected and becomes invisible.

### Rule 10 — All time-dependent values scale by `deltaTime`

No `Sleep()`, no `GetTickCount()`, no magic frame counts. Every timer, animation
frame advance, and movement value is multiplied by `dt` (seconds since last frame).

---

## 8. What Is Built Today

### Working Systems

| System | Location | State |
|---|---|---|
| Win32 window + game loop | `GameApp` | ✅ Complete |
| DirectX 11 device + swap chain | `D3DContext` | ✅ Complete |
| High-resolution frame timer | `GameTimer` | ✅ Complete |
| Stack-based state machine | `StateManager` | ✅ Complete |
| Global event bus | `EventManager` | ✅ Complete |
| Animated world-space sprites | `WorldSpriteRenderer` | ✅ Complete |
| Screen-space UI sprites | `UIRenderer` | ✅ Complete |
| Camera2D (pan + zoom) | `Camera2D` | ✅ Complete |
| Scene graph with layer sorting | `SceneGraph` | ✅ Complete |
| Battle combatants + stats | `IBattler / Combatant` | ✅ Complete |
| Skills (Attack, Rage, Weaken) | `ISkill` subtypes | ✅ Complete |
| Status effects (Weaken) | `IStatusEffect` subtypes | ✅ Complete |
| Action queue (Command pattern) | `ActionQueue` | ✅ Complete |
| Battle FSM + turn order | `BattleManager` | ✅ Complete |
| BattleState (rendered, input) | `BattleState` | ✅ MVP done |

### Playable Flow

1. Game starts → `MenuState`
2. Player presses `Enter` → transitions to `PlayState` (world, character walks)
3. Player presses `B` → `BattleState` pushed onto the stack
4. Combat: Hero vs Goblin A + Goblin B (2 goblins)
5. Player selects skill with `1`/`2`/`3`, cycles targets with `Tab`, confirms with `Enter`
6. Enemy AI auto-attacks after player resolves
7. WIN/LOSE detected → `BattleState` pops → `PlayState` resumes

All combat state output is visible via `OutputDebugStringA` in the VS Output window
(full battle log with HP values, damage numbers, effect applications).

---

## 9. Proposed Roadmap

### Sprint A — Render the Battle UI (High Priority)

The battle system is fully functional but currently invisible to the player
(all output goes to the debug console). The next step is drawing it.

**Files to create:**
- `src/Renderer/BattleFontRenderer.h/.cpp` — wrapper around `DirectXTK::SpriteFont`
  to draw HP bars, names, skill menus, and battle log text on screen.
- `src/UI/BattleHUD.h/.cpp` — owns font renderer instances; called by `BattleState::Render()`.

**Approach:**
1. Download a `SpriteFont` `.spritefont` file (or generate with `MakeSpriteFont` tool).
2. `SpriteFont::DrawString()` — screen-space text at pixel coordinates.
3. Draw HP bars as colored `SpriteBatch::Draw()` calls on a 1×1 white texture, scaled to bar width.

### Sprint B — Data-Driven Characters and Enemies

Currently `PlayerCombatant` and `EnemyCombatant` have hardcoded stats.

**Files to create / modify:**
- `src/Systems/JsonLoader.h/.cpp` — load `CharacterData`, `EnemyData`, `SkillData` from JSON.
- Update `BattleManager::Initialize()` to read `data/enemies/goblins.json` instead of hardcoded values.
- Add `data/characters/hero.json` with all stats.

**Pattern:** `BattleManager::Initialize()` calls `JsonLoader::LoadEnemy("goblin")` which
returns `EnemyData`; `BattleManager` passes it to `EnemyCombatant` constructor.

### Sprint C — More Skills and Status Effects

Extend the skill system without touching existing code (Open/Closed):
- `HealSkill` — restore HP proportional to caster's SPD.
- `ShieldSkill` — reduce incoming damage for 2 turns via `ShieldEffect`.
- `RageStrikeSkill` — deal damage + self-apply rage generation buff.
- `PoisonEffect` / `BurnEffect` — tick damage status effects.

Each skill/effect is a new `.h/.cpp` pair. `BattleManager` or `PlayerCombatant`
registers them at init time based on JSON data.

### Sprint D — QTE System

`QTEState` is the architecture anchor:
```
BattleState: enemy attack resolves
  → ActionQueue: WaitForQTEAction::Execute()
    → StateManager::PushState(QTEState("dodge_window"))
      → QTEState: countdown timer, reads input
      → Success → Broadcast("qte_success") → damage halved
      → Failure → Broadcast("qte_failure") → full damage
      → QTEState pops → BattleState resumes, DamageAction runs
```

Files needed: `src/States/QTEState.h/.cpp`, `src/Battle/WaitForQTEAction.h/.cpp`.

### Sprint E — Cutscene and Lip-Sync System

`CutsceneSystem` plays a scripted sequence from `data/cutscenes/*.json`:
- Each entry: `{ "character": "hero", "dialogue": "...", "audio": "hero_line_01.wav", "duration": 3.2 }`
- `CutsceneState` advances entries on audio completion (via XAudio2 position query).
- `UIRenderer` draws portrait + dialogue box.
- `AnimationComponent::PlayClip("talk")` runs while audio plays (lip-sync approximation).

### Sprint F — Save System

`SaveManager` (Singleton) serializes the active party state to a JSON file:
- Current HP/MP/rage for each character.
- Inventory and equipment.
- World position and active scene.

---

## 10. Onboarding Checklist

Complete this list before writing your first line of code:

- [ ] **Read** `docs/01-gameloop/gameloop.md` — understand `deltaTime` and `GameTimer`
- [ ] **Read** `docs/04-how-did-directX-directXTK-render/README.md` — understand the render pipeline
- [ ] **Build the project:** open a VS 2022 x64 Native Tools command prompt, `cd` to workspace root, run `.\build_src_static.bat 2>&1`
- [ ] **Run `bin\game.exe`** — you should see the game window and be able to press `B` to enter battle
- [ ] **Open the VS Output window** — press `B` and observe the battle log printed by `OutputDebugStringA`
- [ ] **Understand `IGameState` lifecycle** — `OnEnter → Update(dt) → Render → OnExit`
- [ ] **Understand `IAction` contract** — `Execute(dt)` returns `true` when done; `false` to continue next frame
- [ ] **Understand `IBattler` contract** — all combat logic goes through this interface; no raw `PlayerCombatant*` casting unless justified
- [ ] **Read `build_src_static.bat`** — you MUST add every new `.cpp` file to this script or get linker errors
- [ ] **Internalize Rule 1** — only `GameApp::Render()` calls `EndFrame()` / `Present()`

Welcome to the team. The architecture is designed to be extended, not rewritten.
When in doubt, add a new class — do not modify an existing interface.
