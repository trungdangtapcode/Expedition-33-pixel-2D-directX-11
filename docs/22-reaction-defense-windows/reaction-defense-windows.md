# Reaction Defense Windows

## Purpose

Reaction defense windows are the first Sprint 1 implementation slice from the
full-game polish roadmap. They give enemy attacks a data-driven timing prompt
before damage resolves, using the existing animation, QTE overlay, damage
calculator, rage rules, and result tracker.

This is intentionally smaller than a full dodge/parry system. It creates the
correct extension point without replacing bullet-hell attacks or adding another
renderer.

## Runtime Flow

1. An enemy skill JSON can set `reactionWindowPath`.
2. `AttackSkill` loads that file and queues `ReactionDefenseAction` instead of
   the normal `AnimDamageAction` or `BulletHellAction`.
3. `ReactionDefenseAction` plays the attack animation and reads normalized
   animation progress through the existing `battler_get_anim_progress` event.
4. During the configured window it broadcasts `battler_qte_update`, so the
   existing QTE renderer shows the timing diamond.
5. If the player presses the configured key, the action classifies the timing as
   `Perfect`, `Good`, or `Miss`.
6. At the configured damage moment, damage is multiplied by the result:
   perfect can cancel damage, good can reduce damage, miss takes full damage.
7. The action broadcasts `battle_dodge_result`, so the result screen can count
   clean defensive reactions and hits taken.

All combat mutation still happens inside `IAction::Execute`.

## Data

Reaction window files live under:

```text
data/reaction_windows/*.json
```

Current file:

```text
data/reaction_windows/scout_slash.json
```

Fields:

- `id`: stable tuning id for debug and future analytics.
- `inputKey`: supported V1 values are `space`, `enter`, `f`, and `e`.
- `startMoment`: normalized attack animation progress where the prompt appears.
- `damageMoment`: normalized attack animation progress where damage resolves.
- `goodThreshold`: prompt-progress threshold for reduced damage.
- `perfectThreshold`: prompt-progress threshold for no damage.
- `perfectDamageMultiplier`: damage multiplier for perfect timing.
- `goodDamageMultiplier`: damage multiplier for good timing.
- `missDamageMultiplier`: damage multiplier for missing or pressing too early.
- `fadeInRatio`: existing QTE renderer fade-in tuning.
- `fadeOutDuration`: existing QTE renderer result-flash tuning.

Example:

```json
{
  "id": "scout_slash",
  "inputKey": "space",
  "startMoment": 0.32,
  "damageMoment": 0.66,
  "goodThreshold": 0.52,
  "perfectThreshold": 0.82,
  "perfectDamageMultiplier": 0.0,
  "goodDamageMultiplier": 0.35,
  "missDamageMultiplier": 1.0
}
```

Skill files opt in with:

```json
{
  "reactionWindowPath": "data/reaction_windows/scout_slash.json"
}
```

## Current Authoring

The solo skeleton attack and scout skeleton attack now use `scout_slash`.
This makes the early route teach timed defense before later enemies escalate
into bullet-hell patterns.

Bullet-hell remains the better tool for long defense phases. Reaction windows
are for single attack beats: slash, shot, heavy swing, spell impact, or boss
combo hits.

## Future Work

- Add separate visual styling for defensive prompts instead of reusing QTE
  diamonds.
- Add prompt text/icon metadata once the renderer supports it.
- Add SFX ids to reaction data and route them through the SFX bus.
- Add combo definitions so one enemy animation can contain several reaction
  windows.
- Add explicit dodge, parry, block, and counter result enums when mechanics
  need different rewards.

## Verification

- Build with `.\build_src_static.bat 2>&1`.
- Start a solo skeleton fight and confirm the enemy attack shows a timing prompt.
- Press Space near the end of the prompt and confirm no damage on perfect.
- Press Space in the middle and confirm reduced damage on good.
- Miss or press too early and confirm full damage.
- Confirm the victory result screen counts QTE/defense stats.
- Confirm zombie armour and other bullet-hell enemies still use bullet-hell.
