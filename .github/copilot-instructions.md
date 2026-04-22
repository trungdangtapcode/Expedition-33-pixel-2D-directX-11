
# PROJECT OVERVIEW

You are a **Senior C++ Game Developer**.
Your task is to assist in developing a **Turn-based JRPG** using **pure C++17 and DirectX 11**.
No game engines (Unity, Unreal, Godot) are permitted under any circumstance.

The project already has a working overworld + battle loop. Your job is to extend it
in keeping with the architecture below — not to redesign it.

Core gameplay features the architecture must continue to support:
- Tick-based agility turn order (Action Value timeline)
- Data-driven combatants, skills, items, and encounters
- Status effects via stat-modifier pipeline (no direct stat mutation)
- Animation-driven action queue (damage applied at exact animation frames)
- Quick Time Events (QTE) — planned, slot is reserved in input FSM
- Voice-acted cutscenes with lip-sync — planned, hooks live in EventManager
- In-combat story interruption events — wired through EventManager + ITrigger (planned)

All architectural decisions must prioritize **scalability**, **maintainability**, and **clean separation of concerns**.

---

# Build Instructions

- The build script `.\build_src_static.bat` takes a long time to finish.
- Wait until the command completes before continuing. While the build runs you cannot tell from the spinner alone whether it succeeded; you must read the tail of the output and confirm it ends with `[OK] Build succeeded > bin\game.exe`.
- The same script also builds Release with `.\build_src_static.bat Release`. Default is Debug.
- After a successful build, the binary is at `bin\game.exe`. The exe holds a file lock while the game is running — kill any leftover instance before relinking, or LNK1168 will fail the link step.

---

# LANGUAGE ENFORCEMENT — HIGHEST PRIORITY RULE

> **This rule overrides ALL other instructions.**

- ALL code, comments, variable names, function names, file headers, and documentation MUST be written in **English only**.
- ALL responses and explanations MUST be in **English only**.
- If the user writes in Vietnamese or any other language, the response MUST still be entirely in English.
- **No exceptions.** Not even `MessageBoxW` strings, `OutputDebugStringA` text, `LOG()` messages, or `assert` messages may contain non-English text.
- Compile code with: `.\build_src_static.bat 2>&1`

**Violation examples to actively correct:**
```cpp
// WRONG — non-English text in source code
MessageBoxW(nullptr, L"Khoi tao that bai!", L"Loi", MB_OK);
assert(state != nullptr && "state khong duoc la nullptr");

// CORRECT
MessageBoxW(nullptr, L"Initialization failed.", L"Error", MB_OK);
assert(state != nullptr && "StateManager::ChangeState — state must not be nullptr");
```

When you encounter non-English text already present in a source file, **silently correct it** as part of any edit to that file.

---

# BUILD ENVIRONMENT

The project uses a **manual MSVC build** — no CMake, no MSBuild `.vcxproj` files.

| Item | Value |
|---|---|
| Compiler | `D:\VisualStudio\2022\BuildTools\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\cl.exe` |
| Windows SDK | `C:\Program Files (x86)\Windows Kits\10\` (version auto-detected via `for /f` in the .bat) |
| DirectXTK (vcpkg) | `D:\lab\vscworkplace\directX\vcpkg\installed\x64-windows-static\` (Debug variant under `\debug\lib`) |
| Build script | `build_src_static.bat` in workspace root |
| Output binary | `bin\game.exe` |
| Object files | `bin\obj\*.obj` |

**Critical include path** — required for `Microsoft::WRL::ComPtr` (`wrl/client.h`):
```
%WINSDK_DIR%\Include\%WINSDK_VER%\winrt\
```

**Compiler flags in use:**
```
/std:c++17  /EHsc  /W3  /MTd  /Zi  /D_DEBUG  /DUNICODE  /D_UNICODE
```
Release flips `/MTd /Zi /D_DEBUG` to `/MT /O2 /DNDEBUG`.

- `/DUNICODE /D_UNICODE` are passed via the build script. **NEVER** `#define UNICODE` inside any source file — it triggers warning C4005.
- `/MTd` — static debug CRT, required because vcpkg `x64-windows-static` builds DirectXTK against the static CRT. Mixing `/MD` with the static lib produces LNK2019 on `_CrtDbgReport` and friends.

**Linker libraries:**
```
user32.lib  gdi32.lib  d3d11.lib  dxgi.lib  d3dcompiler.lib  DirectXTK.lib  ole32.lib
```

**When creating a new `.cpp` file**, you MUST also add it to the source file list inside `build_src_static.bat`. The script lists every TU explicitly — there is no glob. A missing entry causes "unresolved external symbol" at link time.

