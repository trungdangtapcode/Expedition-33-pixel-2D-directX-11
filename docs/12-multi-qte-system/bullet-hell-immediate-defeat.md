# Bullet Hell Immediate Defeat

## Goal
Bullet-hell attacks can damage the player during a long-running queued action. If that damage defeats the whole player party, the battle must enter defeat immediately instead of waiting for the dodge timer and any follow-up queued actions to finish.

## Design
`BulletHellAction` still owns only the dodge-phase simulation: movement, projectiles, hit detection, and damage application. When its defender is already defeated, or becomes defeated after a projectile hit, it publishes one inactive `BulletHellPayload` and returns `true` so the action can end cleanly.

`BattleManager` owns terminal battle outcomes. After every `ActionQueue::Update()` tick, `HandleResolving()` checks `AllPlayersDefeated()` before waiting for the queue to empty. If the party is defeated, `FinishDefeat()` clears the queue, clears the bullet-hell overlay with an inactive payload, logs the defeat message, and sets `BattleOutcome::DEFEAT`.

This keeps combat mutations inside `IAction::Execute()` while making battle termination responsive to damage that happens inside real-time sub-phases.

## Runtime Contract
- Damage is still applied through `Combatant::TakeDamage()`.
- `BulletHellAction` may end early when its target is defeated.
- `BattleManager` may clear the remaining action queue only after the whole player party is defeated.
- `BattleState` continues to observe `BattleOutcome::DEFEAT` and performs the existing death-animation wait and iris close.
- The bullet-hell renderer must always receive an inactive `verso_bullet_hell_state` payload when the dodge overlay is interrupted.

## Test Notes
- Start a battle against an enemy with a bullet-hell attack.
- Let the heart be hit until the active player party reaches zero HP.
- Confirm the dodge overlay disappears immediately and no additional enemy turns, queued waits, or bullet patterns continue.
- Confirm the battle log shows the localized defeat line and the existing defeat transition runs.
