# Multi-Pattern Bullet-Hell Attacks

## Purpose

Single-pattern enemy attacks become predictable across a full battle. The attack system now lets a single skill choose from multiple bullet-hell pattern files at execution time, so repeat enemy turns can feel varied without adding per-enemy C++ branches.

## Runtime Flow

1. The enemy slot points to an attack skill file through `attackJsonPath`.
2. `EnemyCombatant` loads that file into `JsonLoader::SkillData`.
3. `AttackSkill::Execute` asks `SkillData` for a bullet-hell pattern list.
4. `AttackSkill` selects one pattern according to `bulletHellPatternSelection`.
5. `BulletHellAction` receives the selected pattern path and loads that pattern as before.

The action queue still contains exactly one `BulletHellAction` for the attack. Only the pattern path changes.

## Skill Fields

Existing single-pattern skills still work:

```json
{
  "bulletHellSupported": true,
  "bulletHellPatternPath": "data/bullet_patterns/skeleton_bones.json"
}
```

Multi-pattern skills add a list:

```json
{
  "bulletHellSupported": true,
  "bulletHellPatternPath": "data/bullet_patterns/skeleton_bones.json",
  "bulletHellPatternSelection": "random_no_repeat",
  "bulletHellPatternPaths": [
    "data/bullet_patterns/skeleton_bones.json",
    "data/bullet_patterns/skeleton_crossfire.json",
    "data/bullet_patterns/sine_wave.json"
  ]
}
```

`bulletHellPatternPath` remains as a compatibility fallback. If `bulletHellPatternPaths` is empty, the loader places the single path into the pattern list.

## Selection Modes

- `fixed`: Always use the first pattern. This is the default.
- `random`: Choose any listed pattern each attack.
- `random_no_repeat`: Choose randomly, but avoid the previous pattern when more than one option exists.
- `cycle`: Move through the list in order and wrap back to the start.

The selection mode belongs to the skill JSON, not the enemy C++ class. This keeps enemy behavior data-driven and lets designers change variety without recompiling.

## Current Users

- `data/skills/skeleton_attack.json`: Randomly alternates among bones, crossfire, and sine-wave pressure.
- `data/skills/zombie_armour_attack.json`: Randomly alternates among guard walls, lockstep walls, and a faster archer-style pattern.
- `data/skills/verso_cloned_attack.json`: Randomly alternates among aggressive boss-like bullet patterns.

## Pattern Files

New pattern files:

- `data/bullet_patterns/skeleton_crossfire.json`
- `data/bullet_patterns/zombie_armour_lockstep.json`

These are ordinary `BulletHellPatternData` files. No new action type was needed; they reuse existing spawner components.
