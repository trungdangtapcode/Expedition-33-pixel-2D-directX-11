# Expanded Battle Skills

## Summary

The battle skill system now supports direct damage, HP healing, MP recovery, revive, cleanse, status application, all-enemy attacks, and all-ally support through JSON skill data. Skills still build `IAction` objects and never mutate combat state during input selection.

## Skill Data

Skill JSON uses `kind` for broad UI grouping and `effect` for the actual gameplay branch. Existing damage skills can omit `effect` because `attack`, `damage`, and `rage` resolve to `damage`.

Supported `effect` values:

- `damage`: build one `DamageAction` per resolved target.
- `heal_hp`: build one `HealAction` per living target.
- `heal_mp`: build one `RestoreMpAction` per living target.
- `revive`: build one `ReviveAction` for the selected fallen ally.
- `cleanse`: build one `CleanseAction` for the selected ally.
- `status`: apply `statusEffectId` without direct damage.

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
- `Second Wind`: self HP recovery.

Maelle:

- `Flame Bloom`: all-enemy magic damage with burn chance.
- `Mending Verse`: single-ally HP recovery.
- `Aegis Verse`: all-ally Guard Up.
- `Quickstep`: single-ally Haste.
- `Revival Thread`: revive one fallen ally.
- `Cleansing Note`: cleanse one ally.

## UI Notes

The existing Expedition-style skill menu remains in place. It now shows support effects as effect text instead of pretending they are physical damage. Large detail panels remain hidden during target selection so the selected battler stays readable.

## Adding A Skill

1. Add a JSON file under `data/skills/`.
2. Choose `targeting`, `effect`, `mpCost`, `amount`, and optional `statusEffectId`.
3. Add the path to the character's `skillPaths` array.
4. Add English, Vietnamese, and French localization keys.
5. Reuse an existing icon id from `assets/UI/status_effect_icons.json` or add a new atlas frame.
