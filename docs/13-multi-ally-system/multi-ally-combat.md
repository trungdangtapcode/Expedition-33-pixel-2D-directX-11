# Multi-Ally Combat System & Data-Driven UI Refactor

This document outlines the architectural changes, systems overhauls, and design principles implemented to migrate the engine from a single protagonist system ("Verso") into a fully functional, dynamically scaling multi-ally roleplaying party system.

## 1. System Refactoring Overview

### Party Management (`PartyManager.h/cpp`)
- **Legacy implementation:** Previously acting as a Singleton storing hardcoded `BattlerStats` values strictly reserved and named after Verso.
- **Multi-Ally Architecture:** Converted into a collection-based management container. It now leverages `std::vector<PartyMember>` storing the active squad sequentially. It introduces array index retrieval natively tracking complex object stats across the overworld effortlessly.

### Overworld Inventory (`InventoryState.cpp` / `InventoryStateRender.cpp`)
- Rewrote the Inventory logic and Detail Panels to read their active equipment hooks exclusively from `GetActiveParty()[index]` dynamically looping bindings removing any hard-coded class integrations.

## 2. Battle State Architecture

### Spawning & Combatants (`BattleManager.cpp`)
- Replaced the hardcoded single-spawn structure gracefully. `BattleManager::Initialize` now iterates safely over `PartyManager::GetActiveParty()` generating individual instances of `PlayerCombatant` cleanly mapping corresponding data strings dynamically into the Event Pipeline securely!

### AI Targeting (`EnemyCombatant.cpp`)
- **Legacy Behavior:** Enemies previously returned the rigid first array element `players[0]` (Verso) entirely ignoring sequential players blindly.
- **Dynamic Array Polling:** `ChooseTarget()` mathematically pushes validly `IsAlive()` combatants into an active evaluation list, subsequently rolling `std::rand()` internally outputting organically randomized targets forcing fair distribution!

## 3. UI Abstraction & Layout Architecture

### Health Bar Engine (`HealthBarRenderer.h/cpp`)
- Maintained the engine's original, deeply sophisticated **3-Layer rendering architecture**:
  - `Layer 1`: Opaque Background overlay natively providing framing borders.
  - `Layer 2`: Sub-fraction tracking arrays featuring the visual `Red Lerp` and proportional `White Delay` algorithms accurately retaining `TriggerShake()` on hit.
  - `Layer 3`: Portrait and frame window layer flawlessly rendering natively.
- **Structural Override:** The layout previously statically subtracted screen boundaries `mScreenW - mConfig.textureWidth` rendering sequentially upwards statically locking layouts cleanly! We successfully unlinked these dependencies gracefully forcing the class exclusively into honoring parameter inputs (`renderX`, `renderY`) safely decoupling calculations cleanly!

### Subscriptions & Dynamic Naming
- Overhauled hardcoded Subscription Events! EventManager logic correctly constructs strings dynamically mathematically capturing lowercase parameters flawlessly mapping `"verso_hp_changed"` precisely.

## 4. Layout Math (`JsonLoader.h` & `BattleState.cpp`)

To support data-driven configuration, exact coordinates are securely structured away securely within `data/battle_menu_layout.json` mapped perfectly via `PartyHudConfig`:
```json
  "party_hud_align": "bottom-right",
  "party_hud_origin_x": -286.0,
  "party_hud_origin_y": -286.0,
  "party_hud_spacing_x": -150.0,
  "party_hud_spacing_y": 0.0
```

`BattleState.cpp` loads `mMenuLayout` dynamically checking alignments structurally projecting `spacing_x` arrays naturally allocating members sequentially cleanly stacking bars safely horizontally flawlessly executing dynamically.
