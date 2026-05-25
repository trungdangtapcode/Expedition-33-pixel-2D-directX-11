# Battle Skill UI and Status Visibility

## Goal

The battle skill picker now uses a screen-space card layout inspired by modern tactical JRPG menus.  It is not tied to the battle camera, so camera pans, zooms, and target focus no longer tilt or overlap the menu.

## Renderer Ownership

- `BattleState` owns `BattleSkillMenuRenderer`.
- `BattleSkillMenuRenderer` owns its SpriteBatch, CommonStates, a 1x1 fill texture, and the status icon atlas view.
- `BattleState::Render()` calls the renderer only during `SKILL_SELECT` and `TARGET_SELECT`.
- The old world-space skill list was removed.  The item menu still uses the legacy world-space list until it receives its own pass.

## Data Files

- `data/battle_skill_menu_layout.json` controls card position, card size, page size, detail panel position, icon sizes, colors through alpha values, and animation timing.
- `data/skills/*.json` may include optional UI metadata:
  - `uiSortGroup`
  - `hitCount`
  - `damageGradeKey`
  - `extraRuleKeys`
- `data/status_effects/*.json` may include optional short display metadata:
  - `shortDescriptionKey`
  - `durationLabelKey`

Missing optional fields are safe.  The UI falls back to the skill or status description already used by gameplay.

## Input Model

- Up and Down move inside the currently visible page.
- Left and Right change pages.
- Enter selects a usable skill.
- Back returns to the command list, or from target select back to the same selected skill.

The input controller reads `pageSize` from `data/battle_skill_menu_layout.json` so input and rendering stay aligned.

## Status Icons

`StatusIconRenderer` now has two anchoring modes:

- `Render()` remains the player HP bar path and applies the player HUD offsets from `data/status_effect_ui.json`.
- `RenderAt()` draws directly at a supplied screen-space anchor, used by enemy HP bars and detail previews.

`EnemyHpBarRenderer::GetStatusAnchor()` exposes a stable status strip position under each active enemy bar.  The offsets live in `assets/UI/enemy-hp-ui.json` as `status_anchor_offset_x` and `status_anchor_offset_y`.

## Localization

All player-facing labels are localization keys.  Debug HUD and log labels remain English-only so CLI output does not depend on terminal Unicode support.

New keys include:

- `battle.skill_ui.*`
- `battle.skill_target.*`
- `battle.damage_type.*`
- `battle.damage_grade.*`
- `status.*.short`

## Regression Notes

The renderer is screen-space only.  It does not mutate battle state and does not change skill execution.  MP, rage, target selection, QTE, bullet hell, item use, flee, victory, and defeat flows still run through the existing battle FSM and action queue.
