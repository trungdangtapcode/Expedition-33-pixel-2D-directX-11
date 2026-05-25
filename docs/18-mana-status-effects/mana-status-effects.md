# Mana, Skills, and Status Effects

## Purpose
This feature adds a battle foundation for MP-driven skills and visible status effects without moving combat tuning into C++.

The design keeps the existing battle architecture:
- Skills return `IAction` sequences.
- Combat mutations happen inside actions.
- Stat changes use the `StatModifier` pipeline.
- Player-facing text stays in localization JSON.
- Status visuals use data IDs and an atlas, not hardcoded UI branches.

## Skill Data
Player skills are loaded from `data/skills/*.json`. Character files declare the skill list through `skillPaths`.

Important fields:
- `id`: Stable skill identifier.
- `kind`: `attack`, `damage`, `status`, `support`, or `rage`.
- `nameKey`, `descriptionKey`: Localization keys.
- `iconId`: Optional icon identifier for future skill icon rendering.
- `targeting`: `single_enemy`, `single_ally`, `all_enemies`, `all_allies`, or `self`.
- `mpCost`: MP spent by `ConsumeMpAction` after the skill is committed.
- `statusEffectId`: Optional status effect to apply through `StatusEffectAction`.
- `skillMultiplier`: Damage multiplier for data-driven damage skills.
- `requiresFullRage`, `consumesAllRage`: Rage gating and spending for rage skills.

Basic attacks still use `AttackSkill` because they carry existing movement, animation, QTE, and bullet-hell behavior. Other JSON skills use `DataDrivenSkill`.

## Mana Rules
MP is stored in `BattlerStats` as base battle state:
- `maxMp`
- `mp`

The UI now renders MP from the same HP bar JSON layout. MP spending happens through `ConsumeMpAction` so the action queue remains the only place combat state is mutated.

## Status Data
Status effects are loaded from `data/status_effects/*.json` by `StatusEffectRegistry`.

Important fields:
- `id`: Stable status identifier.
- `nameKey`, `descriptionKey`: Localization keys.
- `iconId`: Atlas frame identifier.
- `category`: `buff`, `debuff`, or `neutral`.
- `durationTurns`: Turn count.
- `stackPolicy`: `refresh`, `stack_intensity`, or `extend_duration`.
- `maxStacks`: Maximum stack count for intensity stacking.
- `dispellable`: Reserved for cleanse filtering.
- `tickDamage`, `tickDamagePerStack`: Start-of-turn damage.
- `modifiers`: Stat modifiers folded by `StatResolver`.

Current shipped effects:
- `burn`: Debuff, stacking damage over time.
- `weaken`: Debuff, flat ATK and DEF reduction.
- `vulnerable`: Debuff, percentage DEF reduction.
- `power_up`: Buff, ATK increase.
- `guard_up`: Buff, DEF increase.
- `haste`: Buff, SPD increase.

## Turn Timing
Status effects can create start-of-turn actions through `BuildTurnStartActions`.

The battle manager resolves those actions before the combatant receives input or AI control. This matters for damage-over-time because Burn can defeat a target before that target acts.

Turn-end duration countdown happens only after the combatant's normal queued actions finish. Newly applied data-driven effects skip their first immediate turn-end countdown so a skill applied during a turn remains visible for its intended duration.

## UI Rendering
`StatusIconRenderer` draws active status icons near each party HP bar.

Data files:
- `assets/UI/status_effect_icons.png`
- `assets/UI/status_effect_icons.json`
- `data/status_effect_ui.json`

The renderer displays:
- A small category frame color.
- The status icon frame from the atlas.
- Remaining turns.
- Stack count when greater than one.
- Overflow count when more effects are active than the configured visible limit.

## Asset Generation
Run:

```bat
python patches\generate_status_effect_icons.py
```

The script writes a deterministic icon atlas and metadata. The output is intentionally simple and replaceable. Production art can replace the PNG and JSON while keeping the same `iconId` contract.

## Extension Rules
- Add a new skill by creating a JSON file and adding it to a character's `skillPaths`.
- Add a new status by creating a JSON file under `data/status_effects`.
- Add a new status icon by updating `patches/generate_status_effect_icons.py`, regenerating the atlas, and using the new `iconId`.
- Add a new status behavior only when modifiers and tick damage cannot express it; implement that behavior inside an `IStatusEffect` or an `IAction`, not inside `BattleManager`.
- Keep CLI/debug names English by using debug localization fallback paths.
