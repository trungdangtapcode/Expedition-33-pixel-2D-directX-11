# Data-Driven Character Architecture

To ensure the game engine scales properly during full production, structural character parameters (such as `hp`, `atk`, `spd`, etc.) have been completely extracted from their natively hard-coded C++ representations and offloaded entirely into standard JSON pipelines cleanly mimicking the external enemy generation.

## 1. Character Configuration Extracted (`verso.json`)

Player definitions now live natively inside the `data/characters/` asset bundle directory.
For example, Verso's foundational mechanics live at `data/characters/verso.json`:

```json
{
  "name": "Verso",
  "hp": 100,
  "maxHp": 100,
  "mp": 50,
  "maxMp": 50,
  "atk": 10,
  "def": 4,
  "matk": 25,
  "mdef": 10,
  "spd": 10,
  "rage": 0,
  "maxRage": 100
}
```

This ensures a game designer can immediately test re-balances (such as lowering `atk`/`def`) instantly without executing the MSVC compiler layout!

## 2. Bootstrapping Strategy (`PartyManager.cpp`)

Because Verso acts as a persistent singleton tracking state continuously between instances (unlike enemies who naturally respawn), his statistics cannot be bound directly into the `BattleManager::Initialize()` cycle, otherwise his Wounds and MP drainage would get overwritten back to `maxHp` every battle!

Instead, parsing is executed **lazily exactly once** dynamically hooked onto the `PartyManager` singleton constructor constraint natively via `JsonLoader::LoadCharacterData()`.

```cpp
PartyManager::PartyManager()
{
    // Bootstrapped safely exactly once when the engine initializes!
    if (!JsonLoader::LoadCharacterData("data/characters/verso.json", mVersoStats)) {
        LOG("[PartyManager] Failed to load config!");
    }
}
```

## 3. The `LoadCharacterData` Engine Pipeline (`JsonLoader.h`)

Paralleling `LoadEnemyEncounterData`, the `detail::ParseInt` and `detail::ValueOf` helper functions securely scrape each requested key securely preventing engine crashes upon typo detection natively overriding to reasonable bounds.
This perfectly ensures the physical asset pipeline fully controls the mechanical scaling and presentation values universally!
