# Enemy System

## Overview

Enemies in this project are fully data-driven. Every enemy -- from overworld
appearance to in-battle stats, animations, attack skills, and BGM -- is
defined in a JSON file under `data/enemies/`. No per-enemy C++ subclass is
needed; the same `OverworldEnemy` entity and `EnemyCombatant` classes handle
all enemies via the `EnemyEncounterData` data bridge.

---

## Data Pipeline

```
data/enemies/skeleton.json          (encounter definition)
         |
         | JsonLoader::LoadEnemyEncounterData()
         v
   EnemyEncounterData struct
         |
         |--- overworld fields ---> OverworldEnemy (SceneGraph entity)
         |                            - renders idle sprite
         |                            - proximity detection
         |
         |--- player presses B near enemy ---
         |
         |--- battleParty[] -----> BattleManager::Initialize()
         |                            |
         |                            | for each EnemySlotData:
         |                            v
         |                         EnemyCombatant(name, stats, attackJsonPath)
         |                            |
         |                            | JsonLoader::LoadSkillData(attackJsonPath)
         |                            v
         |                         AttackSkill(skillData)
         v
   BattleState (pushed on stack, owns BattleManager)
```

---

## Enemy JSON Schema

### Top Level: `EnemyEncounterData`

Top-level fields define the **overworld** representation. These are used by
`OverworldEnemy`'s `WorldSpriteRenderer`.