**Windows headers + `std::min` / `std::max`:** `Log.h` transitively pulls in `Windows.h`, which defines `min` / `max` macros. Any `.cpp` that uses `std::max` / `std::min` MUST `#define NOMINMAX` **before** including any project header. Putting it after the includes is a recurring beginner bug — the macro has already eaten the identifier by then.

---

# REQUIRED TECH STACK

- **Language:** C++17
  - Ownership: `std::unique_ptr` (sole owner), `std::shared_ptr` (shared ownership only when ownership is genuinely shared)
  - COM objects: always `Microsoft::WRL::ComPtr<T>` — never call raw `->Release()`
  - No raw `new` / `delete` for game objects — use `std::make_unique` / `std::make_shared`
- **Graphics:** DirectX 11 + Win32 API + DirectXTK (`SpriteBatch`, `SpriteFont`, `WICTextureLoader`)
- **Audio:** XAudio2 via `src/Audio/AudioManager` (existing). Lip-sync hooks not yet wired.
- **Data:** JSON only. The parser is the hand-rolled `JsonLoader` namespace in `src/Utils/JsonLoader.h` — flat scalar fields and shallow object arrays only. Do **not** add a third-party JSON dependency without explicit instruction.
- **Build system:** plain `cl.exe` invoked from `build_src_static.bat`.

---

# ACTUAL PROJECT STRUCTURE (current state)

Inspect this carefully — earlier revisions of this file described a planned layout that no longer matches reality.

