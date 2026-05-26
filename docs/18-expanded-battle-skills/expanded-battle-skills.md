# Expanded Battle Skills

## Summary

The battle skill system now supports direct damage, HP healing, MP recovery, revive, cleanse, status application, all-enemy attacks, and all-ally support through JSON skill data. Skills still build `IAction` objects and never mutate combat state during input selection.

## Skill Data

Skill JSON uses three separate axes:

- `kind`: broad UI/combat category, such as `damage`, `support`, `heal`, or `rage`.
- `effect`: the gameplay branch, such as `damage`, `heal_hp`, `revive`, or `cleanse`.
- `mechanism`: how the action plays, such as `attack` for the full melee/camera/animation path.
- `attackMotion`: attack-mechanism motion style. `melee` dashes to the target; `stationary` keeps the caster in place for magic and ranged techniques.

Existing damage skills can omit `effect` because `attack`, `damage`, and `rage` resolve to `damage`.

Supported `effect` values:

- `damage`: build one `DamageAction` per resolved target.
- `heal_hp`: build one `HealAction` per living target.
- `heal_mp`: build one `RestoreMpAction` per living target.
- `revive`: build one `ReviveAction` for the selected fallen ally.
- `cleanse`: build one `CleanseAction` for the selected ally.
- `status`: apply `statusEffectId` without direct damage.

Supported `mechanism` values:

- Empty/default: use the generic data-driven cast/support path.
- `attack`: use the full attack mechanism: fight stance, camera follow, melee movement, animation-timed damage, optional QTE or bullet-hell, optional status application, and return to origin.

Damage skills may set `flatBonus` when they need a small guaranteed floor against high-defense enemies. The bonus is still passed through the normal `DamageRequest` and damage pipeline; it is not applied directly by UI or input code.

QTE complexity is per skill:

- Basic attacks use fewer nodes and wider spacing.
- Debuff attacks use medium node counts so the status application still feels earned.
- Finishers and large fire techniques use more nodes, tighter spacing, and higher perfect bonuses.
- `qteMinCount` and `qteMaxCount` are clamped to the renderer limit so a data mistake cannot spawn invisible prompts.

Supported targeting values:

- `self`: commits immediately against the caster.
- `single_enemy`: opens enemy target selection.
- `single_ally`: opens living ally target selection.
- `single_ally_any`: opens special ally selection; revive skills filter this to fallen allies.
- `all_enemies`: commits immediately and resolves every living enemy.
- `all_allies`: commits immediately and resolves every living ally.

## Action Queue Rules

HP, MP, revive, cleanse, damage, and status application all happen inside `IAction::Execute`. This keeps battle state changes deterministic and prevents target menus from changing gameplay state before the player commits a turn.

New skill actions:

- `HealAction`
- `RestoreMpAction`
- `ReviveAction`
- `CleanseAction`

`StatusEffectAction` accepts an optional chance value, so skills like Flame Bloom can roll burn chance during queue resolution instead of during input.

## Added Skills

Verso:

- `Crescent Sweep`: all-enemy physical damage.
- `Sunder Guard`: attack-mechanism damage plus Weaken.
- `Mark Prey`: attack-mechanism damage plus Vulnerable.
- `Second Wind`: self HP recovery.
- `Rage Burst`: attack-mechanism rage finisher.

Maelle:

- `Ember`: melee-motion attack-mechanism fire damage plus Burn.
- `Flame Bloom`: melee-motion all-enemy attack-mechanism fire damage with burn chance.
- `Mending Verse`: single-ally HP recovery.
- `Aegis Verse`: all-ally Guard Up.
- `Quickstep`: single-ally Haste.
- `Revival Thread`: revive one fallen ally.
- `Cleansing Note`: cleanse one ally.

## UI Notes

The existing Expedition-style skill menu remains in place. It now shows support effects as effect text instead of pretending they are physical damage. Large detail panels remain hidden during target selection so the selected battler stays readable.

## Adding A Skill

1. Add a JSON file under `data/skills/`.
2. Choose `mechanism`, `attackMotion`, `targeting`, `effect`, `mpCost`, `amount`, `flatBonus`, and optional `statusEffectId`.
3. Add the path to the character's `skillPaths` array.
4. Add English, Vietnamese, and French localization keys.
5. Reuse an existing icon id from `assets/UI/status_effect_icons.json` or add a new atlas frame.