```json
{
  "name": "Skeleton",
  "texturePath": "assets/animations/skeleton.png",
  "jsonPath": "assets/animations/skeleton.json",
  "idleClip": "idle",
  "contactRadius": 90.0,
  "environmentPath": "assets/environments/battle-paris-view.json",
  "bgmTrackId": "",

  "battleParty": [ ... ]
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | string | yes | Display name shown in HUD |
| `texturePath` | string | yes | Overworld sprite sheet PNG |
| `jsonPath` | string | yes | Overworld animation descriptor JSON |
| `idleClip` | string | yes | Starting animation clip name |
| `contactRadius` | float | no (80.0) | Proximity trigger radius in world pixels |
| `environmentPath` | string | no | Battle background environment JSON |
| `bgmTrackId` | string | no ("") | BGM track id from `bgm.json`; empty = default battle theme |
| `battleParty` | array | yes | 1--3 `EnemySlotData` objects |

### Battle Party Slot: `EnemySlotData`

Each entry in `battleParty[]` defines one enemy combatant in battle.
A single overworld enemy can trigger a multi-enemy fight.

```json
{
  "texturePath": "assets/animations/skeleton.png",
  "jsonPath": "assets/animations/skeleton.json",
  "turnViewPath": "assets/UI/turn-view-skeleton.png",
  "idleClip": "idle",
  "hp": 50,
  "atk": 10,
  "def": 5,
  "spd": 5,
  "expReward": 45,
  "attackJsonPath": "data/skills/skeleton_attack.json",
  "cameraFocusOffsetY": -128.0
}
```

| Field | Type | Default | Description |
|---|---|---|---|
| `texturePath` | string | | Battle sprite PNG |
| `jsonPath` | string | | Battle animation JSON |
| `turnViewPath` | string | | Turn queue portrait PNG |
| `idleClip` | string | | Battle idle animation clip |
| `hp` | int | 0 | Hit points |
| `atk` | int | 0 | Attack power |
| `def` | int | 0 | Defense |
| `spd` | int | 0 | Speed (determines turn order via AV timeline) |
| `expReward` | int | 0 | EXP awarded on kill |
| `attackJsonPath` | string | `skeleton_attack.json` | Path to attack skill JSON |
| `cameraFocusOffsetY` | float | -128.0 | Vertical camera offset in battle |
| `dieClip` | string | `"die"` | (optional) Death animation clip override |
| `attackClip` | string | `"attack-1"` | (optional) Attack animation clip override |
| `walkClip` | string | `"walk"` | (optional) Walk animation clip override |
| `hurtClip` | string | `"hurt"` | (optional) Hurt animation clip override |

---

## Attack Skill JSON

Each enemy references an attack skill JSON that controls melee timing
and optional minigame integration.

```json
{
  "moveDuration": 0.4,
  "returnDuration": 0.35,
  "meleeOffset": 80.0,
  "damageTakenOccurMoment": 0.5,
  "bulletHellSupported": true,
  "bulletHellPatternPath": "data/bullet_patterns/skeleton_bones.json"
}
```

| Field | Type | Description |
|---|---|---|
| `moveDuration` | float | Seconds to lerp to melee range |
| `returnDuration` | float | Seconds to return to origin |
| `meleeOffset` | float | Pixel offset from target at melee range |
| `damageTakenOccurMoment` | float | Normalised [0,1] moment damage applies during attack anim |
| `bulletHellSupported` | bool | Enable bullet-hell dodge phase for player |
| `bulletHellPatternPath` | string | Path to bullet pattern JSON |
| `qteSupported` | bool | Enable QTE (player attacks only) |

---

## Overworld Spawning

Enemies are spawned in `OverworldState::OnEnter()` and `PlayState::OnEnter()`:

```cpp
EnemyEncounterData data{};
if (JsonLoader::LoadEnemyEncounterData("data/enemies/my_enemy.json", data))
{
    OverworldEnemy* e = mScene.Spawn<OverworldEnemy>(
        device, context, data, worldX, worldY, mCamera.get());
    if (e) mOverworldEnemies.push_back(e);
}
```

- `SceneGraph::Spawn<T>` creates a `unique_ptr<OverworldEnemy>` and returns
  a non-owning raw pointer.
- The raw pointer is stored in `mOverworldEnemies` for proximity checks.
- The entity is stationary (no movement AI).
- Battle triggers when the player presses B within `contactRadius`.

### Post-Battle Cleanup

On `"battle_end_victory"` event:
1. `mPendingEnemySource->MarkDefeated()` sets `mAlive = false`
2. The pointer is erased from `mOverworldEnemies`
3. `SceneGraph::PurgeDead()` frees the entity on the next frame

---

## Per-Encounter BGM

Any encounter can play a custom BGM instead of the default battle theme:

1. Add a track to `data/audio/bgm.json`:
   ```json
   { "id": "boss_theme", "path": "assets/sound/OST/boss.mp3" }
   ```
2. Set `bgmTrackId` in the enemy JSON:
   ```json
   "bgmTrackId": "boss_theme"
   ```
3. `BattleState::InitAudio()` checks `mEncounter.bgmTrackId` and broadcasts
   the appropriate BGM event. Empty = default `"battle"` track.

Supported formats: `.wav` (WavLoader) and `.mp3`/`.wma`/`.aac`/`.flac`
(MediaLoader via Windows Media Foundation).

---

## Case Study: Verso Cloned

"Verso Cloned" is a mirror-match boss that reuses the player character's
own assets. It demonstrates every feature of the enemy system.

### `data/enemies/verso_cloned.json`

| Property | Value | Notes |
|---|---|---|
| Overworld sprite | `assets/animations/verso.png` | Same as the player |
| Battle idle clip | `fight-state` | Menacing combat stance |
| Turn view portrait | `assets/UI/turn-view-verso-cloned.png` | Distinct from the player's portrait |
| HP / ATK / DEF / SPD | 220 / 22 / 12 / 14 | ~2x player base stats |
| EXP reward | 200 | High-value encounter |
| Attack skill | `data/skills/verso_cloned_attack.json` | Aggressive melee + bullet hell |
| Custom BGM | `verso_cloned_battle` | `assets/sound/OST/verso-cloned-fight.mp3` |

### Stat Comparison

| Stat | Verso (Player) | Skeleton | Verso Cloned |
|---|---|---|---|
| HP | 100 | 50 | **220** |
| ATK | 10 | 10 | **22** |
| DEF | 4 | 5 | **12** |
| SPD | 10 | 5 | **14** |
| EXP | -- | 45 | **200** |

### Overworld Position

Spawned at `(500, -200)` -- northeast of spawn, away from the skeletons
at `(300, 150)` and `(-250, -100)`.

---

## How to Create a New Enemy

### Step 1: Prepare Assets

- Sprite sheet PNG + animation JSON (can reuse an existing character's)
- Turn queue portrait PNG (place in `assets/UI/`)
- (Optional) Custom BGM track

### Step 2: Create Attack Skill JSON

Create `data/skills/my_enemy_attack.json`:

```json
{
  "moveDuration": 0.4,
  "returnDuration": 0.35,
  "meleeOffset": 80.0,
  "damageTakenOccurMoment": 0.5,
  "bulletHellSupported": true,
  "bulletHellPatternPath": "data/bullet_patterns/skeleton_bones.json"
}
```

### Step 3: Create Encounter JSON

Create `data/enemies/my_enemy.json`:

```json
{
  "name": "My Enemy",
  "texturePath": "assets/animations/my_enemy.png",
  "jsonPath": "assets/animations/my_enemy.json",
  "idleClip": "idle",
  "contactRadius": 90.0,
  "environmentPath": "assets/environments/battle-paris-view.json",
  "bgmTrackId": "",

  "battleParty": [
    {
      "texturePath": "assets/animations/my_enemy.png",
      "jsonPath": "assets/animations/my_enemy.json",
      "turnViewPath": "assets/UI/turn-view-my-enemy.png",
      "idleClip": "idle",
      "hp": 80,
      "atk": 15,
      "def": 8,
      "spd": 7,
      "expReward": 60,
      "attackJsonPath": "data/skills/my_enemy_attack.json",
      "cameraFocusOffsetY": -128.0
    }
  ]
}
```

### Step 4: Spawn in Overworld

Add to `OverworldState::OnEnter()` and `PlayState::OnEnter()`:

```cpp
{
    EnemyEncounterData data{};
    if (JsonLoader::LoadEnemyEncounterData("data/enemies/my_enemy.json", data))
    {
        OverworldEnemy* e = mScene.Spawn<OverworldEnemy>(
            device, context, data, 100.0f, 200.0f, mCamera.get());
        if (e) mOverworldEnemies.push_back(e);
    }
}
```

### Step 5: (Optional) Custom BGM

Add to `data/audio/bgm.json`:

```json
{ "id": "my_enemy_theme", "path": "assets/sound/OST/my_theme.mp3" }
```

Set in enemy JSON:

```json
"bgmTrackId": "my_enemy_theme"
```

No code changes needed for any of these steps (except the spawn in Step 4).