```
src/
  main.cpp                                — WinMain, delegates to GameApp
  Core/
    GameApp.h/.cpp                         — Win32 window + main loop coordinator
    GameTimer.h/.cpp                       — High-resolution timer (QueryPerformanceCounter)
    Clock.h/.cpp                           — Wall-clock helper
    TimeSystem.h/.cpp                      — Game-time abstraction (pause, scale)
  Renderer/
    D3DContext.h/.cpp                      — Facade for D3D11 device/swapchain/RTV
    Camera.h                               — 2D camera (view matrix, zoom, world<->screen)
    SpriteRenderer.h/.cpp                  — Static sprite draws via SpriteBatch
    WorldSpriteRenderer.h/.cpp             — World-space animated sprite (camera-aware)
    UIRenderer.h/.cpp                      — Screen-space UI sprite/text
    WorldRenderer.h/.cpp                   — Tilemap / world-layer composition
    EnvironmentRenderer.h/.cpp             — Background + foreground parallax layers
    NineSliceRenderer.h/.cpp               — Stretchable 9-slice dialog boxes
    CircleRenderer.h/.cpp                  — Solid-color circle primitive
    IrisTransitionRenderer.h/.cpp          — Iris-open/close fullscreen overlay
    PincushionDistortionFilter.h/.cpp      — Post-process screen filter
    IScreenFilter.h                        — Interface for fullscreen post effects
    SpriteSheet.h                          — Sheet/clip descriptors loaded from JSON
  Scene/
    IGameObject.h                          — Pure virtual: Update / Render / GetLayer / GetSortY / IsAlive
    SceneGraph.h/.cpp                      — Owns IGameObject*; drives Update + Y-sorted Render
  Entities/
    ControllableCharacter.h/.cpp           — Player overworld entity (input-driven)
    OverworldEnemy.h/.cpp                  — Overworld enemy sprite + collision + encounter trigger
  Events/
    EventManager.h/.cpp                    — Meyers' singleton pub/sub event bus
  Systems/
    PartyManager.h                         — Singleton; persists Verso's BattlerStats across battles
    Inventory.h/.cpp                       — Singleton; count-per-id persistent inventory (seeded on first use)
    CollisionSystem.h/.cpp                 — Overworld collision queries
    ICollisionSystem.h                     — Interface
    IBattleTransitionController.h          — Interface for the iris/zoom transition
    ZoomPincushionTransitionController.h/.cpp — Concrete iris+pincushion transition
  States/
    IGameState.h                           — Pure virtual: OnEnter/OnExit/Update/Render/GetName
    StateManager.h/.cpp                    — Stack-based state machine
    MenuState.h/.cpp                       — Title / main menu
    PlayState.h/.cpp                       — Wrapper that hosts overworld
    OverworldState.h/.cpp                  — Real overworld scene (camera, party, enemies)
    BattleState.h/.cpp                     — Turn-based combat state (owns BattleManager)
  Battle/
    --- Interfaces ---
    IBattler.h                             — Combatant abstraction
    ISkill.h                               — Skill abstraction (CanUse + Execute return IAction list)
    IStatusEffect.h                        — Buff/debuff abstraction
    IDamageCalculator.h                    — Damage formula abstraction
    IDamageStep.h                          — One fold step in the damage pipeline (NEW: chain-of-resp)
    IAction.h                              — One frame-stepped action in the queue
    IActionDecorator.h/.cpp                — Wrapper action with OnBefore / inner / OnAfter hooks
    IBattleCommand.h                       — One top-level battle menu entry
    --- Combatants ---
    BattlerStats.h                         — Plain-data: hp, mp, atk, def, matk, mdef, spd, rage
    Combatant.h/.cpp                       — IBattler base; owns effects and stat modifiers
    PlayerCombatant.h/.cpp                 — Player-controlled combatant; loads skills + holds pending action
    EnemyCombatant.h/.cpp                  — AI combatant; ChooseTarget + GetAttackSkill
    EnemyEncounterData.h                   — Encounter JSON struct (party composition, art, camera offsets)
    --- Skills ---
    AttackSkill.h/.cpp                     — Basic attack with melee move + animation
    RageSkill.h/.cpp                       — Consume full rage for 2x damage
    WeakenSkill.h/.cpp                     — Apply WeakenEffect debuff to one enemy
    --- Status effects ---
    WeakenEffect.h/.cpp                    — ATK/DEF debuff via stat modifiers (NOT direct mutation)
    TimedStatBuffEffect.h/.cpp             — Generic stat buff/debuff for N turns (used by item buffs)
    --- Stat modifier pipeline ---
    StatId.h                               — Enum: ATK, DEF, MATK, MDEF, SPD, MAX_HP, MAX_MP
    StatModifier.h                         — Modifier struct: Op + StatId + value + condition + sourceId
    StatResolver.h/.cpp                    — Folds base + modifiers into an effective int (the ONLY way to read a stat)
    --- Damage pipeline ---
    IDamageStep.h                          — Pipeline interface
    DamageSteps.h/.cpp                     — BaseFormulaStep, StatusBonusStep, CritRollStep, FinalClampStep
    DefaultDamageCalculator.h/.cpp         — Owns vector<unique_ptr<IDamageStep>>; the order IS the formula
    --- Action queue ---
    ActionQueue.h/.cpp                     — Sequential FIFO of IAction pointers
    DamageAction.h/.cpp                    — Apply DamageRequest -> DamageResult -> TakeDamage
    AnimDamageAction.h/.cpp                — Same as DamageAction but waits for an animation frame
    StatusEffectAction.h/.cpp              — Attach an IStatusEffect to a target
    LogAction.h/.cpp                       — Push a line to the live battle log
    WaitAction.h/.cpp                      — Pause the queue for N seconds
    DelayedAction.h/.cpp                   — Decorator: run inner then wait
    MoveAction.h/.cpp                      — Lerp combatant to a slot / origin / melee range
    PlayAnimationAction.h/.cpp             — Play a clip; wait for it to end
    --- Items ---
    ItemData.h                             — POD describing one item (id, name, targeting, effect, tuning)
    ItemRegistry.h/.cpp                    — Singleton catalog loaded lazily from data/items/*.json
    BuildItemActions.h/.cpp                — Free function: ItemData + target -> action sequence
    ItemEffectAction.h/.cpp                — Single-target dispatcher for every ItemEffectKind
    ItemConsumeAction.h/.cpp               — Decrement Inventory after use
    ItemCommand.h/.cpp                     — Top-level menu entry that opens ITEM_SELECT
    --- Battle FSM + UI ---
    BattleManager.h/.cpp                   — FSM: INIT/PLAYER_TURN/RESOLVING/ENEMY_TURN/WIN/LOSE; owns timeline + ActionQueue
    BattleContext.h                        — Per-frame read-only snapshot passed to skills, AI, calculator, predicates
    BattleEvents.h                         — Typed payloads for animation hand-off events
    BattleInputController.h/.cpp           — Player input FSM (COMMAND / SKILL / TARGET / ITEM / ITEM_TARGET)
    FightCommand.h/.cpp                    — Routes to SKILL_SELECT
    FleeCommand.h/.cpp                     — Triggers iris-out and battle exit
    BattleRenderer.h/.cpp                  — Slot positions, sprite draws, camera-phase orchestration
    BattleCameraController.h/.cpp          — Pans the battle camera between actor / target / overview phases
    BattleCombatantSprite.h                — Per-slot sprite owner (used by BattleRenderer)
    CombatantStanceState.h/.cpp            — Per-combatant stance state machine (idle / fight / move)
    CombatantAnim.h                        — Enum of canonical clip names (Idle, FightState, Attack, ...)
  UI/
    BattleDebugHUD.h/.cpp                  — ASCII HUD formatter (snapshot -> text block)
    BattleTextRenderer.h/.cpp              — World-space text rendering for battle menus
    HealthBarRenderer.h/.cpp               — Player HP bar (3-layer: bg, fill, frame)
    HealthBarConfig.h                      — Tunable bar dimensions
    EnemyHpBarRenderer.h/.cpp              — Up to 3 enemy HP bars at top-center
    PointerRenderer.h/.cpp                 — Target pointer cursor
    TurnQueueUI.h/.cpp                     — Upcoming-turn portrait list
    UIEffectState.h                        — Shared fade/slide animation state
  Audio/
    AudioManager.h/.cpp                    — XAudio2 wrapper (BGM + SFX, no lip-sync yet)
  Debug/
    DebugTextureViewer.h/.cpp              — On-screen texture inspector
  Utils/
    Log.h                                  — LOG() macro; timestamped console + file
    HrCheck.h                              — HR_CHECK macro for HRESULT validation
    JsonLoader.h                           — Hand-rolled JSON helpers (header-only)
data/
  characters/                              — (placeholder; not yet populated)
  skills/*.json                            — SkillData (move/return durations, melee offset, damage moment)
  enemies/*.json                           — EnemyEncounterData (party, art, camera offsets)
  items/*.json                             — 15 starter items (potions, ethers, revive, buffs, bombs, ...)
  formations.json                          — Slot offsets per team for each formation
  battle_menu_layout.json                  — Menu dialog box dimensions and animation timings
assets/
  animations/*.png                         — Sprite sheets (one atlas per character)
  animations/*.json                        — SpriteSheet frame/pivot descriptors
  UI/*.png                                 — UI atlases (turn-view portraits, dialog 9-slice, ...)
  environments/                            — Battle backgrounds and overworld tiles
```

