# Zombie Armour Bullet-Hell Attack

## Purpose

Zombie Armour is a skeleton-family enemy, so its normal attack should enter the same defensive dodge phase used by skeleton enemies instead of falling back to plain animation damage.

The issue was data configuration, not an enemy-name branch in C++. `data/skills/zombie_armour_attack.json` had `bulletHellSupported` set to `false`, so `AttackSkill` selected `AnimDamageAction` instead of `BulletHellAction`.

## Runtime Flow

1. `data/enemies/zombie_armour.json` assigns `attackJsonPath` to `data/skills/zombie_armour_attack.json`.
2. `EnemyCombatant` loads that skill file through `JsonLoader::LoadSkillData`.
3. `AttackSkill::Execute` checks the loaded `SkillData`.
4. If `bulletHellSupported` is true, it queues `BulletHellAction`.
5. `BulletHellAction` loads the configured pattern from `bulletHellPatternPath`.
6. The action broadcasts `verso_bullet_hell_state` so `BattleBulletHellRenderer` can draw the dodge arena, heart, and bullets.

## Zombie Armour Data

Zombie Armour now uses:

```json
{
  "bulletHellSupported": true,
  "bulletHellPatternPath": "data/bullet_patterns/zombie_armour_guard.json"
}
```

The dedicated pattern keeps Zombie Armour tunable without changing skeleton patterns or adding C++ conditionals.

## Pattern Fields

`data/bullet_patterns/zombie_armour_guard.json` controls both gameplay and arena layout:

- `durationSec`: Total dodge phase duration.
- `boxCenterX`, `boxCenterY`: Screen-space center of the dodge arena.
- `boxWidth`, `boxHeight`: Arena size.
- `heartRadius`: Collision radius for the player-controlled heart.
- `heartSpeed`: Player movement speed during the dodge phase.
- `invincibilityDuration`: Temporary invulnerability after a bullet hit.
- `spawners`: Projectile emitters, each with type, texture, speed, radius, spawn rate, damage scaling, and optional sine motion fields.

## No-Hardcode Boundary

The enemy does not decide bullet hell through hardcoded name checks. The switch happens through skill data:

- Zombie Armour attack data enables the mode.
- Zombie Armour pattern data tunes the dodge phase.
- `BulletHellAction` only executes the generic action and reads its parameters from `JsonLoader::BulletHellPatternData`.

Fallback defaults still exist in the loader so malformed files do not crash the game immediately, but authored pattern files should include the full gameplay-tuning schema.

## Damage Context

`BulletHellAction` now receives the live `BattleContext` pointer from `AttackSkill`, matching the rest of the action queue design. This keeps future stat modifiers, status conditions, and battle-wide context rules consistent when a bullet hit applies damage.
