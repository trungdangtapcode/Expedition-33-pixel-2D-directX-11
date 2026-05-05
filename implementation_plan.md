# Exp and Leveling System Design

This document details the architectural rollout of the Character Experience (EXP) and Level Up system. 
As requested, this is the architectural blueprint. I will not implement these features until you review and approve!

## User Review Required

> [!IMPORTANT]
> Please review the math curve for EXP requirements (`BaseExp * (Level^1.5)`) and how stat growth is distributed. Let me know if you would prefer a handcrafted Excel-style EXP table or a mathematical curve.
> Note: You mentioned "experiment stats", which I am assuming translates to "Experience (EXP) stats".

## Proposed Architecture

### 1. Data Structures (`BattlerStats.h` & `EnemyEncounterData.h`)
We will introduce EXP tracking to characters and EXP rewards to enemies.
*   **BattlerGrowth Struct (New):** Defines how much each stat increases upon hitting the next level (e.g. `maxHp: +15, atk: +3, spd: +1`).
*   **BattlerStats Extensions:** Add `level` (default 1) and `exp` (cumulative). Characters will maintain their own cumulative EXP pool.
*   **EnemySlotData Extensions:** Add `expReward` so each enemy in a battle drops a designated amount of experience points on victory.

#### [MODIFY] src/Battle/BattlerStats.h
#### [MODIFY] src/Battle/EnemyEncounterData.h

### 2. JSON Data Definitions
Characters need growth specifications, and enemies need reward outputs. I will upgrade `JsonLoader.h`.
*   **Characters (`verso.json` / `maelle.json`):**
    Introduce a `"growth": {}` object block mapping exact integer increments for `[maxHp, atk, def, matk, mdef, spd]`.
*   **Enemies (`skills/skeleton_attack.json` / encounter config):**
    Introduce an `"expReward"` integer (e.g., Skeleton gives 45 EXP).

#### [MODIFY] src/Utils/JsonLoader.h
#### [MODIFY] data/characters/*.json
#### [MODIFY] data/enemies/*.json

### 3. Level Up Logic (`PartyManager.h` & `PartyManager.cpp`)
Since `PartyManager` safely orchestrates persistent character data across battles, it must own the Level-Up engine.
*   **Mathematical Curve:** We will use a cumulative curve equation instead of flat leveling. For example: `NextLevelExp = 100 * (Level ^ 1.5)`.
*   **`AddExp(int amount)` method:** We will loop over the active party. We increment their EXP, check if it surpasses the curve threshold, and recursively trigger the **Level Up**.
*   **Level Up Event:** Upon triggering, the character's `level` hits `+1`. The system organically iterates every stat using the `BattlerGrowth` deltas defined in their JSON, and fully restores their HP as a bonus mechanic!

#### [MODIFY] src/Systems/PartyManager.h
#### [MODIFY] src/Systems/PartyManager.cpp

### 4. Battle Completion Tally (`BattleState.cpp` / `BattleManager.cpp`)
At the end of an encounter, before `BattleState` forcibly pops from the `StateManager`, it evaluates the kill feed.
*   When `BattleManager::GetOutcome() == BattleOutcome::VICTORY`:
    We loop through all `mEnemies`. We sum `expReward` securely.
    We dispatch `PartyManager::Get().AddExp(totalExp)` to all living party members. 

#### [MODIFY] src/States/BattleState.cpp

## Open Questions

> [!WARNING]
> 1. **Dead Ally Distribution:** Do you want party members who *die* in battle (0 HP at the very end of the fight) to still receive full EXP, half EXP, or 0 EXP?
> 2. **Catch-up EXP:** Should lower-level members receive a tiny multiplier to EXP so they can catch up to Verso faster, or should EXP be strictly divided evenly regardless of level?
> 3. Does "experiment stats" mean "Experience (EXP)" or something completely different to you?

## Verification Plan

### Automated Tests
1. Add temporary skeleton encounters granting `999` EXP to forcibly trigger mult-level ups instantly.
2. Open `assets/characters/verso.json` and grant 50 `atk` per level.
3. Validate through the standard logger `LOG(...)` checking if Verso's Base stats successfully scale through Level 5.

### Manual Verification
1. Boot into the Overworld, collide with an enemy.
2. Validate the JSON parsers safely boot mapping `expReward` and `growth` safely.
3. Complete the battle natively destroying the skeletons, then parse the log output confirming exactly how far Verso leveled up!