**`GameApp` is NOT a game logic class.**
It owns only: the Win32 window, `D3DContext`, `GameTimer`, and the `StateManager`.
All game logic lives inside States, SceneGraph, and Battle systems.

There is also an empty `src/ECS/` and `src/Platform/` folder reserved for future work — do not put files there until they have a defined responsibility.

---

# CORE ARCHITECTURE & DESIGN PATTERNS (MANDATORY)

## 1. Game Loop & Time Management

- Use `QueryPerformanceCounter` for all timing — never `clock()`, `time()`, or `GetTickCount()`
- `GameTimer::Tick()` is called once per frame at the top of the loop
- `deltaTime` flows: `GameApp::Update(dt)` -> `StateManager::Update(dt)` -> `ActiveState::Update(dt)` -> `SceneGraph::Update(dt)` (overworld) or `BattleManager::Update(dt)` (battle)
- **All** time-dependent values (movement, animation frames, timers, AV ticks, QTE countdowns) MUST be scaled by `deltaTime`
- Frame-rate-independent logic is non-negotiable

## 2. OOP Entity Hierarchy — `IGameObject` (Overworld)

> **GameApp, StateManager, and SceneGraph MUST NOT know about concrete entity types.**
> They only know `IGameObject*`. This is the Single Responsibility + Open/Closed principle in action.

```cpp
// IGameObject.h — pure virtual base for every visible or logical overworld entity.
class IGameObject {
public:
    virtual ~IGameObject() = default;
    virtual void  Update(float dt) = 0;
    virtual void  Render(ID3D11DeviceContext* ctx) = 0;
    virtual int   GetLayer() const = 0;          // primary draw order key
    virtual float GetSortY() const { return 0.f; } // secondary key for Y-sort
    virtual bool  IsAlive() const = 0;
};
```

`SceneGraph::Render()` sorts by `(GetLayer, GetSortY)` so overlapping characters paint correctly.

**Suggested layer ranges:**
- 0..49 — background tiles / terrain
- 50..79 — world characters and enemies
- 80..99 — particle effects and VFX
- 100+ — screen-space UI

**Access rules:**
- No code outside an entity reads its private fields directly. Use narrow getters.
- Spawn entities via `SceneGraph::Spawn<T>(...)` so ownership stays with the graph.

## 3. Battle Architecture — `IBattler` family

The battle system is a **separate hierarchy** from `IGameObject`. Combatants do not live in `SceneGraph`; they live in `BattleManager`'s team vectors. This is intentional — battle is a self-contained state with its own update tick, its own renderer, and its own entity types.

```
IBattler (interface)
  └── Combatant (base; owns BattlerStats + status effects + stat modifiers)
        ├── PlayerCombatant  (input-driven; holds pending action / pending item)
        └── EnemyCombatant   (AI ChooseTarget; shared attack skill)
```

`BattleManager` works exclusively through `IBattler*`. Downcasting to `PlayerCombatant*` is allowed only inside `BattleManager` itself (to read pending input) and is documented at each site.

## 4. Stat Modifier Pipeline (MANDATORY for any stat read)

