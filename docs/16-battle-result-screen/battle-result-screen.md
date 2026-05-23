# Battle Result Screen

## Summary

The battle result screen pauses the battle exit flow after a terminal outcome and presents a cinematic overlay on top of the frozen battle scene.

- Victory shows rewards, performance stats, no-damage bonus, and party progression.
- Defeat shows a failed splash, then a retry prompt.
- Retry restores the pre-battle party and wallet snapshots, then rebuilds the same encounter in place.
- Leave after defeat restores the pre-battle party and wallet snapshots, exits to overworld, and does not mark the enemy defeated.

## Data Ownership

- `BattleManager` owns combat simulation and exposes base reward totals.
- `BattleState` owns reward application and result flow timing.
- `BattleResultTracker` accumulates display-only stats from battle events.
- `BattleResultRenderer` draws the overlay from immutable `BattleResultData`.
- `PartyManager` and `Wallet` remain the only durable gameplay progress owners.

Rewards are applied once when victory results are created. This keeps the renderer pure and prevents duplicate EXP or coins if the player waits on the result screen.

## Layout And Audio

`data/battle_result_layout.json` owns screen positions, panel sizes, animation timing, the no-damage bonus percentage, and result SFX ids.
It also owns optional result-art texture paths for the defeat sigil, retry prompt panel, ink vignette, and victory flourish.

The committed generated source sheet lives at:

```text
source_assets/battle_result_ui_raw.png
```

Regenerate the sliced game assets with:

```bat
python patches\process_battle_result_assets.py
```

The script writes:

```text
assets/UI/battle_result/battle_result_defeat_sigil.png
assets/UI/battle_result/battle_result_prompt_panel.png
assets/UI/battle_result/battle_result_ink_vignette.png
assets/UI/battle_result/battle_result_victory_flourish.png
```

Result SFX live under:

```text
assets/sound/SFX/Battle/Result/
```

They are registered in `data/audio/sfx.json`, so the global SFX volume bus controls them automatically.

## Retry Rules

BattleState captures:

- party progress with `PartyManager::CaptureProgress()`
- wallet coins with `Wallet::GetCoins()`

On retry or defeat leave, those snapshots are restored before rebuilding or exiting. Victory does not restore snapshots; it applies rewards, persists surviving HP/MP from combatants, and broadcasts `battle_end_victory` only when the player confirms the result screen.

## Localization

All player-facing result labels use localization keys under `battle.result.*`.

English, Vietnamese, and French keys are provided. Code and docs remain English-only; translated strings live in localization data.

## Testing Checklist

- Build with `.\build_src_static.bat 2>&1`.
- Win a battle and confirm the result screen appears after death animations.
- Confirm EXP and coins are granted once only.
- Confirm no-damage bonus appears only when party damage received is zero.
- Confirm `[F]`, Enter, or Space continues from victory.
- Lose a battle and confirm the failed splash transitions to retry prompt.
- Confirm retry restarts the same encounter from pre-battle state.
- Confirm leave returns to overworld without removing the enemy.