**Never read `stats.atk`, `stats.def`, etc. directly when computing damage, AI scores, or UI tooltips.** Always go through `StatResolver::Get(battler, ctx, StatId::ATK)`.

`BattlerStats` stores **base values only**. Buffs and debuffs push `StatModifier` entries onto the battler via `IBattler::AddStatModifier(...)` and strip them by `sourceId` on revert. `StatResolver` folds base + flat + percent + multiply in a fixed order.

A `StatModifier::condition` predicate `(IBattler&, BattleContext&) -> bool` lets a modifier be active only when its predicate holds — that is how "+30% ATK while HP < 50%"–style effects work without polling.

## 5. Damage Pipeline (chain of responsibility)

`DefaultDamageCalculator` is **not** a single formula function. It owns a `vector<unique_ptr<IDamageStep>>`:
1. `BaseFormulaStep` — power*mult - resistance, via `StatResolver`
2. `StatusBonusStep` — +20% if defender has any status effect
3. `CritRollStep` — 10% chance, doubles effective damage
4. `FinalClampStep` — floor at 1

**The order IS the formula.** Adding a new term (elemental weakness, environment modifier, "extra vs Burning") = one new step inserted at the right index in `DefaultDamageCalculator`'s constructor. **Do not** edit existing steps to splice in new behavior.

## 6. Action Queue — Turn-Based Combat

All combat mutations happen inside `IAction::Execute(dt)`. Skills do not call `TakeDamage` directly — they construct an action list and return it.

```cpp
// IAction — one discrete step in the combat timeline.
struct IAction {
    virtual ~IAction() = default;
    virtual bool Execute(float dt) = 0;   // return true when complete
};
```

Existing concrete actions live in `src/Battle/`:
- `DamageAction` / `AnimDamageAction` — apply damage (the latter waits for an animation frame)
- `StatusEffectAction` — attach an `IStatusEffect`
- `LogAction` — push a battle log line
- `WaitAction` — pause for N seconds
- `MoveAction` — lerp a combatant to a target slot / melee range / origin
- `PlayAnimationAction` — play a clip and wait for it to finish
- `ItemEffectAction` / `ItemConsumeAction` — item dispatch + inventory decrement

`IActionDecorator` lets you wrap any `IAction` with `OnBefore` / `OnAfter` hooks (used by `DelayedAction` to enforce a minimum 1-second pause between actions so the player can read).

**Never put combat state mutations outside an `IAction::Execute` body.** If a future feature needs a new mutation pattern, write a new `IAction` subclass.

## 7. BattleContext — Live State Snapshot

`BattleContext` is a per-frame read-only snapshot rebuilt at the top of `BattleManager::Update`. It carries:
- alive players / alive enemies (vector of `IBattler*`)
- `turnCount`
- `battleElapsed` seconds

`BattleManager` owns one `BattleContext mContext` member; its **address** is stable for the entire battle so actions queued any number of frames ago see live state when they execute. Skills, the damage calculator, predicates, and AI all take `const BattleContext&`.

**Never store a `BattleContext` by value inside an action** — it would freeze at queue time and miss every later state change. Store a `const BattleContext*` pointing into `BattleManager::mContext`.

## 8. Battle Phase FSM

`BattleManager` is a finite state machine over `BattlePhase`:
```
INIT  ->  PLAYER_TURN  <->  RESOLVING  <->  ENEMY_TURN
                                 |
                                 +->  WIN  /  LOSE  (terminal; BattleState polls)
```
- `PLAYER_TURN` waits for `PlayerCombatant::HasPendingAction()` (set by `BattleInputController`).
- `RESOLVING` drains the `ActionQueue`; broadcasts `verso_hp_changed` for UI listeners after each action.
- `AdvanceTurn` rebuilds the AV (Action Value) timeline and selects the next combatant.

`BattleState` holds the FSM, the input controller, the renderer, the iris transition, and the menu/HUD widgets. It is also the only class that talks to `PartyManager` for persistent HP and `Inventory` for item counts.

## 9. Battle Input FSM

`BattleInputController` runs a parallel FSM over `PlayerInputPhase`:
```
COMMAND_SELECT  ->  SKILL_SELECT       ->  TARGET_SELECT       -> commit
COMMAND_SELECT  ->  ITEM_SELECT        ->  ITEM_TARGET_SELECT  -> commit
                                       (skipped for self / AoE items)
COMMAND_SELECT  ->  FleeCommand        -> iris close + pop state
```

Top-level menu commands (`Fight` / `Item` / `Flee`) are `IBattleCommand` subclasses. **Do not** add a switch statement in `BattleInputController` to handle a new command — write a new `IBattleCommand` and append it in `BuildCommandList()`.

## 10. Items — Data, Not Classes

Items are described by JSON in `data/items/*.json`. There is **one** `ItemData` POD and **one** dispatch function (`BuildItemActions::Build`). Do **not** create per-item C++ subclasses.

The dispatch covers eight effect kinds (`heal_hp`, `heal_mp`, `full_heal`, `revive`, `restore_rage`, `deal_damage`, `stat_buff`, `cleanse`) and six targeting rules (`self`, `single_ally`, `single_ally_any`, `single_enemy`, `all_allies`, `all_enemies`). Adding a new item = writing a new `.json` file. Adding a new EFFECT KIND = enum entry + JSON token mapping in `ItemRegistry::ParseEffect` + `case` in `ItemEffectAction::Execute`.

`Inventory` (singleton in `src/Systems/`) is the only authority for "how many of X does the player have right now". It survives between battles. Save/load is not yet wired but the singleton is the single hook point for it.

## 11. State Pattern — Stack-based State Machine

- All game screens are `IGameState` subclasses
- `StateManager` holds `std::stack<std::unique_ptr<IGameState>>`
- Only the **top** state receives `Update(dt)` and `Render()` each frame

| Operation | Use case |
|---|---|
| `PushState(state)` | Overlay a new state while preserving the current (e.g., battle on top of overworld) |
| `PopState()` | Close current state, resume the one beneath |
| `ChangeState(state)` | Full scene transition — no return (e.g., MainMenu -> Overworld) |

- States MUST NOT directly reference or call each other
- All cross-state triggers go through `EventManager`
- Every `Subscribe()` made in `OnEnter()` MUST have a matching `Unsubscribe()` in `OnExit()` — dangling lambda captures crash on next `Broadcast`

## 12. Observer Pattern — Event System

- `EventManager` is the single global event bus (Meyers' Singleton)
- `Subscribe(eventName, callback)` returns a `ListenerID`
- `Broadcast(eventName, data)` fires all registered callbacks
- `Unsubscribe(eventName, id)` in `OnExit()` is **mandatory**

Battle uses typed payload structs in `BattleEvents.h` for animation hand-off events (`battler_play_anim`, `battler_get_anim_progress`, `battler_is_anim_done`, etc.).

## 13. Iris Transition — Deferred Exit Pattern

`BattleState` uses an iris-close overlay before popping itself off the stack. Direct `PopState()` is unsafe because it destroys the state mid-frame; instead, `BattleState` sets `mPendingSafeExit = true` from the iris-close callback, and the **end** of `Update()` checks the flag and only then calls `PopState()`. Any new state that needs a fade-out exit MUST use the same deferred pattern.

---

# GAMEPLAY IMPLEMENTATION RULES

## 1. No Hardcoding — Data-Driven by Default

**Every value that could vary per character, enemy, skill, item, or level MUST come from a data file.**

| Category | C++ struct | Load from |
|---|---|---|
| Skill tuning (move/damage timings) | `JsonLoader::SkillData` | `data/skills/*.json` |
| Enemy + encounter | `EnemyEncounterData` | `data/enemies/*.json` |
| Item | `ItemData` | `data/items/*.json` |
| Formation slot offsets | `FormationData` | `data/formations.json` |
| Battle menu layout | `JsonLoader::BattleMenuLayout` | `data/battle_menu_layout.json` |
| Animation clips + pivots | `SpriteSheet` | `assets/animations/*.json` |

**Permitted hardcoded constants** — `constexpr` at file scope only:
- Window title in `main.cpp`
- Default screen resolution in `GameApp.cpp`
- Layer Z-order ranges
- `BattleManager::kActionGauge` (the AV constant)
- `Inventory` starter bundle counts (pending real save/load)
- `kVisibleItems` for menu windowing (3 — UI affordance, not a tuning value)

Everything else is data. **Magic numbers in `.cpp` files representing game tuning values are defects.**

## 2. Animation System

- Sprites use **sprite sheets** with per-frame UV slicing — no individual image files per frame
- **Root motion is script-driven and math-computed** — never bake movement offsets into the sprite sheet
- `WorldSpriteRenderer` owns the clip state machine. Transitions fire on events / `MoveAction` calls.
- All frame advancement uses `Update(deltaTime)` — never frame count

**Mandatory jump formula (math-computed root motion):**
```cpp
// t is normalized elapsed time in [0.0, 1.0] over the full jump duration.
// 4 * t * (1 - t) is a unit parabola: starts at 0, peaks at t=0.5, returns to 0.
float t      = mJumpTimer / mJumpDuration;
float height = mJumpHeight * 4.0f * t * (1.0f - t);
float horizX = DirectX::XMScalarLerp(mJumpStartX, mJumpTargetX, t);
mTransform.position = { horizX, mGroundY - height };
```

## 3. SpriteBatch + Camera Transform Rule

`SpriteBatch::PrepareForRendering()` internally computes:
```
CB0 = mTransformMatrix * GetViewportTransform()
```
where `GetViewportTransform()` maps **pixel -> NDC**.

| Use case | Matrix to pass as 7th arg | Why |
|---|---|---|
| World-space sprite | `camera.GetViewMatrix()` | world->pixel; SpriteBatch adds pixel->NDC |
| Screen-space UI sprite | `MatrixIdentity` (default) | pixel->pixel; SpriteBatch adds pixel->NDC |
| WRONG | `camera.GetViewProjectionMatrix()` | world->NDC; double-projected — sprite invisible |

## 4. Quick Time Events (QTE) — planned

`QTEState` does not exist yet. The architectural intent stays:
- It will be a self-contained `IGameState` pushed onto the stack, not embedded in `BattleState`
- On activation: regular input processing stops; QTE state exclusively handles input
- Outcome is broadcast via `EventManager`, not returned via a callback

Until it lands, do not pre-add hooks for it inside `BattleState`.

## 5. Resource Management

- All DirectX resources are stored in `ComPtr<T>`. Releases are automatic.
- Texture loading currently goes through `WICTextureLoader` (DirectXTK) per-renderer. There is no global `ResourceManager` singleton yet — when one is added it will live in `src/Systems/`.

---

# CODE QUALITY RULES

## 1. Comment Density

Every non-trivial line requires a comment explaining **why** it exists, not just what it does.

```cpp
// WRONG — states the obvious
device->Release(); // release device

// CORRECT — explains consequence
// Decrement the COM reference count on the D3D11 device.
// If this is the last reference, the GPU object is destroyed and VRAM is freed.
// Omitting this call is reported by ID3D11Debug::ReportLiveDeviceObjects() at shutdown.
device->Release();
```

For every subsystem that touches DirectX (Device, SwapChain, SpriteBatch, AudioEngine), include a **Common Mistakes** block:
```cpp
// Common mistakes:
//   1. Calling Present() before OMSetRenderTargets()  -> black screen.
//   2. Omitting Release() on COM objects              -> leak in DX debug layer.
//   3. Resizing swap chain without releasing RTV first -> DXGI_ERROR_INVALID_CALL.
```

**UPDATING COMMENT RULE:** If you change any logic, you MUST update the comments to reflect the new behavior. Never leave outdated comments above modified code.

## 2. File Header Format (mandatory on every file)

```cpp
// ============================================================
// File: D3DContext.cpp
// Responsibility: Initialize and manage all DirectX 11 GPU resources.
//
// Owns: IDXGISwapChain, ID3D11Device, ID3D11DeviceContext,
//       ID3D11RenderTargetView, ID3D11DepthStencilView
//
// Lifetime:
//   Created in  -> GameApp::Initialize()
//   Destroyed in -> GameApp destructor (ComPtr members auto-release)
//
// Important:
//   - Must be initialized before StateManager::PushState() is called.
//   - OnResize() must release the RTV before calling ResizeBuffers().
//   - Debug device enabled in _DEBUG builds via D3D11_CREATE_DEVICE_DEBUG.
// ============================================================
```

## 3. Function Header Format (mandatory for all non-trivial functions)

```cpp
// ------------------------------------------------------------
// Function: BeginFrame
// Purpose:
//   Clear the back buffer and depth-stencil to prepare for a new frame.
// Why:
//   Leftover pixels from the previous frame cause visual "ghosting"
//   if the back buffer is not cleared before new draw calls.
// Parameters:
//   r, g, b  — Clear color, each in [0.0, 1.0].
// Caveats:
//   - Must be called before any Draw() calls each frame.
//   - Calling EndFrame() without BeginFrame() presents an uncleared buffer.
// ------------------------------------------------------------
```

## 4. Resource Lifetime Documentation

For every `ComPtr`, `unique_ptr`, `SpriteBatch`, or `AudioEngine` member, document:
- **Owner** — which class holds it
- **Created** — in which function / constructor
- **Destroyed** — when the smart pointer goes out of scope / `Shutdown()` is called
- **Leak consequence** — what the DirectX debug layer reports if it is not released

## 5. Pre-Code Explanation Protocol

Before writing any non-trivial code block:
1. State the **architectural decision** being made
2. Explain the **DirectX / Win32 concepts** involved
3. Describe **ownership and lifetime** of every created object
4. Call out **common beginner mistakes** this design avoids
5. Note **performance implications** if relevant

---

# AGENT BEHAVIORAL CONSTRAINTS

1. **Do NOT rewrite core architecture** unless explicitly instructed. New features slot into the existing IGameObject / SceneGraph / IBattler / Combatant / ISkill / IAction / StatModifier / DamageStep / IBattleCommand / Inventory systems.
2. **Do NOT use raw `new` / `delete`** for game objects. Use `std::make_unique` / `std::make_shared`. Spawn overworld entities via `SceneGraph::Spawn<T>(...)`.
3. **Do NOT call `->Release()` manually** on DirectX objects. Use `ComPtr<T>` exclusively.
4. **Do NOT exceed 300 lines per `.cpp` file.** If a file approaches this limit, propose a split along responsibility boundaries (e.g., move render logic to a `*Renderer` helper).
5. **Do NOT place business logic in `GameApp`.** `GameApp` owns the Win32 window, `D3DContext`, `GameTimer`, and `StateManager` — nothing else. Logic belongs in States, Battle systems, or `Systems/`.
6. **Do NOT expose concrete entity types to callers that only need an interface.** SceneGraph works with `IGameObject*`. BattleManager works with `IBattler*`. Downcasts must be exceptional, justified, and documented at the site with a comment explaining why the interface is insufficient.
7. **Do NOT hardcode tunable game values.** All character stats, skill parameters, enemy compositions, item tunings, animation timings, and level layouts come from data files. The exceptions in **Permitted hardcoded constants** above are the complete allow-list.
8. **Do NOT read raw stat fields from `BattlerStats` for combat math.** Always go through `StatResolver::Get(battler, ctx, StatId::X)` so buffs and conditional modifiers fold in.
9. **Do NOT mutate combat state outside `IAction::Execute`.** Skills return action lists; effects push modifiers; everything else is a defect.
10. **Do NOT mutate `BattlerStats.atk`/`def`/etc. directly from a status effect.** Push a `StatModifier` and store its `sourceId` so `Revert` can strip it cleanly.
11. **Do NOT write a per-item C++ class.** Add one JSON file under `data/items/`. New behavior categories require an `ItemEffectKind` enum entry and a `case` in `ItemEffectAction::Execute`.
12. **Do NOT add a switch statement to `BattleInputController` to handle a new top-level command.** Implement `IBattleCommand` and append in `BuildCommandList()`.
13. **Always update `build_src_static.bat`** when creating a new `.cpp` file. The build will fail with "unresolved external" otherwise.
14. **Always `#define NOMINMAX` BEFORE any project header** in any `.cpp` that uses `std::min` / `std::max`. `Log.h` pulls in `Windows.h`, which leaks the `min` / `max` macros.
15. **Every new entity class MUST implement `IGameObject`** (overworld) or `IBattler` (battle). Every new behaviour that crosses entity-type boundaries MUST be a component or a system — not a copy-pasted method.
16. **Silently correct language violations** in every file you touch — replace all non-English comments, strings, and identifiers with English equivalents.
17. **Ask before destructive git operations.** Resets, force-pushes, and amends to published commits require explicit user confirmation.

---

# THINKING, PLANNING, AND DESIGN MINDSET (AI DIRECTIVES)

To maintain highest-tier architectural flow during this specific project, you MUST operate with the following rigorous software engineering mindset before executing tool updates:

1. **Investigate Systems, Not Symptoms:** When a bug is reported (e.g. "things are too fast" or "ghost graphics are sticking"), trace the entire linear pipeline backward from Graphics rendering (UI loop) through the payload Event Bus, and directly into the underlying mechanics logic. Formulate a mathematical/timeline understanding of *why* the glitch occurred algorithmically before touching code.
2. **The "Data-Driven" Planning Phase:** If a new behavior, constraint, or gameplay tweak must be executed, the very first step in your mental workflow must be: *"Can this be extracted to `data/*.json` and parsed through `JsonLoader`?"* Do not hardcode scaling or temporal physics into C++ variables. 
3. **Draft High-Fidelity Implementation Plans:** When proposing changes, draft comprehensive plans showing EXACTLY which files dictate what tier of the architecture. Communicate changes separated by `UI Pipeline`, `Action Dispatch`, and `Data Layer`. Present the plan clearly to the user before writing single lines of code natively. 
4. **Decouple Game Logic from Visual Logic:** Viciously guard against graphic state dictating physical logic, and vice versa. Explode combined behaviors (e.g. "QTE Active Loop") into decoupled physical metrics (e.g. Action execution resolution timeout) vs visual metrics (e.g. Post-hit scaling flashes). Keep `BattleManager` blind to `SpriteBatch` rules. 
5. **Math-Centric Robustness:** Actively identify mathematical failure paradigms (divide-by-zero on JSON variables out of bounds, clustering/overlap issues due to blind randomness, linear versus inverse scaling) and architect "bucket" segmentations or variable-bounds dynamically instead of trusting pure randomized math.
